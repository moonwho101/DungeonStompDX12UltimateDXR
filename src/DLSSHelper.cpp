//***************************************************************************************
// DLSSHelper.cpp - NVIDIA DLSS integration implementation
//***************************************************************************************

#include "DLSSHelper.h"
#include "../Common/d3dApp.h"
#include <cmath>
#include <d3dcompiler.h>

#ifdef HAS_DLSS_SDK
#include <nvsdk_ngx.h>
#include <nvsdk_ngx_defs.h>
#include <nvsdk_ngx_helpers.h>
#endif

using namespace DirectX;
using Microsoft::WRL::ComPtr;

// DLSS Application ID (use 0 for development/testing)
static const unsigned long long DLSS_APP_ID = 0;

DLSSHelper::DLSSHelper() = default;

DLSSHelper::~DLSSHelper() {
	Shutdown();
}

//-----------------------------------------------------------------------------
// Halton sequence for temporal jitter (base 2 and 3)
//-----------------------------------------------------------------------------
float DLSSHelper::HaltonSequence(int index, int base) {
	float result = 0.0f;
	float fraction = 1.0f / static_cast<float>(base);
	int i = index;
	while (i > 0) {
		result += fraction * static_cast<float>(i % base);
		i /= base;
		fraction /= static_cast<float>(base);
	}
	return result;
}

void DLSSHelper::GetJitterOffset(UINT frameIndex, float &jitterX, float &jitterY) const {
	// Use Halton(2,3) sequence, 64-sample cycle
	int idx = (frameIndex % 64) + 1;
	jitterX = HaltonSequence(idx, 2) - 0.5f;
	jitterY = HaltonSequence(idx, 3) - 0.5f;
}

const char *DLSSHelper::GetQualityModeName(DLSSQualityMode mode) {
	switch (mode) {
	case DLSSQualityMode::UltraPerformance: return "Ultra Performance";
	case DLSSQualityMode::MaxPerformance:   return "Max Performance";
	case DLSSQualityMode::Balanced:         return "Balanced";
	case DLSSQualityMode::Quality:          return "Quality";
	case DLSSQualityMode::UltraQuality:     return "Ultra Quality";
	case DLSSQualityMode::DLAA:             return "DLAA";
	default:                                return "Off";
	}
}

DLSSQualityMode DLSSHelper::GetNextQualityMode() const {
	int next = static_cast<int>(mCurrentMode) + 1;
	if (next >= static_cast<int>(DLSSQualityMode::Count))
		next = static_cast<int>(DLSSQualityMode::UltraPerformance);
	return static_cast<DLSSQualityMode>(next);
}

