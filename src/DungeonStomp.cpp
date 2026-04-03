//***************************************************************************************
// DungeonStompApp.cpp by Mark Longo (C) 2022 All Rights Reserved.
//***************************************************************************************

#include "../Common/d3dApp.h"
#include "../Common/MathHelper.h"
#include "../Common/UploadBuffer.h"
#include "../Common/GeometryGenerator.h"
#include "FrameResource.h"
#include "Dungeon.h"
#include <d3dtypes.h>
#include "world.hpp"
#include "GlobalSettings.hpp"
#include "Missle.hpp"
#include "GameLogic.hpp"
#include "DungeonStomp.hpp"
#include "Ssao.h"
#include "CameraBob.hpp"
#include "VRSHelper.h"
#include "DXRHelper.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

Font LoadFont(LPCWSTR filename, int windowWidth, int windowHeight);

const int gNumFrameResources = 3;

extern Font arialFont; // this will store our arial font information
extern int maxNumTextCharacters;
extern int maxNumRectangleCharacters;

CameraBob bobY;
CameraBob bobX;

std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
extern int number_of_tex_aliases;

ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;
extern ID3D12PipelineState *textPSO;                    // pso containing a pipeline state
extern ID3D12PipelineState *rectanglePSO[MaxRectangle]; // pso containing a pipeline state

DungeonStompApp *gApp = nullptr;

