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
#include "CameraBob.hpp"

using namespace DirectX;

extern int trueplayernum;
extern PLAYER *player_list;
extern int displayShadowMap;
extern int displayShadowMapKeyPress;
extern bool enableSSao;
extern bool enableSSaoKey;
extern bool enableCameraBob;
extern bool enableCameraBobKey;
extern bool enableNormalmap;
extern bool enableNormalmapKey;
extern bool enableVsync;
extern bool enableVsyncKey;
extern bool enableShadowmapFeature;
extern bool enableShadowmapFeatureKey;
extern bool enableVRS;
extern bool enableVRSKey;
extern bool enablePlayerHUD;
extern bool enablePlayerHUDKey;
extern bool enableDXR;
extern bool enableDXRKey;
extern char gActionMessage[2048];
extern int playercurrentmove;
extern CameraBob bobY;
extern CameraBob bobX;
extern float angy;
extern float look_up_ang;
extern float look_roll_ang;
extern float k;
extern XMFLOAT3 m_vEyePt;
extern XMFLOAT3 m_vLookatPt;
extern XMFLOAT3 GunTruesave;
extern int cnt;
extern D3DVERTEX2 *src_v;
extern int number_of_polys_per_frame;
extern POLY_SORT ObjectsToDraw[MAX_NUM_QUADS];
extern BOOL *dp_command_index_mode;
extern D3DPRIMITIVETYPE *dp_commands;
extern int *verts_per_poly;
extern int *texture_list_buffer;
extern int playerObjectStart;
extern int playerGunObjectStart;
extern int playerObjectEnd;
extern TEXTUREMAPPING TexMap[MAX_NUM_TEXTURES];
extern int number_of_tex_aliases;
extern int sliststart;
extern SCROLLLISTING scrolllist1[50];

int UpdateScrollList(int r, int g, int b);
float fixangle(float angle, float node);
float FastDistance(float fx, float fy, float fz);

VOID UpdateControls();
HRESULT FrameMove(double fTime, FLOAT fTimeKey);
void UpdateWorld(float fElapsedTime);

void DungeonStompApp::Update(const GameTimer &gt) {
	float t = gt.DeltaTime();
	UpdateControls();
	FrameMove(0.0f, t);
	UpdateWorld(t);
	OnKeyboardInput(gt);

	bobY.update(t);
	bobX.update(t);

	UpdateCamera(gt);

	// Cycle through the circular frame resource array.
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence) {
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	// mLightRotationAngle += 0.1f * gt.DeltaTime();
	XMMATRIX R = XMMatrixRotationY(mLightRotationAngle);
	for (int i = 0; i < 3; ++i) {
		XMVECTOR lightDir = XMLoadFloat3(&mBaseLightDirections[i]);
		lightDir = XMVector3TransformNormal(lightDir, R);
		XMStoreFloat3(&mRotatedLightDirections[i], lightDir);
	}

	if (!enableDXR) {
		UpdateObjectCBs(gt);
		UpdateMaterialCBs(gt);
		UpdateShadowTransform(gt, 0);
		UpdateMainPassCB(gt);
		UpdateSsaoCB(gt);
		UpdateShadowPassCB(gt);
	}
	// DisplayPlayerCaption();
	UpdateDungeon(gt);
}

void DungeonStompApp::OnMouseDown(WPARAM btnState, int x, int y) {
}

void DungeonStompApp::OnMouseUp(WPARAM btnState, int x, int y) {
}

void DungeonStompApp::OnMouseMove(WPARAM btnState, int x, int y) {
}