//-----------------------------------------------------------------------------
// Calculate render resolution based on quality mode
//-----------------------------------------------------------------------------
void DLSSHelper::CalculateRenderResolution() {
#ifdef HAS_DLSS_SDK
	if (mNGXParams && mDisplayWidth > 0 && mDisplayHeight > 0) {
		NVSDK_NGX_PerfQuality_Value perfQuality;
		switch (mCurrentMode) {
		case DLSSQualityMode::UltraPerformance:
			perfQuality = NVSDK_NGX_PerfQuality_Value_UltraPerformance;
			break;
		case DLSSQualityMode::MaxPerformance:
			perfQuality = NVSDK_NGX_PerfQuality_Value_MaxPerf;
			break;
		case DLSSQualityMode::Balanced:
			perfQuality = NVSDK_NGX_PerfQuality_Value_Balanced;
			break;
		case DLSSQualityMode::Quality:
			perfQuality = NVSDK_NGX_PerfQuality_Value_MaxQuality;
			break;
		case DLSSQualityMode::UltraQuality:
			perfQuality = NVSDK_NGX_PerfQuality_Value_UltraQuality;
			break;
		case DLSSQualityMode::DLAA:
			perfQuality = NVSDK_NGX_PerfQuality_Value_DLAA;
			break;
		default:
			mRenderWidth = mDisplayWidth;
			mRenderHeight = mDisplayHeight;
			return;
		}

		unsigned int optW = 0, optH = 0, maxW = 0, maxH = 0, minW = 0, minH = 0;
		float sharpness = 0.0f;

		NVSDK_NGX_Result result = NGX_DLSS_GET_OPTIMAL_SETTINGS(
		    static_cast<NVSDK_NGX_Parameter *>(mNGXParams),
		    mDisplayWidth, mDisplayHeight,
		    perfQuality,
		    &optW, &optH,
		    &maxW, &maxH,
		    &minW, &minH,
		    &sharpness);

		if (NVSDK_NGX_SUCCEED(result) && optW > 0 && optH > 0) {
			mRenderWidth = optW;
			mRenderHeight = optH;
			mSharpness = sharpness;
		} else {
			// Fallback: use fixed ratios
			float ratio = 0.67f;
			if (mCurrentMode == DLSSQualityMode::MaxPerformance) ratio = 0.50f;
			else if (mCurrentMode == DLSSQualityMode::Balanced) ratio = 0.58f;
			else if (mCurrentMode == DLSSQualityMode::UltraPerformance) ratio = 0.33f;
			else if (mCurrentMode == DLSSQualityMode::UltraQuality) ratio = 0.77f;
			else if (mCurrentMode == DLSSQualityMode::DLAA) ratio = 1.0f;
			mRenderWidth = static_cast<UINT>(mDisplayWidth * ratio);
			mRenderHeight = static_cast<UINT>(mDisplayHeight * ratio);
		}
		return;
	}
#endif
	// Fallback without SDK: use fixed ratios
	float ratio = 0.67f;
	switch (mCurrentMode) {
	case DLSSQualityMode::UltraPerformance: ratio = 0.33f; break;
	case DLSSQualityMode::MaxPerformance:   ratio = 0.50f; break;
	case DLSSQualityMode::Balanced:         ratio = 0.58f; break;
	case DLSSQualityMode::Quality:          ratio = 0.67f; break;
	case DLSSQualityMode::UltraQuality:     ratio = 0.77f; break;
	case DLSSQualityMode::DLAA:             ratio = 1.0f;  break;
	default: ratio = 1.0f; break;
	}
	mRenderWidth = max(1U, static_cast<UINT>(mDisplayWidth * ratio));
	mRenderHeight = max(1U, static_cast<UINT>(mDisplayHeight * ratio));
}

