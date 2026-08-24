#include "d3dtypes.h"
#include "LoadWorld.hpp"
#include "world.hpp"
#include "GlobalSettings.hpp"
#include <string.h>
#include "GameLogic.hpp"
#include "Missle.hpp"
#include "../Common/MathHelper.h"
#include "mikktspace.h"
#include <cstddef>
#include <functional>
#include <unordered_map>
#include <vector>
#include <algorithm>

int itemlistcount = 0;

void ConvertTraingleStrip(int fan_cnt);

void ConvertTraingleFan(int fan_cnt);
extern OBJECTLIST *oblist;
extern OBJECTDATA *obdata;
extern D3DVERTEX2 boundingbox[2000];
int cnt_f = 0;
float px[100], py[100], pz[100], pw[100];
float mx[100], my[100], mz[100], mw[100];
float cx[100], cy[100], cz[100], cw[100];
float tx[10000], ty[10000];

int sharedv[2000];
int track[60000];

void SmoothNormals(int start_cnt);
extern SWITCHMOD *switchmodify;
int countswitches = 0;

int *verts_per_poly;
int number_of_polys_per_frame;
int *faces_per_poly;
int *src_f;
D3DVERTEX2 temp_v[MAX_NUM_QUADS];
int tempvcounter = 0;
D3DVERTEX2 *src_v;
int drawthistri = 1;
extern float culldist;

D3DPRIMITIVETYPE *dp_commands;

BOOL *dp_command_index_mode;

#define USE_INDEXED_DP 0
#define USE_NON_INDEXED_DP 1

float k = (float)0.017453292;

struct CUSTOMVERTEX {
	FLOAT X, Y, Z;
	DWORD COLOR;
};

// LPDIRECT3DVERTEXBUFFER9 v_buffer = NULL;    // the pointer to the vertex buffer

float sin_table[361];
float cos_table[361];

int src_collide[MAX_NUM_QUADS];

// LPDIRECT3DVERTEXBUFFER9 g_pVB = NULL; // Buffer to hold vertices
// LPDIRECT3DVERTEXBUFFER9 g_pVBBoundingBox = NULL; // Buffer to hold vertices
// LPDIRECT3DVERTEXBUFFER9 g_pVBMonsterCaption = NULL; // Buffer to hold vertices

float playerx = 0;
float playery = 0;
float playerz = 0;
float rotatex = 0.0f;
float rotatey = 0.0f;

FLOAT fTimeKeysave = 0;

PLAYER *item_list;
PLAYER *player_list2;
PLAYER *player_list;

int *texture_list_buffer;
int g_ob_vert_count = 0;
extern TEXTUREMAPPING TexMap[MAX_NUM_TEXTURES];
void DrawModel();
int num_light_sources = 0;

extern float gametimerAnimation;

void ConvertQuad(int fan_cnt);

void CalculateTangentBinormal(D3DVERTEX2 &vertex1, D3DVERTEX2 &vertex2, D3DVERTEX2 &vertex3) {
	float vector1[3], vector2[3];
	float tuVector[2], tvVector[2];
	float den, length;

	// Calculate the two vectors for this face.
	vector1[0] = vertex2.x - vertex1.x;
	vector1[1] = vertex2.y - vertex1.y;
	vector1[2] = vertex2.z - vertex1.z;

	vector2[0] = vertex3.x - vertex1.x;
	vector2[1] = vertex3.y - vertex1.y;
	vector2[2] = vertex3.z - vertex1.z;

	// Calculate the tu and tv texture space vectors.
	tuVector[0] = vertex2.tu - vertex1.tu;
	tvVector[0] = vertex2.tv - vertex1.tv;

	tuVector[1] = vertex3.tu - vertex1.tu;
	tvVector[1] = vertex3.tv - vertex1.tv;

	// Calculate the denominator of the tangent/binormal equation.
	float result = (tuVector[0] * tvVector[1] - tuVector[1] * tvVector[0]);

	if (result == 0) {
		vertex1.nmx = vertex1.nmy = vertex1.nmz = 0;
		vertex2.nmx = vertex2.nmy = vertex2.nmz = 0;
		vertex3.nmx = vertex3.nmy = vertex3.nmz = 0;
		return;
	}

	den = 1.0f / result;

	// Calculate the cross products and multiply by the coefficient to get the tangent and binormal.
	D3DVERTEX2 tangent, binormal;
	tangent.x = (tvVector[1] * vector1[0] - tvVector[0] * vector2[0]) * den;
	tangent.y = (tvVector[1] * vector1[1] - tvVector[0] * vector2[1]) * den;
	tangent.z = (tvVector[1] * vector1[2] - tvVector[0] * vector2[2]) * den;

	binormal.x = (tuVector[0] * vector2[0] - tuVector[1] * vector1[0]) * den;
	binormal.y = (tuVector[0] * vector2[1] - tuVector[1] * vector1[1]) * den;
	binormal.z = (tuVector[0] * vector2[2] - tuVector[1] * vector1[2]) * den;

	// Normalize the tangent
	length = sqrtf(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z);
	tangent.x /= length;
	tangent.y /= length;
	tangent.z /= length;

	// Normalize the binormal
	length = sqrtf(binormal.x * binormal.x + binormal.y * binormal.y + binormal.z * binormal.z);
	binormal.x /= length;
	binormal.y /= length;
	binormal.z /= length;

	vertex1.nmx = vertex2.nmx = vertex3.nmx = tangent.x;
	vertex1.nmy = vertex2.nmy = vertex3.nmy = tangent.y;
	vertex1.nmz = vertex2.nmz = vertex3.nmz = tangent.z;
}

void CalculateVertNormalAndTangent(VERT &vertex1, VERT &vertex2, VERT &vertex3, const VERT &tex1, const VERT &tex2, const VERT &tex3) {
	float vector1[3], vector2[3];
	float tuVector[2], tvVector[2];

	// Calculate the two vectors for this face.
	vector1[0] = vertex2.x - vertex1.x;
	vector1[1] = vertex2.y - vertex1.y;
	vector1[2] = vertex2.z - vertex1.z;

	vector2[0] = vertex3.x - vertex1.x;
	vector2[1] = vertex3.y - vertex1.y;
	vector2[2] = vertex3.z - vertex1.z;

	// Calculate surface normal (vector1 x vector2)
	float nx = vector1[1] * vector2[2] - vector1[2] * vector2[1];
	float ny = vector1[2] * vector2[0] - vector1[0] * vector2[2];
	float nz = vector1[0] * vector2[1] - vector1[1] * vector2[0];

	float length = sqrtf(nx * nx + ny * ny + nz * nz);
	if (length > 0.00001f) {
		nx /= length;
		ny /= length;
		nz /= length;
	} else {
		// Default fallback normal for degenerate triangles
		nx = 0.0f;
		ny = 1.0f;
		nz = 0.0f;
	}

	vertex1.nx = vertex2.nx = vertex3.nx = nx;
	vertex1.ny = vertex2.ny = vertex3.ny = ny;
	vertex1.nz = vertex2.nz = vertex3.nz = nz;

	// Calculate texture space vectors
	tuVector[0] = tex2.x - tex1.x;
	tvVector[0] = -(tex2.y - tex1.y);

	tuVector[1] = tex3.x - tex1.x;
	tvVector[1] = -(tex3.y - tex1.y);

	float result = (tuVector[0] * tvVector[1] - tuVector[1] * tvVector[0]);

	if (fabsf(result) < 1e-12f) {
		// Pick ANY vector not parallel to the normal
		float ax = (fabsf(nx) > 0.9f) ? 0.0f : 1.0f;
		float ay = 0.0f;
		float az = (fabsf(nz) > 0.9f) ? 0.0f : 1.0f;

		// Cross to get a tangent perpendicular to the normal
		float tanX = ay * nz - az * ny;
		float tanY = az * nx - ax * nz;
		float tanZ = ax * ny - ay * nx;

		float len = sqrtf(tanX * tanX + tanY * tanY + tanZ * tanZ);
		if (len > 0.00001f) {
			tanX /= len;
			tanY /= len;
			tanZ /= len;
		} else {
			tanX = 1.0f;
			tanY = 0.0f;
			tanZ = 0.0f;
		}

		vertex1.nmx = vertex2.nmx = vertex3.nmx = tanX;
		vertex1.nmy = vertex2.nmy = vertex3.nmy = tanY;
		vertex1.nmz = vertex2.nmz = vertex3.nmz = tanZ;
		return;
	}

	float den = 1.0f / result;

	float tanX = (tvVector[1] * vector1[0] - tvVector[0] * vector2[0]) * den;
	float tanY = (tvVector[1] * vector1[1] - tvVector[0] * vector2[1]) * den;
	float tanZ = (tvVector[1] * vector1[2] - tvVector[0] * vector2[2]) * den;

	// Gram-Schmidt orthogonalization: ensure Tangent is orthogonal to Normal
	float dot = tanX * nx + tanY * ny + tanZ * nz;
	tanX -= nx * dot;
	tanY -= ny * dot;
	tanZ -= nz * dot;

	length = sqrtf(tanX * tanX + tanY * tanY + tanZ * tanZ);
	if (length > 0.00001f) {
		tanX /= length;
		tanY /= length;
		tanZ /= length;
	} else {
		// Pick a vector not parallel to normal
		float ax = (fabsf(nx) > 0.9f) ? 0.0f : 1.0f;
		float ay = 0.0f;
		float az = (fabsf(nz) > 0.9f) ? 0.0f : 1.0f;

		tanX = ay * nz - az * ny;
		tanY = az * nx - ax * nz;
		tanZ = ax * ny - ay * nx;

		float len = sqrtf(tanX * tanX + tanY * tanY + tanZ * tanZ);
		if (len > 0.00001f) {
			tanX /= len;
			tanY /= len;
			tanZ /= len;
		} else {
			tanX = 1.0f;
			tanY = 0.0f;
			tanZ = 0.0f;
		}
	}

	vertex1.nmx = vertex2.nmx = vertex3.nmx = tanX;
	vertex1.nmy = vertex2.nmy = vertex3.nmy = tanY;
	vertex1.nmz = vertex2.nmz = vertex3.nmz = tanZ;
}

void SmoothVertArrayNoHash(VERT *verts, int num_verts, float smooth_threshold) {
	const float epsilon = 0.0001f;

	std::vector<uint8_t> tracked(num_verts, 0);

	for (int i = 0; i < num_verts; i++) {
		if (tracked[i] == 0) {
			float x = verts[i].x;
			float y = verts[i].y;
			float z = verts[i].z;

			XMVECTOR ni = XMVectorSet(verts[i].nx, verts[i].ny, verts[i].nz, 0.0f);

			std::vector<int> shared;
			shared.push_back(i);

			for (int j = i + 1; j < num_verts; j++) {
				if (tracked[j] == 0) {
					if (fabsf(verts[j].x - x) < epsilon &&
					    fabsf(verts[j].y - y) < epsilon &&
					    fabsf(verts[j].z - z) < epsilon) {

						XMVECTOR nj = XMVectorSet(verts[j].nx, verts[j].ny, verts[j].nz, 0.0f);
						float dot = XMVectorGetX(XMVector3Dot(ni, nj));

						if (dot > smooth_threshold) {
							shared.push_back(j);
						}
					}
				}
			}

			if (shared.size() > 1) {
				XMVECTOR sumN = XMVectorZero();
				XMVECTOR sumT = XMVectorZero();

				for (int idx : shared) {
					sumN = XMVectorAdd(sumN, XMVectorSet(verts[idx].nx, verts[idx].ny, verts[idx].nz, 0.0f));
					sumT = XMVectorAdd(sumT, XMVectorSet(verts[idx].nmx, verts[idx].nmy, verts[idx].nmz, 0.0f));
				}

				XMVECTOR avgN = XMVector3Normalize(sumN);
				XMVECTOR avgT = XMVector3Normalize(XMVectorSubtract(sumT, XMVectorMultiply(avgN, XMVector3Dot(avgN, sumT))));

				XMFLOAT3 fN, fT;
				XMStoreFloat3(&fN, avgN);
				XMStoreFloat3(&fT, avgT);

				for (int idx : shared) {
					verts[idx].nx = fN.x;
					verts[idx].ny = fN.y;
					verts[idx].nz = fN.z;
					verts[idx].nmx = fT.x;
					verts[idx].nmy = fT.y;
					verts[idx].nmz = fT.z;
					tracked[idx] = 1;
				}
			} else {
				tracked[i] = 1;
			}
		}
	}
}

struct ThreeDSMikkUserData {
	int pmodel_id;
	VERT *frame_verts;
	int total_faces;
	const int *corner_to_global_v;
};

static int ThreeDSMikk_GetNumFaces(const SMikkTSpaceContext *pContext) {
	auto *userData = static_cast<ThreeDSMikkUserData *>(pContext->m_pUserData);
	return userData->total_faces;
}

static int ThreeDSMikk_GetNumVerticesOfFace(const SMikkTSpaceContext *pContext, const int iFace) {
	return 3;
}

static void ThreeDSMikk_GetPosition(const SMikkTSpaceContext *pContext, float fvPosOut[], const int iFace, const int iVert) {
	auto *userData = static_cast<ThreeDSMikkUserData *>(pContext->m_pUserData);
	int corner_idx = iFace * 3 + iVert;
	int global_v = userData->corner_to_global_v[corner_idx];
	const VERT &v = userData->frame_verts[global_v];
	fvPosOut[0] = v.x;
	fvPosOut[1] = v.z;
	fvPosOut[2] = v.y;
}

