//***************************************************************************************
// DungeonStompApp_Render.cpp by Mark Longo (C) 2026 All Rights Reserved.
//***************************************************************************************

#include "../Common/d3dApp.h"
#include "../Common/MathHelper.h"
#include "FrameResource.h"
#include "Dungeon.h"
#include <d3dtypes.h>
#include "world.hpp"
#include "GlobalSettings.hpp"
#include "Missle.hpp"
#include "GameLogic.hpp"
#include "DungeonStomp.hpp"
#include "Ssao.h"
#include "VRSHelper.h"
#include "DXRHelper.h"

using namespace DirectX;

extern bool enableDXR;
extern bool enableSSao;
extern bool drawingSSAO;
extern bool enableVRS;
extern bool enablePlayerHUD;
extern int cnt;
extern int trueplayernum;
extern bool drawingShadowMap;
extern bool enableShadowmapFeature;
extern int number_of_polys_per_frame;
extern POLY_SORT ObjectsToDraw[MAX_NUM_QUADS];
extern int *verts_per_poly;
extern bool enableVsync;
extern bool enableNormalmap;
extern Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap;

extern int displayCaptureIndex[1000];
extern int displayCaptureCount[1000];
extern int displayCapture;
extern int gravityon;
extern int outside;
extern OBJECTLIST *oblist;
extern int oblist_length;
extern GUNLIST *your_missle;
extern GUNLIST *your_gun;
extern int current_gun;
extern XMFLOAT3 mRotatedLightDirections[3];
extern int playerObjectStart;
extern int playerGunObjectStart;
extern int playerObjectEnd;
extern int number_of_tex_aliases;
extern bool ObjectHasShadow(int object_id);
extern int *texture_list_buffer;
extern TEXTUREMAPPING TexMap[MAX_NUM_TEXTURES];

float FastDistance(float fx, float fy, float fz);

