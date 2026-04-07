//***************************************************************************************
// DLSSHelper.h - NVIDIA DLSS (Deep Learning Super Sampling) integration for DX12
//
// Requires the NVIDIA DLSS SDK. Download from:
//   https://developer.nvidia.com/rtx/dlss/get-started
//
// Setup:
//   1. Download the DLSS SDK and extract it
//   2. Copy include files (nvsdk_ngx*.h) to ThirdParty/DLSS/include/
//   3. Copy lib/x64/nvsdk_ngx_d.lib (debug) or nvsdk_ngx_s.lib (release) to ThirdParty/DLSS/lib/x64/
//   4. Copy the nvngx_dlss.dll to your bin/ directory
//   5. Define HAS_DLSS_SDK in preprocessor definitions to enable DLSS compilation
//***************************************************************************************

#pragma once

#include "../Common/d3dUtil.h"
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

// DLSS Quality Mode presets
enum class DLSSQualityMode {
	Off = 0,
	UltraPerformance, // ~33% render resolution
	MaxPerformance,   // ~50% render resolution
	Balanced,         // ~58% render resolution
	Quality,          // ~67% render resolution
	UltraQuality,     // ~77% render resolution
	DLAA,             // 100% render resolution (anti-aliasing only)
	Count
};

// Motion vector constant buffer (matches MotionVectors.hlsl)
struct MotionVectorCB {
	DirectX::XMFLOAT4X4 InvViewProj;
	DirectX::XMFLOAT4X4 PrevViewProj;
	float RenderWidth;
	float RenderHeight;
	float InvRenderWidth;
	float InvRenderHeight;
};

class DLSSHelper {
  public:
	DLSSHelper();
	~DLSSHelper();

	// Initialize DLSS. Returns true if DLSS is available and ready.
	bool Initialize(ID3D12Device *device,
	                ID3D12GraphicsCommandList *cmdList,
	                ID3D12CommandQueue *cmdQueue,
	                UINT displayWidth, UINT displayHeight,
	                DXGI_FORMAT backBufferFormat);

	// Set quality mode and recreate DLSS feature. Returns false if mode not supported.
	bool SetQualityMode(ID3D12GraphicsCommandList *cmdList, DLSSQualityMode mode);

	// Execute the DLSS upscaling pass.
	// colorInput:    rendered scene at render resolution
	// depthInput:    depth buffer at render resolution (R32_FLOAT or D32_FLOAT)
	// motionVectors: screen-space motion vectors at render resolution (R16G16_FLOAT)
	// jitterX/Y:     sub-pixel jitter offsets applied to the projection matrix
	// deltaTime:     frame delta time in seconds
	// reset:         true to reset temporal history (e.g., on camera cut)
	void Evaluate(ID3D12GraphicsCommandList *cmdList,
	              ID3D12Resource *colorInput,
	              ID3D12Resource *depthInput,
	              ID3D12Resource *motionVectors,
	              float jitterX, float jitterY,
	              float deltaTime,
	              bool reset = false);

	// Generate camera-based motion vectors from depth buffer reprojection.
	// Uses a compute shader to derive per-pixel motion from depth + camera matrices.
	void GenerateMotionVectors(ID3D12GraphicsCommandList *cmdList,
	                           ID3D12Resource *depthBuffer,
	                           const DirectX::XMFLOAT4X4 &invViewProj,
	                           const DirectX::XMFLOAT4X4 &prevViewProj);

	// Copy DLSS output to the back buffer.
	void CopyOutputToBackBuffer(ID3D12GraphicsCommandList *cmdList,
	                            ID3D12Resource *backBuffer);

	// Handle window/display resize.
	void OnResize(ID3D12GraphicsCommandList *cmdList, UINT displayWidth, UINT displayHeight);

	// Shutdown and release NGX resources.
	void Shutdown();

	// --- Accessors ---

	bool IsSupported() const { return mSupported; }
	bool IsInitialized() const { return mInitialized; }
	DLSSQualityMode GetQualityMode() const { return mCurrentMode; }

