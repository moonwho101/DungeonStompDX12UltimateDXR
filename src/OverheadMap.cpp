//***************************************************************************************
// OverheadMap.cpp
//***************************************************************************************

#include "OverheadMap.h"

OverheadMap::OverheadMap(ID3D12Device *device, UINT width, UINT height) {
	md3dDevice = device;

	mWidth = width;
	mHeight = height;

	mViewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
	mScissorRect = { 0, 0, (int)width, (int)height };

	BuildResource();
}

UINT OverheadMap::Width() const {
	return mWidth;
}

UINT OverheadMap::Height() const {
	return mHeight;
}

ID3D12Resource *OverheadMap::Resource() {
	return mColor.Get();
}

ID3D12Resource *OverheadMap::DepthResource() {
	return mDepth.Get();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE OverheadMap::Rtv() const {
	return mhCpuRtv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE OverheadMap::Dsv() const {
	return mhCpuDsv;
}

D3D12_VIEWPORT OverheadMap::Viewport() const {
	return mViewport;
}

D3D12_RECT OverheadMap::ScissorRect() const {
	return mScissorRect;
}

void OverheadMap::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
                                   CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
                                   CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv) {
	mhCpuSrv = hCpuSrv;
	mhCpuRtv = hCpuRtv;
	mhCpuDsv = hCpuDsv;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = ColorFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.PlaneSlice = 0;
	md3dDevice->CreateShaderResourceView(mColor.Get(), &srvDesc, mhCpuSrv);

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = ColorFormat;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;
	md3dDevice->CreateRenderTargetView(mColor.Get(), &rtvDesc, mhCpuRtv);

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = DepthFormat;
	dsvDesc.Texture2D.MipSlice = 0;
	md3dDevice->CreateDepthStencilView(mDepth.Get(), &dsvDesc, mhCpuDsv);
}

void OverheadMap::BuildResource() {
	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = mWidth;
	texDesc.Height = mHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = ColorFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	// UAV access lets the DXR ray generation shader write the same texture the raster path renders to.
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	D3D12_CLEAR_VALUE colorClear = {};
	colorClear.Format = ColorFormat;
	colorClear.Color[0] = 0.02f;
	colorClear.Color[1] = 0.02f;
	colorClear.Color[2] = 0.03f;
	colorClear.Color[3] = 1.0f;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
	    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
	    D3D12_HEAP_FLAG_NONE,
	    &texDesc,
	    D3D12_RESOURCE_STATE_GENERIC_READ,
	    &colorClear,
	    IID_PPV_ARGS(&mColor)));

	D3D12_RESOURCE_DESC depthDesc = texDesc;
	depthDesc.Format = DepthFormat;
	depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE depthClear = {};
	depthClear.Format = DepthFormat;
	depthClear.DepthStencil.Depth = 1.0f;
	depthClear.DepthStencil.Stencil = 0;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
	    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
	    D3D12_HEAP_FLAG_NONE,
	    &depthDesc,
	    D3D12_RESOURCE_STATE_DEPTH_WRITE,
	    &depthClear,
	    IID_PPV_ARGS(&mDepth)));
}
