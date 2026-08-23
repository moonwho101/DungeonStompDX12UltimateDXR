/*
 *  MikkTSpace Implementation
 *  Standard Tangent Space Generator by Morten S. Mikkelsen.
 */

#include "mikktspace.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <vector>
#include <algorithm>

#define MIKK_PI 3.14159265358979323846f

struct SCorner {
	int iFace;
	int iVert;
	float pos[3];
	float norm[3];
	float tex[2];
	float tang[3];
	float bitang[3];
	float magS;
	float magT;
	float sign;
	int iIndex;
};

struct STriangle {
	int iCorner[3];
	float faceNorm[3];
};

struct SWeldGroup {
	std::vector<int> corners;
};

static float GetCosAngle(const float v1[3], const float v2[3]) {
	float fDot = v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
	float fLen1 = sqrtf(v1[0] * v1[0] + v1[1] * v1[1] + v1[2] * v1[2]);
	float fLen2 = sqrtf(v2[0] * v2[0] + v2[1] * v2[1] + v2[2] * v2[2]);
	if (fLen1 > 1e-6f && fLen2 > 1e-6f) {
		float fCos = fDot / (fLen1 * fLen2);
		if (fCos > 1.0f) fCos = 1.0f;
		if (fCos < -1.0f) fCos = -1.0f;
		return fCos;
	}
	return 1.0f;
}

static void Normalize(float v[3]) {
	float fLen = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	if (fLen > 1e-6f) {
		v[0] /= fLen;
		v[1] /= fLen;
		v[2] /= fLen;
	} else {
		v[0] = v[1] = v[2] = 0.0f;
	}
}

tbool genTangSpaceDefault(const SMikkTSpaceContext * pContext) {
	return genTangSpace(pContext, 180.0f);
}