void DungeonStompApp::Draw(const GameTimer &gt) {
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

	ProcessLights11();

	ID3D12DescriptorHeap *descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(1, descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	if (!enableDXR) {
		// Bind null SRV for shadow map pass.
		mCommandList->SetGraphicsRootDescriptorTable(5, mNullSrv);

		// Render shadow map to texture.
		DrawSceneToShadowMap(gt);
	}

	if (enableSSao && !enableDXR) {

		drawingSSAO = true;
		// Normal/depth pass.
		DrawNormalsAndDepth(gt);
		drawingSSAO = false;

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			mDepthStencilBuffer.Get(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			D3D12_RESOURCE_STATE_GENERIC_READ));

		// Compute SSAO.
		mCommandList->SetGraphicsRootSignature(mSsaoRootSignature.Get());
		mSsao->ComputeSsao(mCommandList.Get(), mCurrFrameResource, 3);

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			mDepthStencilBuffer.Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_STATE_DEPTH_WRITE));
	}

	// Main rendering pass.

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
	                                                                       D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// DXR rendering path
	if (enableDXR && mDXRInitialized && cnt > 0) {
		// Build/update acceleration structures (must be done after command list reset)
		auto currDungeonVB = mCurrFrameResource->DungeonVB.get();
		static bool blasBuilt = false;
		static int lastBlasCnt = 0;

		// Force a full rebuild when the vertex count changes (geometry
		// topology changed).  DXR AS updates require identical counts.
		bool forceRebuild = (blasBuilt && cnt != lastBlasCnt);
		bool isUpdate = blasBuilt && !forceRebuild;

		mDXRHelper->BuildBLAS(
		    md3dDevice.Get(),
		    mCommandList.Get(),
		    currDungeonVB->Resource(),
		    cnt,
		    sizeof(Vertex),
		    isUpdate);
		mDXRHelper->BuildTLAS(md3dDevice.Get(), mCommandList.Get(), isUpdate);
		blasBuilt = true;
		lastBlasCnt = cnt;

		// Dispatch rays for raytracing
		mDXRHelper->DispatchRays(mCommandList.Get(), mClientWidth, mClientHeight);

		// Copy raytracing output to back buffer
		mDXRHelper->CopyOutputToBackBuffer(mCommandList.Get(), CurrentBackBuffer());

		// DispatchRays binds the DXR heap; restore the main SRV heap so subsequent
		// graphics calls (HUD, text) can use handles from mSrvDescriptorHeap.
		ID3D12DescriptorHeap *srvHeaps[] = { mSrvDescriptorHeap.Get() };
		mCommandList->SetDescriptorHeaps(1, srvHeaps);
		mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

		// Begin main render pass. Preserve the raytracing output we copied to the back buffer
		// instead of clearing it so HUD can be drawn on top of the DXR result.
		D3D12_RENDER_PASS_RENDER_TARGET_DESC mainRtDesc = {};
		mainRtDesc.cpuDescriptor = CurrentBackBufferView();
		mainRtDesc.BeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
		mainRtDesc.EndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;

		D3D12_RENDER_PASS_DEPTH_STENCIL_DESC mainDsDesc = {};
		mainDsDesc.cpuDescriptor = DepthStencilView();
		mainDsDesc.DepthBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
		mainDsDesc.DepthBeginningAccess.Clear.ClearValue.Format = mDepthStencilFormat;
		mainDsDesc.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth = 1.0f;
		mainDsDesc.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Stencil = 0;
		mainDsDesc.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
		mainDsDesc.StencilBeginningAccess.Clear.ClearValue = mainDsDesc.DepthBeginningAccess.Clear.ClearValue;
		mainDsDesc.DepthEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
		mainDsDesc.StencilEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;

		mCommandList->BeginRenderPass(1, &mainRtDesc, &mainDsDesc, D3D12_RENDER_PASS_FLAG_NONE);

		auto passCB = mCurrFrameResource->PassCB->Resource();
		mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

		if (enablePlayerHUD) {
			DisplayHud();
			SetDungeonText();
		}

		ScanMod(gt.DeltaTime());
		FlushRectangles();
		FlushText();

		// Reset VRS to full rate after rendering
		if (enableVRS && mVRSHelper.IsSupported()) {
			mVRSHelper.SetFullRate(mCommandList.Get());
		}

		mCommandList->EndRenderPass();

	} else {
		// Begin main render pass with clear (standard rasterization path).
		D3D12_RENDER_PASS_RENDER_TARGET_DESC mainRtDesc = {};
		mainRtDesc.cpuDescriptor = CurrentBackBufferView();
		mainRtDesc.BeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
		mainRtDesc.BeginningAccess.Clear.ClearValue.Format = mBackBufferFormat;
		memcpy(mainRtDesc.BeginningAccess.Clear.ClearValue.Color, &mMainPassCB.FogColor, 4 * sizeof(float));
		mainRtDesc.EndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;

		D3D12_RENDER_PASS_DEPTH_STENCIL_DESC mainDsDesc = {};
		mainDsDesc.cpuDescriptor = DepthStencilView();
		mainDsDesc.DepthBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
		mainDsDesc.DepthBeginningAccess.Clear.ClearValue.Format = mDepthStencilFormat;
		mainDsDesc.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth = 1.0f;
		mainDsDesc.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Stencil = 0;
		mainDsDesc.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
		mainDsDesc.StencilBeginningAccess.Clear.ClearValue = mainDsDesc.DepthBeginningAccess.Clear.ClearValue;
		mainDsDesc.DepthEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
		mainDsDesc.StencilEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;

		mCommandList->BeginRenderPass(1, &mainRtDesc, &mainDsDesc, D3D12_RENDER_PASS_FLAG_NONE);

		auto passCB = mCurrFrameResource->PassCB->Resource();
		mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

		// Render the main scene (VRS is applied inside DrawRenderItems per draw type)
		DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque], gt);

		// Reset VRS to full rate after rendering
		if (enableVRS && mVRSHelper.IsSupported()) {
			mVRSHelper.SetFullRate(mCommandList.Get());
		}

		mCommandList->EndRenderPass();
	}

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
	                                                                       D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList *cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	if (enableVsync) {
		ThrowIfFailed(mSwapChain->Present(1, 0)); // vsync on
	} else {
		// Tearing is allowed in any windowed mode (including borderless) — only exclusive fullscreen forbids it.
		UINT presentFlags = mTearingSupported ? DXGI_PRESENT_ALLOW_TEARING : 0;
		ThrowIfFailed(mSwapChain->Present(0, presentFlags)); // vsync off
	}

	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	mCurrFrameResource->Fence = ++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point.
	// Because we are on the GPU timeline, the new fence point won't be
	// set until the GPU finishes processing all the commands prior to this Signal().
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void DungeonStompApp::DrawSceneToShadowMap(const GameTimer &gt) {
	mCommandList->RSSetViewports(1, &mShadowMap->Viewport());
	mCommandList->RSSetScissorRects(1, &mShadowMap->ScissorRect());

	// Change to DEPTH_WRITE.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Resource(),
	                                                                       D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

	// Depth-only render pass for shadow map.
	D3D12_RENDER_PASS_DEPTH_STENCIL_DESC shadowDsDesc = {};
	shadowDsDesc.cpuDescriptor = mShadowMap->Dsv();
	shadowDsDesc.DepthBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
	shadowDsDesc.DepthBeginningAccess.Clear.ClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	shadowDsDesc.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth = 1.0f;
	shadowDsDesc.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Stencil = 0;
	shadowDsDesc.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
	shadowDsDesc.StencilBeginningAccess.Clear.ClearValue = shadowDsDesc.DepthBeginningAccess.Clear.ClearValue;
	shadowDsDesc.DepthEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
	shadowDsDesc.StencilEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;

	mCommandList->BeginRenderPass(0, nullptr, &shadowDsDesc, D3D12_RENDER_PASS_FLAG_NONE);

	// Bind the pass constant buffer for the shadow map pass.
	auto passCB = mCurrFrameResource->PassCB->Resource();
	D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + 1 * passCBByteSize;
	mCommandList->SetGraphicsRootConstantBufferView(2, passCBAddress);

	mCommandList->SetPipelineState(mPSOs["shadow_opaque"].Get());

	if (enableShadowmapFeature) {
		drawingShadowMap = true;
		DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque], gt);
		drawingShadowMap = false;
	}

	mCommandList->EndRenderPass();

	// Change back to GENERIC_READ so we can read the texture in a shader.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Resource(),
	                                                                       D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void DungeonStompApp::DrawNormalsAndDepth(const GameTimer &gt) {
	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	auto normalMap = mSsao->NormalMap();
	auto normalMapRtv = mSsao->NormalMapRtv();

	// Change to RENDER_TARGET.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(normalMap,
	                                                                       D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Render pass for normals/depth.
	D3D12_RENDER_PASS_RENDER_TARGET_DESC normalRtDesc = {};
	normalRtDesc.cpuDescriptor = normalMapRtv;
	normalRtDesc.BeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
	normalRtDesc.BeginningAccess.Clear.ClearValue.Format = Ssao::NormalMapFormat;
	normalRtDesc.BeginningAccess.Clear.ClearValue.Color[0] = 0.0f;
	normalRtDesc.BeginningAccess.Clear.ClearValue.Color[1] = 0.0f;
	normalRtDesc.BeginningAccess.Clear.ClearValue.Color[2] = 1.0f;
	normalRtDesc.BeginningAccess.Clear.ClearValue.Color[3] = 0.0f;
	normalRtDesc.EndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;

	D3D12_RENDER_PASS_DEPTH_STENCIL_DESC normalDsDesc = {};
	normalDsDesc.cpuDescriptor = DepthStencilView();
	normalDsDesc.DepthBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
	normalDsDesc.DepthBeginningAccess.Clear.ClearValue.Format = mDepthStencilFormat;
	normalDsDesc.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth = 1.0f;
	normalDsDesc.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Stencil = 0;
	normalDsDesc.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
	normalDsDesc.StencilBeginningAccess.Clear.ClearValue = normalDsDesc.DepthBeginningAccess.Clear.ClearValue;
	normalDsDesc.DepthEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
	normalDsDesc.StencilEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;

	mCommandList->BeginRenderPass(1, &normalRtDesc, &normalDsDesc, D3D12_RENDER_PASS_FLAG_NONE);

	// Bind the constant buffer for this pass.
	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

	mCommandList->SetPipelineState(mPSOs["drawNormals"].Get());

	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque], gt);

	mCommandList->EndRenderPass();

	// Change back to GENERIC_READ so we can read the texture in a shader.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(normalMap,
	                                                                       D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void DungeonStompApp::DrawRenderItemsFL(ID3D12GraphicsCommandList *cmdList, const std::vector<RenderItem *> &ritems) {
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

	CD3DX12_GPU_DESCRIPTOR_HANDLE tex3(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	tex3.Offset(484, mCbvSrvDescriptorSize);
	cmdList->SetGraphicsRootDescriptorTable(6, tex3); // Set the gCubeMap

	// auto ri = ritems[1];

	// For each render item...
	// for (size_t i = 0; i < ritems.size(); ++i)
	//{
	auto ri = ritems[1];

	cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
	cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
	cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

	D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;

	UINT materialIndex = mMaterials["flat"].get()->MatCBIndex;

	D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + materialIndex * matCBByteSize;

	cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);
	cmdList->SetGraphicsRootConstantBufferView(1, matCBAddress);

	cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	//}
}