//-----------------------------------------------------------------------------
// Create render-resolution resources (color, depth, motion vectors, output)
//-----------------------------------------------------------------------------
void DLSSHelper::CreateRenderResources(ID3D12Device *device) {
	ReleaseRenderResources();

	// --- Low-res color render target ---
	{
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Width = mRenderWidth;
		desc.Height = mRenderHeight;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = mBackBufferFormat;
		desc.SampleDesc.Count = 1;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = mBackBufferFormat;
		clearValue.Color[0] = 0.0f;
		clearValue.Color[1] = 0.0f;
		clearValue.Color[2] = 0.0f;
		clearValue.Color[3] = 1.0f;

		ThrowIfFailed(device->CreateCommittedResource(
		    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		    D3D12_HEAP_FLAG_NONE,
		    &desc,
		    D3D12_RESOURCE_STATE_RENDER_TARGET,
		    &clearValue,
		    IID_PPV_ARGS(&mColorBuffer)));
		mColorBuffer->SetName(L"DLSS Color Buffer");
	}

	// --- Low-res depth buffer (R32_TYPELESS for DSV + SRV) ---
	{
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Width = mRenderWidth;
		desc.Height = mRenderHeight;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_R32_TYPELESS;
		desc.SampleDesc.Count = 1;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = DXGI_FORMAT_D32_FLOAT;
		clearValue.DepthStencil.Depth = 1.0f;
		clearValue.DepthStencil.Stencil = 0;

		ThrowIfFailed(device->CreateCommittedResource(
		    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		    D3D12_HEAP_FLAG_NONE,
		    &desc,
		    D3D12_RESOURCE_STATE_DEPTH_WRITE,
		    &clearValue,
		    IID_PPV_ARGS(&mDepthBuffer)));
		mDepthBuffer->SetName(L"DLSS Depth Buffer");
	}

	// --- Motion vector buffer (R16G16_FLOAT, UAV for compute shader) ---
	{
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Width = mRenderWidth;
		desc.Height = mRenderHeight;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_R16G16_FLOAT;
		desc.SampleDesc.Count = 1;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		ThrowIfFailed(device->CreateCommittedResource(
		    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		    D3D12_HEAP_FLAG_NONE,
		    &desc,
		    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		    nullptr,
		    IID_PPV_ARGS(&mMotionVectorBuffer)));
		mMotionVectorBuffer->SetName(L"DLSS Motion Vectors");
	}

	// --- DLSS output buffer (display resolution) ---
	{
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Width = mDisplayWidth;
		desc.Height = mDisplayHeight;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = mBackBufferFormat;
		desc.SampleDesc.Count = 1;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		ThrowIfFailed(device->CreateCommittedResource(
		    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		    D3D12_HEAP_FLAG_NONE,
		    &desc,
		    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		    nullptr,
		    IID_PPV_ARGS(&mDLSSOutput)));
		mDLSSOutput->SetName(L"DLSS Output");
	}

	// --- Create RTV heap (1 descriptor for color buffer) ---
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = 1;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mRtvHeap)));

		mColorRTV = mRtvHeap->GetCPUDescriptorHandleForHeapStart();
		device->CreateRenderTargetView(mColorBuffer.Get(), nullptr, mColorRTV);
	}

	// --- Create DSV heap (1 descriptor for depth buffer) ---
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = 1;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mDsvHeap)));

		mDepthDSV = mDsvHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		device->CreateDepthStencilView(mDepthBuffer.Get(), &dsvDesc, mDepthDSV);
	}

	// --- Create SRV/UAV heap (SRV for depth, UAV for motion vectors, SRV for depth read) ---
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = 3;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mSrvUavHeap)));

		UINT incrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle(mSrvUavHeap->GetCPUDescriptorHandleForHeapStart());

		// Slot 0: SRV for depth buffer (R32_FLOAT view of R32_TYPELESS)
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(mDepthBuffer.Get(), &srvDesc, handle);

		// Slot 1: UAV for motion vectors
		handle.Offset(1, incrementSize);
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		device->CreateUnorderedAccessView(mMotionVectorBuffer.Get(), nullptr, &uavDesc, handle);

		// Slot 2: CBV for motion vector constant buffer
		handle.Offset(1, incrementSize);
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = mMVConstantBuffer->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = d3dUtil::CalcConstantBufferByteSize(sizeof(MotionVectorCB));
		device->CreateConstantBufferView(&cbvDesc, handle);
	}

	// --- Setup viewport and scissor rect at render resolution ---
	mRenderViewport.TopLeftX = 0.0f;
	mRenderViewport.TopLeftY = 0.0f;
	mRenderViewport.Width = static_cast<float>(mRenderWidth);
	mRenderViewport.Height = static_cast<float>(mRenderHeight);
	mRenderViewport.MinDepth = 0.0f;
	mRenderViewport.MaxDepth = 1.0f;

	mRenderScissorRect = { 0, 0, static_cast<LONG>(mRenderWidth), static_cast<LONG>(mRenderHeight) };

	char buf[256];
	sprintf_s(buf, "DLSS: Render %ux%u -> Display %ux%u (%s)\n",
	          mRenderWidth, mRenderHeight, mDisplayWidth, mDisplayHeight,
	          GetQualityModeName(mCurrentMode));
	OutputDebugStringA(buf);
}

void DLSSHelper::ReleaseRenderResources() {
	mColorBuffer.Reset();
	mDepthBuffer.Reset();
	mMotionVectorBuffer.Reset();
	mDLSSOutput.Reset();
	mRtvHeap.Reset();
	mDsvHeap.Reset();
	mSrvUavHeap.Reset();
}

//-----------------------------------------------------------------------------
// Create motion vector compute pipeline
//-----------------------------------------------------------------------------
void DLSSHelper::CreateMotionVectorPipeline(ID3D12Device *device) {
	// --- Root signature for motion vector compute shader ---
	// Root parameter 0: Descriptor table (SRV depth, UAV motion vectors, CBV constants)
	CD3DX12_DESCRIPTOR_RANGE1 ranges[3];
	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0: depth
	ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0); // u0: motion vectors
	ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // b0: constants

	CD3DX12_ROOT_PARAMETER1 rootParams[1];
	rootParams[0].InitAsDescriptorTable(3, ranges);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc;
	rootSigDesc.Init_1_1(1, rootParams, 0, nullptr);

	ComPtr<ID3DBlob> serializedRootSig;
	ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3DX12SerializeVersionedRootSignature(&rootSigDesc,
	    D3D_ROOT_SIGNATURE_VERSION_1_1, &serializedRootSig, &errorBlob);
	if (FAILED(hr)) {
		if (errorBlob) {
			OutputDebugStringA(static_cast<char *>(errorBlob->GetBufferPointer()));
		}
		ThrowIfFailed(hr);
	}

	ThrowIfFailed(device->CreateRootSignature(0,
	    serializedRootSig->GetBufferPointer(),
	    serializedRootSig->GetBufferSize(),
	    IID_PPV_ARGS(&mMVRootSignature)));

	// --- Compile compute shader ---
	ComPtr<ID3DBlob> csBlob;
	ComPtr<ID3DBlob> csErrors;