void DungeonStompApp::OnKeyboardInput(const GameTimer &gt) {
	// rise from the dead
	if (player_list[trueplayernum].bIsPlayerAlive == FALSE) {
		if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
			player_list[trueplayernum].bIsPlayerAlive = TRUE;
			player_list[trueplayernum].health = player_list[trueplayernum].hp;
		}
	}

	if (GetAsyncKeyState('M') && !displayShadowMapKeyPress) {
		if (displayShadowMap)
			displayShadowMap = 0;
		else
			displayShadowMap = 1;
	}

	if (GetAsyncKeyState('M')) {
		displayShadowMapKeyPress = 1;
	} else {
		displayShadowMapKeyPress = 0;
	}

	if (GetAsyncKeyState('O') && !enableSSaoKey) {
		if (enableSSao) {
			enableSSao = 0;
			strcpy_s(gActionMessage, "SSAO Disabled");
			UpdateScrollList(0, 255, 255);
		} else {
			strcpy_s(gActionMessage, "SSAO Enabled");
			UpdateScrollList(0, 255, 255);
			enableSSao = 1;
		}
	}

	if (GetAsyncKeyState('O')) {
		enableSSaoKey = 1;
	} else {
		enableSSaoKey = 0;
	}

	if (GetAsyncKeyState('B') && !enableCameraBobKey) {
		if (enableCameraBob) {
			enableCameraBob = false;
			strcpy_s(gActionMessage, "Camera bob Disabled");
			UpdateScrollList(0, 255, 255);
		} else {
			strcpy_s(gActionMessage, "Camera bob Enabled");
			UpdateScrollList(0, 255, 255);
			enableCameraBob = true;
		}
	}

	if (GetAsyncKeyState('B')) {
		enableCameraBobKey = 1;
	} else {
		enableCameraBobKey = 0;
	}

	if (GetAsyncKeyState('N') && !enableNormalmapKey) {
		if (enableNormalmap) {
			enableNormalmap = false;
			SetTextureNormalMapEmpty();
			strcpy_s(gActionMessage, "Normal map Disabled");
			UpdateScrollList(0, 255, 255);
		} else {
			SetTextureNormalMap();
			strcpy_s(gActionMessage, "Normal map Enabled");
			UpdateScrollList(0, 255, 255);
			enableNormalmap = true;
		}
	}

	if (GetAsyncKeyState('N')) {
		enableNormalmapKey = 1;
	} else {
		enableNormalmapKey = 0;
	}

	if (GetAsyncKeyState('V') && !enableVsyncKey) {
		if (enableVsync) {
			enableVsync = false;
			strcpy_s(gActionMessage, "VSync Disabled");
			UpdateScrollList(0, 255, 255);
		} else {
			strcpy_s(gActionMessage, "VSync Enabled");
			UpdateScrollList(0, 255, 255);
			enableVsync = true;
		}
	}

	if (GetAsyncKeyState('V')) {
		enableVsyncKey = 1;
	} else {
		enableVsyncKey = 0;
	}

	if (GetAsyncKeyState('J') && !enableShadowmapFeatureKey) {
		enableShadowmapFeature = !enableShadowmapFeature;
		if (enableShadowmapFeature) {
			strcpy_s(gActionMessage, "Shadowmap Feature Enabled");
		} else {
			strcpy_s(gActionMessage, "Shadowmap Feature Disabled");
		}
		UpdateScrollList(0, 255, 255);
	}
	if (GetAsyncKeyState('J')) {
		enableShadowmapFeatureKey = true;
	} else {
		enableShadowmapFeatureKey = false;
	}

	if (GetAsyncKeyState('T') && !enableVRSKey) {
		enableVRS = !enableVRS;
		if (enableVRS && mVRSHelper.IsSupported()) {
			strcpy_s(gActionMessage, "Variable Rate Shading Enabled");
		} else if (!mVRSHelper.IsSupported()) {
			enableVRS = false;
			strcpy_s(gActionMessage, "VRS Not Supported on this GPU");
		} else {
			strcpy_s(gActionMessage, "Variable Rate Shading Disabled");
		}
		UpdateScrollList(0, 255, 255);
	}
	if (GetAsyncKeyState('T')) {
		enableVRSKey = true;
	} else {
		enableVRSKey = false;
	}

	if (GetAsyncKeyState('H') && !enablePlayerHUDKey) {
		enablePlayerHUD = !enablePlayerHUD;
		if (enablePlayerHUD) {
			strcpy_s(gActionMessage, "Player HUD Enabled");
		} else {
			strcpy_s(gActionMessage, "Player HUD Disabled");
		}
		UpdateScrollList(0, 255, 255);
	}
	if (GetAsyncKeyState('H')) {
		enablePlayerHUDKey = true;
	} else {
		enablePlayerHUDKey = false;
	}

	// DXR toggle ('R' key)
	if (GetAsyncKeyState('R') && !enableDXRKey) {
		enableDXR = !enableDXR;
		if (enableDXR && mDXRInitialized) {
			strcpy_s(gActionMessage, "DirectX Raytracing Enabled");
		} else if (!mDXRInitialized) {
			enableDXR = false;
			strcpy_s(gActionMessage, "DXR Not Supported on this GPU");
		} else {
			strcpy_s(gActionMessage, "DirectX Raytracing Disabled");
		}
		UpdateScrollList(0, 255, 255);
	}
	if (GetAsyncKeyState('R')) {
		enableDXRKey = true;
	} else {
		enableDXRKey = false;
	}
}