static void ThreeDSMikk_GetNormal(const SMikkTSpaceContext *pContext, float fvNormOut[], const int iFace, const int iVert) {
	auto *userData = static_cast<ThreeDSMikkUserData *>(pContext->m_pUserData);
	int corner_idx = iFace * 3 + iVert;
	int global_v = userData->corner_to_global_v[corner_idx];
	const VERT &v = userData->frame_verts[global_v];
	fvNormOut[0] = v.nx;
	fvNormOut[1] = v.ny;
	fvNormOut[2] = v.nz;
}

static void ThreeDSMikk_GetTexCoord(const SMikkTSpaceContext *pContext, float fvTexcOut[], const int iFace, const int iVert) {
	auto *userData = static_cast<ThreeDSMikkUserData *>(pContext->m_pUserData);
	int corner_idx = iFace * 3 + iVert;
	const VERT &t = pmdata[userData->pmodel_id].t[corner_idx];
	fvTexcOut[0] = t.x * pmdata[userData->pmodel_id].skx;
	fvTexcOut[1] = 1.0f - (t.y * pmdata[userData->pmodel_id].sky);
}

static void ThreeDSMikk_SetTSpaceBasic(const SMikkTSpaceContext *pContext, const float fvTangent[], const float fSign, const int iFace, const int iVert) {
	auto *userData = static_cast<ThreeDSMikkUserData *>(pContext->m_pUserData);
	int corner_idx = iFace * 3 + iVert;
	int global_v = userData->corner_to_global_v[corner_idx];
	userData->frame_verts[global_v].nmx = fvTangent[0];
	userData->frame_verts[global_v].nmy = fvTangent[1];
	userData->frame_verts[global_v].nmz = fvTangent[2];
}

void Compute3DSModelNormals(int pmodel_id) {
	if (pmodel_id < 0)
		return;

	int num_frames = pmdata[pmodel_id].num_frames;
	if (num_frames <= 0)
		return;

	int total_num_verts = pmdata[pmodel_id].num_verts;
	int num_poly = pmdata[pmodel_id].num_polys_per_frame;

	if (total_num_verts <= 0 || num_poly <= 0)
		return;

	for (int frame_num = 0; frame_num < num_frames; frame_num++) {
		VERT *frame_verts = pmdata[pmodel_id].w[frame_num];
		if (!frame_verts)
			continue;

		// 1. Reset vertex normals and tangents to zero
		for (int v = 0; v < total_num_verts; v++) {
			frame_verts[v].nx = frame_verts[v].ny = frame_verts[v].nz = 0.0f;
			frame_verts[v].nmx = frame_verts[v].nmy = frame_verts[v].nmz = 0.0f;
		}

		// 2. Compute face normals and tangents per object and assign directly
		std::vector<int> corner_to_global_v(pmdata[pmodel_id].num_faces * 3, 0);
		int v_start = 0;
		int face_i_count = 0;

		for (int i = 0; i < num_poly; i++) {
			const int num_verts_per_poly = pmdata[pmodel_id].num_verts_per_object[i];
			const int num_faces_per_poly = pmdata[pmodel_id].num_faces_per_object[i];

			for (int j = 0; j < num_faces_per_poly; j++) {
				int idx0 = pmdata[pmodel_id].f[face_i_count + 0];
				int idx1 = pmdata[pmodel_id].f[face_i_count + 1];
				int idx2 = pmdata[pmodel_id].f[face_i_count + 2];

				int g0 = v_start + idx0;
				int g1 = v_start + idx1;
				int g2 = v_start + idx2;

				corner_to_global_v[face_i_count + 0] = g0;
				corner_to_global_v[face_i_count + 1] = g1;
				corner_to_global_v[face_i_count + 2] = g2;

				if (g0 >= 0 && g0 < total_num_verts &&
				    g1 >= 0 && g1 < total_num_verts &&
				    g2 >= 0 && g2 < total_num_verts) {

					VERT tmp0, tmp1, tmp2;

					tmp0.x = frame_verts[g0].x;
					tmp0.y = frame_verts[g0].z;
					tmp0.z = frame_verts[g0].y;

					tmp1.x = frame_verts[g1].x;
					tmp1.y = frame_verts[g1].z;
					tmp1.z = frame_verts[g1].y;

					tmp2.x = frame_verts[g2].x;
					tmp2.y = frame_verts[g2].z;
					tmp2.z = frame_verts[g2].y;

					CalculateVertNormalAndTangent(
					    tmp0, tmp1, tmp2,
					    pmdata[pmodel_id].t[face_i_count + 0],
					    pmdata[pmodel_id].t[face_i_count + 1],
					    pmdata[pmodel_id].t[face_i_count + 2]);

					// 3DS winding order correction: negate face normal and tangent so they point outward
					// tmp0.nx = -tmp0.nx;
					// tmp0.ny = -tmp0.ny;
					// tmp0.nz = -tmp0.nz;
					// tmp0.nmx = -tmp0.nmx;
					// tmp0.nmy = -tmp0.nmy;
					// tmp0.nmz = -tmp0.nmz;

					frame_verts[g0].nx += tmp0.nx;
					frame_verts[g0].ny += tmp0.ny;
					frame_verts[g0].nz += tmp0.nz;
					frame_verts[g1].nx += tmp0.nx;
					frame_verts[g1].ny += tmp0.ny;
					frame_verts[g1].nz += tmp0.nz;
					frame_verts[g2].nx += tmp0.nx;
					frame_verts[g2].ny += tmp0.ny;
					frame_verts[g2].nz += tmp0.nz;

					frame_verts[g0].nmx += tmp0.nmx;
					frame_verts[g0].nmy += tmp0.nmy;
					frame_verts[g0].nmz += tmp0.nmz;
					frame_verts[g1].nmx += tmp0.nmx;
					frame_verts[g1].nmy += tmp0.nmy;
					frame_verts[g1].nmz += tmp0.nmz;
					frame_verts[g2].nmx += tmp0.nmx;
					frame_verts[g2].nmy += tmp0.nmy;
					frame_verts[g2].nmz += tmp0.nmz;
				}

				face_i_count += 3;
			}

			v_start += num_verts_per_poly;
		}

		// Normalize accumulated vertex normals before MikkTSpace
		for (int v = 0; v < total_num_verts; v++) {
			float len = sqrtf(frame_verts[v].nx * frame_verts[v].nx +
			                  frame_verts[v].ny * frame_verts[v].ny +
			                  frame_verts[v].nz * frame_verts[v].nz);
			if (len > 1e-6f) {
				frame_verts[v].nx /= len;
				frame_verts[v].ny /= len;
				frame_verts[v].nz /= len;
			} else {
				frame_verts[v].nx = 0.0f;
				frame_verts[v].ny = 1.0f;
				frame_verts[v].nz = 0.0f;
			}
		}

		// 3. Generate tangents using MikkTSpace
		SMikkTSpaceInterface mikkInterface = {};
		mikkInterface.m_getNumFaces = ThreeDSMikk_GetNumFaces;
		mikkInterface.m_getNumVerticesOfFace = ThreeDSMikk_GetNumVerticesOfFace;
		mikkInterface.m_getPosition = ThreeDSMikk_GetPosition;
		mikkInterface.m_getNormal = ThreeDSMikk_GetNormal;
		mikkInterface.m_getTexCoord = ThreeDSMikk_GetTexCoord;
		mikkInterface.m_setTSpaceBasic = ThreeDSMikk_SetTSpaceBasic;

		ThreeDSMikkUserData userData;
		userData.pmodel_id = pmodel_id;
		userData.frame_verts = frame_verts;
		userData.total_faces = face_i_count / 3;
		userData.corner_to_global_v = corner_to_global_v.data();

		SMikkTSpaceContext mikkContext = {};
		mikkContext.m_pInterface = &mikkInterface;
		mikkContext.m_pUserData = &userData;

		genTangSpaceDefault(&mikkContext);

		// Post-check tangents and normals to ensure valid non-zero unit vectors
		for (int v = 0; v < total_num_verts; v++) {
			float nlen = sqrtf(frame_verts[v].nx * frame_verts[v].nx +
			                   frame_verts[v].ny * frame_verts[v].ny +
			                   frame_verts[v].nz * frame_verts[v].nz);
			if (nlen < 1e-5f) {
				frame_verts[v].nx = 0.0f;
				frame_verts[v].ny = 1.0f;
				frame_verts[v].nz = 0.0f;
			}
			float tlen = sqrtf(frame_verts[v].nmx * frame_verts[v].nmx +
			                   frame_verts[v].nmy * frame_verts[v].nmy +
			                   frame_verts[v].nmz * frame_verts[v].nmz);
			if (tlen < 1e-5f) {
				float ax = (fabsf(frame_verts[v].nx) > 0.9f) ? 0.0f : 1.0f;
				float ay = 0.0f;
				float az = (fabsf(frame_verts[v].nz) > 0.9f) ? 0.0f : 1.0f;

				float tanX = ay * frame_verts[v].nz - az * frame_verts[v].ny;
				float tanY = az * frame_verts[v].nx - ax * frame_verts[v].nz;
				float tanZ = ax * frame_verts[v].ny - ay * frame_verts[v].nx;

				float len = sqrtf(tanX * tanX + tanY * tanY + tanZ * tanZ);
				if (len > 1e-5f) {
					frame_verts[v].nmx = tanX / len;
					frame_verts[v].nmy = tanY / len;
					frame_verts[v].nmz = tanZ / len;
				} else {
					frame_verts[v].nmx = 1.0f;
					frame_verts[v].nmy = 0.0f;
					frame_verts[v].nmz = 0.0f;
				}
			}
		}
	}
}

struct MD2MikkUserData {
	int pmodel_id;
	VERT *frame_verts;
	int total_triangles;
};

static int MD2Mikk_GetNumFaces(const SMikkTSpaceContext *pContext) {
	auto *userData = static_cast<MD2MikkUserData *>(pContext->m_pUserData);
	return userData->total_triangles;
}

static int MD2Mikk_GetNumVerticesOfFace(const SMikkTSpaceContext *pContext, const int iFace) {
	return 3;
}

static void MD2Mikk_GetPosition(const SMikkTSpaceContext *pContext, float fvPosOut[], const int iFace, const int iVert) {
	auto *userData = static_cast<MD2MikkUserData *>(pContext->m_pUserData);
	int corner_idx = iFace * 3 + iVert;
	int v_idx = pmdata[userData->pmodel_id].f[corner_idx];
	const VERT &v = userData->frame_verts[v_idx];
	fvPosOut[0] = v.x;
	fvPosOut[1] = v.y;
	fvPosOut[2] = v.z;
}

static void MD2Mikk_GetNormal(const SMikkTSpaceContext *pContext, float fvNormOut[], const int iFace, const int iVert) {
	auto *userData = static_cast<MD2MikkUserData *>(pContext->m_pUserData);
	int corner_idx = iFace * 3 + iVert;
	int v_idx = pmdata[userData->pmodel_id].f[corner_idx];
	const VERT &v = userData->frame_verts[v_idx];
	fvNormOut[0] = v.nx;
	fvNormOut[1] = v.ny;
	fvNormOut[2] = v.nz;
}

static void MD2Mikk_GetTexCoord(const SMikkTSpaceContext *pContext, float fvTexcOut[], const int iFace, const int iVert) {
	auto *userData = static_cast<MD2MikkUserData *>(pContext->m_pUserData);
	int corner_idx = iFace * 3 + iVert;
	const VERT &t = pmdata[userData->pmodel_id].t[corner_idx];
	fvTexcOut[0] = t.x * pmdata[userData->pmodel_id].skx;
	fvTexcOut[1] = t.y * pmdata[userData->pmodel_id].sky;
}

static void MD2Mikk_SetTSpaceBasic(const SMikkTSpaceContext *pContext, const float fvTangent[], const float fSign, const int iFace, const int iVert) {
	auto *userData = static_cast<MD2MikkUserData *>(pContext->m_pUserData);
	int corner_idx = iFace * 3 + iVert;
	int v_idx = pmdata[userData->pmodel_id].f[corner_idx];
	userData->frame_verts[v_idx].nmx = fvTangent[0];
	userData->frame_verts[v_idx].nmy = fvTangent[1];
	userData->frame_verts[v_idx].nmz = fvTangent[2];
}