#if defined(DEBUG) || defined(_DEBUG)
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

	hr = D3DCompileFromFile(L"../Shaders/MotionVectors.hlsl", nullptr, nullptr,
	                        "CSMain", "cs_5_1", compileFlags, 0, &csBlob, &csErrors);
	if (FAILED(hr)) {
		if (csErrors) {
			OutputDebugStringA(static_cast<char *>(csErrors->GetBufferPointer()));
		}
		OutputDebugStringA("DLSS: Failed to compile MotionVectors.hlsl compute shader.\n");
		return;
	}

	// --- Create compute PSO ---
	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = mMVRootSignature.Get();
	psoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };

	ThrowIfFailed(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&mMVPipelineState)));
}

void DLSSHelper::CreateMotionVectorResources(ID3D12Device *device) {
	// Constant buffer for motion vector generation
	UINT cbSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MotionVectorCB));

	ThrowIfFailed(device->CreateCommittedResource(
	    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
	    D3D12_HEAP_FLAG_NONE,
	    &CD3DX12_RESOURCE_DESC::Buffer(cbSize),
	    D3D12_RESOURCE_STATE_GENERIC_READ,
	    nullptr,
	    IID_PPV_ARGS(&mMVConstantBuffer)));
	mMVConstantBuffer->SetName(L"DLSS MV Constant Buffer");

	ThrowIfFailed(mMVConstantBuffer->Map(0, nullptr, reinterpret_cast<void **>(&mMVCBMappedData)));
}

//-----------------------------------------------------------------------------
// Initialize
//-----------------------------------------------------------------------------
bool DLSSHelper::Initialize(ID3D12Device *device,
                            ID3D12GraphicsCommandList *cmdList,
                            ID3D12CommandQueue *cmdQueue,
                            UINT displayWidth, UINT displayHeight,
                            DXGI_FORMAT backBufferFormat) {
	mDevice = device;
	mCommandQueue = cmdQueue;
	mDisplayWidth = displayWidth;
	mDisplayHeight = displayHeight;
	mBackBufferFormat = backBufferFormat;

#ifdef HAS_DLSS_SDK
	// Initialize NGX
	NVSDK_NGX_Result result = NVSDK_NGX_D3D12_Init(DLSS_APP_ID, L".", device);
	if (NVSDK_NGX_FAILED(result)) {
		char buf[256];
		sprintf_s(buf, "DLSS: NGX initialization failed (0x%08x). NVIDIA GPU with driver 470+ required.\n",
		          static_cast<unsigned int>(result));
		OutputDebugStringA(buf);
		mSupported = false;
		return false;
	}

	// Get capability parameters
	NVSDK_NGX_Parameter *params = nullptr;
	result = NVSDK_NGX_D3D12_GetCapabilityParameters(&params);
	if (NVSDK_NGX_FAILED(result) || !params) {
		OutputDebugStringA("DLSS: Failed to get NGX capability parameters.\n");
		NVSDK_NGX_D3D12_Shutdown1(device);
		mSupported = false;
		return false;
	}
	mNGXParams = params;

	// Check DLSS availability
	int dlssAvailable = 0;
	params->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &dlssAvailable);
	if (!dlssAvailable) {
		OutputDebugStringA("DLSS: Super Sampling not available on this hardware.\n");
		NVSDK_NGX_D3D12_Shutdown1(device);
		mNGXParams = nullptr;
		mSupported = false;
		return false;
	}

	mSupported = true;
	OutputDebugStringA("DLSS: NVIDIA DLSS is supported and available.\n");

	// Create motion vector pipeline and resources
	CreateMotionVectorResources(device);
	CreateMotionVectorPipeline(device);

	// Calculate initial render resolution
	CalculateRenderResolution();
	CreateRenderResources(device);

	// Create DLSS feature
	if (!SetQualityMode(cmdList, mCurrentMode)) {
		OutputDebugStringA("DLSS: Failed to create initial DLSS feature.\n");
		Shutdown();
		return false;
	}

	mInitialized = true;
	OutputDebugStringA("DLSS: Initialization complete.\n");
	return true;