void DungeonStompApp::UpdateCamera(const GameTimer &gt) {
	float dt = gt.DeltaTime();

	// Decay landing dip
	mLandingDip -= 150.0f * dt;
	if (mLandingDip < 0.0f)
		mLandingDip = 0.0f;

	// Disable for now
	mLandingDip = 0.0f;

	// Idle sway
	float swayY = 0.0f;
	if (playercurrentmove == 0 && enableCameraBob) {
		mIdleSwayTime += dt;
		swayY = sinf(mIdleSwayTime * 1.25f) * 1.2f; // Subtle breathing
	} else {
		mIdleSwayTime = 0.0f;
	}

	// Turn leaning
	float deltaAngy = angy - mLastAngy;
	if (deltaAngy > 180.0f)
		deltaAngy -= 360.0f;
	if (deltaAngy < -180.0f)
		deltaAngy += 360.0f;
	mLastAngy = angy;

	float turnTarget = -deltaAngy * 0.4f; // Lean into turns
	mTurnRoll += (turnTarget - mTurnRoll) * MathHelper::Clamp(dt * 8.0f, 0.0f, 1.0f);

	float adjust = 50.0f;
	float bx = 0.0f;
	float by = 0.0f;

	bx = bobX.getY();
	by = bobY.getY();

	if (player_list[trueplayernum].bIsPlayerAlive == FALSE) {
		// Dead on floor
		adjust = 0.0f;
	}

	mEyePos.x = m_vEyePt.x;
	mEyePos.y = m_vEyePt.y + adjust;
	mEyePos.z = m_vEyePt.z;

	player_list[trueplayernum].x = m_vEyePt.x;
	player_list[trueplayernum].y = m_vEyePt.y + adjust;
	player_list[trueplayernum].z = m_vEyePt.z;

	XMVECTOR pos, target;

	XMFLOAT3 newspot;
	XMFLOAT3 newspot2;

	if (enableCameraBob) {
		float step_left_angy = 0;
		float r = 15.0f;

		step_left_angy = angy - 90;

		if (step_left_angy < 0)
			step_left_angy += 360;

		if (step_left_angy >= 360)
			step_left_angy = step_left_angy - 360;

		r = bx;

		if (playercurrentmove == 1 || playercurrentmove == 4) {
			// Player is moving, ensure bobbing is active
			if (!bobX.getIsBobbing())
				bobX.SinWave(bobX.getSpeed(), bobX.getAmplitude(), bobX.getFrequency());
			if (!bobY.getIsBobbing())
				bobY.SinWave(bobY.getSpeed(), bobY.getAmplitude(), bobY.getFrequency());
		} else if (playercurrentmove == 0) {
			// Player is not moving, stop bobbing
			bobX.stopBobbing();
			bobY.stopBobbing();
		}

		// bx and by will now smoothly interpolate to 0 when bobbing stops,
		// due to the changes in CameraBob::update()
		r = bx; // bx is bobX.getY() which is updated with damping

		newspot.x = player_list[trueplayernum].x + r * sinf(step_left_angy * k);
		newspot.y = player_list[trueplayernum].y + by - mLandingDip + swayY;
		newspot.z = player_list[trueplayernum].z + r * cosf(step_left_angy * k);

		float cameradist = 50.0f;

		float newangle = 0;
		newangle = fixangle(look_up_ang, 90);

		newspot2.x = newspot.x + cameradist * sinf(newangle * k) * sinf(angy * k);
		newspot2.y = newspot.y + cameradist * cosf(newangle * k);
		newspot2.z = newspot.z + cameradist * sinf(newangle * k) * cosf(angy * k);

		mEyePos = newspot;
		GunTruesave = newspot;

		// Build the view matrix.

		pos = XMVectorSet(newspot.x, newspot.y, newspot.z, 1.0f);
		target = XMVectorSet(newspot2.x, newspot2.y, newspot2.z, 1.0f);
	} else {
		// Build the view matrix.
		pos = XMVectorSet(mEyePos.x, mEyePos.y - mLandingDip + swayY, mEyePos.z, 1.0f);
		target = XMVectorSet(m_vLookatPt.x, m_vLookatPt.y + adjust, m_vLookatPt.z, 1.0f);

		GunTruesave = mEyePos;
	}

	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	// Check for collision and nan errors
	XMVECTOR EyeDirection = XMVectorSubtract(pos, target);
	// assert(!XMVector3Equal(EyeDirection, XMVectorZero()));
	if (XMVector3Equal(EyeDirection, XMVectorZero())) {
		return;
	}

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);

	float totalRoll = look_roll_ang + mTurnRoll;
	if (totalRoll != 0.0f) {
		float rollRad = totalRoll * (3.14159265f / 180.0f);
		// Multiply rolling on the view-space Z axis
		view = XMMatrixMultiply(view, XMMatrixRotationZ(-rollRad));
	}

	XMStoreFloat4x4(&mView, view);

	mSceneBounds.Center = XMFLOAT3(mEyePos.x, mEyePos.y, mEyePos.z);
}