void ComputeMD2ModelNormals(int pmodel_id) {
	if (pmodel_id < 0)
		return;

	int num_frames = pmdata[pmodel_id].num_frames;
	if (num_frames <= 0)
		return;

	int num_poly = pmdata[pmodel_id].num_polys_per_frame;
	int num_indices = pmdata[pmodel_id].num_verts; // total face vertex count
	if (num_poly <= 0 || num_indices < 3)
		return;

	int max_v_idx = 0;
	for (int i = 0; i < num_indices; i++) {
		if (pmdata[pmodel_id].f[i] > max_v_idx) {
			max_v_idx = pmdata[pmodel_id].f[i];
		}
	}
	int num_verts = max_v_idx + 1;

	for (int frame_num = 0; frame_num < num_frames; frame_num++) {
		VERT *frame_verts = pmdata[pmodel_id].w[frame_num];
		if (!frame_verts)
			continue;

		// 1. Reset vertex normals and tangents to zero
		for (int v = 0; v < num_verts; v++) {
			frame_verts[v].nx = frame_verts[v].ny = frame_verts[v].nz = 0.0f;
			frame_verts[v].nmx = frame_verts[v].nmy = frame_verts[v].nmz = 0.0f;
		}

		// 2. Calculate face/vertex normals
		int i_count = 0;
		for (int i = 0; i < num_poly; i++) {
			int num_verts_per_poly = pmdata[pmodel_id].num_vert[i];

			for (int j = 0; j + 2 < num_verts_per_poly; j += 3) {
				int idx0 = pmdata[pmodel_id].f[i_count + j + 0];
				int idx1 = pmdata[pmodel_id].f[i_count + j + 1];
				int idx2 = pmdata[pmodel_id].f[i_count + j + 2];

				if (idx0 >= 0 && idx0 < num_verts &&
				    idx1 >= 0 && idx1 < num_verts &&
				    idx2 >= 0 && idx2 < num_verts) {

					VERT tmp0, tmp1, tmp2;

					tmp0.x = frame_verts[idx0].x;
					tmp0.y = frame_verts[idx0].z;
					tmp0.z = frame_verts[idx0].y;

					tmp1.x = frame_verts[idx1].x;
					tmp1.y = frame_verts[idx1].z;
					tmp1.z = frame_verts[idx1].y;

					tmp2.x = frame_verts[idx2].x;
					tmp2.y = frame_verts[idx2].z;
					tmp2.z = frame_verts[idx2].y;

					CalculateVertNormalAndTangent(
					    tmp0, tmp1, tmp2,
					    pmdata[pmodel_id].t[i_count + j + 0],
					    pmdata[pmodel_id].t[i_count + j + 1],
					    pmdata[pmodel_id].t[i_count + j + 2]);

					frame_verts[idx0].nx += tmp0.nx;
					frame_verts[idx0].ny += tmp0.ny;
					frame_verts[idx0].nz += tmp0.nz;
					frame_verts[idx1].nx += tmp0.nx;
					frame_verts[idx1].ny += tmp0.ny;
					frame_verts[idx1].nz += tmp0.nz;
					frame_verts[idx2].nx += tmp0.nx;
					frame_verts[idx2].ny += tmp0.ny;
					frame_verts[idx2].nz += tmp0.nz;

					frame_verts[idx0].nmx += tmp0.nmx;
					frame_verts[idx0].nmy += tmp0.nmy;
					frame_verts[idx0].nmz += tmp0.nmz;
					frame_verts[idx1].nmx += tmp0.nmx;
					frame_verts[idx1].nmy += tmp0.nmy;
					frame_verts[idx1].nmz += tmp0.nmz;
					frame_verts[idx2].nmx += tmp0.nmx;
					frame_verts[idx2].nmy += tmp0.nmy;
					frame_verts[idx2].nmz += tmp0.nmz;
				}
			}

			i_count += num_verts_per_poly;
		}

		// 2.5 Normalize accumulated vertex normals
		for (int v = 0; v < num_verts; v++) {
			float nx = frame_verts[v].nx;
			float ny = frame_verts[v].ny;
			float nz = frame_verts[v].nz;

			float len = sqrtf(nx * nx + ny * ny + nz * nz);
			if (len > 0.00001f) {
				frame_verts[v].nx = nx / len;
				frame_verts[v].ny = ny / len;
				frame_verts[v].nz = nz / len;
			} else {
				frame_verts[v].nx = 0.0f;
				frame_verts[v].ny = 1.0f;
				frame_verts[v].nz = 0.0f;
			}
		}

		// 3. Generate tangents using MikkTSpace
		SMikkTSpaceInterface mikkInterface = {};
		mikkInterface.m_getNumFaces = MD2Mikk_GetNumFaces;
		mikkInterface.m_getNumVerticesOfFace = MD2Mikk_GetNumVerticesOfFace;
		mikkInterface.m_getPosition = MD2Mikk_GetPosition;
		mikkInterface.m_getNormal = MD2Mikk_GetNormal;
		mikkInterface.m_getTexCoord = MD2Mikk_GetTexCoord;
		mikkInterface.m_setTSpaceBasic = MD2Mikk_SetTSpaceBasic;

		MD2MikkUserData userData;
		userData.pmodel_id = pmodel_id;
		userData.frame_verts = frame_verts;
		userData.total_triangles = num_indices / 3;

		SMikkTSpaceContext mikkContext = {};
		mikkContext.m_pInterface = &mikkInterface;
		mikkContext.m_pUserData = &userData;

		genTangSpaceDefault(&mikkContext);

		// Post-check tangents and normals to ensure valid non-zero unit vectors
		for (int v = 0; v < num_verts; v++) {
			float nlen = sqrtf(frame_verts[v].nx * frame_verts[v].nx +
			                   frame_verts[v].ny * frame_verts[v].ny +
			                   frame_verts[v].nz * frame_verts[v].nz);
			if (nlen < 1e-5f) {
				frame_verts[v].nx = 0.0f;
				frame_verts[v].ny = 1.0f;
				frame_verts[v].nz = 0.0f;
			}
			float tlen = sqrtf(frame_verts[v].nmx * frame_verts[v].nmx +
			                   frame_verts[v].nmy * frame_verts[v].nmy +
			                   frame_verts[v].nmz * frame_verts[v].nmz);
			if (tlen < 1e-5f) {
				float ax = (fabsf(frame_verts[v].nx) > 0.9f) ? 0.0f : 1.0f;
				float ay = 0.0f;
				float az = (fabsf(frame_verts[v].nz) > 0.9f) ? 0.0f : 1.0f;

				float tanX = ay * frame_verts[v].nz - az * frame_verts[v].ny;
				float tanY = az * frame_verts[v].nx - ax * frame_verts[v].nz;
				float tanZ = ax * frame_verts[v].ny - ay * frame_verts[v].nx;

				float len = sqrtf(tanX * tanX + tanY * tanY + tanZ * tanZ);
				if (len > 1e-5f) {
					frame_verts[v].nmx = tanX / len;
					frame_verts[v].nmy = tanY / len;
					frame_verts[v].nmz = tanZ / len;
				} else {
					frame_verts[v].nmx = 1.0f;
					frame_verts[v].nmy = 0.0f;
					frame_verts[v].nmz = 0.0f;
				}
			}
		}
	}
}

struct ObDataMikkUserData {
	int obj_idx;
	int total_triangles;
};

static int ObDataMikk_GetNumFaces(const SMikkTSpaceContext *pContext) {
	auto *userData = static_cast<ObDataMikkUserData *>(pContext->m_pUserData);
	return userData->total_triangles;
}

static int ObDataMikk_GetNumVerticesOfFace(const SMikkTSpaceContext *pContext, const int iFace) {
	return 3;
}

static void ObDataMikk_GetPosition(const SMikkTSpaceContext *pContext, float fvPosOut[], const int iFace, const int iVert) {
	auto *userData = static_cast<ObDataMikkUserData *>(pContext->m_pUserData);
	int v_idx = iFace * 3 + iVert;
	const VERT &v = obdata[userData->obj_idx].v[v_idx];
	fvPosOut[0] = v.x;
	fvPosOut[1] = v.y;
	fvPosOut[2] = v.z;
}

static void ObDataMikk_GetNormal(const SMikkTSpaceContext *pContext, float fvNormOut[], const int iFace, const int iVert) {
	auto *userData = static_cast<ObDataMikkUserData *>(pContext->m_pUserData);
	int v_idx = iFace * 3 + iVert;
	const VERT &v = obdata[userData->obj_idx].v[v_idx];
	fvNormOut[0] = v.nx;
	fvNormOut[1] = v.ny;
	fvNormOut[2] = v.nz;
}

static void ObDataMikk_GetTexCoord(const SMikkTSpaceContext *pContext, float fvTexcOut[], const int iFace, const int iVert) {
	auto *userData = static_cast<ObDataMikkUserData *>(pContext->m_pUserData);
	int v_idx = iFace * 3 + iVert;
	const VERT &t = obdata[userData->obj_idx].t[v_idx];
	fvTexcOut[0] = t.x;
	fvTexcOut[1] = t.y;
}

static void ObDataMikk_SetTSpaceBasic(const SMikkTSpaceContext *pContext, const float fvTangent[], const float fSign, const int iFace, const int iVert) {
	auto *userData = static_cast<ObDataMikkUserData *>(pContext->m_pUserData);
	int v_idx = iFace * 3 + iVert;
	obdata[userData->obj_idx].v[v_idx].nmx = fvTangent[0];
	obdata[userData->obj_idx].v[v_idx].nmy = fvTangent[1];
	obdata[userData->obj_idx].v[v_idx].nmz = fvTangent[2];
}

void ComputeObDataNormals(int obj_idx) {
	if (obj_idx < 0)
		return;

	int v_count = num_vert_per_object[obj_idx];
	if (v_count < 3)
		return;

	for (int v_i = 0; v_i + 2 < v_count; v_i += 3) {
		CalculateVertNormalAndTangent(
		    obdata[obj_idx].v[v_i],
		    obdata[obj_idx].v[v_i + 1],
		    obdata[obj_idx].v[v_i + 2],
		    obdata[obj_idx].t[v_i],
		    obdata[obj_idx].t[v_i + 1],
		    obdata[obj_idx].t[v_i + 2]);
	}

	SMikkTSpaceInterface mikkInterface = {};
	mikkInterface.m_getNumFaces = ObDataMikk_GetNumFaces;
	mikkInterface.m_getNumVerticesOfFace = ObDataMikk_GetNumVerticesOfFace;
	mikkInterface.m_getPosition = ObDataMikk_GetPosition;
	mikkInterface.m_getNormal = ObDataMikk_GetNormal;
	mikkInterface.m_getTexCoord = ObDataMikk_GetTexCoord;
	mikkInterface.m_setTSpaceBasic = ObDataMikk_SetTSpaceBasic;

	ObDataMikkUserData userData;
	userData.obj_idx = obj_idx;
	userData.total_triangles = v_count / 3;

	SMikkTSpaceContext mikkContext = {};
	mikkContext.m_pInterface = &mikkInterface;
	mikkContext.m_pUserData = &userData;

	genTangSpaceDefault(&mikkContext);

	// Post-check tangents and normals for ObData
	for (int v = 0; v < v_count; v++) {
		auto &vert = obdata[obj_idx].v[v];
		float nlen = sqrtf(vert.nx * vert.nx + vert.ny * vert.ny + vert.nz * vert.nz);
		if (nlen < 1e-5f) {
			vert.nx = 0.0f;
			vert.ny = 1.0f;
			vert.nz = 0.0f;
		}
		float tlen = sqrtf(vert.nmx * vert.nmx + vert.nmy * vert.nmy + vert.nmz * vert.nmz);
		if (tlen < 1e-5f) {
			float ax = (fabsf(vert.nx) > 0.9f) ? 0.0f : 1.0f;
			float ay = 0.0f;
			float az = (fabsf(vert.nz) > 0.9f) ? 0.0f : 1.0f;

			float tanX = ay * vert.nz - az * vert.ny;
			float tanY = az * vert.nx - ax * vert.nz;
			float tanZ = ax * vert.ny - ay * vert.nx;

			float len = sqrtf(tanX * tanX + tanY * tanY + tanZ * tanZ);
			if (len > 1e-5f) {
				vert.nmx = tanX / len;
				vert.nmy = tanY / len;
				vert.nmz = tanZ / len;
			} else {
				vert.nmx = 1.0f;
				vert.nmy = 0.0f;
				vert.nmz = 0.0f;
			}
		}
	}
}

void Smooth3DSModelNormals(int pmodel_id) {
	if (pmodel_id < 0)
		return;

	int num_frames = pmdata[pmodel_id].num_frames;
	if (num_frames <= 0)
		return;

	int total_num_verts = pmdata[pmodel_id].num_verts;
	if (total_num_verts <= 0)
		return;

	for (int frame_num = 0; frame_num < num_frames; frame_num++) {
		VERT *frame_verts = pmdata[pmodel_id].w[frame_num];
		if (!frame_verts)
			continue;

		SmoothVertArrayNoHash(frame_verts, total_num_verts, 0.45f);
	}
}

void SmoothMD2ModelNormals(int pmodel_id) {
	if (pmodel_id < 0)
		return;

	int num_frames = pmdata[pmodel_id].num_frames;
	if (num_frames <= 0)
		return;

	int num_indices = pmdata[pmodel_id].num_verts; // for MD2 models, num_verts stores total triangle list indices
	if (num_indices < 3)
		return;

	int max_v_idx = 0;
	for (int i = 0; i < num_indices; i++) {
		if (pmdata[pmodel_id].f[i] > max_v_idx) {
			max_v_idx = pmdata[pmodel_id].f[i];
		}
	}
	int num_verts = max_v_idx + 1;

	for (int frame_num = 0; frame_num < num_frames; frame_num++) {
		VERT *frame_verts = pmdata[pmodel_id].w[frame_num];
		if (!frame_verts)
			continue;

		SmoothVertArrayNoHash(frame_verts, num_verts, 0.2f);
	}
}

bool ObjectHasShadow(int object_id) {

	if (object_id == -99 || object_id == -111 || object_id == -1) {
		return false;
	}

	if (obdata[object_id].shadow) {
		return true;
	}

	return false;
}

struct ObjectMikkUserData {
	D3DVERTEX2 *verts;
	int start_cnt;
	int total_faces;
};

static int ObjectMikk_GetNumFaces(const SMikkTSpaceContext *pContext) {
	auto *userData = static_cast<ObjectMikkUserData *>(pContext->m_pUserData);
	return userData->total_faces;
}

static int ObjectMikk_GetNumVerticesOfFace(const SMikkTSpaceContext *pContext, const int iFace) {
	return 3;
}

static void ObjectMikk_GetPosition(const SMikkTSpaceContext *pContext, float fvPosOut[], const int iFace, const int iVert) {
	auto *userData = static_cast<ObjectMikkUserData *>(pContext->m_pUserData);
	int idx = userData->start_cnt + iFace * 3 + iVert;
	const D3DVERTEX2 &v = userData->verts[idx];
	fvPosOut[0] = v.x;
	fvPosOut[1] = v.y;
	fvPosOut[2] = v.z;
}

static void ObjectMikk_GetNormal(const SMikkTSpaceContext *pContext, float fvNormOut[], const int iFace, const int iVert) {
	auto *userData = static_cast<ObjectMikkUserData *>(pContext->m_pUserData);
	int idx = userData->start_cnt + iFace * 3 + iVert;
	const D3DVERTEX2 &v = userData->verts[idx];
	fvNormOut[0] = v.nx;
	fvNormOut[1] = v.ny;
	fvNormOut[2] = v.nz;
}

