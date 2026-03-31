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
		// Compute SSAO.
		mCommandList->SetGraphicsRootSignature(mSsaoRootSignature.Get());
		mSsao->ComputeSsao(mCommandList.Get(), mCurrFrameResource, 3);
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
	auto objectCB = mCurrFrameResource->ObjectCB->Resource();

	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i) {
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;

		cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

void DungeonStompApp::DrawRenderItems(ID3D12GraphicsCommandList *cmdList, const std::vector<RenderItem *> &ritems, const GameTimer &gt) {
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i) {
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;

		cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);
		cmdList->SetGraphicsRootConstantBufferView(1, matCBAddress);

		if (ri->Geo->Name == "waterGeo") {
			DrawDungeon(cmdList, ritems, FALSE, false, enableNormalmap);
			break;
		} else {

			cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
		}
	}
}

void DungeonStompApp::DrawDungeon(ID3D12GraphicsCommandList *cmdList, const std::vector<RenderItem *> &ritems, BOOL isAlpha, bool isTorch, bool normalMap) {
	// static int savelastmove = 0;
	// static int savelastmove2 = 0;

	// savelastmove2 = playercurrentmove;

	// Apply Variable Rate Shading (VRS) based on render type
	if (enableVRS && mVRSHelper.IsSupported()) {
		if (drawingShadowMap) {
			// Shadow pass doesn't need high precision
			mVRSHelper.SetShadingRate(mCommandList.Get(), D3D12_SHADING_RATE_2X2);
		} else if (drawingSSAO) {
			// SSAO normals/depth pass
			mVRSHelper.SetShadingRate(mCommandList.Get(), D3D12_SHADING_RATE_1X2);
		} else {
			// Main scene pass - full rate
			mVRSHelper.SetFullRate(mCommandList.Get());
		}
	}

	for (int currentObject = 0; currentObject < number_of_polys_per_frame; currentObject++) {

		int srcStart = ObjectsToDraw[currentObject].srcstart;
		int vertsPerPoly = verts_per_poly[currentObject];
		int vertIndex = ObjectsToDraw[currentObject].vert_index;

		int textureAliasNumber = texture_list_buffer[vertIndex];

		short tex_id = (short)TexMap[textureAliasNumber].texture;

		int normalMapAliasId = TexMap[textureAliasNumber].normalmaptextureid;
		int normalMapTexture = (normalMapAliasId >= 0) ? TexMap[normalMapAliasId].texture : -1;

		if (tex_id < 0)
			continue;

		if (tex_id >= MAX_NUM_TEXTURES)
			continue;

		if (isTorch == false) {
			if (normalMap == true && normalMapTexture != -1) {
				if (enableSSao && !drawingShadowMap)
					mCommandList->SetPipelineState(mPSOs["normalMapSsao"].Get());
				else
					mCommandList->SetPipelineState(mPSOs["normalMap"].Get());
			} else {
				if (enableSSao && !drawingShadowMap)
					mCommandList->SetPipelineState(mPSOs["opaqueSsao"].Get());
				else
					mCommandList->SetPipelineState(mPSOs["opaque"].Get());
			}
		} else {
			mCommandList->SetPipelineState(mPSOs["torch"].Get());
		}

		if (drawingShadowMap) {
			mCommandList->SetPipelineState(mPSOs["shadow_opaque"].Get());
		}

		if (drawingSSAO) {
			mCommandList->SetPipelineState(mPSOs["drawNormals"].Get());
		}

		mCommandList->SetGraphicsRootDescriptorTable(3, GetGpuSrv(tex_id));

		if (normalMap == true && normalMapTexture != -1) {
			mCommandList->SetGraphicsRootDescriptorTable(6, GetGpuSrv(normalMapTexture));
		} else {
			mCommandList->SetGraphicsRootDescriptorTable(6, mNullSrv);
		}

		if (enableSSao && !drawingShadowMap) {
			mCommandList->SetGraphicsRootDescriptorTable(4, mSsao->AmbientMapSrv());
		} else {
			mCommandList->SetGraphicsRootDescriptorTable(4, mNullSrv);
		}

		if (!drawingShadowMap) {
			mCommandList->SetGraphicsRootDescriptorTable(5, mShadowMap->Srv());
		}

		mCommandList->DrawInstanced(vertsPerPoly, 1, srcStart, 0);
	}
}

void ProcessMissleLights();
void ProcessPlayerLight();
void ProcessTorchLight();
void ProcessMonstersLights();
void ProcessDungeonLights();

void DungeonStompApp::ProcessLights11() {
	int i;
	for (i = 0; i < MaxLights; i++) {
		LightContainer[i].Strength = { 0.0f, 0.0f, 0.0f };
	}

	ProcessMissleLights();
	ProcessPlayerLight();
	ProcessTorchLight();
	ProcessMonstersLights();
	ProcessDungeonLights();
}