#else
	OutputDebugStringA("DLSS: SDK not available (HAS_DLSS_SDK not defined). DLSS disabled.\n");
	OutputDebugStringA("DLSS: Download SDK from https://developer.nvidia.com/rtx/dlss/get-started\n");
	OutputDebugStringA("DLSS: Place headers in ThirdParty/DLSS/include/ and libs in ThirdParty/DLSS/lib/x64/\n");
	OutputDebugStringA("DLSS: Define HAS_DLSS_SDK preprocessor to enable.\n");
	mSupported = false;
	mInitialized = false;

	// Still create motion vector pipeline for when SDK becomes available
	CreateMotionVectorResources(device);
	CreateMotionVectorPipeline(device);
	CalculateRenderResolution();
	CreateRenderResources(device);

	return false;
#endif
}

//-----------------------------------------------------------------------------
// Set quality mode and create/recreate DLSS feature
//-----------------------------------------------------------------------------
bool DLSSHelper::SetQualityMode(ID3D12GraphicsCommandList *cmdList, DLSSQualityMode mode) {
	mCurrentMode = mode;
	CalculateRenderResolution();

	if (mDevice) {
		CreateRenderResources(mDevice);
	}

#ifdef HAS_DLSS_SDK
	if (!mSupported || !mNGXParams)
		return false;

	auto params = static_cast<NVSDK_NGX_Parameter *>(mNGXParams);

	// Release existing feature
	if (mDLSSHandle) {
		NVSDK_NGX_D3D12_ReleaseFeature(static_cast<NVSDK_NGX_Handle *>(mDLSSHandle));
		mDLSSHandle = nullptr;
	}

	// Map quality mode to NGX perf quality
	NVSDK_NGX_PerfQuality_Value perfQuality;
	switch (mode) {
	case DLSSQualityMode::UltraPerformance:
		perfQuality = NVSDK_NGX_PerfQuality_Value_UltraPerformance;
		break;
	case DLSSQualityMode::MaxPerformance:
		perfQuality = NVSDK_NGX_PerfQuality_Value_MaxPerf;
		break;
	case DLSSQualityMode::Balanced:
		perfQuality = NVSDK_NGX_PerfQuality_Value_Balanced;
		break;
	case DLSSQualityMode::Quality:
		perfQuality = NVSDK_NGX_PerfQuality_Value_MaxQuality;
		break;
	case DLSSQualityMode::UltraQuality:
		perfQuality = NVSDK_NGX_PerfQuality_Value_UltraQuality;
		break;
	case DLSSQualityMode::DLAA:
		perfQuality = NVSDK_NGX_PerfQuality_Value_DLAA;
		break;
	default:
		return false;
	}

	// Create DLSS feature
	NVSDK_NGX_DLSS_Create_Params dlssCreateParams = {};
	dlssCreateParams.Feature.InWidth = mRenderWidth;
	dlssCreateParams.Feature.InHeight = mRenderHeight;
	dlssCreateParams.Feature.InTargetWidth = mDisplayWidth;
	dlssCreateParams.Feature.InTargetHeight = mDisplayHeight;
	dlssCreateParams.Feature.InPerfQualityValue = perfQuality;
	dlssCreateParams.InFeatureCreateFlags =
	    NVSDK_NGX_DLSS_Feature_Flags_MVLowRes |
	    NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;

	NVSDK_NGX_Handle *handle = nullptr;
	NVSDK_NGX_Result result = NGX_D3D12_CREATE_DLSS_EXT(
	    cmdList,
	    1, 1,
	    &handle,
	    params,
	    &dlssCreateParams);

	if (NVSDK_NGX_FAILED(result) || !handle) {
		char buf[256];
		sprintf_s(buf, "DLSS: Failed to create feature for mode %s (0x%08x)\n",
		          GetQualityModeName(mode), static_cast<unsigned int>(result));
		OutputDebugStringA(buf);
		return false;
	}

	mDLSSHandle = handle;

	char buf[256];
	sprintf_s(buf, "DLSS: Feature created - %s (%ux%u -> %ux%u)\n",
	          GetQualityModeName(mode), mRenderWidth, mRenderHeight,
	          mDisplayWidth, mDisplayHeight);
	OutputDebugStringA(buf);
	return true;

#else
	// Without SDK, we still set up resources for the render resolution change
	return false;
#endif
}