static void ObjectMikk_GetTexCoord(const SMikkTSpaceContext *pContext, float fvTexcOut[], const int iFace, const int iVert) {
	auto *userData = static_cast<ObjectMikkUserData *>(pContext->m_pUserData);
	int idx = userData->start_cnt + iFace * 3 + iVert;
	const D3DVERTEX2 &v = userData->verts[idx];
	fvTexcOut[0] = v.tu;
	fvTexcOut[1] = v.tv;
}

static void ObjectMikk_SetTSpaceBasic(const SMikkTSpaceContext *pContext, const float fvTangent[], const float fSign, const int iFace, const int iVert) {
	auto *userData = static_cast<ObjectMikkUserData *>(pContext->m_pUserData);
	int idx = userData->start_cnt + iFace * 3 + iVert;
	userData->verts[idx].nmx = fvTangent[0];
	userData->verts[idx].nmy = fvTangent[1];
	userData->verts[idx].nmz = fvTangent[2];
}

void ObjectToD3DVertList(int ob_type, float angle, int oblist_index) {
	int ob_vert_count = 0;
	int poly = num_polys_per_object[ob_type];
	float wx = oblist[oblist_index].x;
	float wy = oblist[oblist_index].y;
	float wz = oblist[oblist_index].z;
	float workx = 0;
	float worky = 0;
	float workz = 0;

	float cosine = (float)cos(angle * k);
	float sine = (float)sin(angle * k);

	int start_cnt = cnt;

	for (int w = 0; w < poly; w++) {
		int num_vert = obdata[ob_type].num_vert[w];
		int fan_cnt = cnt;
		int ctext;

		// Reset mx/my/mz only for used vertices
		for (int v = 0; v < num_vert; v++) {
			mx[v] = my[v] = mz[v] = 0.0f;
		}

		// Texture selection
		if (strstr(oblist[oblist_index].name, "!") != NULL) {
			ObjectsToDraw[number_of_polys_per_frame].texture = oblist[oblist_index].monstertexture;
			ctext = oblist[oblist_index].monstertexture;
			ObjectsToDraw[number_of_polys_per_frame].objectId = ob_type;
		} else {
			ObjectsToDraw[number_of_polys_per_frame].texture = obdata[ob_type].tex[w];
			ctext = obdata[ob_type].tex[w];
			ObjectsToDraw[number_of_polys_per_frame].objectId = ob_type;
		}

		ObjectsToDraw[number_of_polys_per_frame].castshaddow = oblist[oblist_index].castshadow ? 1 : 0;
		texture_list_buffer[number_of_polys_per_frame] = ctext;

		int cresult = CycleBitMap(ctext);
		if (cresult != -1) {
			oblist[oblist_index].monstertexture = cresult;

			XMFLOAT3 normroadold{ 50, 0, 0 };
			XMFLOAT3 work1{ m_vEyePt.x, m_vEyePt.y, m_vEyePt.z };
			XMFLOAT3 work2{ wx, wy, wz };

			XMVECTOR final = XMVector3Normalize(XMLoadFloat3(&work1) - XMLoadFloat3(&work2));
			XMVECTOR final2 = XMVector3Normalize(XMLoadFloat3(&normroadold));
			float fDot = XMVectorGetX(XMVector3Dot(final, final2));
			float convangle = (float)acos(fDot) / k;
			fDot = (work2.z < work1.z) ? convangle : 180.0f + (180.0f - convangle);

			cosine = (float)cos(fDot * k);
			sine = (float)sin(fDot * k);
		}

		// Rotation matrix for DirectX Math transformation of normals and tangents
		XMMATRIX rotMat = XMMATRIX(
		    cosine, 0.0f, sine, 0.0f,
		    0.0f, 1.0f, 0.0f, 0.0f,
		    -sine, 0.0f, cosine, 0.0f,
		    0.0f, 0.0f, 0.0f, 1.0f);

		// Vertex transformation and texture coordinates
		for (int vert_cnt = 0; vert_cnt < num_vert; vert_cnt++) {
			const auto &v = obdata[ob_type].v[ob_vert_count];
			const auto &t = obdata[ob_type].t[ob_vert_count];

			tx[vert_cnt] = t.x;
			ty[vert_cnt] = t.y;

			mx[vert_cnt] = wx + (v.x * cosine - v.z * sine);
			my[vert_cnt] = wy + v.y;
			mz[vert_cnt] = wz + (v.x * sine + v.z * cosine);

			ob_vert_count++;
			g_ob_vert_count++;
		}

		verts_per_poly[number_of_polys_per_frame] = num_vert;
		ObjectsToDraw[number_of_polys_per_frame].vertsperpoly = num_vert;
		ObjectsToDraw[number_of_polys_per_frame].srcstart = cnt;

		D3DPRIMITIVETYPE poly_command = obdata[ob_type].poly_cmd[w];

		// Texture mapping branch
		bool use_texmap = obdata[ob_type].use_texmap[w] != FALSE;
		int vert_base = ob_vert_count - num_vert;
		for (int i = 0; i < num_vert; i++) {
			const auto &v = obdata[ob_type].v[vert_base + i];

			src_v[cnt].x = D3DVAL(mx[i]);
			src_v[cnt].y = D3DVAL(my[i]);
			src_v[cnt].z = D3DVAL(mz[i]);
			src_v[cnt].tu = D3DVAL(use_texmap ? TexMap[ctext].tu[i] : tx[i]);
			src_v[cnt].tv = D3DVAL(use_texmap ? TexMap[ctext].tv[i] : ty[i]);
			src_v[cnt].CastShadow = 0;

			src_collide[cnt] = objectcollide == 1 ? 1 : 0;

			// Transform normal and tangent from model using DirectX Math
			XMVECTOR nVec = XMVectorSet(v.nx, v.ny, v.nz, 0.0f);
			XMVECTOR tVec = XMVectorSet(v.nmx, v.nmy, v.nmz, 0.0f);

			XMVECTOR rotN = XMVector3TransformNormal(nVec, rotMat);
			float nLen = XMVectorGetX(XMVector3Length(rotN));
			rotN = (nLen > 1e-5f) ? XMVectorScale(rotN, 1.0f / nLen) : XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

			XMVECTOR rotT = XMVector3TransformNormal(tVec, rotMat);
			float tLen = XMVectorGetX(XMVector3Length(rotT));
			rotT = (tLen > 1e-5f) ? XMVectorScale(rotT, 1.0f / tLen) : XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

			XMFLOAT3 fn, ft;
			XMStoreFloat3(&fn, rotN);
			XMStoreFloat3(&ft, rotT);

			src_v[cnt].nx = fn.x;
			src_v[cnt].ny = fn.y;
			src_v[cnt].nz = fn.z;

			src_v[cnt].nmx = ft.x;
			src_v[cnt].nmy = ft.y;
			src_v[cnt].nmz = ft.z;

			cnt++;
		}

		ObjectsToDraw[number_of_polys_per_frame].vert_index = number_of_polys_per_frame;

		dp_commands[number_of_polys_per_frame] = poly_command;
		dp_command_index_mode[number_of_polys_per_frame] = USE_NON_INDEXED_DP;

		if (poly_command == D3DPT_TRIANGLESTRIP) {
			ConvertTraingleStrip(fan_cnt);
			dp_commands[number_of_polys_per_frame] = D3DPT_TRIANGLELIST;
			if (num_vert > 3) {
				num_vert = (num_vert - 3) * 3;
				verts_per_poly[number_of_polys_per_frame] = (num_vert + 3);
			}
			num_triangles_in_scene += (num_vert - 2);
		} else if (poly_command == D3DPT_TRIANGLEFAN) {
			ConvertTraingleFan(fan_cnt);
			dp_commands[number_of_polys_per_frame] = D3DPT_TRIANGLELIST;
			if (num_vert > 3) {
				num_vert = (num_vert - 3) * 3;
				verts_per_poly[number_of_polys_per_frame] = (num_vert + 3);
			}
			num_triangles_in_scene += (num_vert - 2);
		}
		if (poly_command == D3DPT_TRIANGLELIST)
			num_triangles_in_scene += (num_vert / 3);

		number_of_polys_per_frame++;
		num_verts_in_scene += num_vert;
		num_dp_commands_in_scene++;
	}
	// Uncomment if you want to smooth normals for specific objects
	// if (ob_type == 121 || ob_type == 169 || ob_type == 170 || ob_type == 58 || strstr(oblist[oblist_index].name, "door") != NULL) {
	//     SmoothNormals(start_cnt);
	// }
}

void DrawBoundingBox() {

	ObjectsToDraw[number_of_polys_per_frame].srcstart = cnt;
	ObjectsToDraw[number_of_polys_per_frame].objectId = -1;
	ObjectsToDraw[number_of_polys_per_frame].srcfstart = 0;

	ObjectsToDraw[number_of_polys_per_frame].vert_index = number_of_polys_per_frame;
	ObjectsToDraw[number_of_polys_per_frame].texture = 276;
	ObjectsToDraw[number_of_polys_per_frame].vertsperpoly = 3;
	ObjectsToDraw[number_of_polys_per_frame].facesperpoly = 1;

	texture_list_buffer[number_of_polys_per_frame] = 238; // 263

	int test = (countboundingbox / 4) * 6;
	verts_per_poly[number_of_polys_per_frame] = test;
	dp_command_index_mode[number_of_polys_per_frame] = USE_NON_INDEXED_DP;
	dp_commands[number_of_polys_per_frame] = D3DPT_TRIANGLELIST;

	// int num_vert = (countboundingbox - 3) * 3;
	// verts_per_poly[number_of_polys_per_frame] = (num_vert + 3);

	number_of_polys_per_frame++;

	int fan_cnt = cnt;

	int uvcount = 0;

	for (int i = 0; i < countboundingbox; i++) {
		src_v[cnt].x = boundingbox[i].x;
		src_v[cnt].y = boundingbox[i].y;
		src_v[cnt].z = boundingbox[i].z;

		if (uvcount == 0) {
			src_v[cnt].tu = 0.0f;
			src_v[cnt].tv = 0.0f;
			uvcount++;
		} else if (uvcount == 1) {
			src_v[cnt].tu = 0.0f;
			src_v[cnt].tv = 1.0f;
			uvcount++;
		} else if (uvcount == 2) {
			src_v[cnt].tu = 1.0f;
			src_v[cnt].tv = 0.0f;
			uvcount++;
		} else if (uvcount == 3) {
			src_v[cnt].tu = 1.0f;
			src_v[cnt].tv = 1.0f;

			uvcount = 0;
		} else {
			uvcount++;
		}

		cnt++;
	}

	ConvertQuad(fan_cnt);
}