void DungeonStompApp::UpdateObjectCBs(const GameTimer &gt) {
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto &e : mAllRitems) {
		// Only update the cbuffer data if the constants have changed.
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0) {
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
			// objConstants.MaterialIndex = e->Mat->MatCBIndex;

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void DungeonStompApp::UpdateMaterialCBs(const GameTimer &gt) {
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for (auto &e : mMaterials) {
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material *mat = e.second.get();
		if (mat->NumFramesDirty > 0) {
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));
			matConstants.Metal = mat->Metal;

			// matConstants.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;
			// matConstants.NormalMapIndex = mat->NormalSrvHeapIndex;

			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void DungeonStompApp::UpdateShadowTransform(const GameTimer &gt, int light) {
	// Only the first "main" light casts a shadow.
	XMVECTOR lightDir = XMLoadFloat3(&mRotatedLightDirections[0]);
	XMVECTOR lightPos = -2.0f * mSceneBounds.Radius * lightDir;
	XMVECTOR targetPos = XMLoadFloat3(&mSceneBounds.Center);
	XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

	XMStoreFloat3(&mLightPosW, lightPos);

	// Transform bounding sphere to light space.
	XMFLOAT3 sphereCenterLS;
	XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

	// Ortho frustum in light space encloses scene.
	float l = sphereCenterLS.x - (mSceneBounds.Radius);
	float b = sphereCenterLS.y - mSceneBounds.Radius;
	float n = sphereCenterLS.z - mSceneBounds.Radius;
	float r = sphereCenterLS.x + mSceneBounds.Radius;
	float t = sphereCenterLS.y + mSceneBounds.Radius;
	float f = sphereCenterLS.z + mSceneBounds.Radius;

	// Adjust shadowmap depending on which direction you are facing.
	if ((angy >= 0.00 && angy <= 90.0f) || (angy >= 270.0f && angy <= 360.0f)) {
		l = sphereCenterLS.x - (mSceneBounds.Radius * 1.645f);
	} else {
		r = sphereCenterLS.x + (mSceneBounds.Radius * 1.645f);
	}

	mLightNearZ = n;
	mLightFarZ = f;
	XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
	    0.5f, 0.0f, 0.0f, 0.0f,
	    0.0f, -0.5f, 0.0f, 0.0f,
	    0.0f, 0.0f, 1.0f, 0.0f,
	    0.5f, 0.5f, 0.0f, 1.0f);

	XMMATRIX S = lightView * lightProj * T;
	XMStoreFloat4x4(&mLightView, lightView);
	XMStoreFloat4x4(&mLightProj, lightProj);
	XMStoreFloat4x4(&mShadowTransform, S);
}

void DungeonStompApp::UpdateMainPassCB(const GameTimer &gt) {

	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
	    0.5f, 0.0f, 0.0f, 0.0f,
	    0.0f, -0.5f, 0.0f, 0.0f,
	    0.0f, 0.0f, 1.0f, 0.0f,
	    0.5f, 0.5f, 0.0f, 1.0f);

	XMMATRIX viewProjTex = XMMatrixMultiply(viewProj, T);

	XMMATRIX shadowTransform = XMLoadFloat4x4(&mShadowTransform);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProjTex, XMMatrixTranspose(viewProjTex));
	XMStoreFloat4x4(&mMainPassCB.ShadowTransform, XMMatrixTranspose(shadowTransform));

	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();

	// mMainPassCB.AmbientLight = { 0.1f, 0.1f, 0.15f, 1.0f };
	mMainPassCB.AmbientLight = { 0.55f, 0.55f, 0.55f, 1.0f };
	// XMVECTOR lightDir = -MathHelper::SphericalToCartesian(1.0f, mSunTheta, mSunPhi);
	// XMStoreFloat3(&mMainPassCB.Lights[0].Direction, lightDir);
	// mMainPassCB.Lights[0].Strength = { 1.0f, 1.0f, 0.9f };

	for (int i = 0; i < MaxLights; i++) {
		mMainPassCB.Lights[i].Direction = LightContainer[i].Direction;
		mMainPassCB.Lights[i].Strength = LightContainer[i].Strength;
		mMainPassCB.Lights[i].Position = LightContainer[i].Position;
		mMainPassCB.Lights[i].FalloffEnd = LightContainer[i].FalloffEnd;
		mMainPassCB.Lights[i].FalloffStart = LightContainer[i].FalloffStart;
		mMainPassCB.Lights[i].SpotPower = LightContainer[i].SpotPower;
	}

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void DungeonStompApp::UpdateShadowPassCB(const GameTimer &gt) {
	XMMATRIX view = XMLoadFloat4x4(&mLightView);
	XMMATRIX proj = XMLoadFloat4x4(&mLightProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	UINT w = mShadowMap->Width();
	UINT h = mShadowMap->Height();

	XMStoreFloat4x4(&mShadowPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mShadowPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mShadowPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mShadowPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mShadowPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mShadowPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mShadowPassCB.EyePosW = mLightPosW;
	mShadowPassCB.RenderTargetSize = XMFLOAT2((float)w, (float)h);
	mShadowPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / w, 1.0f / h);
	mShadowPassCB.NearZ = mLightNearZ;
	mShadowPassCB.FarZ = mLightFarZ;

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(1, mShadowPassCB);
}

void DungeonStompApp::UpdateSsaoCB(const GameTimer &gt) {
	SsaoConstants ssaoCB;

	// XMMATRIX P = mCamera.GetProj();

	XMMATRIX P = XMLoadFloat4x4(&mProj);

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
	    0.5f, 0.0f, 0.0f, 0.0f,
	    0.0f, -0.5f, 0.0f, 0.0f,
	    0.0f, 0.0f, 1.0f, 0.0f,
	    0.5f, 0.5f, 0.0f, 1.0f);

	ssaoCB.Proj = mMainPassCB.Proj;
	ssaoCB.InvProj = mMainPassCB.InvProj;
	XMStoreFloat4x4(&ssaoCB.ProjTex, XMMatrixTranspose(P * T));

	mSsao->GetOffsetVectors(ssaoCB.OffsetVectors);

	auto blurWeights = mSsao->CalcGaussWeights(2.5f);
	ssaoCB.BlurWeights[0] = XMFLOAT4(&blurWeights[0]);
	ssaoCB.BlurWeights[1] = XMFLOAT4(&blurWeights[4]);
	ssaoCB.BlurWeights[2] = XMFLOAT4(&blurWeights[8]);

	ssaoCB.InvRenderTargetSize = XMFLOAT2(1.0f / mSsao->SsaoMapWidth(), 1.0f / mSsao->SsaoMapHeight());

	// Coordinates given in view space.
	ssaoCB.OcclusionRadius = 7.0f;
	ssaoCB.OcclusionFadeStart = 0.2f;
	ssaoCB.OcclusionFadeEnd = 0.4f;
	ssaoCB.SurfaceEpsilon = 0.20f;

	auto currSsaoCB = mCurrFrameResource->SsaoCB.get();
	currSsaoCB->CopyData(0, ssaoCB);
}