	UINT GetRenderWidth() const { return mRenderWidth; }
	UINT GetRenderHeight() const { return mRenderHeight; }
	UINT GetDisplayWidth() const { return mDisplayWidth; }
	UINT GetDisplayHeight() const { return mDisplayHeight; }

	// Get the DLSS output resource (display resolution).
	ID3D12Resource *GetOutputResource() const { return mDLSSOutput.Get(); }

	// Get the motion vector buffer (render resolution, R16G16_FLOAT).
	ID3D12Resource *GetMotionVectorResource() const { return mMotionVectorBuffer.Get(); }

	// Get the render-resolution depth buffer (R32_FLOAT, SRV-readable).
	ID3D12Resource *GetDepthResource() const { return mDepthBuffer.Get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetDepthDSV() const { return mDepthDSV; }

	// Get the low-res color render target.
	ID3D12Resource *GetRenderTarget() const { return mColorBuffer.Get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTargetRTV() const { return mColorRTV; }

	// Get the render-resolution viewport and scissor rect.
	const D3D12_VIEWPORT &RenderViewport() const { return mRenderViewport; }
	const D3D12_RECT &RenderScissorRect() const { return mRenderScissorRect; }

	// Compute Halton jitter offset for temporal stability.
	// Returns jitter in pixel space (needs to be converted to clip space for projection matrix).
	void GetJitterOffset(UINT frameIndex, float &jitterX, float &jitterY) const;

	// Get quality mode display name.
	static const char *GetQualityModeName(DLSSQualityMode mode);

	// Cycle to the next quality mode (wraps around, skips Off).
	DLSSQualityMode GetNextQualityMode() const;

  private:
	void CreateRenderResources(ID3D12Device *device);
	void ReleaseRenderResources();
	void CalculateRenderResolution();
	void CreateMotionVectorPipeline(ID3D12Device *device);
	void CreateMotionVectorResources(ID3D12Device *device);

	static float HaltonSequence(int index, int base);

	bool mSupported = false;
	bool mInitialized = false;
	DLSSQualityMode mCurrentMode = DLSSQualityMode::Quality;

	UINT mDisplayWidth = 0;
	UINT mDisplayHeight = 0;
	UINT mRenderWidth = 0;
	UINT mRenderHeight = 0;
	float mSharpness = 0.0f;

	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	// NGX handles (void* to avoid requiring NGX headers in the header)
#ifdef HAS_DLSS_SDK
	void *mNGXParams = nullptr;   // NVSDK_NGX_Parameter*
	void *mDLSSHandle = nullptr;  // NVSDK_NGX_Handle*
#endif

	// Render resources
	ComPtr<ID3D12Resource> mColorBuffer;     // Low-res color render target
	ComPtr<ID3D12Resource> mDepthBuffer;     // Low-res depth (R32_TYPELESS for DSV+SRV)
	ComPtr<ID3D12Resource> mMotionVectorBuffer; // Motion vectors (R16G16_FLOAT UAV)
	ComPtr<ID3D12Resource> mDLSSOutput;      // DLSS output (display resolution)

	// Descriptor heaps for DLSS-owned resources
	ComPtr<ID3D12DescriptorHeap> mRtvHeap;
	ComPtr<ID3D12DescriptorHeap> mDsvHeap;
	ComPtr<ID3D12DescriptorHeap> mSrvUavHeap; // SRV for depth, UAV for motion vectors

	D3D12_CPU_DESCRIPTOR_HANDLE mColorRTV = {};
	D3D12_CPU_DESCRIPTOR_HANDLE mDepthDSV = {};

	// Motion vector compute pipeline
	ComPtr<ID3D12RootSignature> mMVRootSignature;
	ComPtr<ID3D12PipelineState> mMVPipelineState;
	ComPtr<ID3D12Resource> mMVConstantBuffer;
	UINT8 *mMVCBMappedData = nullptr;

	// Viewport/scissor at render resolution
	D3D12_VIEWPORT mRenderViewport = {};
	D3D12_RECT mRenderScissorRect = {};

	// Device/queue references
	ID3D12Device *mDevice = nullptr;
	ID3D12CommandQueue *mCommandQueue = nullptr;
};