void PlayerToD3DVertList(int pmodel_id, int curr_frame, float angle, int texture_alias, int tex_flag, float xt, float yt, float zt, int nextFrame, float fDot2) {
	// Normalize angle (degrees)
	if (angle >= 360.0f)
		angle -= 360.0f;
	if (angle < 0.0f)
		angle += 360.0f;

	// Indexed 3DS models: forward to indexed path with the correct frame
	if (pmdata[pmodel_id].use_indexed_primitive == TRUE) {
		PlayerToD3DIndexedVertList(pmodel_id, curr_frame, angle, texture_alias, tex_flag, xt, yt, zt, fDot2);
		return;
	}

	// MD2 models
	if (curr_frame >= pmdata[pmodel_id].num_frames)
		curr_frame = 0;

	const float cosine = (float)cos(angle * k);
	const float sine = (float)sin(angle * k);

	const float wx = xt;
	const float wy = yt;
	const float wz = zt;

	int i_count = 0;
	const int num_poly = pmdata[pmodel_id].num_polys_per_frame;
	const int start_cnt = cnt;

	for (int i = 0; i < num_poly; i++) {
		const D3DPRIMITIVETYPE p_command = pmdata[pmodel_id].poly_cmd[i];
		int num_verts_per_poly = pmdata[pmodel_id].num_vert[i];

		ObjectsToDraw[number_of_polys_per_frame].srcstart = cnt;
		ObjectsToDraw[number_of_polys_per_frame].objectId = -1;

		const int fan_cnt = cnt;
		int triVertexCounter = 0;

		XMMATRIX rotYaw = XMMATRIX(
		    cosine, 0.0f, sine, 0.0f,
		    0.0f, 1.0f, 0.0f, 0.0f,
		    -sine, 0.0f, cosine, 0.0f,
		    0.0f, 0.0f, 0.0f, 1.0f);

		XMMATRIX rotMat;
		if (weapondrop == 1 && fDot2 != 0.0f) {
			const float cp = cosf(fDot2 * k);
			const float sp = sinf(fDot2 * k);
			XMMATRIX rotPitch = XMMATRIX(
			    cp, -sp, 0.0f, 0.0f,
			    sp, cp, 0.0f, 0.0f,
			    0.0f, 0.0f, 1.0f, 0.0f,
			    0.0f, 0.0f, 0.0f, 1.0f);
			rotMat = rotPitch * rotYaw;
		} else {
			rotMat = rotYaw;
		}

		for (int j = 0; j < num_verts_per_poly; j++) {
			const short v_index = pmdata[pmodel_id].f[i_count];

			const vert_ptr tp = &pmdata[pmodel_id].w[curr_frame][v_index];

			float x, y, z;
			float nx, ny, nz, nmx, nmy, nmz;

			if (nextFrame != -1) {
				const vert_ptr tpNextFrame = &pmdata[pmodel_id].w[nextFrame][v_index];
				const float t = (gametimerAnimation > 0.0f && gametimerAnimation < 1.0f) ? gametimerAnimation : 0.0f;

				if (t > 0.0f) {
					x = tp->x + t * (tpNextFrame->x - tp->x);
					z = tp->y + t * (tpNextFrame->y - tp->y);
					y = tp->z + t * (tpNextFrame->z - tp->z);

					nx = tp->nx + t * (tpNextFrame->nx - tp->nx);
					ny = tp->ny + t * (tpNextFrame->ny - tp->ny);
					nz = tp->nz + t * (tpNextFrame->nz - tp->nz);

					nmx = tp->nmx + t * (tpNextFrame->nmx - tp->nmx);
					nmy = tp->nmy + t * (tpNextFrame->nmy - tp->nmy);
					nmz = tp->nmz + t * (tpNextFrame->nmz - tp->nmz);
				} else {
					x = tp->x;
					z = tp->y;
					y = tp->z;
					nx = tp->nx;
					ny = tp->ny;
					nz = tp->nz;
					nmx = tp->nmx;
					nmy = tp->nmy;
					nmz = tp->nmz;
				}
			} else {
				x = tp->x;
				z = tp->y;
				y = tp->z;
				nx = tp->nx;
				ny = tp->ny;
				nz = tp->nz;
				nmx = tp->nmx;
				nmy = tp->nmy;
				nmz = tp->nmz;
			}

			if (weapondrop == 1) {
				y -= 40.0f;
			}

			// Apply optional pitch (fDot2) only for weapondrop path (matches previous behavior)
			if (weapondrop == 1 && fDot2 != 0.0f) {
				const float cp = cosf(fDot2 * k);
				const float sp = sinf(fDot2 * k);
				const float px = (y * sp + x * cp);
				const float py = (y * cp - x * sp);
				// z unchanged
				x = px;
				y = py;
			}

			// Yaw
			float rx = (x * cosine - z * sine);
			float ry = y;
			float rz = (x * sine + z * cosine);

			// Translate
			rx += wx;
			ry += wy;
			rz += wz;

			// UVs
			const float tx = pmdata[pmodel_id].t[i_count].x * pmdata[pmodel_id].skx;
			const float ty = pmdata[pmodel_id].t[i_count].y * pmdata[pmodel_id].sky;

			// Write vertex
			src_v[cnt].x = D3DVAL(rx);
			src_v[cnt].y = D3DVAL(ry);
			src_v[cnt].z = D3DVAL(rz);
			src_v[cnt].tu = D3DVAL(tx);
			src_v[cnt].tv = D3DVAL(ty);
			src_v[cnt].CastShadow = 1;
			src_collide[cnt] = 1;

			// Transform normal and tangent from model using DirectX Math
			XMVECTOR nVec = XMVectorSet(nx, ny, nz, 0.0f);
			XMVECTOR tVec = XMVectorSet(nmx, nmy, nmz, 0.0f);

			XMVECTOR rotN = XMVector3TransformNormal(nVec, rotMat);
			float nLen = XMVectorGetX(XMVector3Length(rotN));
			rotN = (nLen > 1e-5f) ? XMVectorScale(rotN, 1.0f / nLen) : XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

			XMVECTOR rotT = XMVector3TransformNormal(tVec, rotMat);
			float tLen = XMVectorGetX(XMVector3Length(rotT));
			rotT = (tLen > 1e-5f) ? XMVectorScale(rotT, 1.0f / tLen) : XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

			XMFLOAT3 fn, ft;
			XMStoreFloat3(&fn, rotN);
			XMStoreFloat3(&ft, rotT);

			src_v[cnt].nx = fn.x;
			src_v[cnt].ny = fn.y;
			src_v[cnt].nz = fn.z;

			src_v[cnt].nmx = ft.x;
			src_v[cnt].nmy = ft.y;
			src_v[cnt].nmz = ft.z;

			cnt++;
			i_count++;
		}

		ObjectsToDraw[number_of_polys_per_frame].vert_index = number_of_polys_per_frame;
		ObjectsToDraw[number_of_polys_per_frame].texture = texture_alias;
		ObjectsToDraw[number_of_polys_per_frame].vertsperpoly = num_verts_per_poly;

		verts_per_poly[number_of_polys_per_frame] = num_verts_per_poly;
		dp_command_index_mode[number_of_polys_per_frame] = USE_NON_INDEXED_DP;
		dp_commands[number_of_polys_per_frame] = p_command;

		if (p_command == D3DPT_TRIANGLESTRIP) {
			num_triangles_in_scene += (num_verts_per_poly - 2);
			ConvertTraingleStrip(fan_cnt);
			dp_commands[number_of_polys_per_frame] = D3DPT_TRIANGLELIST;

			if (num_verts_per_poly > 3) {
				num_verts_per_poly = (num_verts_per_poly - 3) * 3;
				verts_per_poly[number_of_polys_per_frame] = (num_verts_per_poly + 3);
			}
		} else if (p_command == D3DPT_TRIANGLEFAN) {
			ConvertTraingleFan(fan_cnt);
			dp_commands[number_of_polys_per_frame] = D3DPT_TRIANGLELIST;
			num_triangles_in_scene += (num_verts_per_poly - 2);

			if (num_verts_per_poly > 3) {
				num_verts_per_poly = (num_verts_per_poly - 3) * 3;
				verts_per_poly[number_of_polys_per_frame] = (num_verts_per_poly + 3);
			}
		} else if (p_command == D3DPT_TRIANGLELIST) {
			num_triangles_in_scene += (num_verts_per_poly / 3);
		}

		num_verts_in_scene += num_verts_per_poly;
		num_dp_commands_in_scene++;

		texture_list_buffer[number_of_polys_per_frame] = texture_alias;

		number_of_polys_per_frame++;
	}

	return;
}

int tracknormal[MAX_NUM_QUADS];

// Static buffers for ultimate-speed smoothing (no allocations)
#define SN_HASH_SIZE 16384
#define SN_MAX_VERTS 250000
static int sn_head[SN_HASH_SIZE];
static int sn_next[SN_MAX_VERTS];
static int sn_group[512];

void SmoothNormals(int start_cnt) {
	const float epsilon = 0.0001f;
	const float inv_eps = 1.0f / epsilon;
	const float smooth_threshold = 0.2f;

	const int total = cnt - start_cnt;
	if (total <= 1 || total >= SN_MAX_VERTS)
		return;

	// 1. Initialize hash table (O(SN_HASH_SIZE) or O(total))
	// We only clear the buckets we might use if we want, but memset is extremely fast.
	memset(sn_head, -1, sizeof(sn_head));

	// 2. Spatial Hashing (O(N))
	for (int i = 0; i < total; ++i) {
		const auto &v = src_v[start_cnt + i];
		// Quantize to grid
		int64_t qx = (int64_t)(v.x * inv_eps);
		int64_t qy = (int64_t)(v.y * inv_eps);
		int64_t qz = (int64_t)(v.z * inv_eps);

		// Fast bitwise hash (Murmur-like)
		uint32_t h = (uint32_t)((qx * 73856093) ^ (qy * 19349663) ^ (qz * 83492791)) & (SN_HASH_SIZE - 1);

		sn_next[i] = sn_head[h];
		sn_head[h] = i;
	}

	// 3. Process buckets (O(N))
	static bool processed[SN_MAX_VERTS];
	memset(processed, 0, total * sizeof(bool));

	for (int i = 0; i < total; ++i) {
		if (processed[i])
			continue;

		const auto &v_base = src_v[start_cnt + i];
		int pg_cnt = 0;
		sn_group[pg_cnt++] = start_cnt + i;
		processed[i] = true;

		// Re-compute hash to find bucket
		int64_t qx = (int64_t)(v_base.x * inv_eps);
		int64_t qy = (int64_t)(v_base.y * inv_eps);
		int64_t qz = (int64_t)(v_base.z * inv_eps);
		uint32_t h = (uint32_t)((qx * 73856093) ^ (qy * 19349663) ^ (qz * 83492791)) & (SN_HASH_SIZE - 1);

		// Scan bucket for epsilon neighbors
		int entry = sn_head[h];
		while (entry != -1) {
			if (!processed[entry]) {
				const auto &v_test = src_v[start_cnt + entry];
				if (fabsf(v_test.x - v_base.x) < epsilon &&
				    fabsf(v_test.y - v_base.y) < epsilon &&
				    fabsf(v_test.z - v_base.z) < epsilon) {
					if (pg_cnt < 512) {
						sn_group[pg_cnt++] = start_cnt + entry;
						processed[entry] = true;
					}
				}
			}
			entry = sn_next[entry];
		}

		if (pg_cnt < 2)
			continue;

		// 4. Smooth within position group (SIMD)
		bool sub_proc[512] = { false };
		for (int j = 0; j < pg_cnt; ++j) {
			if (sub_proc[j])
				continue;

			int sg_cnt = 0;
			int baseIdx = sn_group[j];
			int smooth_group[512];
			smooth_group[sg_cnt++] = baseIdx;
			sub_proc[j] = true;

			XMVECTOR ni = XMVectorSet(src_v[baseIdx].nx, src_v[baseIdx].ny, src_v[baseIdx].nz, 0.0f);

			for (int k = j + 1; k < pg_cnt; ++k) {
				if (sub_proc[k])
					continue;
				int testIdx = sn_group[k];
				XMVECTOR nj = XMVectorSet(src_v[testIdx].nx, src_v[testIdx].ny, src_v[testIdx].nz, 0.0f);

				if (XMVectorGetX(XMVector3Dot(ni, nj)) > smooth_threshold) {
					smooth_group[sg_cnt++] = testIdx;
					sub_proc[k] = true;
				}
			}

			if (sg_cnt > 1) {
				XMVECTOR sumN = XMVectorZero();
				XMVECTOR sumT = XMVectorZero();
				for (int k = 0; k < sg_cnt; ++k) {
					const auto &v = src_v[smooth_group[k]];
					sumN = XMVectorAdd(sumN, XMVectorSet(v.nx, v.ny, v.nz, 0.0f));
					sumT = XMVectorAdd(sumT, XMVectorSet(v.nmx, v.nmy, v.nmz, 0.0f));
				}
				sumN = XMVector3Normalize(sumN);
				// Gram-Schmidt orthogonalization: ensure Tangent is orthogonal to Normal
				sumT = XMVector3Normalize(XMVectorSubtract(sumT, XMVectorMultiply(sumN, XMVector3Dot(sumN, sumT))));

				XMFLOAT3 fN, fT;
				XMStoreFloat3(&fN, sumN);
				XMStoreFloat3(&fT, sumT);

				for (int k = 0; k < sg_cnt; ++k) {
					auto &v = src_v[smooth_group[k]];
					v.nx = fN.x;
					v.ny = fN.y;
					v.nz = fN.z;
					v.nmx = fT.x;
					v.nmy = fT.y;
					v.nmz = fT.z;
				}
			}
		}
	}
}
void SmoothNormalsNoHash(int start_cnt) {
	// Smooth the vertex normals out so the models look less blocky.
	// Improvements:
	// - Use epsilon for position comparison to handle floating point errors from tessellation.
	// - Use a dot product threshold to avoid smoothing across sharp edges (smooth when you can).
	// - Better management of shared vertex groups.

	const float epsilon = 0.0001f;
	const float smooth_threshold = 0.4f; // approx 60 degrees. 0.7f is 45 deg. 0.5f is more aggressive.

	// 0.707: Smooths up to 45� (Common default for most models).
	// 0.500: Smooths up to 60�.
	// 0.000: Smooths up to 90� (Everything up to a perfect right angle will be smoothed).

	for (int i = start_cnt; i < cnt; i++) {
		tracknormal[i] = 0;
	}

	for (int i = start_cnt; i < cnt; i++) {
		if (tracknormal[i] == 0) {
			float x = src_v[i].x;
			float y = src_v[i].y;
			float z = src_v[i].z;

			XMVECTOR ni = XMVectorSet(src_v[i].nx, src_v[i].ny, src_v[i].nz, 0);

			int scount = 0;
			// Pass 1: find all vertices at the same location with similar normals
			for (int j = i; j < cnt; j++) {
				if (tracknormal[j] == 0) {
					// Check position with epsilon
					if (fabsf(src_v[j].x - x) < epsilon &&
					    fabsf(src_v[j].y - y) < epsilon &&
					    fabsf(src_v[j].z - z) < epsilon) {

						// Condition for smoothing: similar normals (smooth when you can)
						XMVECTOR nj = XMVectorSet(src_v[j].nx, src_v[j].ny, src_v[j].nz, 0);
						float dot = XMVectorGetX(XMVector3Dot(ni, nj));

						// If normals are within the threshold, or if they are very similar (likely part of same quad)
						if (dot > smooth_threshold) {
							if (scount < 2000) {
								sharedv[scount++] = j;
							}
						}
					}
				}
			}

			if (scount > 1) {
				XMVECTOR sum = XMVectorSet(0, 0, 0, 0);
				XMVECTOR sumtan = XMVectorSet(0, 0, 0, 0);

				for (int k = 0; k < scount; k++) {
					sum = sum + XMVectorSet(src_v[sharedv[k]].nx, src_v[sharedv[k]].ny, src_v[sharedv[k]].nz, 0);
					sumtan = sumtan + XMVectorSet(src_v[sharedv[k]].nmx, src_v[sharedv[k]].nmy, src_v[sharedv[k]].nmz, 0);
				}

				XMVECTOR average = XMVector3Normalize(sum);
				// Gram-Schmidt orthogonalization: ensure Tangent is orthogonal to Normal (matches SmoothNormals)
				XMVECTOR averagetan = XMVector3Normalize(XMVectorSubtract(sumtan, XMVectorMultiply(average, XMVector3Dot(average, sumtan))));

				XMFLOAT3 final2;
				XMStoreFloat3(&final2, average);

				XMFLOAT3 finaltan;
				XMStoreFloat3(&finaltan, averagetan);

				for (int k = 0; k < scount; k++) {
					src_v[sharedv[k]].nx = final2.x;
					src_v[sharedv[k]].ny = final2.y;
					src_v[sharedv[k]].nz = final2.z;

					src_v[sharedv[k]].nmx = finaltan.x;
					src_v[sharedv[k]].nmy = finaltan.y;
					src_v[sharedv[k]].nmz = finaltan.z;

					tracknormal[sharedv[k]] = 1;
				}
			} else if (scount == 1) {
				// Even if only one matched the normal threshold, we must mark it as tracked
				// to avoid re-processing it as a new seed.
				tracknormal[sharedv[0]] = 1;
			}
		}
	}
}