void TriggerLandingDip(float amount) {
	if (gApp)
		gApp->mLandingDip = amount;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
                   PSTR cmdLine, int showCmd) {
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try {
		DungeonStompApp theApp(hInstance);
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	} catch (DxException &e) {
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

DungeonStompApp::DungeonStompApp(HINSTANCE hInstance)
    : D3DApp(hInstance) {
	gApp = this;

	// Estimate the scene bounding sphere
	mSceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);

	float scale = 1415.0f;
	mSceneBounds.Radius = sqrtf((10.0f * 10.0f) * scale + (15.0f * 15.0f) * scale);
}

DungeonStompApp::~DungeonStompApp() {
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

bool DungeonStompApp::Initialize() {
	if (!D3DApp::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// Get the increment size of a descriptor in this heap type.  This is hardware specific, so we have
	// to query this information.
	mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	mDungeon = std::make_unique<Dungeon>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);

	mShadowMap = std::make_unique<ShadowMap>(md3dDevice.Get(), 2048, 2048);

	mSsao = std::make_unique<Ssao>(
	    md3dDevice.Get(),
	    mCommandList.Get(),
	    mClientWidth, mClientHeight);

	// Initialize Variable Rate Shading (DX12 Ultimate)
	mVRSHelper.Initialize(md3dDevice.Get());

	// Initialize DirectX Raytracing (DXR)
	mDXRHelper = std::make_unique<DXRHelper>();
	if (mDX12UltimateFeatures.RaytracingSupported) {
		mDXRInitialized = mDXRHelper->Initialize(
		    md3dDevice.Get(),
		    mCommandList.Get(),
		    mClientWidth, mClientHeight,
		    mBackBufferFormat);
		if (mDXRInitialized) {
			OutputDebugStringA("DXR: DirectX Raytracing initialized successfully.\n");
		}
	} else {
		OutputDebugStringA("DXR: Raytracing not supported on this device.\n");
	}
	BuildMaterials();
	LoadTextures();
	BuildRootSignature();
	BuildSsaoRootSignature();
	BuildDescriptorHeaps();

	// Copy texture descriptors to DXR heap for raytracing
	if (mDXRInitialized && mDXRHelper) {
		mDXRHelper->CopyTextureDescriptors(md3dDevice.Get(), mSrvDescriptorHeap.Get(), MAX_NUM_TEXTURES);
	}

	BuildShadersAndInputLayout();
	BuildLandGeometry();
	BuildDungeonGeometryBuffers();

	BuildRenderItems();
	BuildFrameResources();
	BuildPSOs();
	mSsao->SetPSOs(mPSOs["ssao"].Get(), mPSOs["ssaoBlur"].Get());

	InitDS();

	// Set headbob
	bobX.SinWave(4.0f, 2.0f, 2.0f);
	bobY.SinWave(4.0f, 2.0f, 4.0f);

	arialFont = LoadFont(L"Arial.fnt", 800, 600);

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList *cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Set the Text Buffer
	textVertexBufferView.BufferLocation = textVertexBuffer->GetGPUVirtualAddress();
	textVertexBufferView.StrideInBytes = sizeof(TextVertex);
	textVertexBufferView.SizeInBytes = maxNumTextCharacters * sizeof(TextVertex);

	// Set the Rectangle Buffer
	for (int i = 0; i < MaxRectangle; ++i) {
		rectangleVertexBufferView[i].BufferLocation = rectangleVertexBuffer[i]->GetGPUVirtualAddress();
		rectangleVertexBufferView[i].StrideInBytes = sizeof(TextVertex);
		rectangleVertexBufferView[i].SizeInBytes = maxNumRectangleCharacters * sizeof(TextVertex);
	}
	// Wait until initialization is complete.
	FlushCommandQueue();

#if defined(DEBUG) || defined(_DEBUG)
#else
	// Go borderless fullscreen in release mode (modern approach, no exclusive fullscreen).
	ToggleBorderlessFullscreen();
#endif

	return true;
}

void DungeonStompApp::OnResize() {
	D3DApp::OnResize();

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	// XMMATRIX P = XMMatrixPerspectiveFovLH(5*MathHelper::Pi/18, AspectRatio(), 1.0f, 10000.0f); //50
	// XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f); //45

	float angle = 50.0f;
	float fov = angle * (MathHelper::Pi / 180.0f);
	cullAngle = 60.0f;

	XMMATRIX P = XMMatrixPerspectiveFovLH(fov, AspectRatio(), 1.0f, 10000.0f);

	XMStoreFloat4x4(&mProj, P);

	if (mSsao != nullptr) {
		mSsao->OnResize(mClientWidth, mClientHeight);

		// Resources changed, so need to rebuild descriptors.
		mSsao->RebuildDescriptors(mDepthStencilBuffer.Get());
	}

	// Resize DXR output texture
	if (mDXRHelper != nullptr && mDXRInitialized) {
		mDXRHelper->OnResize(md3dDevice.Get(), mClientWidth, mClientHeight);
	}
}

void DungeonStompApp::BuildRootSignature() {

	const int rootItems = 8;

	// Use RS 1.1 descriptor ranges with flags for driver optimization.
	D3D12_DESCRIPTOR_RANGE1 texTable0 = {};
	texTable0.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	texTable0.NumDescriptors = 1;
	texTable0.BaseShaderRegister = 0;
	texTable0.RegisterSpace = 0;
	texTable0.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	texTable0.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE1 texTable1 = {};
	texTable1.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	texTable1.NumDescriptors = 1;
	texTable1.BaseShaderRegister = 1;
	texTable1.RegisterSpace = 0;
	texTable1.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	texTable1.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE1 texTable2 = {};
	texTable2.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	texTable2.NumDescriptors = 1;
	texTable2.BaseShaderRegister = 2;
	texTable2.RegisterSpace = 0;
	texTable2.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	texTable2.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE1 texTable3 = {};
	texTable3.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	texTable3.NumDescriptors = 2;
	texTable3.BaseShaderRegister = 3;
	texTable3.RegisterSpace = 0;
	texTable3.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	texTable3.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE1 texTable4 = {};
	texTable4.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	texTable4.NumDescriptors = 1;
	texTable4.BaseShaderRegister = 5;
	texTable4.RegisterSpace = 0;
	texTable4.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	texTable4.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// Root parameters using RS 1.1 with DATA_VOLATILE for CBVs.
	D3D12_ROOT_PARAMETER1 slotRootParameter[rootItems] = {};

	slotRootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	slotRootParameter[0].Descriptor.ShaderRegister = 0;
	slotRootParameter[0].Descriptor.RegisterSpace = 0;
	slotRootParameter[0].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE;
	slotRootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	slotRootParameter[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	slotRootParameter[1].Descriptor.ShaderRegister = 1;
	slotRootParameter[1].Descriptor.RegisterSpace = 0;
	slotRootParameter[1].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE;
	slotRootParameter[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	slotRootParameter[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	slotRootParameter[2].Descriptor.ShaderRegister = 2;
	slotRootParameter[2].Descriptor.RegisterSpace = 0;
	slotRootParameter[2].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE;
	slotRootParameter[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	slotRootParameter[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	slotRootParameter[3].DescriptorTable.NumDescriptorRanges = 1;
	slotRootParameter[3].DescriptorTable.pDescriptorRanges = &texTable0;
	slotRootParameter[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	slotRootParameter[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	slotRootParameter[4].DescriptorTable.NumDescriptorRanges = 1;
	slotRootParameter[4].DescriptorTable.pDescriptorRanges = &texTable1;
	slotRootParameter[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	slotRootParameter[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	slotRootParameter[5].DescriptorTable.NumDescriptorRanges = 1;
	slotRootParameter[5].DescriptorTable.pDescriptorRanges = &texTable2;
	slotRootParameter[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	slotRootParameter[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	slotRootParameter[6].DescriptorTable.NumDescriptorRanges = 1;
	slotRootParameter[6].DescriptorTable.pDescriptorRanges = &texTable3;
	slotRootParameter[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	slotRootParameter[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	slotRootParameter[7].DescriptorTable.NumDescriptorRanges = 1;
	slotRootParameter[7].DescriptorTable.pDescriptorRanges = &texTable4;
	slotRootParameter[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	auto staticSamplers = GetStaticSamplers();
	const D3D12_ROOT_SIGNATURE_FLAGS rootSigFlags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = E_FAIL;

	if (mDX12UltimateFeatures.HighestRootSignatureVersion >= D3D_ROOT_SIGNATURE_VERSION_1_1) {
		// Build versioned root signature desc (RS 1.1)
		D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionedRootSigDesc = {};
		versionedRootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		versionedRootSigDesc.Desc_1_1.NumParameters = rootItems;
		versionedRootSigDesc.Desc_1_1.pParameters = slotRootParameter;
		versionedRootSigDesc.Desc_1_1.NumStaticSamplers = (UINT)staticSamplers.size();
		versionedRootSigDesc.Desc_1_1.pStaticSamplers = staticSamplers.data();
		versionedRootSigDesc.Desc_1_1.Flags = rootSigFlags;

		hr = D3D12SerializeVersionedRootSignature(&versionedRootSigDesc,
		                                          serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
	} else {
		// Fallback for older drivers/runtime: RS 1.0 equivalent layout.
		CD3DX12_DESCRIPTOR_RANGE texTable0_10;
		texTable0_10.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_DESCRIPTOR_RANGE texTable1_10;
		texTable1_10.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

		CD3DX12_DESCRIPTOR_RANGE texTable2_10;
		texTable2_10.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);

		CD3DX12_DESCRIPTOR_RANGE texTable3_10;
		texTable3_10.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 3);

		CD3DX12_DESCRIPTOR_RANGE texTable4_10;
		texTable4_10.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5);

		CD3DX12_ROOT_PARAMETER slotRootParameter10[rootItems];
		slotRootParameter10[0].InitAsConstantBufferView(0);
		slotRootParameter10[1].InitAsConstantBufferView(1);
		slotRootParameter10[2].InitAsConstantBufferView(2);
		slotRootParameter10[3].InitAsDescriptorTable(1, &texTable0_10, D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter10[4].InitAsDescriptorTable(1, &texTable1_10, D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter10[5].InitAsDescriptorTable(1, &texTable2_10, D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter10[6].InitAsDescriptorTable(1, &texTable3_10, D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter10[7].InitAsDescriptorTable(1, &texTable4_10, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		    rootItems,
		    slotRootParameter10,
		    (UINT)staticSamplers.size(),
		    staticSamplers.data(),
		    rootSigFlags);

		hr = D3D12SerializeRootSignature(
		    &rootSigDesc,
		    D3D_ROOT_SIGNATURE_VERSION_1,
		    serializedRootSig.GetAddressOf(),
		    errorBlob.GetAddressOf());

		OutputDebugStringA("Main root signature: using RS 1.0 fallback.\n");
	}

	if (errorBlob != nullptr) {
		::OutputDebugStringA((char *)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
	    0,
	    serializedRootSig->GetBufferPointer(),
	    serializedRootSig->GetBufferSize(),
	    IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void DungeonStompApp::BuildSsaoRootSignature() {
	D3D12_DESCRIPTOR_RANGE1 texTable0 = {};
	texTable0.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	texTable0.NumDescriptors = 2;
	texTable0.BaseShaderRegister = 0;
	texTable0.RegisterSpace = 0;
	texTable0.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	texTable0.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE1 texTable1 = {};
	texTable1.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	texTable1.NumDescriptors = 1;
	texTable1.BaseShaderRegister = 2;
	texTable1.RegisterSpace = 0;
	texTable1.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	texTable1.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// Root parameters using RS 1.1
	D3D12_ROOT_PARAMETER1 slotRootParameter[4] = {};

	// CBV at b0
	slotRootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	slotRootParameter[0].Descriptor.ShaderRegister = 0;
	slotRootParameter[0].Descriptor.RegisterSpace = 0;
	slotRootParameter[0].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE;
	slotRootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// 32-bit constants at b1
	slotRootParameter[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	slotRootParameter[1].Constants.ShaderRegister = 1;
	slotRootParameter[1].Constants.RegisterSpace = 0;
	slotRootParameter[1].Constants.Num32BitValues = 1;
	slotRootParameter[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// SRV table (2 textures)
	slotRootParameter[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	slotRootParameter[2].DescriptorTable.NumDescriptorRanges = 1;
	slotRootParameter[2].DescriptorTable.pDescriptorRanges = &texTable0;
	slotRootParameter[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// SRV table (1 texture)
	slotRootParameter[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	slotRootParameter[3].DescriptorTable.NumDescriptorRanges = 1;
	slotRootParameter[3].DescriptorTable.pDescriptorRanges = &texTable1;
	slotRootParameter[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
	    0,                                 // shaderRegister
	    D3D12_FILTER_MIN_MAG_MIP_POINT,    // filter
	    D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
	    D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
	    D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
	    1,                                 // shaderRegister
	    D3D12_FILTER_MIN_MAG_MIP_LINEAR,   // filter
	    D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
	    D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
	    D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC depthMapSam(
	    2,                                 // shaderRegister
	    D3D12_FILTER_MIN_MAG_MIP_LINEAR,   // filter
	    D3D12_TEXTURE_ADDRESS_MODE_BORDER, // addressU
	    D3D12_TEXTURE_ADDRESS_MODE_BORDER, // addressV
	    D3D12_TEXTURE_ADDRESS_MODE_BORDER, // addressW
	    0.0f,
	    0,
	    D3D12_COMPARISON_FUNC_LESS_EQUAL,
	    D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
	    3,                                // shaderRegister
	    D3D12_FILTER_MIN_MAG_MIP_LINEAR,  // filter
	    D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
	    D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
	    D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	std::array<CD3DX12_STATIC_SAMPLER_DESC, 4> staticSamplers = {
		pointClamp, linearClamp, depthMapSam, linearWrap
	};
	const D3D12_ROOT_SIGNATURE_FLAGS rootSigFlags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = E_FAIL;

	if (mDX12UltimateFeatures.HighestRootSignatureVersion >= D3D_ROOT_SIGNATURE_VERSION_1_1) {
		// A root signature is an array of root parameters (RS 1.1).
		D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionedRootSigDesc = {};
		versionedRootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		versionedRootSigDesc.Desc_1_1.NumParameters = 4;
		versionedRootSigDesc.Desc_1_1.pParameters = slotRootParameter;
		versionedRootSigDesc.Desc_1_1.NumStaticSamplers = (UINT)staticSamplers.size();
		versionedRootSigDesc.Desc_1_1.pStaticSamplers = staticSamplers.data();
		versionedRootSigDesc.Desc_1_1.Flags = rootSigFlags;

		hr = D3D12SerializeVersionedRootSignature(&versionedRootSigDesc,
		                                          serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
	} else {
		CD3DX12_DESCRIPTOR_RANGE texTable0_10;
		texTable0_10.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);

		CD3DX12_DESCRIPTOR_RANGE texTable1_10;
		texTable1_10.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);

		CD3DX12_ROOT_PARAMETER slotRootParameter10[4];
		slotRootParameter10[0].InitAsConstantBufferView(0);
		slotRootParameter10[1].InitAsConstants(1, 1, 0, D3D12_SHADER_VISIBILITY_ALL);
		slotRootParameter10[2].InitAsDescriptorTable(1, &texTable0_10, D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter10[3].InitAsDescriptorTable(1, &texTable1_10, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		    4,
		    slotRootParameter10,
		    (UINT)staticSamplers.size(),
		    staticSamplers.data(),
		    rootSigFlags);

		hr = D3D12SerializeRootSignature(
		    &rootSigDesc,
		    D3D_ROOT_SIGNATURE_VERSION_1,
		    serializedRootSig.GetAddressOf(),
		    errorBlob.GetAddressOf());

		OutputDebugStringA("SSAO root signature: using RS 1.0 fallback.\n");
	}

	if (errorBlob != nullptr) {
		::OutputDebugStringA((char *)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
	    0,
	    serializedRootSig->GetBufferPointer(),
	    serializedRootSig->GetBufferSize(),
	    IID_PPV_ARGS(mSsaoRootSignature.GetAddressOf())));
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> DungeonStompApp::GetStaticSamplers() {
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
	    0,                                // shaderRegister
	    D3D12_FILTER_MIN_MAG_MIP_POINT,   // filter
	    D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
	    D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
	    D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
	    1,                                 // shaderRegister
	    D3D12_FILTER_MIN_MAG_MIP_POINT,    // filter
	    D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
	    D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
	    D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
	    2,                                // shaderRegister
	    D3D12_FILTER_MIN_MAG_MIP_LINEAR,  // filter
	    D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
	    D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
	    D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
	    3,                                 // shaderRegister
	    D3D12_FILTER_MIN_MAG_MIP_LINEAR,   // filter
	    D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
	    D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
	    D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
	    4,                               // shaderRegister
	    D3D12_FILTER_ANISOTROPIC,        // filter
	    D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressU
	    D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressV
	    D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressW
	    0.0f,                            // mipLODBias
	    8);                              // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
	    5,                                // shaderRegister
	    D3D12_FILTER_ANISOTROPIC,         // filter
	    D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressU
	    D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressV
	    D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressW
	    0.0f,                             // mipLODBias
	    8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC shadow(
	    6,                                                // shaderRegister
	    D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // filter
	    D3D12_TEXTURE_ADDRESS_MODE_BORDER,                // addressU
	    D3D12_TEXTURE_ADDRESS_MODE_BORDER,                // addressV
	    D3D12_TEXTURE_ADDRESS_MODE_BORDER,                // addressW
	    0.0f,                                             // mipLODBias
	    16,                                               // maxAnisotropy
	    D3D12_COMPARISON_FUNC_LESS_EQUAL,
	    D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp,
		shadow
	};
}

void DungeonStompApp::BuildShadersAndInputLayout() {

	const D3D_SHADER_MACRO defines[] = {
		"FOG", "1",
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO sasoDefines[] = {
		"FOG", "1",
		"ALPHA_TEST", "1",
		"SSAO", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO alphaTestDefines[] = {
		"FOG", "1",
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO torchTestDefines[] = {
		"TORCH_TEST", "1",
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShaderDXC(L"..\\Shaders\\Default.hlsl", nullptr, "VS", "vs_6_0");
	mShaders["opaquePS"] = d3dUtil::CompileShaderDXC(L"..\\Shaders\\Default.hlsl", defines, "PS", "ps_6_0");
	mShaders["opaqueSsaoPS"] = d3dUtil::CompileShaderDXC(L"..\\Shaders\\Default.hlsl", sasoDefines, "PS", "ps_6_0");

	mShaders["alphaTestedPS"] = d3dUtil::CompileShaderDXC(L"..\\Shaders\\Default.hlsl", alphaTestDefines, "PS", "ps_6_0");
	mShaders["torchPS"] = d3dUtil::CompileShaderDXC(L"..\\Shaders\\Default.hlsl", torchTestDefines, "PS", "ps_6_0");

	mShaders["normalMapVS"] = d3dUtil::CompileShaderDXC(L"..\\Shaders\\NormalMap.hlsl", nullptr, "VS", "vs_6_0");
	mShaders["normalMapPS"] = d3dUtil::CompileShaderDXC(L"..\\Shaders\\NormalMap.hlsl", defines, "PS", "ps_6_0");

	mShaders["normalMapSsaoPS"] = d3dUtil::CompileShaderDXC(L"..\\Shaders\\NormalMap.hlsl", sasoDefines, "PS", "ps_6_0");

	mShaders["shadowVS"] = d3dUtil::CompileShaderDXC(L"..\\Shaders\\Shadows.hlsl", nullptr, "VS", "vs_6_0");
	mShaders["shadowOpaquePS"] = d3dUtil::CompileShaderDXC(L"..\\Shaders\\Shadows.hlsl", nullptr, "PS", "ps_6_0");
	mShaders["shadowAlphaTestedPS"] = d3dUtil::CompileShaderDXC(L"..\\Shaders\\Shadows.hlsl", alphaTestDefines, "PS", "ps_6_0");

	mShaders["skyVS"] = d3dUtil::CompileShaderDXC(L"..\\Shaders\\Sky.hlsl", nullptr, "VS", "vs_6_0");
	mShaders["skyPS"] = d3dUtil::CompileShaderDXC(L"..\\Shaders\\Sky.hlsl", nullptr, "PS", "ps_6_0");

	mShaders["drawNormalsVS"] = d3dUtil::CompileShaderDXC(L"..\\Shaders\\DrawNormals.hlsl", nullptr, "VS", "vs_6_0");
	mShaders["drawNormalsPS"] = d3dUtil::CompileShaderDXC(L"..\\Shaders\\DrawNormals.hlsl", nullptr, "PS", "ps_6_0");

	mShaders["ssaoVS"] = d3dUtil::CompileShaderDXC(L"..\\Shaders\\Ssao.hlsl", nullptr, "VS", "vs_6_0");
	mShaders["ssaoPS"] = d3dUtil::CompileShaderDXC(L"..\\Shaders\\Ssao.hlsl", nullptr, "PS", "ps_6_0");

	mShaders["ssaoBlurVS"] = d3dUtil::CompileShaderDXC(L"..\\Shaders\\SsaoBlur.hlsl", nullptr, "VS", "vs_6_0");
	mShaders["ssaoBlurPS"] = d3dUtil::CompileShaderDXC(L"..\\Shaders\\SsaoBlur.hlsl", nullptr, "PS", "ps_6_0");

	// Text PSO
	// compile vertex shader using DXC (SM 6.0)
	Microsoft::WRL::ComPtr<ID3DBlob> textVertexShader = d3dUtil::CompileShaderDXC(
	    L"..\\Shaders\\TextVertexShader.hlsl", nullptr, "main", "vs_6_0");

	// fill out a shader bytecode structure, which is basically just a pointer
	// to the shader bytecode and the size of the shader bytecode
	D3D12_SHADER_BYTECODE textVertexShaderBytecode = {};
	textVertexShaderBytecode.BytecodeLength = textVertexShader->GetBufferSize();
	textVertexShaderBytecode.pShaderBytecode = textVertexShader->GetBufferPointer();

	// compile pixel shader
	Microsoft::WRL::ComPtr<ID3DBlob> textPixelShader = d3dUtil::CompileShaderDXC(
	    L"..\\Shaders\\TextPixelShader.hlsl", nullptr, "main", "ps_6_0");

	// fill out shader bytecode structure for pixel shader
	D3D12_SHADER_BYTECODE textPixelShaderBytecode = {};
	textPixelShaderBytecode.BytecodeLength = textPixelShader->GetBufferSize();
	textPixelShaderBytecode.pShaderBytecode = textPixelShader->GetBufferPointer();

	// Rectangle PSO
	// compile vertex shader
	Microsoft::WRL::ComPtr<ID3DBlob> rectangleVertexShader = d3dUtil::CompileShaderDXC(
	    L"..\\Shaders\\RectangleVertexShader.hlsl", nullptr, "main", "vs_6_0");

	// fill out a shader bytecode structure, which is basically just a pointer
	// to the shader bytecode and the size of the shader bytecode
	D3D12_SHADER_BYTECODE rectangleVertexShaderBytecode = {};
	rectangleVertexShaderBytecode.BytecodeLength = rectangleVertexShader->GetBufferSize();
	rectangleVertexShaderBytecode.pShaderBytecode = rectangleVertexShader->GetBufferPointer();

	// compile pixel shader
	Microsoft::WRL::ComPtr<ID3DBlob> rectanglePixelShader = d3dUtil::CompileShaderDXC(
	    L"..\\Shaders\\RectanglePixelShader.hlsl", nullptr, "main", "ps_6_0");

	// fill out shader bytecode structure for pixel shader
	D3D12_SHADER_BYTECODE rectanglePixelShaderBytecode = {};
	rectanglePixelShaderBytecode.BytecodeLength = rectanglePixelShader->GetBufferSize();
	rectanglePixelShaderBytecode.pShaderBytecode = rectanglePixelShader->GetBufferPointer();

	// compile pixel shader
	Microsoft::WRL::ComPtr<ID3DBlob> rectanglePixelMapShader = d3dUtil::CompileShaderDXC(
	    L"..\\Shaders\\RectanglePixelMapShader.hlsl", nullptr, "main", "ps_6_0");

	// fill out shader bytecode structure for pixel shader
	D3D12_SHADER_BYTECODE rectanglePixelMapShaderBytecode = {};
	rectanglePixelMapShaderBytecode.BytecodeLength = rectanglePixelMapShader->GetBufferSize();
	rectanglePixelMapShaderBytecode.pShaderBytecode = rectanglePixelMapShader->GetBufferPointer();

	HRESULT hr = S_OK; // For PSO creation calls

	mInputLayout = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_INPUT_ELEMENT_DESC textInputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 }
	};

	// fill out an input layout description structure
	D3D12_INPUT_LAYOUT_DESC textInputLayoutDesc = {};

	// we can get the number of elements in an array by "sizeof(array) / sizeof(arrayElementType)"
	textInputLayoutDesc.NumElements = sizeof(textInputLayout) / sizeof(D3D12_INPUT_ELEMENT_DESC);
	textInputLayoutDesc.pInputElementDescs = textInputLayout;

	// create the text pipeline state object (PSO)

	DXGI_SAMPLE_DESC sampleDesc = {};
	sampleDesc.Count = 1; // multisample count (no multisampling, so we just put 1, since we still need 1 sample)

	D3D12_GRAPHICS_PIPELINE_STATE_DESC textpsoDesc = {};
	textpsoDesc.InputLayout = textInputLayoutDesc;
	textpsoDesc.pRootSignature = mRootSignature.Get();
	textpsoDesc.VS = textVertexShaderBytecode;
	textpsoDesc.PS = textPixelShaderBytecode;
	textpsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	textpsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	textpsoDesc.SampleDesc = sampleDesc;
	textpsoDesc.SampleMask = 0xffffffff;
	textpsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

	D3D12_BLEND_DESC textBlendStateDesc = {};
	textBlendStateDesc.AlphaToCoverageEnable = FALSE;
	textBlendStateDesc.IndependentBlendEnable = FALSE;
	textBlendStateDesc.RenderTarget[0].BlendEnable = TRUE;

	textBlendStateDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	textBlendStateDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	textBlendStateDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;

	textBlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
	textBlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
	textBlendStateDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

	textBlendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	textpsoDesc.BlendState = textBlendStateDesc;
	textpsoDesc.NumRenderTargets = 1;
	D3D12_DEPTH_STENCIL_DESC textDepthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	textDepthStencilDesc.DepthEnable = false;
	textpsoDesc.DepthStencilState = textDepthStencilDesc;

	// create the text pso
	hr = md3dDevice->CreateGraphicsPipelineState(&textpsoDesc, IID_PPV_ARGS(&textPSO));

	// create the rectangles for HUD
	for (int i = 0; i < MaxRectangle; i++) {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC rectanglepsoDesc = {};
		rectanglepsoDesc.InputLayout = textInputLayoutDesc;
		rectanglepsoDesc.pRootSignature = mRootSignature.Get();
		rectanglepsoDesc.VS = rectangleVertexShaderBytecode;
		rectanglepsoDesc.PS = rectanglePixelShaderBytecode;
		rectanglepsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		rectanglepsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		rectanglepsoDesc.SampleDesc = sampleDesc;
		rectanglepsoDesc.SampleMask = 0xffffffff;
		rectanglepsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

		D3D12_BLEND_DESC rectangleBlendStateDesc = {};
		rectangleBlendStateDesc.AlphaToCoverageEnable = FALSE;
		rectangleBlendStateDesc.IndependentBlendEnable = FALSE;
		rectangleBlendStateDesc.RenderTarget[0].BlendEnable = TRUE;

		if (i == 4) {
			//	// shadowmap/ssoa texture - make it not transparent
			rectangleBlendStateDesc.RenderTarget[0].BlendEnable = FALSE;
			// rectanglepsoDesc.PS = rectanglePixelMapShaderBytecode;
		}

		// if (i == MaxRectangle - 2 || i == MaxRectangle - 3) {
		//	//make the dice not transparent
		//	rectangleBlendStateDesc.RenderTarget[0].BlendEnable = FALSE;
		// }

		// if (i == 4) {
		//	// make the logo not transparent
		//	rectangleBlendStateDesc.RenderTarget[0].BlendEnable = FALSE;
		// }

		rectangleBlendStateDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rectangleBlendStateDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		rectangleBlendStateDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;

		// if (i == 0) {
		rectangleBlendStateDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_COLOR;
		rectangleBlendStateDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_COLOR;
		rectangleBlendStateDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		//}

		rectangleBlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
		rectangleBlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
		rectangleBlendStateDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

		rectangleBlendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		rectanglepsoDesc.BlendState = rectangleBlendStateDesc;
		rectanglepsoDesc.NumRenderTargets = 1;
		D3D12_DEPTH_STENCIL_DESC rectangleDepthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		rectangleDepthStencilDesc.DepthEnable = false;
		rectanglepsoDesc.DepthStencilState = rectangleDepthStencilDesc;

		// create the rectangle pso
		hr = md3dDevice->CreateGraphicsPipelineState(&rectanglepsoDesc, IID_PPV_ARGS(&rectanglePSO[i]));
	}
}

void DungeonStompApp::BuildLandGeometry() {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData grid = geoGen.CreateSphere(0.5f, 20, 20);

	std::vector<Vertex> vertices(grid.Vertices.size());
	for (size_t i = 0; i < grid.Vertices.size(); ++i) {
		vertices[i].Pos = grid.Vertices[i].Position;
		vertices[i].Normal = grid.Vertices[i].Normal;
		vertices[i].TexC = grid.Vertices[i].TexC;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	std::vector<std::uint16_t> indices = grid.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "landGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
	                                                    mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
	                                                   mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;

	mGeometries["landGeo"] = std::move(geo);
}

void DungeonStompApp::BuildDungeonGeometryBuffers() {
	std::vector<std::uint16_t> indices(3 * mDungeon->TriangleCount()); // 3 indices per face
	assert(mDungeon->VertexCount() < 0x0000ffff);

	// UINT vbByteSize = mDungeon->VertexCount() * sizeof(Vertex);
	UINT vbByteSize = MAX_NUM_QUADS * sizeof(Vertex);
	UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "waterGeo";

	// Set dynamically.
	geo->VertexBufferCPU = nullptr;
	geo->VertexBufferGPU = nullptr;

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
	                                                   mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;

	mGeometries["waterGeo"] = std::move(geo);
}

void DungeonStompApp::BuildPSOs() {

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS = {
		reinterpret_cast<BYTE *>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS = {
		reinterpret_cast<BYTE *>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueSsaoPsoDesc;

	//
	// PSO for opaqueSsao objects.
	//
	ZeroMemory(&opaqueSsaoPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaqueSsaoPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaqueSsaoPsoDesc.pRootSignature = mRootSignature.Get();
	opaqueSsaoPsoDesc.VS = {
		reinterpret_cast<BYTE *>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	opaqueSsaoPsoDesc.PS = {
		reinterpret_cast<BYTE *>(mShaders["opaqueSsaoPS"]->GetBufferPointer()),
		mShaders["opaqueSsaoPS"]->GetBufferSize()
	};
	opaqueSsaoPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaqueSsaoPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaqueSsaoPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaqueSsaoPsoDesc.SampleMask = UINT_MAX;
	opaqueSsaoPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaqueSsaoPsoDesc.NumRenderTargets = 1;
	opaqueSsaoPsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaqueSsaoPsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaqueSsaoPsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaqueSsaoPsoDesc.DSVFormat = mDepthStencilFormat;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueSsaoPsoDesc, IID_PPV_ARGS(&mPSOs["opaqueSsao"])));

	//
	// PSO for shadow map pass.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC smapPsoDesc = opaquePsoDesc;
	smapPsoDesc.RasterizerState.DepthBias = 100000;
	smapPsoDesc.RasterizerState.DepthBiasClamp = 0.0f;
	smapPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
	smapPsoDesc.pRootSignature = mRootSignature.Get();
	smapPsoDesc.VS = {
		reinterpret_cast<BYTE *>(mShaders["shadowVS"]->GetBufferPointer()),
		mShaders["shadowVS"]->GetBufferSize()
	};
	smapPsoDesc.PS = {
		reinterpret_cast<BYTE *>(mShaders["shadowOpaquePS"]->GetBufferPointer()),
		mShaders["shadowOpaquePS"]->GetBufferSize()
	};

	// Shadow map pass does not have a render target.
	smapPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	smapPsoDesc.NumRenderTargets = 0;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&smapPsoDesc, IID_PPV_ARGS(&mPSOs["shadow_opaque"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC normalMapPsoDesc;

	//
	// PSO for normalMap objects.
	//
	ZeroMemory(&normalMapPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	normalMapPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	normalMapPsoDesc.pRootSignature = mRootSignature.Get();
	normalMapPsoDesc.VS = {
		reinterpret_cast<BYTE *>(mShaders["normalMapVS"]->GetBufferPointer()),
		mShaders["normalMapVS"]->GetBufferSize()
	};
	normalMapPsoDesc.PS = {
		reinterpret_cast<BYTE *>(mShaders["normalMapPS"]->GetBufferPointer()),
		mShaders["normalMapPS"]->GetBufferSize()
	};
	normalMapPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	normalMapPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	normalMapPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	normalMapPsoDesc.SampleMask = UINT_MAX;
	normalMapPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	normalMapPsoDesc.NumRenderTargets = 1;
	normalMapPsoDesc.RTVFormats[0] = mBackBufferFormat;
	normalMapPsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	normalMapPsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	normalMapPsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&normalMapPsoDesc, IID_PPV_ARGS(&mPSOs["normalMap"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC normalMapSsaoPSoDesc;

	//
	// PSO for normalSasoMap objects.
	//
	ZeroMemory(&normalMapSsaoPSoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	normalMapSsaoPSoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	normalMapSsaoPSoDesc.pRootSignature = mRootSignature.Get();
	normalMapSsaoPSoDesc.VS = {
		reinterpret_cast<BYTE *>(mShaders["normalMapVS"]->GetBufferPointer()),
		mShaders["normalMapVS"]->GetBufferSize()
	};
	normalMapSsaoPSoDesc.PS = {
		reinterpret_cast<BYTE *>(mShaders["normalMapSsaoPS"]->GetBufferPointer()),
		mShaders["normalMapSsaoPS"]->GetBufferSize()
	};
	normalMapSsaoPSoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	normalMapSsaoPSoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	normalMapSsaoPSoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	normalMapSsaoPSoDesc.SampleMask = UINT_MAX;
	normalMapSsaoPSoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	normalMapSsaoPSoDesc.NumRenderTargets = 1;
	normalMapSsaoPSoDesc.RTVFormats[0] = mBackBufferFormat;
	normalMapSsaoPSoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	normalMapSsaoPSoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	normalMapSsaoPSoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&normalMapSsaoPSoDesc, IID_PPV_ARGS(&mPSOs["normalMapSsao"])));

	//
	// PSO for transparent objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;

	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_COLOR;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_BLEND_FACTOR;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mPSOs["transparent"])));

	//
	// PSO for alpha tested objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestedPsoDesc = opaquePsoDesc;
	alphaTestedPsoDesc.PS = {
		reinterpret_cast<BYTE *>(mShaders["alphaTestedPS"]->GetBufferPointer()),
		mShaders["alphaTestedPS"]->GetBufferSize()
	};
	alphaTestedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&alphaTestedPsoDesc, IID_PPV_ARGS(&mPSOs["alphaTested"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC torchPsoDesc = opaquePsoDesc;

	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_COLOR;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_BLEND_FACTOR;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	torchPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;

	torchPsoDesc.PS = {
		reinterpret_cast<BYTE *>(mShaders["torchPS"]->GetBufferPointer()),
		mShaders["torchPS"]->GetBufferSize()
	};
	torchPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&torchPsoDesc, IID_PPV_ARGS(&mPSOs["torchTested"])));

	//
	// PSO for sky.
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC cubePsoDesc;
	ZeroMemory(&cubePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	cubePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	cubePsoDesc.pRootSignature = mRootSignature.Get();
	cubePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	cubePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	cubePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	cubePsoDesc.SampleMask = UINT_MAX;
	cubePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	cubePsoDesc.NumRenderTargets = 1;
	cubePsoDesc.RTVFormats[0] = mBackBufferFormat;
	cubePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	cubePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	cubePsoDesc.DSVFormat = mDepthStencilFormat;

	// The camera is inside the sky sphere, so just turn off culling.
	cubePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	// Make sure the depth function is LESS_EQUAL and not just LESS.
	// Otherwise, the normalized depth values at z = 1 (NDC) will
	// fail the depth test if the depth buffer was cleared to 1.
	cubePsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	cubePsoDesc.pRootSignature = mRootSignature.Get();
	cubePsoDesc.VS = {
		reinterpret_cast<BYTE *>(mShaders["skyVS"]->GetBufferPointer()),
		mShaders["skyVS"]->GetBufferSize()
	};
	cubePsoDesc.PS = {
		reinterpret_cast<BYTE *>(mShaders["skyPS"]->GetBufferPointer()),
		mShaders["skyPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&cubePsoDesc, IID_PPV_ARGS(&mPSOs["sky"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC basePsoDesc;

	ZeroMemory(&basePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	basePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	basePsoDesc.pRootSignature = mRootSignature.Get();

	basePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	basePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	basePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	basePsoDesc.SampleMask = UINT_MAX;
	basePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	basePsoDesc.NumRenderTargets = 1;
	basePsoDesc.RTVFormats[0] = mBackBufferFormat;
	basePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	basePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	basePsoDesc.DSVFormat = mDepthStencilFormat;

	//
	// PSO for drawing normals.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC drawNormalsPsoDesc = basePsoDesc;
	drawNormalsPsoDesc.VS = {
		reinterpret_cast<BYTE *>(mShaders["drawNormalsVS"]->GetBufferPointer()),
		mShaders["drawNormalsVS"]->GetBufferSize()
	};
	drawNormalsPsoDesc.PS = {
		reinterpret_cast<BYTE *>(mShaders["drawNormalsPS"]->GetBufferPointer()),
		mShaders["drawNormalsPS"]->GetBufferSize()
	};
	drawNormalsPsoDesc.RTVFormats[0] = Ssao::NormalMapFormat;
	drawNormalsPsoDesc.SampleDesc.Count = 1;
	drawNormalsPsoDesc.SampleDesc.Quality = 0;
	drawNormalsPsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&drawNormalsPsoDesc, IID_PPV_ARGS(&mPSOs["drawNormals"])));

	//
	// PSO for SSAO.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoPsoDesc = basePsoDesc;
	ssaoPsoDesc.InputLayout = { nullptr, 0 };
	ssaoPsoDesc.pRootSignature = mSsaoRootSignature.Get();
	ssaoPsoDesc.VS = {
		reinterpret_cast<BYTE *>(mShaders["ssaoVS"]->GetBufferPointer()),
		mShaders["ssaoVS"]->GetBufferSize()
	};
	ssaoPsoDesc.PS = {
		reinterpret_cast<BYTE *>(mShaders["ssaoPS"]->GetBufferPointer()),
		mShaders["ssaoPS"]->GetBufferSize()
	};

	// SSAO effect does not need the depth buffer.
	ssaoPsoDesc.DepthStencilState.DepthEnable = false;
	ssaoPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	ssaoPsoDesc.RTVFormats[0] = Ssao::AmbientMapFormat;
	ssaoPsoDesc.SampleDesc.Count = 1;
	ssaoPsoDesc.SampleDesc.Quality = 0;
	ssaoPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&ssaoPsoDesc, IID_PPV_ARGS(&mPSOs["ssao"])));

	//
	// PSO for SSAO blur.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoBlurPsoDesc = ssaoPsoDesc;
	ssaoBlurPsoDesc.VS = {
		reinterpret_cast<BYTE *>(mShaders["ssaoBlurVS"]->GetBufferPointer()),
		mShaders["ssaoBlurVS"]->GetBufferSize()
	};
	ssaoBlurPsoDesc.PS = {
		reinterpret_cast<BYTE *>(mShaders["ssaoBlurPS"]->GetBufferPointer()),
		mShaders["ssaoBlurPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&ssaoBlurPsoDesc, IID_PPV_ARGS(&mPSOs["ssaoBlur"])));
}

void DungeonStompApp::BuildFrameResources() {
	for (int i = 0; i < gNumFrameResources; ++i) {
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
		                                                          2, (UINT)mAllRitems.size(), (UINT)mMaterials.size(), mDungeon->VertexCount()));
	}
}

void DungeonStompApp::BuildMaterials() {
	// Pseudocode plan:
	// 1. Open "materials.txt" for reading.
	// 2. For each material entry in the file:
	//    a. Read material name and all properties (MatCBIndex, DiffuseAlbedo, FresnelR0, Roughness, Metal, DiffuseSrvHeapIndex).
	//    b. Create a Material object, set its properties.
	//    c. Add to mMaterials map with the name as key.
	// 3. Close the file.

	std::ifstream infile("materials.txt");
	if (!infile.is_open()) {
		// Fallback: create a default material if file not found
		auto defaultMat = std::make_unique<Material>();
		defaultMat->Name = "default";
		defaultMat->MatCBIndex = 0;
		defaultMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		defaultMat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
		defaultMat->Roughness = 0.815f;
		defaultMat->Metal = 0.1f;
		mMaterials["default"] = std::move(defaultMat);
		return;
	}

	std::string line;
	while (std::getline(infile, line)) {
		// Skip empty lines or comments
		if (line.empty() || line[0] == '#')
			continue;

		std::istringstream iss(line);
		std::string name;
		int matCBIndex = -1;
		float daR, daG, daB, daA;
		float frR, frG, frB;
		float roughness, metal;
		int diffuseSrvHeapIndex = -1;

		// Format: name matCBIndex daR daG daB daA frR frG frB roughness metal [diffuseSrvHeapIndex]
		iss >> name >> matCBIndex >> daR >> daG >> daB >> daA >> frR >> frG >> frB >> roughness >> metal;
		if (!(iss >> diffuseSrvHeapIndex)) {
			diffuseSrvHeapIndex = 0; // default if not present
		}

		auto mat = std::make_unique<Material>();
		mat->Name = name;
		mat->MatCBIndex = matCBIndex;
		mat->DiffuseAlbedo = XMFLOAT4(daR, daG, daB, daA);
		mat->FresnelR0 = XMFLOAT3(frR, frG, frB);
		mat->Roughness = roughness;
		mat->Metal = metal;
		mat->DiffuseSrvHeapIndex = diffuseSrvHeapIndex;

		mMaterials[name] = std::move(mat);
	}
	infile.close();
}

void DungeonStompApp::BuildRenderItems() {
	auto dungeonRitem = std::make_unique<RenderItem>();

	dungeonRitem->World = MathHelper::Identity4x4();
	dungeonRitem->ObjCBIndex = 0;
	dungeonRitem->Mat = mMaterials["water"].get();
	dungeonRitem->Geo = mGeometries["waterGeo"].get();
	dungeonRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	dungeonRitem->IndexCount = dungeonRitem->Geo->DrawArgs["grid"].IndexCount;
	dungeonRitem->StartIndexLocation = dungeonRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	dungeonRitem->BaseVertexLocation = dungeonRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

	mDungeonRitem = dungeonRitem.get();

	mRitemLayer[(int)RenderLayer::Opaque].push_back(dungeonRitem.get());

	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->World = MathHelper::Identity4x4();

	gridRitem->ObjCBIndex = 1;
	gridRitem->Mat = mMaterials["grass"].get();
	gridRitem->Geo = mGeometries["landGeo"].get();
	gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());

	mAllRitems.push_back(std::move(dungeonRitem));
	mAllRitems.push_back(std::move(gridRitem));
}

extern bool ObjectHasShadow(int object_id);

float DungeonStompApp::GetHillsHeight(float x, float z) const {
	return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

XMFLOAT3 DungeonStompApp::GetHillsNormal(float x, float z) const {
	// n = (-df/dx, 1, -df/dz)
	XMFLOAT3 n(
	    -0.03f * z * cosf(0.1f * x) - 0.3f * cosf(0.1f * z),
	    1.0f,
	    -0.3f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z));

	XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
	XMStoreFloat3(&n, unitNormal);

	return n;
}

void DungeonStompApp::LoadTextures() {
	auto woodCrateTex = std::make_unique<Texture>();
	woodCrateTex->Name = "woodCrateTex";
	woodCrateTex->Filename = L"../Textures/bricks2.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
	                                                  mCommandList.Get(), woodCrateTex->Filename.c_str(),
	                                                  woodCrateTex->Resource, woodCrateTex->UploadHeap));

	auto grassTex = std::make_unique<Texture>();
	grassTex->Name = "grassTex";
	grassTex->Filename = L"../Textures/WoodCrate01.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
	                                                  mCommandList.Get(), grassTex->Filename.c_str(),
	                                                  grassTex->Resource, grassTex->UploadHeap));

	mTextures[grassTex->Name] = std::move(grassTex);
	mTextures[woodCrateTex->Name] = std::move(woodCrateTex);
}

void DungeonStompApp::CreateRtvAndDsvDescriptorHeaps() {
	// Add +6 RTV for cube render target.
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount + 3;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
	    &rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

	// Add +1 DSV for shadow map.
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 2;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
	    &dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
}

void DungeonStompApp::BuildDescriptorHeaps() {
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = MAX_NUM_TEXTURES;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto woodCrateTex = mTextures["woodCrateTex"]->Resource;
	auto grassTex = mTextures["grassTex"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = woodCrateTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = woodCrateTex->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	md3dDevice->CreateShaderResourceView(woodCrateTex.Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = grassTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(grassTex.Get(), &srvDesc, hDescriptor);

	LoadRRTextures11("textures.dat");

	// create upload heap. We will fill this with data for our text

	HRESULT hr = md3dDevice->CreateCommittedResource(
	    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),                          // upload heap
	    D3D12_HEAP_FLAG_NONE,                                                      // no flags
	    &CD3DX12_RESOURCE_DESC::Buffer(maxNumTextCharacters * sizeof(TextVertex)), // resource description for a buffer
	    D3D12_RESOURCE_STATE_GENERIC_READ,                                         // GPU will read from this buffer and copy its contents to the default heap
	    nullptr,
	    IID_PPV_ARGS(&textVertexBuffer));

	textVertexBuffer->SetName(L"Text Vertex Buffer Upload Resource Heap");

	CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU. (so end is less than or equal to begin)
	// map the resource heap to get a gpu virtual address to the beginning of the heap
	hr = textVertexBuffer->Map(0, &readRange, reinterpret_cast<void **>(&textVBGPUAddress));

	for (int i = 0; i < MaxRectangle; ++i) {
		// create upload heap. We will fill this with data for our text
		hr = md3dDevice->CreateCommittedResource(
		    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),                               // upload heap
		    D3D12_HEAP_FLAG_NONE,                                                           // no flags
		    &CD3DX12_RESOURCE_DESC::Buffer(maxNumRectangleCharacters * sizeof(TextVertex)), // resource description for a buffer
		    D3D12_RESOURCE_STATE_GENERIC_READ,                                              // GPU will read from this buffer and copy its contents to the default heap
		    nullptr,
		    IID_PPV_ARGS(&rectangleVertexBuffer[i]));

		rectangleVertexBuffer[i]->SetName(L"Rectangle Vertex Buffer Upload Resource Heap");

		CD3DX12_RANGE readRange2(0, 0);
		// map the resource heap to get a gpu virtual address to the beginning of the heap
		hr = rectangleVertexBuffer[i]->Map(0, &readRange2, reinterpret_cast<void **>(&rectangleVBGPUAddress[i]));
	}

	mSkyTexHeapIndex = 485; // sunsetcube1024 is 486th alias -> index 485
	mShadowMapHeapIndex = (UINT)number_of_tex_aliases + 1;

	mSsaoHeapIndexStart = mShadowMapHeapIndex + 1;
	mSsaoAmbientMapIndex = mSsaoHeapIndexStart + 3;

	// mNullCubeSrvIndex = mShadowMapHeapIndex + 1;
	// mNullTexSrvIndex = mNullCubeSrvIndex + 1;

	mNullCubeSrvIndex = mSsaoHeapIndexStart + 5;
	mNullTexSrvIndex1 = mNullCubeSrvIndex + 1;
	mNullTexSrvIndex2 = mNullTexSrvIndex1 + 1;

	auto srvCpuStart = mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	auto srvGpuStart = mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	auto dsvCpuStart = mDsvHeap->GetCPUDescriptorHandleForHeapStart();

	auto nullSrv = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, mNullCubeSrvIndex, mCbvSrvUavDescriptorSize);
	mNullSrv = CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, mNullCubeSrvIndex, mCbvSrvUavDescriptorSize);

	md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);
	nullSrv.Offset(1, mCbvSrvUavDescriptorSize);

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);

	mShadowMap->BuildDescriptors(
	    GetCpuSrv(mShadowMapHeapIndex),
	    GetGpuSrv(mShadowMapHeapIndex),
	    GetDsv(1));

	/*mShadowMap->BuildDescriptors(
	    CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, mShadowMapHeapIndex, mCbvSrvUavDescriptorSize),
	    CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, mShadowMapHeapIndex, mCbvSrvUavDescriptorSize),
	    CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvCpuStart, 1, mDsvDescriptorSize));*/

	mSsao->BuildDescriptors(
	    mDepthStencilBuffer.Get(),
	    GetCpuSrv(mSsaoHeapIndexStart),
	    GetGpuSrv(mSsaoHeapIndexStart),
	    GetRtv(SwapChainBufferCount),
	    mCbvSrvUavDescriptorSize,
	    mRtvDescriptorSize);

	nullSrv.Offset(1, mCbvSrvUavDescriptorSize);
	md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DungeonStompApp::GetCpuSrv(int index) const {
	auto srv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	srv.Offset(index, mCbvSrvUavDescriptorSize);
	return srv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE DungeonStompApp::GetGpuSrv(int index) const {
	auto srv = CD3DX12_GPU_DESCRIPTOR_HANDLE(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	srv.Offset(index, mCbvSrvUavDescriptorSize);
	return srv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DungeonStompApp::GetDsv(int index) const {
	auto dsv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mDsvHeap->GetCPUDescriptorHandleForHeapStart());
	dsv.Offset(index, mDsvDescriptorSize);
	return dsv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DungeonStompApp::GetRtv(int index) const {
	auto rtv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	rtv.Offset(index, mRtvDescriptorSize);
	return rtv;
}

BOOL DungeonStompApp::LoadRRTextures11(char *filename) {
	FILE *fp;
	char s[256];
	char p[256];
	char f[256];

	int y_count = 30;
	int done = 0;
	int object_count = 0;
	int vert_count = 0;
	int pv_count = 0;
	int poly_count = 0;
	int tex_alias_counter = 0;
	int tex_counter = 0;
	int i;
	BOOL start_flag = TRUE;
	BOOL found;

	if (fopen_s(&fp, filename, "r") != 0) {
		// PrintMessage(hwnd, "ERROR can't open ", filename, SCN_AND_FILE);
		// MessageBox(hwnd, filename, "Error can't open", MB_OK);
		// return FALSE;
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

	int flip = 0;

	while (done == 0) {
		found = FALSE;
		fscanf_s(fp, "%s", &s, 256);

		if (strcmp(s, "AddTexture") == 0) {
			fscanf_s(fp, "%s", &p, 256);
			// remember the file
			strcpy_s(f, 256, p);
			tex_counter++;
		}

		if (strcmp(s, "Alias") == 0) {
			fscanf_s(fp, "%s", &p, 256);
			fscanf_s(fp, "%s", &p, 256);
			strcpy_s((char *)TexMap[tex_alias_counter].tex_alias_name, 100, (char *)&p);

			TexMap[tex_alias_counter].texture = tex_counter - 1;

			bool exists = true;
			FILE *fp4 = NULL;
			fopen_s(&fp4, f, "rb");
			if (fp4 == NULL) {
				exists = false;
			}

			auto currentTex = std::make_unique<Texture>();
			currentTex->Name = p;

			if (exists) {
				// currentTex->Filename = charToWChar("../Textures/ruin1.dds");
				currentTex->Filename = charToWChar(f);
			} else {
				currentTex->Filename = charToWChar("../Textures/WoodCrate01.dds");
			}

			DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
			                                    mCommandList.Get(), currentTex->Filename.c_str(),
			                                    currentTex->Resource, currentTex->UploadHeap);

			// Default to woodcrate if the texture will not load
			if (currentTex->Resource == NULL) {
				currentTex->Filename = charToWChar("../Textures/WoodCrate01.dds");
				DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
				                                    mCommandList.Get(), currentTex->Filename.c_str(),
				                                    currentTex->Resource, currentTex->UploadHeap);
			}

			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = currentTex->Resource->GetDesc().Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // Reset to 2D for each loop
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = currentTex->Resource->GetDesc().MipLevels;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

			if (strcmp(p, "sunsetcube1024") == 0) {

				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
				srvDesc.TextureCube.MostDetailedMip = 0;
				srvDesc.TextureCube.MipLevels = currentTex->Resource->GetDesc().MipLevels;
				srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
				srvDesc.Format = currentTex->Resource->GetDesc().Format;
			} else {
			}

			srvDesc.Format = currentTex->Resource->GetDesc().Format;
			md3dDevice->CreateShaderResourceView(currentTex->Resource.Get(), &srvDesc, hDescriptor);

			// auto a = mTextures[currentTex->Name].get();

			mTextures[currentTex->Name] = std::move(currentTex);

			// next descriptor
			hDescriptor.Offset(1, mCbvSrvDescriptorSize);

			fscanf_s(fp, "%s", &p, 256);
			if (strcmp(p, "AlphaTransparent") == 0)
				TexMap[tex_alias_counter].is_alpha_texture = TRUE;

			i = tex_alias_counter;

			fscanf_s(fp, "%s", &p, 256);

			if (strcmp(p, "WHOLE") == 0) {
				TexMap[i].tu[0] = (float)0.0;
				TexMap[i].tv[0] = (float)1.0;
				TexMap[i].tu[1] = (float)0.0;
				TexMap[i].tv[1] = (float)0.0;
				TexMap[i].tu[2] = (float)1.0;
				TexMap[i].tv[2] = (float)1.0;
				TexMap[i].tu[3] = (float)1.0;
				TexMap[i].tv[3] = (float)0.0;
			}

			if (strcmp(p, "TL_QUAD") == 0) {
				TexMap[i].tu[0] = (float)0.0;
				TexMap[i].tv[0] = (float)0.5;
				TexMap[i].tu[1] = (float)0.0;
				TexMap[i].tv[1] = (float)0.0;
				TexMap[i].tu[2] = (float)0.5;
				TexMap[i].tv[2] = (float)0.5;
				TexMap[i].tu[3] = (float)0.5;
				TexMap[i].tv[3] = (float)0.0;
			}

			if (strcmp(p, "TR_QUAD") == 0) {
				TexMap[i].tu[0] = (float)0.5;
				TexMap[i].tv[0] = (float)0.5;
				TexMap[i].tu[1] = (float)0.5;
				TexMap[i].tv[1] = (float)0.0;
				TexMap[i].tu[2] = (float)1.0;
				TexMap[i].tv[2] = (float)0.5;
				TexMap[i].tu[3] = (float)1.0;
				TexMap[i].tv[3] = (float)0.0;
			}
			if (strcmp(p, "LL_QUAD") == 0) {
				TexMap[i].tu[0] = (float)0.0;
				TexMap[i].tv[0] = (float)1.0;
				TexMap[i].tu[1] = (float)0.0;
				TexMap[i].tv[1] = (float)0.5;
				TexMap[i].tu[2] = (float)0.5;
				TexMap[i].tv[2] = (float)1.0;
				TexMap[i].tu[3] = (float)0.5;
				TexMap[i].tv[3] = (float)0.5;
			}
			if (strcmp(p, "LR_QUAD") == 0) {
				TexMap[i].tu[0] = (float)0.5;
				TexMap[i].tv[0] = (float)1.0;
				TexMap[i].tu[1] = (float)0.5;
				TexMap[i].tv[1] = (float)0.5;
				TexMap[i].tu[2] = (float)1.0;
				TexMap[i].tv[2] = (float)1.0;
				TexMap[i].tu[3] = (float)1.0;
				TexMap[i].tv[3] = (float)0.5;
			}
			if (strcmp(p, "TOP_HALF") == 0) {
				TexMap[i].tu[0] = (float)0.0;
				TexMap[i].tv[0] = (float)0.5;
				TexMap[i].tu[1] = (float)0.0;
				TexMap[i].tv[1] = (float)0.0;
				TexMap[i].tu[2] = (float)1.0;
				TexMap[i].tv[2] = (float)0.5;
				TexMap[i].tu[3] = (float)1.0;
				TexMap[i].tv[3] = (float)0.0;
			}
			if (strcmp(p, "BOT_HALF") == 0) {
				TexMap[i].tu[0] = (float)0.0;
				TexMap[i].tv[0] = (float)1.0;
				TexMap[i].tu[1] = (float)0.0;
				TexMap[i].tv[1] = (float)0.5;
				TexMap[i].tu[2] = (float)1.0;
				TexMap[i].tv[2] = (float)1.0;
				TexMap[i].tu[3] = (float)1.0;
				TexMap[i].tv[3] = (float)0.5;
			}
			if (strcmp(p, "LEFT_HALF") == 0) {
				TexMap[i].tu[0] = (float)0.0;
				TexMap[i].tv[0] = (float)1.0;
				TexMap[i].tu[1] = (float)0.0;
				TexMap[i].tv[1] = (float)0.0;
				TexMap[i].tu[2] = (float)0.5;
				TexMap[i].tv[2] = (float)1.0;
				TexMap[i].tu[3] = (float)0.5;
				TexMap[i].tv[3] = (float)0.0;
			}
			if (strcmp(p, "RIGHT_HALF") == 0) {
				TexMap[i].tu[0] = (float)0.5;
				TexMap[i].tv[0] = (float)1.0;
				TexMap[i].tu[1] = (float)0.5;
				TexMap[i].tv[1] = (float)0.0;
				TexMap[i].tu[2] = (float)1.0;
				TexMap[i].tv[2] = (float)1.0;
				TexMap[i].tu[3] = (float)1.0;
				TexMap[i].tv[3] = (float)0.0;
			}
			if (strcmp(p, "TL_TRI") == 0) {
				TexMap[i].tu[0] = (float)0.0;
				TexMap[i].tv[0] = (float)0.0;
				TexMap[i].tu[1] = (float)1.0;
				TexMap[i].tv[1] = (float)0.0;
				TexMap[i].tu[2] = (float)0.0;
				TexMap[i].tv[2] = (float)1.0;
			}
			if (strcmp(p, "BR_TRI") == 0) {
			}

			fscanf_s(fp, "%s", &p, 256);
			auto &texMaterial = TexMap[tex_alias_counter].material;
			strcpy_s((char *)texMaterial.name, 100, (char *)&p);

			texMaterial.roughness = 0.5f;
			texMaterial.metallic = 0.0f;
			texMaterial.DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
			texMaterial.FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);

			std::string textureType = texMaterial.name;
			auto materialIt = mMaterials.find(textureType);
			if (materialIt != mMaterials.end() && materialIt->second) {
				const Material *material = materialIt->second.get();
				texMaterial.roughness = material->Roughness;
				texMaterial.metallic = material->Metal;
				texMaterial.DiffuseAlbedo = material->DiffuseAlbedo;
				texMaterial.FresnelR0 = material->FresnelR0;
			}

			tex_alias_counter++;
			found = TRUE;
		}

		if (strcmp(s, "END_FILE") == 0) {
			// PrintMessage(hwnd, "\n", NULL, LOGFILE_ONLY);
			number_of_tex_aliases = tex_alias_counter;
			found = TRUE;
			done = 1;
		}

		if (found == FALSE) {
			// PrintMessage(hwnd, "File Error: Syntax problem :", p, SCN_AND_FILE);
			// MessageBox(hwnd, "p", "File Error: Syntax problem ", MB_OK);
			// return FALSE;
		}
	}
	fclose(fp);

	SetTextureNormalMap();

	return TRUE;
}

void DungeonStompApp::SetTextureNormalMap() {

	char junk[255];

	for (int i = 0; i < number_of_tex_aliases; i++) {

		sprintf_s(junk, "%s_nm", TexMap[i].tex_alias_name);

		int normalmap = -1;

		for (int j = 0; j < number_of_tex_aliases; j++) {
			if (strstr(TexMap[j].tex_alias_name, "_nm") != 0) {

				if (strcmp(TexMap[j].tex_alias_name, junk) == 0) {
					TexMap[i].normalmaptextureid = j;
				}
			}
		}
	}
}

void DungeonStompApp::SetTextureNormalMapEmpty() {

	for (int i = 0; i < number_of_tex_aliases; i++) {
		TexMap[i].normalmaptextureid = -1;
	}
}