//-----------------------------------------------------------------------------
// Evaluate DLSS (upscale)
//-----------------------------------------------------------------------------
void DLSSHelper::Evaluate(ID3D12GraphicsCommandList *cmdList,
                          ID3D12Resource *colorInput,
                          ID3D12Resource *depthInput,
                          ID3D12Resource *motionVectors,
                          float jitterX, float jitterY,
                          float deltaTime,
                          bool reset) {
#ifdef HAS_DLSS_SDK
	if (!mInitialized || !mDLSSHandle || !mNGXParams)
		return;

	auto params = static_cast<NVSDK_NGX_Parameter *>(mNGXParams);
	auto handle = static_cast<NVSDK_NGX_Handle *>(mDLSSHandle);

	// Transition resources to the states DLSS expects
	D3D12_RESOURCE_BARRIER barriers[4];
	int numBarriers = 0;

	// Color input -> SRV (shader resource)
	barriers[numBarriers++] = CD3DX12_RESOURCE_BARRIER::Transition(
	    colorInput, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	// Depth -> SRV
	if (depthInput) {
		barriers[numBarriers++] = CD3DX12_RESOURCE_BARRIER::Transition(
		    depthInput, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}

	// Motion vectors should already be in UAV state from compute, transition to SRV
	if (motionVectors) {
		barriers[numBarriers++] = CD3DX12_RESOURCE_BARRIER::Transition(
		    motionVectors, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}

	// Output -> UAV
	barriers[numBarriers++] = CD3DX12_RESOURCE_BARRIER::Transition(
	    mDLSSOutput.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	numBarriers--; // Already in UAV state, skip

	cmdList->ResourceBarrier(numBarriers, barriers);

	// Evaluate DLSS
	NVSDK_NGX_D3D12_DLSS_Eval_Params evalParams = {};
	evalParams.Feature.pInColor = colorInput;
	evalParams.Feature.pInOutput = mDLSSOutput.Get();
	evalParams.Feature.InSharpness = mSharpness;
	evalParams.pInDepth = depthInput;
	evalParams.pInMotionVectors = motionVectors;
	evalParams.InJitterOffsetX = jitterX;
	evalParams.InJitterOffsetY = jitterY;
	evalParams.InReset = reset ? 1 : 0;
	evalParams.InMVScaleX = 1.0f;
	evalParams.InMVScaleY = 1.0f;
	evalParams.InRenderSubrectDimensions.Width = mRenderWidth;
	evalParams.InRenderSubrectDimensions.Height = mRenderHeight;

	NVSDK_NGX_Result result = NGX_D3D12_EVALUATE_DLSS_EXT(
	    cmdList,
	    handle,
	    params,
	    &evalParams);

	if (NVSDK_NGX_FAILED(result)) {
		char buf[256];
		sprintf_s(buf, "DLSS: Evaluate failed (0x%08x)\n", static_cast<unsigned int>(result));
		OutputDebugStringA(buf);
	}

	// Transition resources back
	numBarriers = 0;
	barriers[numBarriers++] = CD3DX12_RESOURCE_BARRIER::Transition(
	    colorInput, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

	if (depthInput) {
		barriers[numBarriers++] = CD3DX12_RESOURCE_BARRIER::Transition(
		    depthInput, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	}

	if (motionVectors) {
		barriers[numBarriers++] = CD3DX12_RESOURCE_BARRIER::Transition(
		    motionVectors, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}

	cmdList->ResourceBarrier(numBarriers, barriers);

#else
	(void)cmdList;
	(void)colorInput;
	(void)depthInput;
	(void)motionVectors;
	(void)jitterX;
	(void)jitterY;
	(void)deltaTime;
	(void)reset;
#endif
}

//-----------------------------------------------------------------------------
// Generate motion vectors from depth reprojection
//-----------------------------------------------------------------------------
void DLSSHelper::GenerateMotionVectors(ID3D12GraphicsCommandList *cmdList,
                                       ID3D12Resource *depthBuffer,
                                       const DirectX::XMFLOAT4X4 &invViewProj,
                                       const DirectX::XMFLOAT4X4 &prevViewProj) {
	if (!mMVPipelineState || !mMVRootSignature || !mMVConstantBuffer)
		return;

	// Update constant buffer
	MotionVectorCB cb;
	cb.InvViewProj = invViewProj;
	cb.PrevViewProj = prevViewProj;
	cb.RenderWidth = static_cast<float>(mRenderWidth);
	cb.RenderHeight = static_cast<float>(mRenderHeight);
	cb.InvRenderWidth = 1.0f / static_cast<float>(mRenderWidth);
	cb.InvRenderHeight = 1.0f / static_cast<float>(mRenderHeight);
	memcpy(mMVCBMappedData, &cb, sizeof(MotionVectorCB));

	// Transition depth to SRV for reading
	D3D12_RESOURCE_BARRIER barriers[1];
	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
	    depthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	cmdList->ResourceBarrier(1, barriers);

	// Set compute pipeline
	cmdList->SetComputeRootSignature(mMVRootSignature.Get());
	cmdList->SetPipelineState(mMVPipelineState.Get());

	// Bind descriptor heap
	ID3D12DescriptorHeap *heaps[] = { mSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(1, heaps);
	cmdList->SetComputeRootDescriptorTable(0, mSrvUavHeap->GetGPUDescriptorHandleForHeapStart());

	// Dispatch compute shader (8x8 thread groups)
	UINT groupsX = (mRenderWidth + 7) / 8;
	UINT groupsY = (mRenderHeight + 7) / 8;
	cmdList->Dispatch(groupsX, groupsY, 1);

	// Transition depth back to depth write
	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
	    depthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	cmdList->ResourceBarrier(1, barriers);
}

//-----------------------------------------------------------------------------
// Copy DLSS output to back buffer
//-----------------------------------------------------------------------------
void DLSSHelper::CopyOutputToBackBuffer(ID3D12GraphicsCommandList *cmdList,
                                         ID3D12Resource *backBuffer) {
	D3D12_RESOURCE_BARRIER barriers[2];
	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
	    mDLSSOutput.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
	    backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
	cmdList->ResourceBarrier(2, barriers);

	cmdList->CopyResource(backBuffer, mDLSSOutput.Get());

	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
	    mDLSSOutput.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
	    backBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
	cmdList->ResourceBarrier(2, barriers);
}

//-----------------------------------------------------------------------------
// Handle resize
//-----------------------------------------------------------------------------
void DLSSHelper::OnResize(ID3D12GraphicsCommandList *cmdList, UINT displayWidth, UINT displayHeight) {
	if (displayWidth == mDisplayWidth && displayHeight == mDisplayHeight)
		return;

	mDisplayWidth = displayWidth;
	mDisplayHeight = displayHeight;

	CalculateRenderResolution();

	if (mDevice) {
		CreateRenderResources(mDevice);
	}

#ifdef HAS_DLSS_SDK
	if (mInitialized && mSupported) {
		SetQualityMode(cmdList, mCurrentMode);
	}
#else
	(void)cmdList;
#endif

	char buf[256];
	sprintf_s(buf, "DLSS: Resized - Render %ux%u -> Display %ux%u\n",
	          mRenderWidth, mRenderHeight, mDisplayWidth, mDisplayHeight);
	OutputDebugStringA(buf);
}

//-----------------------------------------------------------------------------
// Shutdown
//-----------------------------------------------------------------------------
void DLSSHelper::Shutdown() {
#ifdef HAS_DLSS_SDK
	if (mDLSSHandle) {
		NVSDK_NGX_D3D12_ReleaseFeature(static_cast<NVSDK_NGX_Handle *>(mDLSSHandle));
		mDLSSHandle = nullptr;
	}
	if (mDevice) {
		NVSDK_NGX_D3D12_Shutdown1(mDevice);
	}
	mNGXParams = nullptr;
#endif

	if (mMVCBMappedData && mMVConstantBuffer) {
		mMVConstantBuffer->Unmap(0, nullptr);
		mMVCBMappedData = nullptr;
	}

	ReleaseRenderResources();
	mMVConstantBuffer.Reset();
	mMVRootSignature.Reset();
	mMVPipelineState.Reset();

	mInitialized = false;
	mSupported = false;
	mDevice = nullptr;
	mCommandQueue = nullptr;
}
