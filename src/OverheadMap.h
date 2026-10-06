//***************************************************************************************
// OverheadMap.h
// Offscreen colour + depth target holding a top-down view of the dungeon.
// The colour texture can be rendered to (rasterization) or written by a ray
// generation shader (DXR) and is then shown as a HUD minimap.
//***************************************************************************************

#pragma once

#include "../Common/d3dUtil.h"

class OverheadMap {
  public:
	OverheadMap(ID3D12Device *device, UINT width, UINT height);

	OverheadMap(const OverheadMap &rhs) = delete;
	OverheadMap &operator=(const OverheadMap &rhs) = delete;
	~OverheadMap() = default;

	UINT Width() const;
	UINT Height() const;
	ID3D12Resource *Resource();
	ID3D12Resource *DepthResource();
	CD3DX12_CPU_DESCRIPTOR_HANDLE Rtv() const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv() const;

	D3D12_VIEWPORT Viewport() const;
	D3D12_RECT ScissorRect() const;

	void BuildDescriptors(
	    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
	    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
	    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv);

	// Colour format must match the back buffer so the regular scene PSOs can render into it.
	static const DXGI_FORMAT ColorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	static const DXGI_FORMAT DepthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

  private:
	void BuildResource();

  private:
	ID3D12Device *md3dDevice = nullptr;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	UINT mWidth = 0;
	UINT mHeight = 0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuRtv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuDsv;

	Microsoft::WRL::ComPtr<ID3D12Resource> mColor = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> mDepth = nullptr;
};