void DungeonStompApp::DrawRenderItems(ID3D12GraphicsCommandList *cmdList, const std::vector<RenderItem *> &ritems, const GameTimer &gt) {

	auto ri = ritems[0];

	cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
	cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
	cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

	CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	tex.Offset(1, mCbvSrvDescriptorSize);
	cmdList->SetGraphicsRootDescriptorTable(3, tex);

	bool enablePSO = true;

	if (drawingShadowMap || drawingSSAO) {
		enablePSO = false;
	}

	if (enablePSO) {
		if (enableSSao) {
			mCommandList->SetPipelineState(mPSOs["normalMapSsao"].Get());
		} else {
			mCommandList->SetPipelineState(mPSOs["normalMap"].Get());
		}
	}
	// VRS: Full rate for normal-mapped geometry (high-frequency detail benefits most)
	if (enableVRS && mVRSHelper.IsSupported()) {
		mVRSHelper.SetFullRate(mCommandList.Get());
	}
	// Draw dungeon, monsters and items with normal maps
	DrawDungeon(cmdList, ritems, false, false, true);

	if (enablePSO) {
		if (enableSSao) {
			mCommandList->SetPipelineState(mPSOs["opaqueSsao"].Get());
		} else {
			mCommandList->SetPipelineState(mPSOs["opaque"].Get());
		}
	}
	// VRS: Reduced rate for non-normal-mapped geometry (less detail to preserve)
	if (enableVRS && mVRSHelper.IsSupported()) {
		mVRSHelper.SetReducedRate(mCommandList.Get());
	}
	// Draw dungeon, monsters and items without normal maps
	DrawDungeon(cmdList, ritems, false, false, false);

	if (enablePSO) {
		mCommandList->SetPipelineState(mPSOs["transparent"].Get());
	}
	// VRS: Reduced rate for transparent items
	if (enableVRS && mVRSHelper.IsSupported()) {
		mVRSHelper.SetReducedRate(mCommandList.Get());
	}
	// Draw alpha transparent items
	DrawDungeon(cmdList, ritems, true);

	if (enablePSO) {
		mCommandList->SetPipelineState(mPSOs["torchTested"].Get());
	}
	// VRS: Reduced rate for torches/effects (visual noise masks quality differences)
	if (enableVRS && mVRSHelper.IsSupported()) {
		mVRSHelper.SetReducedRate(mCommandList.Get());
	}
	// Draw the torches and effects
	DrawDungeon(cmdList, ritems, true, true);

	if (enablePSO) {

		tex.Offset(377, mCbvSrvDescriptorSize);
		cmdList->SetGraphicsRootDescriptorTable(3, tex);

		cmdList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		// cmdList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		if (enablePlayerHUD) {
			for (int i = 0; i < displayCapture; i++) {
				for (int j = 0; j < displayCaptureCount[i]; j++) {
					cmdList->DrawInstanced(4, 1, displayCaptureIndex[i] + (j * 4), 0);
				}
			}
		}

		// Draw the skybox
		if (!gravityon || outside) {
			mCommandList->SetPipelineState(mPSOs["sky"].Get());
			// VRS: Reduced rate for skybox (low-detail, distant geometry)
			if (enableVRS && mVRSHelper.IsSupported()) {
				mVRSHelper.SetReducedRate(mCommandList.Get());
			}
			DrawRenderItemsFL(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);
		}

		if (enablePlayerHUD) {
			DisplayHud();
			SetDungeonText();
		}

		ScanMod(gt.DeltaTime());
		FlushRectangles();
		FlushText();
	}

	return;
}