void DungeonStompApp::UpdateDungeon(const GameTimer &gt) {
	// Update the dungeon vertex buffer with the new solution.
	auto currDungeonVB = mCurrFrameResource->DungeonVB.get();
	Vertex v;

	for (int j = 0; j < cnt; j++) {
		v.Pos.x = src_v[j].x;
		v.Pos.y = src_v[j].y;
		v.Pos.z = src_v[j].z;

		v.Normal.x = src_v[j].nx;
		v.Normal.y = src_v[j].ny;
		v.Normal.z = src_v[j].nz;

		v.TexC.x = src_v[j].tu;
		v.TexC.y = src_v[j].tv;

		v.TangentU.x = src_v[j].nmx;
		v.TangentU.y = src_v[j].nmy;
		v.TangentU.z = src_v[j].nmz;

		v.CastShadow = src_v[j].CastShadow;

		currDungeonVB->CopyData(j, v);
	}

	// Set the dynamic VB of the dungeon renderitem to the current frame VB.
	mDungeonRitem->Geo->VertexBufferGPU = currDungeonVB->Resource();

	// Update DXR scene constants (AS building happens in Draw after command list reset)
	if (enableDXR && mDXRInitialized && cnt > 0) {
		// Select per-frame DXR buffers matching the current frame resource
		mDXRHelper->SetFrameIndex(mCurrFrameResourceIndex);

		// Build per-primitive texture indices from ObjectsToDraw
		// Each triangle (3 vertices) gets assigned its object's texture
		UINT totalTriangles = cnt / 3;
		std::vector<UINT> primitiveTextureIndices(totalTriangles, 999); // Initialize with invalid value to detect gaps
		std::vector<INT> primitiveNormalMapIndices(totalTriangles, -1); // -1 = no normal map

		static bool debugOnce = true;
		int trianglesSet = 0;

		for (int currentObject = 0; currentObject < number_of_polys_per_frame; currentObject++) {
			int srcStart = ObjectsToDraw[currentObject].srcstart;
			// Use verts_per_poly[] instead of ObjectsToDraw.vertsperpoly because
			// the latter is not updated after triangle strip/fan conversion
			int vertsPerPoly = verts_per_poly[currentObject];
			int vertIndex = ObjectsToDraw[currentObject].vert_index;

			// Get texture number through texture_list_buffer and TexMap
			int textureAliasNumber = texture_list_buffer[vertIndex];
			int textureNumber = TexMap[textureAliasNumber].texture;
			int normalMapAliasId = TexMap[textureAliasNumber].normalmaptextureid;
			int normalMapTexture = (normalMapAliasId >= 0) ? TexMap[normalMapAliasId].texture : -1;

			// Calculate triangle range for this object
			// vertsperpoly is the vertex count (3 per triangle for triangle list)
			int startTriangle = srcStart / 3;
			int numTriangles = vertsPerPoly / 3;

			// Debug: print first few objects
			if (debugOnce && currentObject < 10) {
				char buf[256];
				sprintf_s(buf, "DXR Tex: Obj %d: srcStart=%d, vertsPerPoly=%d, startTri=%d, numTri=%d, tex=%d\n",
				          currentObject, srcStart, vertsPerPoly, startTriangle, numTriangles, textureNumber);
				OutputDebugStringA(buf);
			}

			// Assign alias to all triangles in this object
			for (int t = 0; t < numTriangles && (startTriangle + t) < (int)totalTriangles; t++) {
				primitiveTextureIndices[startTriangle + t] = (UINT)textureAliasNumber;
				trianglesSet++;
			}
		}

		// Count unset triangles (still have value 999)
		if (debugOnce) {
			int unsetCount = 0;
			for (UINT i = 0; i < totalTriangles; i++) {
				if (primitiveTextureIndices[i] == 999)
					unsetCount++;
			}
			char buf[256];
			sprintf_s(buf, "DXR Tex: totalTriangles=%d, trianglesSet=%d, unsetCount=%d, polysPerFrame=%d\n",
			          totalTriangles, trianglesSet, unsetCount, number_of_polys_per_frame);
			OutputDebugStringA(buf);
			debugOnce = false;
		}

		// Replace 999 with 0 for unset triangles
		for (UINT i = 0; i < totalTriangles; i++) {
			if (primitiveTextureIndices[i] == 999)
				primitiveTextureIndices[i] = 0;
		}
		// Upload primitive alias indices to DXR (reusing texture indices buffer)
		mDXRHelper->UpdatePrimitiveTextureIndices(md3dDevice.Get(), primitiveTextureIndices.data(), totalTriangles);

		// Upload alias material data to DXR
		std::vector<DXRMaterialData> aliasData(number_of_tex_aliases);
		for (int i = 0; i < number_of_tex_aliases; i++) {
			aliasData[i].TextureIndex = (UINT)TexMap[i].texture;
			aliasData[i].NormalMapIndex = TexMap[i].normalmaptextureid;
			aliasData[i].Roughness = TexMap[i].material.roughness;
			aliasData[i].Metallic = TexMap[i].material.metallic;
		}
		mDXRHelper->UpdateAliasData(md3dDevice.Get(), aliasData.data(), (UINT)aliasData.size());

		// Update scene constants for DXR
		XMMATRIX view = XMLoadFloat4x4(&mView);
		XMMATRIX proj = XMLoadFloat4x4(&mProj);
		XMMATRIX viewProj = view * proj;
		XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewProj);
		XMFLOAT4X4 invViewProjF;
		XMStoreFloat4x4(&invViewProjF, XMMatrixTranspose(invViewProj));

		mDXRHelper->UpdateSceneConstants(
		    invViewProjF,
		    mEyePos,
		    mMainPassCB.AmbientLight,
		    LightContainer,
		    MaxLights,
		    gt.TotalTime(),
		    0.5f,  // Default roughness (deprecated, now per-alias)
		    0.0f); // Default metallic (deprecated, now per-alias)
	}
}
