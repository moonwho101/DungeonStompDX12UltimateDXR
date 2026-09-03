#ifndef __MIKKTSPACE_H__
#define __MIKKTSPACE_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef int tbool;
#ifndef true
// #define true 1
#endif
#ifndef false
// #define false 0
#endif

typedef struct SMikkTSpaceContext SMikkTSpaceContext;

typedef struct {
	int (*m_getNumFaces)(const SMikkTSpaceContext *pContext);
	int (*m_getNumVerticesOfFace)(const SMikkTSpaceContext *pContext, const int iFace);
	void (*m_getPosition)(const SMikkTSpaceContext *pContext, float fvPosOut[], const int iFace, const int iVert);
	void (*m_getNormal)(const SMikkTSpaceContext *pContext, float fvNormOut[], const int iFace, const int iVert);
	void (*m_getTexCoord)(const SMikkTSpaceContext *pContext, float fvTexcOut[], const int iFace, const int iVert);
	void (*m_setTSpaceBasic)(const SMikkTSpaceContext *pContext, const float fvTangent[], const float fSign, const int iFace, const int iVert);
	void (*m_setTSpace)(const SMikkTSpaceContext *pContext, const float fvTangent[], const float fvBiTangent[], const float fMagS, const float fMagT, const tbool bIsOrientationPreserving, const int iFace, const int iVert);
} SMikkTSpaceInterface;

struct SMikkTSpaceContext {
	SMikkTSpaceInterface *m_pInterface;
	void *m_pUserData;
};

tbool genTangSpaceDefault(const SMikkTSpaceContext *pContext);
tbool genTangSpace(const SMikkTSpaceContext *pContext, const float fAngularThreshold);

#ifdef __cplusplus
}
#endif

#endif