void ComputerWeightedAverages(int start_cnt);

void SmoothNormalsWeighted(int start_cnt) {

	for (int i = 0; i < MAX_NUM_QUADS; i++) {
		tracknormal[i] = 0;
	}

	ComputerWeightedAverages(start_cnt);

	// Smooth the vertex normals out so the models look less blocky.
	int scount = 0;

	for (int i = start_cnt; i < cnt; i++) {
		if (tracknormal[i] == 0) {
			float x = src_v[i].x;
			float y = src_v[i].y;
			float z = src_v[i].z;

			scount = 0;

			for (int j = start_cnt; j < cnt; j++) {
				// if (i != j) {
				if (tracknormal[j] == 0 && x == src_v[j].x && y == src_v[j].y && z == src_v[j].z) {

					// if (src_v[j].weight < 45.0f) {
					// found shared vertex
					sharedv[scount] = j;
					scount++;
					//}
				}
				//}
			}

			if (scount > 0) {
				XMVECTOR sum = XMVectorSet(0, 0, 0, 0);
				XMVECTOR sumtan = XMVectorSet(0, 0, 0, 0);

				XMFLOAT3 x1, xtan;
				XMVECTOR average;

				XMVECTOR work = XMVectorSet(0, 0, 0, 0);

				XMFLOAT3 finalweight;

				float weight = 0;
				float area = 0;

				for (int k = 0; k < scount; k++) {

					weight = src_v[sharedv[k]].weight;
					area = src_v[sharedv[k]].area;

					if (weight > 90.0f) {
						int a = 1;
					}

					// weight = (float)acos(weight) / (float)0.017453292;;

					work = XMVectorSet(src_v[sharedv[k]].nx, src_v[sharedv[k]].ny, src_v[sharedv[k]].nz, 0);
					work = work * (weight * area);
					// work = XMVector3Normalize(work);
					XMStoreFloat3(&finalweight, work);

					x1.x = finalweight.x;
					x1.y = finalweight.y;
					x1.z = finalweight.z;

					// x1.x = src_v[sharedv[k]].nx;
					// x1.y = src_v[sharedv[k]].ny;
					// x1.z = src_v[sharedv[k]].nz;
					sum = sum + XMLoadFloat3(&x1);

					work = XMVectorSet(src_v[sharedv[k]].nmx, src_v[sharedv[k]].nmy, src_v[sharedv[k]].nmz, 0);
					work = work * (weight * area);
					// work = XMVector3Normalize(work);
					XMStoreFloat3(&finalweight, work);

					xtan.x = finalweight.x;
					xtan.y = finalweight.y;
					xtan.z = finalweight.z;
					sumtan = sumtan + XMLoadFloat3(&xtan);
				}

				// sum = sum / (float)scount;

				XMFLOAT3 final2, finaltan;

				average = XMVector3Normalize(sum);
				XMStoreFloat3(&final2, average);

				XMVECTOR average_tan = XMVector3Normalize(XMVectorSubtract(sumtan, XMVectorMultiply(average, XMVector3Dot(average, sumtan))));
				XMStoreFloat3(&finaltan, average_tan);

				for (int k = 0; k < scount; k++) {
					src_v[sharedv[k]].nx = final2.x;
					src_v[sharedv[k]].ny = final2.y;
					src_v[sharedv[k]].nz = final2.z;

					src_v[sharedv[k]].nmx = finaltan.x;
					src_v[sharedv[k]].nmy = finaltan.y;
					src_v[sharedv[k]].nmz = finaltan.z;

					tracknormal[sharedv[k]] = 1;
				}
			}
		}
	}
}

void ComputerWeightedAverages(int start_cnt) {

	int count = 0;

	XMFLOAT3 vw1, vw2, vw3;

	XMVECTOR P1, P2;
	XMVECTOR vDiff;
	XMVECTOR vDiff2;

	XMVECTOR final;
	XMVECTOR final2;
	XMVECTOR fDotVector;
	float fDot;

	for (int i = start_cnt; i < cnt; i = i + 3) {

		vw1.x = src_v[i].x;
		vw1.y = src_v[i].y;
		vw1.z = src_v[i].z;
		// vw1.x = 0.0f;
		// vw1.y = 0;
		// vw1.z = 0;

		vw2.x = src_v[i + 1].x;
		vw2.y = src_v[i + 1].y;
		vw2.z = src_v[i + 1].z;
		// vw2.x = 0;
		// vw2.y = 0;
		// vw2.z = 50.0f;

		vw3.x = src_v[i + 2].x;
		vw3.y = src_v[i + 2].y;
		vw3.z = src_v[i + 2].z;
		// vw3.x = 50.0f;
		// vw3.y = 0;
		// vw3.z = 0;

		// v1, v2, v3 are the vertices of face A
		// if face B shares v1 {
		//	angle = angle_between_vectors(v1 - v2, v1 - v3)
		//		n += (face B facet normal) * (face B surface area) * angle // multiply by angle
		//}
		// if face B shares v2 {
		//	angle = angle_between_vectors(v2 - v1, v2 - v3)
		//		n += (face B facet normal) * (face B surface area) * angle // multiply by angle
		//}
		// if face B shares v3 {
		//	angle = angle_between_vectors(v3 - v1, v3 - v2)
		//		n += (face B facet normal) * (face B surface area) * angle // multiply by angle
		//}

		// VW1
		P1 = XMLoadFloat3(&vw1);
		P2 = XMLoadFloat3(&vw2);
		vDiff = P1 - P2;

		final = XMVector3Normalize(vDiff);

		P1 = XMLoadFloat3(&vw1);
		P2 = XMLoadFloat3(&vw3);
		vDiff2 = P1 - P2;
		final2 = XMVector3Normalize(vDiff2);

		fDotVector = XMVector3Dot(final, final2);
		fDot = XMVectorGetX(fDotVector);
		fDot = MathHelper::Clamp(fDot, -0.99999f, 0.99999f);

		fDot = (float)acos(fDot) / k;

		XMVECTOR vCross = XMVector3Cross(vDiff, vDiff2);
		XMFLOAT3 finalCross;
		XMStoreFloat3(&finalCross, vCross);

		// Set the area of the triangle
		src_v[i].area = .5f * sqrtf(fabsf(powf(finalCross.x, 2.0f)) + fabsf(powf(finalCross.y, 2.0f)) + fabsf(powf(finalCross.z, 2.0f)));
		src_v[i + 1].area = src_v[i].area;
		src_v[i + 2].area = src_v[i].area;

		src_v[i].weight = fabsf(fDot);

		// VW2
		P1 = XMLoadFloat3(&vw2);
		P2 = XMLoadFloat3(&vw1);
		vDiff = P1 - P2;

		final = XMVector3Normalize(vDiff);

		P1 = XMLoadFloat3(&vw2);
		P2 = XMLoadFloat3(&vw3);
		vDiff2 = P1 - P2;
		final2 = XMVector3Normalize(vDiff2);

		fDotVector = XMVector3Dot(final, final2);
		fDot = XMVectorGetX(fDotVector);
		fDot = MathHelper::Clamp(fDot, -0.99999f, 0.99999f);
		fDot = (float)acos(fDot) / k;

		vCross = XMVector3Cross(vDiff, vDiff2);
		finalCross;
		XMStoreFloat3(&finalCross, vCross);
		// src_v[i + 1].area = .05f * sqrtf(fabsf(finalCross.x) + fabsf(finalCross.y) + fabsf(finalCross.z));

		src_v[i + 1].weight = fabsf(fDot);

		// VW3
		P1 = XMLoadFloat3(&vw3);
		P2 = XMLoadFloat3(&vw1);
		vDiff = P1 - P2;

		final = XMVector3Normalize(vDiff);

		P1 = XMLoadFloat3(&vw3);
		P2 = XMLoadFloat3(&vw2);
		vDiff2 = P1 - P2;
		final2 = XMVector3Normalize(vDiff2);

		fDotVector = XMVector3Dot(final, final2);
		fDot = XMVectorGetX(fDotVector);
		fDot = MathHelper::Clamp(fDot, -0.99999f, 0.99999f);
		fDot = (float)acos(fDot) / k;

		vCross = XMVector3Cross(vDiff, vDiff2);
		finalCross;
		XMStoreFloat3(&finalCross, vCross);
		// src_v[i +2].area = .05f * sqrtf(fabsf(finalCross.x) + fabsf(finalCross.y) + fabsf(finalCross.z));
		src_v[i + 2].weight = fabsf(fDot);
	}
}

void ConvertTraingleFan(int fan_cnt) {
	int counter = 0;

	for (int i = fan_cnt; i < cnt; i++) {
		if (counter < 3) {
			temp_v[counter] = src_v[i];
			counter++;
		} else {
			temp_v[counter] = src_v[fan_cnt];
			counter++;
			temp_v[counter] = src_v[i - 1];
			counter++;
			temp_v[counter] = src_v[i];
			counter++;
		}
	}

	int normal = 0;

	for (int i = 0; i < counter; i++) {
		src_v[fan_cnt + i] = temp_v[i];

		if (normal == 2) {
			normal = 0;
			XMFLOAT3 vw1 = { src_v[(fan_cnt + i) - 2].x, src_v[(fan_cnt + i) - 2].y, src_v[(fan_cnt + i) - 2].z };
			XMFLOAT3 vw2 = { src_v[(fan_cnt + i) - 1].x, src_v[(fan_cnt + i) - 1].y, src_v[(fan_cnt + i) - 1].z };
			XMFLOAT3 vw3 = { src_v[(fan_cnt + i)].x, src_v[(fan_cnt + i)].y, src_v[(fan_cnt + i)].z };

			XMVECTOR vDiff = XMLoadFloat3(&vw1) - XMLoadFloat3(&vw2);
			XMVECTOR vDiff2 = XMLoadFloat3(&vw3) - XMLoadFloat3(&vw2);
			XMVECTOR vCross = XMVector3Cross(vDiff, vDiff2);
			XMVECTOR final = XMVector3Normalize(vCross);

			XMFLOAT3 final2;
			XMStoreFloat3(&final2, final);

			float workx = -final2.x;
			float worky = -final2.y;
			float workz = -final2.z;

			src_v[(fan_cnt + i) - 2].nx = workx;
			src_v[(fan_cnt + i) - 2].ny = worky;
			src_v[(fan_cnt + i) - 2].nz = workz;

			src_v[(fan_cnt + i) - 1].nx = workx;
			src_v[(fan_cnt + i) - 1].ny = worky;
			src_v[(fan_cnt + i) - 1].nz = workz;

			src_v[(fan_cnt + i)].nx = workx;
			src_v[(fan_cnt + i)].ny = worky;
			src_v[(fan_cnt + i)].nz = workz;

			CalculateTangentBinormal(src_v[(fan_cnt + i) - 2], src_v[(fan_cnt + i) - 1], src_v[(fan_cnt + i)]);
		} else {
			normal++;
		}
	}
	cnt = fan_cnt + counter;
}

void ConvertTraingleStrip(int fan_cnt) {
	int counter = 0;
	int v = 0;

	// Combine loops to reduce redundant operations
	for (int i = fan_cnt; i < cnt; i++) {
		if (counter < 3) {
			temp_v[counter++] = src_v[i];
		} else {
			if (v == 0) {
				temp_v[counter++] = src_v[i];
				temp_v[counter++] = src_v[i - 1];
				temp_v[counter++] = src_v[i - 2];
				v = 1;
			} else {
				temp_v[counter++] = src_v[i - 2];
				temp_v[counter++] = src_v[i - 1];
				temp_v[counter++] = src_v[i];
				v = 0;
			}
		}
	}

	// Directly process vertices without intermediate copying
	for (int i = 0; i < counter; i += 3) {
		XMFLOAT3 vw1 = { temp_v[i].x, temp_v[i].y, temp_v[i].z };
		XMFLOAT3 vw2 = { temp_v[i + 1].x, temp_v[i + 1].y, temp_v[i + 1].z };
		XMFLOAT3 vw3 = { temp_v[i + 2].x, temp_v[i + 2].y, temp_v[i + 2].z };

		XMVECTOR vDiff = XMLoadFloat3(&vw1) - XMLoadFloat3(&vw2);
		XMVECTOR vDiff2 = XMLoadFloat3(&vw3) - XMLoadFloat3(&vw2);
		XMVECTOR vCross = XMVector3Cross(vDiff, vDiff2);
		XMVECTOR final = XMVector3Normalize(vCross);

		XMFLOAT3 final2;
		XMStoreFloat3(&final2, final);

		float workx = -final2.x;
		float worky = -final2.y;
		float workz = -final2.z;

		for (int j = 0; j < 3; j++) {
			temp_v[i + j].nx = workx;
			temp_v[i + j].ny = worky;
			temp_v[i + j].nz = workz;
		}

		CalculateTangentBinormal(temp_v[i], temp_v[i + 1], temp_v[i + 2]);
	}

	// Update src_v in bulk
	std::copy(temp_v, temp_v + counter, src_v + fan_cnt);
	cnt = fan_cnt + counter;
}