void DungeonStompApp::DrawDungeon(ID3D12GraphicsCommandList *cmdList, const std::vector<RenderItem *> &ritems, BOOL isAlpha, bool isTorch, bool normalMap) {

	auto ri = ritems[0];

	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

	bool draw = true;

	int currentObject = 0;
	for (currentObject = 0; currentObject < number_of_polys_per_frame; currentObject++) {
		int i = ObjectsToDraw[currentObject].vert_index;
		int vert_index = ObjectsToDraw[currentObject].srcstart;
		int fperpoly = ObjectsToDraw[currentObject].srcfstart;
		int face_index = ObjectsToDraw[currentObject].srcfstart;

		int texture_alias_number = texture_list_buffer[i];
		int texture_number = TexMap[texture_alias_number].texture;

		int normal_map_texture = TexMap[texture_alias_number].normalmaptextureid;

		if (texture_alias_number == 104) {
			TexMap[texture_alias_number].is_alpha_texture = 1;
		}

		draw = true;

		if (isAlpha) {
			if (texture_number >= 94 && texture_number <= 101 ||
			    texture_number >= 289 - 1 && texture_number <= 296 - 1 ||
			    texture_number >= 279 - 1 && texture_number <= 288 - 1 ||
			    texture_number >= 206 - 1 && texture_number <= 210 - 1 ||
			    texture_number == 378) {

				if (isAlpha && !isTorch) {
					draw = false;
				}

				if (isAlpha && isTorch) {
					draw = true;
				}
			}
		}

		if (normal_map_texture == -1 && normalMap) {
			draw = false;
		}

		if (!normalMap && normal_map_texture != -1) {
			draw = false;
		}

		int oid = 0;

		if (drawingSSAO || drawingShadowMap) {
			oid = ObjectsToDraw[currentObject].objectId;

			// Don't draw player captions
			if (oid == -99) {
				draw = false;
			}
		}

		if (drawingShadowMap) {

			if (oid == -1) {
				// Draw 3DS and MD2 Shadows
				draw = true;
			} else {
				draw = false;
			}

			if (oid > 0) {
				// Draw objects.dat that have SHADOW attribute set to 1
				if (ObjectHasShadow(oid)) {
					draw = true;
				}
			}

			if (ObjectsToDraw[currentObject].castshaddow == 0) {
				draw = false;
			}
		}

		if (currentObject >= playerGunObjectStart && currentObject < playerObjectStart && drawingShadowMap) {
			// don't draw the onscreen player weapon
			draw = false;
		}

		if (currentObject >= playerObjectStart && currentObject < playerObjectEnd && !drawingShadowMap) {
			draw = false;
		}

		if (currentObject >= playerObjectStart && currentObject < playerObjectEnd && drawingShadowMap) {
			draw = true;
		}

		if (draw) {

			// default,grass,water,brick,stone,tile,crate,ice,bone,metal,wood
			auto textureType = TexMap[texture_number].material.name;

			UINT materialIndex = mMaterials[textureType].get()->MatCBIndex;

			D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
			D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + materialIndex * matCBByteSize;

			cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress); // Set cbPerObject
			cmdList->SetGraphicsRootConstantBufferView(1, matCBAddress); // Set cbMaterial

			CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
			tex.Offset(texture_number, mCbvSrvDescriptorSize);
			cmdList->SetGraphicsRootDescriptorTable(3, tex); // Set gDiffuseMap

			CD3DX12_GPU_DESCRIPTOR_HANDLE nullTex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
			nullTex.Offset(mNullTexSrvIndex1, mCbvSrvDescriptorSize);

			CD3DX12_GPU_DESCRIPTOR_HANDLE tex3(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
			if (drawingShadowMap || drawingSSAO) {
				tex3 = nullTex;
			} else {
				tex3.Offset(number_of_tex_aliases + 1, mCbvSrvDescriptorSize);
			}
			cmdList->SetGraphicsRootDescriptorTable(5, tex3); // Set gShadowMap

			CD3DX12_GPU_DESCRIPTOR_HANDLE tex4(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
			if (drawingShadowMap || drawingSSAO) {
				tex4 = nullTex;
			} else {
				tex4.Offset(number_of_tex_aliases + 2, mCbvSrvDescriptorSize);
			}
			cmdList->SetGraphicsRootDescriptorTable(7, tex4); // Set gSsaoMap

			// CHECK THIS
			if (normalMap && !drawingShadowMap && !drawingSSAO) {
				CD3DX12_GPU_DESCRIPTOR_HANDLE tex2(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
				tex2.Offset(normal_map_texture, mCbvSrvDescriptorSize);
				cmdList->SetGraphicsRootDescriptorTable(4, tex2); // Set gNormalMap
			}

			if (dp_command_index_mode[i] == 1 && TexMap[texture_alias_number].is_alpha_texture == isAlpha) { // USE_NON_INDEXED_DP
				int v = verts_per_poly[currentObject];

				if (dp_commands[currentObject] == D3DPT_TRIANGLELIST) {
					cmdList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				} else if (dp_commands[currentObject] == D3DPT_TRIANGLESTRIP) {
					cmdList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				}
				cmdList->DrawInstanced(v, 1, vert_index, 0);
			}
		}
	}

	UINT materialIndex = mMaterials["default"].get()->MatCBIndex;

	D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
	D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + materialIndex * matCBByteSize;

	cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);
	cmdList->SetGraphicsRootConstantBufferView(1, matCBAddress);
}

void DungeonStompApp::ProcessLights11() {
	// D = directional, P = pointlight, M = misslelight, C = sword light, S = spotlight
	// 012345678901234567890123456
	// 012345678901234567890123456
	// DPPPPPPPPPPPMMMMCSSSSSSSSSS
	// XXXXXXXX    XX  XXXXX

	int sort[200];
	float dist[200];
	int obj[200];
	int temp;

	for (int i = 0; i < MaxLights; i++) {
		LightContainer[i].Strength = { 1.0f, 1.0f, 1.0f };
		LightContainer[i].FalloffStart = 80.0f;
		LightContainer[i].Direction = { 0.0f, -1.0f, 0.0f };
		LightContainer[i].FalloffEnd = 120.0f;
		LightContainer[i].Position = DirectX::XMFLOAT3{ 0.0f, 9000.0f, 0.0f };
		LightContainer[i].SpotPower = 0.0f;
	}

	// first light is directional
	LightContainer[0].Strength = { 0.15f, 0.15f, 0.15f };
	LightContainer[0].Direction = mRotatedLightDirections[0];

	int dcount = 0;
	// Find lights
	for (int q = 0; q < oblist_length; q++) {
		int ob_type = oblist[q].type;
		float qdist = FastDistance(m_vEyePt.x - oblist[q].x,
		                           m_vEyePt.y - oblist[q].y,
		                           m_vEyePt.z - oblist[q].z);
		// if (ob_type == 57)
		if (ob_type == 6 && oblist[q].light_source->command == 900)
		// if (ob_type == 6 && qdist < 2500 && oblist[q].light_source->command == 900)
		{
			dist[dcount] = qdist;
			sort[dcount] = dcount;
			obj[dcount] = q;
			dcount++;
		}
	}
	// sorting - ASCENDING ORDER
	for (int i = 0; i < dcount; i++) {
		for (int j = i + 1; j < dcount; j++) {
			if (dist[sort[i]] > dist[sort[j]]) {
				temp = sort[i];
				sort[i] = sort[j];
				sort[j] = temp;
			}
		}
	}

	if (dcount > 16) {
		dcount = 16;
	}

	for (int i = 0; i < dcount; i++) {
		int q = obj[sort[i]];
		float dist2 = dist[sort[i]];

		int angle = (int)oblist[q].rot_angle;
		int ob_type = oblist[q].type;

		//+1 because 0 is reserved for directional light
		LightContainer[i + 1].Strength = { 9.0f, 9.0f, 9.0f };
		LightContainer[i + 1].Position = DirectX::XMFLOAT3{ oblist[q].x, oblist[q].y + 43.0f, oblist[q].z };
	}

	int count = 0;

	for (int misslecount = 0; misslecount < MAX_MISSLE; misslecount++) {
		if (your_missle[misslecount].active == 1) {
			if (count < 4) {

				float r = MathHelper::RandF(10.0f, 100.0f);

				LightContainer[12 + count].Position = DirectX::XMFLOAT3{ your_missle[misslecount].x, your_missle[misslecount].y, your_missle[misslecount].z };
				LightContainer[12 + count].Strength = DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f };
				LightContainer[12 + count].FalloffStart = 100.0f;
				LightContainer[12 + count].Direction = { 0.0f, -1.0f, 0.0f };
				LightContainer[12 + count].FalloffEnd = 200.0f;
				LightContainer[12 + count].SpotPower = 0.0f;

				if (your_missle[misslecount].model_id == 103) {
					LightContainer[12 + count].Strength = DirectX::XMFLOAT3{ 0.0f, 3.0f, 2.843f };
				} else if (your_missle[misslecount].model_id == 104) {
					LightContainer[12 + count].Strength = DirectX::XMFLOAT3{ 3.0f, 0.396f, 0.5f };
				} else if (your_missle[misslecount].model_id == 105) {
					LightContainer[12 + count].Strength = DirectX::XMFLOAT3{ 1.91f, 3.1f, 1.0f };
				}
				count++;
			}
		}
	}

	bool flamesword = false;

	if (strstr(your_gun[current_gun].gunname, "FLAME") != NULL ||
	    strstr(your_gun[current_gun].gunname, "ICE") != NULL ||
	    strstr(your_gun[current_gun].gunname, "LIGHTNINGSWORD") != NULL) {
		flamesword = true;
	}

	if (flamesword) {

		int spot = 16;

		LightContainer[spot].Position = DirectX::XMFLOAT3{ m_vEyePt.x, m_vEyePt.y + 25.0f, m_vEyePt.z };
		LightContainer[spot].Strength = DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f };
		LightContainer[spot].FalloffStart = 200.0f;
		LightContainer[spot].Direction = { 0.0f, -1.0f, 0.0f };
		LightContainer[spot].FalloffEnd = 300.0f;
		LightContainer[spot].SpotPower = 0.0f;

		if (strstr(your_gun[current_gun].gunname, "SUPERFLAME") != NULL) {
			LightContainer[spot].Strength = DirectX::XMFLOAT3{ 8.0f, 0.867f, 0.0f };
		} else if (strstr(your_gun[current_gun].gunname, "FLAME") != NULL) {
			LightContainer[spot].Strength = DirectX::XMFLOAT3{ 7.0f, 0.369f, 0.0f };
		} else if (strstr(your_gun[current_gun].gunname, "ICE") != NULL) {

			LightContainer[spot].Strength = DirectX::XMFLOAT3{ 0.0f, 0.796f, 5.0f };
		} else if (strstr(your_gun[current_gun].gunname, "LIGHTNINGSWORD") != NULL) {
			LightContainer[spot].Strength = DirectX::XMFLOAT3{ 6.0f, 6.0f, 6.0f };
		}
	}

	count = 0;
	dcount = 0;

	// Find lights SPOT
	for (int q = 0; q < oblist_length; q++) {
		int ob_type = oblist[q].type;
		float qdist = FastDistance(m_vEyePt.x - oblist[q].x,
		                           m_vEyePt.y - oblist[q].y,
		                           m_vEyePt.z - oblist[q].z);
		// if (ob_type == 6)
		if (ob_type == 6 && qdist < 2500 && oblist[q].light_source->command == 1) {
			dist[dcount] = qdist;
			sort[dcount] = dcount;
			obj[dcount] = q;
			dcount++;
		}
	}

	// sorting - ASCENDING ORDER
	for (int i = 0; i < dcount; i++) {
		for (int j = i + 1; j < dcount; j++) {
			if (dist[sort[i]] > dist[sort[j]]) {
				temp = sort[i];
				sort[i] = sort[j];
				sort[j] = temp;
			}
		}
	}

	if (dcount > 10) {
		dcount = 10;
	}

	for (int i = 0; i < dcount; i++) {
		int q = obj[sort[i]];
		float dist2 = dist[sort[i]];
		int angle = (int)oblist[q].rot_angle;
		int ob_type = oblist[q].type;
		float adjust = 0.0f;
		LightContainer[i + 17].Position = DirectX::XMFLOAT3{ oblist[q].x, oblist[q].y + 0.0f, oblist[q].z };
		LightContainer[i + 17].Strength = DirectX::XMFLOAT3{ (float)oblist[q].light_source->rcolour + adjust, (float)oblist[q].light_source->gcolour + adjust, (float)oblist[q].light_source->bcolour + adjust };
		LightContainer[i + 17].FalloffStart = 600.0f;
		LightContainer[i + 17].Direction = { oblist[q].light_source->direction_x, oblist[q].light_source->direction_y, oblist[q].light_source->direction_z };
		LightContainer[i + 17].FalloffEnd = 650.0f;
		LightContainer[i + 17].SpotPower = 1.9f;
	}
}