tbool genTangSpace(const SMikkTSpaceContext * pContext, const float fAngularThreshold) {
	if (pContext == NULL || pContext->m_pInterface == NULL)
		return false;

	SMikkTSpaceInterface *pIntf = pContext->m_pInterface;
	if (pIntf->m_getNumFaces == NULL || pIntf->m_getNumVerticesOfFace == NULL ||
	    pIntf->m_getPosition == NULL || pIntf->m_getNormal == NULL ||
	    pIntf->m_getTexCoord == NULL) {
		return false;
	}

	if (pIntf->m_setTSpaceBasic == NULL && pIntf->m_setTSpace == NULL)
		return false;

	int iNrFaces = pIntf->m_getNumFaces(pContext);
	if (iNrFaces <= 0)
		return true;

	const float fThresCos = cosf((fAngularThreshold * MIKK_PI) / 180.0f);

	std::vector<STriangle> triangles(iNrFaces);
	std::vector<SCorner> corners;
	corners.reserve(iNrFaces * 3);

	// 1. Extract face corner data and calculate per-face un-normalized tangents
	for (int f = 0; f < iNrFaces; ++f) {
		int iVerts = pIntf->m_getNumVerticesOfFace(pContext, f);
		if (iVerts != 3) {
			// Currently support 3 vertices per face (triangles)
			continue;
		}

		float pos[3][3], norm[3][3], tex[3][2];
		for (int v = 0; v < 3; ++v) {
			pIntf->m_getPosition(pContext, pos[v], f, v);
			pIntf->m_getNormal(pContext, norm[v], f, v);
			pIntf->m_getTexCoord(pContext, tex[v], f, v);
			Normalize(norm[v]);
		}

		// Calculate face normal
		float dp1[3] = { pos[1][0] - pos[0][0], pos[1][1] - pos[0][1], pos[1][2] - pos[0][2] };
		float dp2[3] = { pos[2][0] - pos[0][0], pos[2][1] - pos[0][1], pos[2][2] - pos[0][2] };

		float faceNorm[3] = {
			dp1[1] * dp2[2] - dp1[2] * dp2[1],
			dp1[2] * dp2[0] - dp1[0] * dp2[2],
			dp1[0] * dp2[1] - dp1[1] * dp2[0]
		};
		Normalize(faceNorm);
		triangles[f].faceNorm[0] = faceNorm[0];
		triangles[f].faceNorm[1] = faceNorm[1];
		triangles[f].faceNorm[2] = faceNorm[2];

		float du1 = tex[1][0] - tex[0][0];
		float dv1 = tex[1][1] - tex[0][1];
		float du2 = tex[2][0] - tex[0][0];
		float dv2 = tex[2][1] - tex[0][1];

		float fDet = du1 * dv2 - dv1 * du2;

		float faceTang[3] = { 0.0f, 0.0f, 0.0f };
		float faceBiTang[3] = { 0.0f, 0.0f, 0.0f };

		if (fabsf(fDet) > 1e-12f) {
			float fInvDet = 1.0f / fDet;
			faceTang[0] = (dv2 * dp1[0] - dv1 * dp2[0]) * fInvDet;
			faceTang[1] = (dv2 * dp1[1] - dv1 * dp2[1]) * fInvDet;
			faceTang[2] = (dv2 * dp1[2] - dv1 * dp2[2]) * fInvDet;

			faceBiTang[0] = (du1 * dp2[0] - du2 * dp1[0]) * fInvDet;
			faceBiTang[1] = (du1 * dp2[1] - du2 * dp1[1]) * fInvDet;
			faceBiTang[2] = (du1 * dp2[2] - du2 * dp1[2]) * fInvDet;
		}

		for (int v = 0; v < 3; ++v) {
			SCorner c;
			c.iFace = f;
			c.iVert = v;
			c.pos[0] = pos[v][0]; c.pos[1] = pos[v][1]; c.pos[2] = pos[v][2];
			c.norm[0] = norm[v][0]; c.norm[1] = norm[v][1]; c.norm[2] = norm[v][2];
			c.tex[0] = tex[v][0]; c.tex[1] = tex[v][1];

			if (fabsf(fDet) <= 1e-12f) {
				// Degenerate UVs fallback: pick arbitrary vector orthogonal to normal
				float vAx[3] = { fabsf(c.norm[0]) > 0.9f ? 0.0f : 1.0f, 0.0f, fabsf(c.norm[2]) > 0.9f ? 0.0f : 1.0f };
				c.tang[0] = vAx[1] * c.norm[2] - vAx[2] * c.norm[1];
				c.tang[1] = vAx[2] * c.norm[0] - vAx[0] * c.norm[2];
				c.tang[2] = vAx[0] * c.norm[1] - vAx[1] * c.norm[0];

				c.bitang[0] = c.norm[1] * c.tang[2] - c.norm[2] * c.tang[1];
				c.bitang[1] = c.norm[2] * c.tang[0] - c.norm[0] * c.tang[2];
				c.bitang[2] = c.norm[0] * c.tang[1] - c.norm[1] * c.tang[0];
			} else {
				c.tang[0] = faceTang[0];
				c.tang[1] = faceTang[1];
				c.tang[2] = faceTang[2];

				c.bitang[0] = faceBiTang[0];
				c.bitang[1] = faceBiTang[1];
				c.bitang[2] = faceBiTang[2];
			}

			c.magS = sqrtf(c.tang[0] * c.tang[0] + c.tang[1] * c.tang[1] + c.tang[2] * c.tang[2]);
			c.magT = sqrtf(c.bitang[0] * c.bitang[0] + c.bitang[1] * c.bitang[1] + c.bitang[2] * c.bitang[2]);
			c.sign = 1.0f;
			c.iIndex = (int)corners.size();

			triangles[f].iCorner[v] = c.iIndex;
			corners.push_back(c);
		}
	}

	int iTotalCorners = (int)corners.size();

	// 2. Spatial welding and smooth group assignment based on position, normal, texcoord, and angular threshold
	const float epsilon = 1e-4f;
	std::vector<int> cornerToWeldGroup(iTotalCorners, -1);
	std::vector<SWeldGroup> weldGroups;

	for (int i = 0; i < iTotalCorners; ++i) {
		if (cornerToWeldGroup[i] != -1) continue;

		SWeldGroup group;
		group.corners.push_back(i);
		int iGroupIdx = (int)weldGroups.size();
		cornerToWeldGroup[i] = iGroupIdx;

		for (int j = i + 1; j < iTotalCorners; ++j) {
			if (cornerToWeldGroup[j] != -1) continue;

			if (fabsf(corners[j].pos[0] - corners[i].pos[0]) < epsilon &&
			    fabsf(corners[j].pos[1] - corners[i].pos[1]) < epsilon &&
			    fabsf(corners[j].pos[2] - corners[i].pos[2]) < epsilon &&
			    fabsf(corners[j].norm[0] - corners[i].norm[0]) < epsilon &&
			    fabsf(corners[j].norm[1] - corners[i].norm[1]) < epsilon &&
			    fabsf(corners[j].norm[2] - corners[i].norm[2]) < epsilon &&
			    fabsf(corners[j].tex[0] - corners[i].tex[0]) < epsilon &&
			    fabsf(corners[j].tex[1] - corners[i].tex[1]) < epsilon) {

				float fCosN = GetCosAngle(triangles[corners[i].iFace].faceNorm, triangles[corners[j].iFace].faceNorm);
				if (fCosN >= fThresCos) {
					group.corners.push_back(j);
					cornerToWeldGroup[j] = iGroupIdx;
				}
			}
		}

		weldGroups.push_back(group);
	}

	// 3. Accumulate tangents across weld groups and orthogonalize
	for (const auto &group : weldGroups) {
		float avgTang[3] = { 0.0f, 0.0f, 0.0f };
		float avgBiTang[3] = { 0.0f, 0.0f, 0.0f };

		for (int idx : group.corners) {
			avgTang[0] += corners[idx].tang[0];
			avgTang[1] += corners[idx].tang[1];
			avgTang[2] += corners[idx].tang[2];

			avgBiTang[0] += corners[idx].bitang[0];
			avgBiTang[1] += corners[idx].bitang[1];
			avgBiTang[2] += corners[idx].bitang[2];
		}

		for (int idx : group.corners) {
			float norm[3] = { corners[idx].norm[0], corners[idx].norm[1], corners[idx].norm[2] };
			float tang[3] = { avgTang[0], avgTang[1], avgTang[2] };
			float bitang[3] = { avgBiTang[0], avgBiTang[1], avgBiTang[2] };

			// Gram-Schmidt orthogonalization: tang = tang - norm * (norm . tang)
			float fDotN = tang[0] * norm[0] + tang[1] * norm[1] + tang[2] * norm[2];
			tang[0] -= norm[0] * fDotN;
			tang[1] -= norm[1] * fDotN;
			tang[2] -= norm[2] * fDotN;
			Normalize(tang);

			// Orientation sign check using cross(norm, tang) . bitang
			float vCross[3] = {
				norm[1] * tang[2] - norm[2] * tang[1],
				norm[2] * tang[0] - norm[0] * tang[2],
				norm[0] * tang[1] - norm[1] * tang[0]
			};
			float fBiDot = vCross[0] * bitang[0] + vCross[1] * bitang[1] + vCross[2] * bitang[2];
			float fSign = (fBiDot < 0.0f) ? -1.0f : 1.0f;

			if (pIntf->m_setTSpaceBasic != NULL) {
				pIntf->m_setTSpaceBasic(pContext, tang, fSign, corners[idx].iFace, corners[idx].iVert);
			}
			if (pIntf->m_setTSpace != NULL) {
				pIntf->m_setTSpace(pContext, tang, bitang, corners[idx].magS, corners[idx].magT, fSign > 0.0f, corners[idx].iFace, corners[idx].iVert);
			}
		}
	}

	return true;
}