void ConvertQuad(int fan_cnt) {

	int counter = 0;
	int v = 0;
	int quad = 0;

	for (int i = fan_cnt; i < cnt; i++) {

		if (quad >= 3) {

			temp_v[counter].x = src_v[i - 3].x;
			temp_v[counter].y = src_v[i - 3].y;
			temp_v[counter].z = src_v[i - 3].z;
			temp_v[counter].nx = src_v[i - 3].nx;
			temp_v[counter].ny = src_v[i - 3].ny;
			temp_v[counter].nz = src_v[i - 3].nz;
			temp_v[counter].tu = src_v[i - 3].tu;
			temp_v[counter].tv = src_v[i - 3].tv;
			counter++;

			temp_v[counter].x = src_v[i - 2].x;
			temp_v[counter].y = src_v[i - 2].y;
			temp_v[counter].z = src_v[i - 2].z;
			temp_v[counter].nx = src_v[i - 2].nx;
			temp_v[counter].ny = src_v[i - 2].ny;
			temp_v[counter].nz = src_v[i - 2].nz;
			temp_v[counter].tu = src_v[i - 2].tu;
			temp_v[counter].tv = src_v[i - 2].tv;
			counter++;

			temp_v[counter].x = src_v[i - 1].x;
			temp_v[counter].y = src_v[i - 1].y;
			temp_v[counter].z = src_v[i - 1].z;
			temp_v[counter].nx = src_v[i - 1].nx;
			temp_v[counter].ny = src_v[i - 1].ny;
			temp_v[counter].nz = src_v[i - 1].nz;
			temp_v[counter].tu = src_v[i - 1].tu;
			temp_v[counter].tv = src_v[i - 1].tv;
			counter++;

			// 2nd

			temp_v[counter].x = src_v[i].x;
			temp_v[counter].y = src_v[i].y;
			temp_v[counter].z = src_v[i].z;
			temp_v[counter].nx = src_v[i].nx;
			temp_v[counter].ny = src_v[i].ny;
			temp_v[counter].nz = src_v[i].nz;
			temp_v[counter].tu = src_v[i].tu;
			temp_v[counter].tv = src_v[i].tv;
			counter++;

			temp_v[counter].x = src_v[i - 1].x;
			temp_v[counter].y = src_v[i - 1].y;
			temp_v[counter].z = src_v[i - 1].z;
			temp_v[counter].nx = src_v[i - 1].nx;
			temp_v[counter].ny = src_v[i - 1].ny;
			temp_v[counter].nz = src_v[i - 1].nz;
			temp_v[counter].tu = src_v[i - 1].tu;
			temp_v[counter].tv = src_v[i - 1].tv;
			counter++;

			temp_v[counter].x = src_v[i - 2].x;
			temp_v[counter].y = src_v[i - 2].y;
			temp_v[counter].z = src_v[i - 2].z;
			temp_v[counter].nx = src_v[i - 2].nx;
			temp_v[counter].ny = src_v[i - 2].ny;
			temp_v[counter].nz = src_v[i - 2].nz;
			temp_v[counter].tu = src_v[i - 2].tu;
			temp_v[counter].tv = src_v[i - 2].tv;
			counter++;

			quad = 0;

		} else {
			quad++;
		}
	}

	int normal = 0;

	for (int i = 0; i < counter; i++) {
		src_v[fan_cnt + i].x = temp_v[i].x;
		src_v[fan_cnt + i].y = temp_v[i].y;
		src_v[fan_cnt + i].z = temp_v[i].z;

		src_v[fan_cnt + i].nx = temp_v[i].nx;
		src_v[fan_cnt + i].ny = temp_v[i].ny;
		src_v[fan_cnt + i].nz = temp_v[i].nz;

		src_v[fan_cnt + i].tu = temp_v[i].tu;
		src_v[fan_cnt + i].tv = temp_v[i].tv;

		// don't collide with visual box
		src_collide[fan_cnt + i] = 0;

		if (normal == 2) {

			normal = 0;
			XMFLOAT3 vw1, vw2, vw3;

			vw1.x = D3DVAL(src_v[(fan_cnt + i) - 2].x);
			vw1.y = D3DVAL(src_v[(fan_cnt + i) - 2].y);
			vw1.z = D3DVAL(src_v[(fan_cnt + i) - 2].z);

			vw2.x = D3DVAL(src_v[(fan_cnt + i) - 1].x);
			vw2.y = D3DVAL(src_v[(fan_cnt + i) - 1].y);
			vw2.z = D3DVAL(src_v[(fan_cnt + i) - 1].z);

			vw3.x = D3DVAL(src_v[(fan_cnt + i)].x);
			vw3.y = D3DVAL(src_v[(fan_cnt + i)].y);
			vw3.z = D3DVAL(src_v[(fan_cnt + i)].z);

			XMVECTOR vDiff = XMLoadFloat3(&vw1) - XMLoadFloat3(&vw2);
			XMVECTOR vDiff2 = XMLoadFloat3(&vw3) - XMLoadFloat3(&vw2);

			XMVECTOR vCross, final;
			vCross = XMVector3Cross(vDiff, vDiff2);
			final = XMVector3Normalize(vCross);

			XMFLOAT3 final2;
			XMStoreFloat3(&final2, final);

			float workx = (-final2.x);
			float worky = (-final2.y);
			float workz = (-final2.z);

			src_v[(fan_cnt + i) - 2].nx = workx;
			src_v[(fan_cnt + i) - 2].ny = worky;
			src_v[(fan_cnt + i) - 2].nz = workz;

			src_v[(fan_cnt + i) - 1].nx = workx;
			src_v[(fan_cnt + i) - 1].ny = worky;
			src_v[(fan_cnt + i) - 1].nz = workz;

			src_v[(fan_cnt + i)].nx = workx;
			src_v[(fan_cnt + i)].ny = worky;
			src_v[(fan_cnt + i)].nz = workz;

			CalculateTangentBinormal(src_v[(fan_cnt + i) - 2], src_v[(fan_cnt + i) - 1], src_v[(fan_cnt + i)]);

		} else {
			normal++;
		}
	}
	cnt = fan_cnt + counter;
}

int GetNextFrame(int monsterId);

void DrawMonsters() {
	int cullflag = 0;
	int merchantfound = 0;
	BOOL use_player_skins_flag = false;
	int getgunid = 0;
	int monsteron = 0;
	for (int i = 0; i < num_monsters; i++) {

		if (monster_list[i].bIsPlayerValid == TRUE && monster_list[i].bStopAnimating == FALSE ||
		    monster_list[i].bIsPlayerValid == FALSE && monster_list[i].bStopAnimating == FALSE ||
		    monster_list[i].bIsPlayerAlive == FALSE && monster_list[i].bStopAnimating == TRUE

		) {
			cullflag = 0;
			for (int cullloop = 0; cullloop < monstercount; cullloop++) {

				if (monstercull[cullloop] == monster_list[i].monsterid) {
					cullflag = 1;
					break;
				}
			}

			if (cullflag == 1) {
				float wx = monster_list[i].x;
				float wy = monster_list[i].y;
				float wz = monster_list[i].z;

				float qdist = FastDistance(player_list[trueplayernum].x - wx, player_list[trueplayernum].y - wy, player_list[trueplayernum].z - wz);

				// if (strcmp(monster_list[i].rname, "CENTAUR") == 0 && qdist <= 100.0f && merchantfound == 0)
				//{
				//	//just found
				//	merchantfound = 1;
				//	DisplayDialogText("Press SPACE to buy and sell items", 0.0f);
				// }

				XMFLOAT3 work1;
				work1.x = wx;
				work1.y = wy;
				work1.z = wz;

				monsteron = CalculateView(m_vEyePt, work1, cullAngle, true);
				if (monsteron) {

					use_player_skins_flag = 0;

					int nextFrame = GetNextFrame(i);

					PlayerToD3DVertList(monster_list[i].model_id,
					                    monster_list[i].current_frame, monster_list[i].rot_angle,
					                    monster_list[i].skin_tex_id,
					                    USE_PLAYERS_SKIN, monster_list[i].x, monster_list[i].y, monster_list[i].z, nextFrame);

					for (int q = 0; q < countmodellist; q++) {

						if (strcmp(model_list[q].name, monster_list[i].rname) == 0) {
							getgunid = q;
							break;
						}
					}

					if (strcmp(model_list[getgunid].monsterweapon, "NONE") != 0 && monster_list[i].bIsPlayerAlive == TRUE) {

						PlayerToD3DVertList(FindModelID(model_list[getgunid].monsterweapon),
						                    monster_list[i].current_frame, monster_list[i].rot_angle,
						                    FindGunTexture(model_list[getgunid].monsterweapon),
						                    USE_PLAYERS_SKIN, monster_list[i].x, monster_list[i].y, monster_list[i].z, nextFrame);
					}
				} else {
				}
			}
		}
	}
}

int GetNextFrame(int monsterId) {

	int mod_id = monster_list[monsterId].model_id;
	int curr_frame = monster_list[monsterId].current_frame;
	int sequence = monster_list[monsterId].current_sequence;
	int stop_frame = pmdata[mod_id].sequence_stop_frame[monster_list[monsterId].current_sequence];
	int startframe = pmdata[mod_id].sequence_start_frame[monster_list[monsterId].current_sequence];
	int nextFrame = 0;

	if (monster_list[monsterId].bStopAnimating)
		return -1;

	if (curr_frame >= stop_frame) {

		nextFrame = pmdata[mod_id].sequence_start_frame[sequence];

	} else {
		nextFrame = curr_frame + 1;
	}

	return nextFrame;
}

int GetNextFramePlayer() {

	int mod_id = player_list[trueplayernum].model_id;
	int curr_frame = player_list[trueplayernum].current_frame;
	int stop_frame = pmdata[mod_id].sequence_stop_frame[player_list[trueplayernum].current_sequence];
	int startframe = pmdata[mod_id].sequence_start_frame[player_list[trueplayernum].current_sequence];
	int nextFrame = 0;

	if (curr_frame >= stop_frame) {
		int curr_seq = player_list[trueplayernum].current_sequence;
		nextFrame = pmdata[mod_id].sequence_start_frame[curr_seq];
		player_list[trueplayernum].animationdir = 1;
	} else {
		nextFrame = curr_frame + 1;
	}
	return nextFrame;
}

int FindModelID(char *p) {
	int i = 0;

	for (i = 0; i < countmodellist; i++) {
		if (strcmp(model_list[i].name, p) == 0) {
			return model_list[i].model_id;
		}
	}

	for (i = 0; i < num_your_guns; i++) {
		if (strcmp(your_gun[i].gunname, p) == 0) {
			return your_gun[i].model_id;
		}
	}

	return 0;
}

void AddItem(float x, float y, float z, float rot_angle, float monsterid, float monstertexture, float monnum, char modelid[80], char modeltexture[80], int ability) {
	// if (monsterenable == 0)
	// return;

	item_list[itemlistcount].bIsPlayerValid = TRUE;
	item_list[itemlistcount].bIsPlayerAlive = TRUE;
	item_list[itemlistcount].x = x;
	item_list[itemlistcount].y = y;
	item_list[itemlistcount].z = z;
	item_list[itemlistcount].rot_angle = rot_angle;
	item_list[itemlistcount].model_id = (int)monsterid;
	item_list[itemlistcount].skin_tex_id = (int)monstertexture;

	item_list[itemlistcount].current_sequence = 0;
	item_list[itemlistcount].current_frame = 85;

	item_list[itemlistcount].draw_external_wep = FALSE;

	item_list[itemlistcount].monsterid = (int)monnum;

	item_list[itemlistcount].ability = (int)ability;
	item_list[itemlistcount].gold = (int)ability;
	item_list[itemlistcount].firespeed = 0;
	item_list[itemlistcount].attackspeed = 0;
	item_list[itemlistcount].applydamageonce = 0;
	strcpy_s(item_list[itemlistcount].rname, modelid);
	strcpy_s(item_list[itemlistcount].texturename, modeltexture);

	itemlistcount++;
}

void DrawItems(float fElapsedTime) {
	BOOL use_player_skins_flag = false;
	int cullflag = 0;
	int monsteron = 0;
	float rotateSpeed = 100.0f * fElapsedTime;
	const float pickupRiseSpeed = 350.0f; // world units per second

	for (int i = 0; i < itemlistcount; i++) {
		if (item_list[i].bIsPlayerValid == TRUE) {
			bool pickupFxActive = (item_list[i].attackspeed > 0);
			if (pickupFxActive) {
				item_list[i].y = item_list[i].y + (pickupRiseSpeed * fElapsedTime);

				if (item_list[i].y - item_list[i].guny > 150.0f) {
					item_list[i].bIsPlayerValid = FALSE;
					item_list[i].attackspeed = 0;
					continue;
				}
			}

			float qdist = FastDistance(
			    m_vEyePt.x - item_list[i].x,
			    m_vEyePt.y - item_list[i].y,
			    m_vEyePt.z - item_list[i].z);

			if (qdist < 1100.0f) {

				cullflag = 0;
				for (int cullloop = 0; cullloop < monstercount; cullloop++) {
					if (monstercull[cullloop] == item_list[i].monsterid) {
						cullflag = 1;
						break;
					}
				}

				if (item_list[i].monsterid == 9999 && item_list[i].bIsPlayerAlive == TRUE)
					cullflag = 1;

				if (pickupFxActive)
					cullflag = 1;

				if (cullflag == 1) {
					float wx = item_list[i].x;
					float wy = item_list[i].y;
					float wz = item_list[i].z;

					XMFLOAT3 work1;
					work1.x = wx;
					work1.y = wy;
					work1.z = wz;

					monsteron = CalculateView(m_vEyePt, work1, cullAngle, true);

					if (monsteron) {

						use_player_skins_flag = 1;
						int mtexlookup;

						if (strcmp(item_list[i].rname, "COIN") == 0) {

							item_list[i].rot_angle = fixangle(item_list[i].rot_angle, rotateSpeed);

							PlayerToD3DVertList(item_list[i].model_id,
							                    item_list[i].current_frame, item_list[i].rot_angle,
							                    1,
							                    USE_DEFAULT_MODEL_TEX, item_list[i].x, item_list[i].y, item_list[i].z);
						} else if (strcmp(item_list[i].rname, "diamond") == 0) {
							item_list[i].rot_angle = fixangle(item_list[i].rot_angle, rotateSpeed);
							PlayerToD3DVertList(item_list[i].model_id,
							                    item_list[i].current_frame, item_list[i].rot_angle,
							                    1,
							                    USE_DEFAULT_MODEL_TEX, item_list[i].x, item_list[i].y, item_list[i].z);
						} else if (strcmp(item_list[i].rname, "SCROLL-MAGICMISSLE-") == 0 ||
						           strcmp(item_list[i].rname, "SCROLL-FIREBALL-") == 0 ||
						           strcmp(item_list[i].rname, "SCROLL-LIGHTNING-") == 0 ||
						           strcmp(item_list[i].rname, "SCROLL-HEALING-") == 0) {
							if (item_list[i].monsterid == 9999)
								item_list[i].rot_angle = fixangle(item_list[i].rot_angle, rotateSpeed);

							PlayerToD3DVertList(item_list[i].model_id,
							                    item_list[i].current_frame, item_list[i].rot_angle,
							                    1,
							                    USE_DEFAULT_MODEL_TEX, item_list[i].x, item_list[i].y, item_list[i].z);
						} else if (strcmp(item_list[i].rname, "KEY2") == 0) {
							item_list[i].rot_angle = fixangle(item_list[i].rot_angle, rotateSpeed);
							PlayerToD3DVertList(item_list[i].model_id,
							                    item_list[i].current_frame, item_list[i].rot_angle,
							                    1,
							                    USE_DEFAULT_MODEL_TEX, item_list[i].x, item_list[i].y, item_list[i].z);
						} else if (strcmp(item_list[i].rname, "spellbook") == 0) {
							item_list[i].rot_angle = fixangle(item_list[i].rot_angle, rotateSpeed);
							PlayerToD3DVertList(item_list[i].model_id,
							                    item_list[i].current_frame, item_list[i].rot_angle,
							                    1,
							                    USE_DEFAULT_MODEL_TEX, item_list[i].x, item_list[i].y, item_list[i].z);
						} else if (strcmp(item_list[i].rname, "AXE") == 0 ||
						           strcmp(item_list[i].rname, "FLAMESWORD") == 0 ||
						           strcmp(item_list[i].rname, "BASTARDSWORD") == 0 ||
						           strcmp(item_list[i].rname, "BATTLEAXE") == 0 ||
						           strcmp(item_list[i].rname, "ICESWORD") == 0 ||
						           strcmp(item_list[i].rname, "MORNINGSTAR") == 0 ||
						           strcmp(item_list[i].rname, "SPIKEDFLAIL") == 0 ||
						           strcmp(item_list[i].rname, "SPLITSWORD") == 0 ||
						           strcmp(item_list[i].rname, "SUPERFLAMESWORD") == 0 ||
						           strcmp(item_list[i].rname, "LIGHTNINGSWORD") == 0

						) {

							mtexlookup = FindGunTexture(item_list[i].rname);

							PlayerToD3DVertList(item_list[i].model_id,
							                    item_list[i].current_frame, item_list[i].rot_angle,
							                    mtexlookup,
							                    USE_DEFAULT_MODEL_TEX, item_list[i].x, item_list[i].y, item_list[i].z);
						} else if (strcmp(item_list[i].rname, "POTION") == 0) {
							// if (maingameloop)
							item_list[i].rot_angle = fixangle(item_list[i].rot_angle, rotateSpeed);

							PlayerToD3DVertList(item_list[i].model_id,
							                    item_list[i].current_frame, item_list[i].rot_angle,
							                    1,
							                    USE_DEFAULT_MODEL_TEX, item_list[i].x, item_list[i].y, item_list[i].z);
						} else {
							int displayitem = 1;
							if (strcmp(item_list[i].rname, "spiral") == 0) {
								displayitem = 0;
							}
							if (displayitem == 1) {
								PlayerToD3DVertList(item_list[i].model_id,
								                    item_list[i].current_frame, item_list[i].rot_angle,
								                    1,
								                    USE_DEFAULT_MODEL_TEX, item_list[i].x, item_list[i].y, item_list[i].z);
							}
						}
					}
				}
			}
		}
	}
}

void PlayerToD3DIndexedVertList(int pmodel_id, int curr_frame, float angle, int texture_alias, int tex_flag, float xt, float yt, float zt, float fDot2) {
	if (curr_frame >= pmdata[pmodel_id].num_frames)
		curr_frame = 0;

	const float cosine = (float)cos(angle * k);
	const float sine = (float)sin(angle * k);

	const float wx = xt;
	const float wy = yt;
	const float wz = zt;

	int i_count = 0;
	int face_i_count = 0;

	const int num_poly = pmdata[pmodel_id].num_polys_per_frame;

	XMMATRIX rotYaw = XMMATRIX(
	    cosine, 0.0f, sine, 0.0f,
	    0.0f, 1.0f, 0.0f, 0.0f,
	    -sine, 0.0f, cosine, 0.0f,
	    0.0f, 0.0f, 0.0f, 1.0f);

	XMMATRIX rotMat;
	if (fDot2 != 0.0f) {
		const float cp = cosf(fDot2 * k);
		const float sp = sinf(fDot2 * k);
		XMMATRIX rotPitch = XMMATRIX(
		    cp, -sp, 0.0f, 0.0f,
		    sp, cp, 0.0f, 0.0f,
		    0.0f, 0.0f, 1.0f, 0.0f,
		    0.0f, 0.0f, 0.0f, 1.0f);
		rotMat = rotPitch * rotYaw;
	} else {
		rotMat = rotYaw;
	}

	for (int i = 0; i < num_poly; i++) {
		const int num_verts_per_poly = pmdata[pmodel_id].num_verts_per_object[i];
		const int num_faces_per_poly = pmdata[pmodel_id].num_faces_per_object[i];
		const int dwIndexCount = num_faces_per_poly * 3;

		ObjectsToDraw[number_of_polys_per_frame].srcstart = cnt;
		ObjectsToDraw[number_of_polys_per_frame].objectId = -1;

		for (int f = 0; f < num_faces_per_poly; f++) {
			for (int c = 0; c < 3; c++) {
				int local_v = pmdata[pmodel_id].f[face_i_count + c];
				int global_v = i_count + local_v;

				const auto &vert = pmdata[pmodel_id].w[curr_frame][global_v];
				float x = vert.x;
				float z = vert.y;
				float y = vert.z;

				float rx, ry, rz;

				if (fDot2 != 0.0f) {
					float newx = (y * sinf(fDot2 * k) + x * cosf(fDot2 * k));
					float newy = (y * cosf(fDot2 * k) - x * sinf(fDot2 * k));
					float newz = z;

					float yawx = (newx * cosine - newz * sine);
					float yawy = newy;
					float yawz = (newx * sine + newz * cosine);

					rx = yawx + wx;
					ry = yawy + wy;
					rz = yawz + wz;
				} else {
					rx = (x * cosine - z * sine) + wx;
					ry = y + wy;
					rz = (x * sine + z * cosine) + wz;
				}

				float tx = pmdata[pmodel_id].t[face_i_count + c].x * pmdata[pmodel_id].skx;
				float ty = pmdata[pmodel_id].t[face_i_count + c].y * pmdata[pmodel_id].sky;
				ty = 1.0f - ty;

				src_v[cnt].x = D3DVAL(rx);
				src_v[cnt].y = D3DVAL(ry);
				src_v[cnt].z = D3DVAL(rz);
				src_v[cnt].tu = D3DVAL(tx);
				src_v[cnt].tv = D3DVAL(ty);
				src_v[cnt].CastShadow = 1;
				src_collide[cnt] = 1;

				XMVECTOR nVec = XMVectorSet(vert.nx, vert.ny, vert.nz, 0.0f);
				XMVECTOR tVec = XMVectorSet(vert.nmx, vert.nmy, vert.nmz, 0.0f);

				XMVECTOR rotN = XMVector3TransformNormal(nVec, rotMat);
				float nLen = XMVectorGetX(XMVector3Length(rotN));
				rotN = (nLen > 1e-5f) ? XMVectorScale(rotN, 1.0f / nLen) : XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

				XMVECTOR rotT = XMVector3TransformNormal(tVec, rotMat);
				float tLen = XMVectorGetX(XMVector3Length(rotT));
				rotT = (tLen > 1e-5f) ? XMVectorScale(rotT, 1.0f / tLen) : XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

				XMFLOAT3 fn, ft;
				XMStoreFloat3(&fn, rotN);
				XMStoreFloat3(&ft, rotT);

				src_v[cnt].nx = fn.x;
				src_v[cnt].ny = fn.y;
				src_v[cnt].nz = fn.z;

				src_v[cnt].nmx = ft.x;
				src_v[cnt].nmy = ft.y;
				src_v[cnt].nmz = ft.z;

				cnt++;
			}
			face_i_count += 3;
		}

		i_count += num_verts_per_poly;

		int ctext = (tex_flag == USE_PLAYERS_SKIN) ? texture_alias : pmdata[pmodel_id].texture_list[i];

		ObjectsToDraw[number_of_polys_per_frame].vert_index = number_of_polys_per_frame;
		ObjectsToDraw[number_of_polys_per_frame].texture = ctext;
		ObjectsToDraw[number_of_polys_per_frame].vertsperpoly = dwIndexCount;
		ObjectsToDraw[number_of_polys_per_frame].facesperpoly = num_faces_per_poly;

		verts_per_poly[number_of_polys_per_frame] = dwIndexCount;
		faces_per_poly[number_of_polys_per_frame] = num_faces_per_poly;

		dp_command_index_mode[number_of_polys_per_frame] = USE_NON_INDEXED_DP;
		dp_commands[number_of_polys_per_frame] = D3DPT_TRIANGLELIST;

		num_triangles_in_scene += num_faces_per_poly;
		num_verts_in_scene += dwIndexCount;
		num_dp_commands_in_scene++;

		texture_list_buffer[number_of_polys_per_frame] = ctext;

		number_of_polys_per_frame++;
	}
	return;
}

void AddModel(float x, float y, float z, float rot_angle, float monsterid, float monstertexture, float monnum, char modelid[80], char modeltexture[80], int ability) {

	// if (monsterenable == 0)
	//	return;

	player_list2[num_players2].bIsPlayerValid = TRUE;

	player_list2[num_players2].x = x;
	player_list2[num_players2].y = y;
	player_list2[num_players2].z = z;
	player_list2[num_players2].rot_angle = rot_angle;
	player_list2[num_players2].model_id = (int)monsterid;
	player_list2[num_players2].skin_tex_id = (int)monstertexture;

	player_list2[num_players2].current_sequence = 0;
	player_list2[num_players2].ability = ability;
	player_list2[num_players2].draw_external_wep = FALSE;

	player_list2[num_players2].monsterid = (int)monnum;

	strcpy_s(player_list2[num_players2].rname, modelid);
	strcpy_s(player_list2[num_players2].texturename, modeltexture);

	if (strstr(modelid, "switch") != NULL) {

		switchmodify[countswitches].num = num_players2;
		switchmodify[countswitches].objectid = (int)monsterid;
		switchmodify[countswitches].active = 0;

		if (rot_angle >= 0.0f && rot_angle < 90.0f) {
			switchmodify[countswitches].direction = 1;
			switchmodify[countswitches].savelocation = x;
		}
		if (rot_angle >= 90.0f && rot_angle < 180.0f) {
			switchmodify[countswitches].direction = 2;
			switchmodify[countswitches].savelocation = z;
		}
		if (rot_angle >= 180.0f && rot_angle < 270.0f) {
			switchmodify[countswitches].direction = 3;
			switchmodify[countswitches].savelocation = x;
		}
		if (rot_angle >= 270.0f && rot_angle <= 360.0f) {
			switchmodify[countswitches].direction = 4;
			switchmodify[countswitches].savelocation = z;
		}
		switchmodify[countswitches].x = x;
		switchmodify[countswitches].y = y;
		switchmodify[countswitches].z = z;
		switchmodify[countswitches].count = 0;
		countswitches++;
	}

	num_players2++;
}

int FindGunTexture(char *p) {
	int i = 0;

	for (i = 0; i < num_your_guns; i++) {
		if (strcmp(your_gun[i].gunname, p) == 0) {

			return your_gun[i].skin_tex_id;
		}
	}

	return 0;
}

int CycleBitMap(int i) {

	char texname[200];
	char junk[200];
	char junk2[200];

	char *p;
	int talias;
	int tnum;
	int num = 0;
	int count = 0;
	int result;

	talias = i;

	strcpy_s(texname, TexMap[talias].tex_alias_name);

	p = strstr(texname, "@");

	if (p != NULL) {
		if (maingameloop2 == 0)
			return FindTextureAlias(texname);
		strcpy_s(junk, p + 1);

		while (texname[num] != '@')
			junk2[count++] = texname[num++];

		junk2[count] = '\0';

		tnum = atoi(junk);

		tnum++;

		sprintf_s(junk, "%s@%d", junk2, tnum);

		result = FindTextureAlias(junk);
		if (result == -1) {
			sprintf_s(junk, "%s@1", junk2);
			result = FindTextureAlias(junk);
		}

		return result;
	}

	return -1;
}
