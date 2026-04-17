#include "col_local.h"
#include "World.hpp"
#include "GlobalSettings.hpp"
#include <DirectXMath.h>

using namespace DirectX;

// Collision
extern CollisionPacket colPack2;
CollisionPacket collisionPackage;
int collisionRecursionDepth = 0;
static VECTOR firstSlideNormal;
int lastcollide = 0;
int collisioncode = 1;
int objectcollide = 0;
int foundcollisiontrue = 0;
int currentmonstercollisionid = -1;
extern float collisiondist;
extern BOOL foundcollision;

extern CollisionPacket collisionPackage; // Stores all the parameters and returnvalues
extern int collisionRecursionDepth;      // Internal variable tracking the recursion depth

extern CollisionPacket collisionPackage; // Stores all the parameters and returnvalues
extern int collisionRecursionDepth;      // Internal variable tracking the recursion depth

float **triangle_pool; // Stores the pointers to the traingles used for collision detection
int numtriangle;       // Number of traingles in pool
float unitsPerMeter;   // Set this to match application scale..
VECTOR gravity;        // Gravity

void checkTriangle(CollisionPacket *colPackage, VECTOR p1, VECTOR p2, VECTOR p3);

float calcy = 0;
float lastmodely = 0;
BOOL foundcollision = 0;
double nearestDistance = -99;
CollisionPacket colPack2;
extern int g_ob_vert_count;
float collisiondist = 4190.0f;

float mxc[100], myc[100], mzc[100], mwc[100];

CollisionTrace trace; // Output structure telling the application everything about the collision

extern XMFLOAT3 eRadius;

XMFLOAT3 eTest;

void ObjectCollision();
float FastDistance(float fx, float fy, float fz);
void calculate_block_location();
extern int countboundingbox;
extern D3DVERTEX2 boundingbox[2000];
extern int src_collide[MAX_NUM_QUADS];

extern int collideWithBoundingBox;
extern int endc;

struct TCollisionPacket {
	// data about player movement
	XMFLOAT3 velocity;
	XMFLOAT3 sourcePoint;
	// radius of ellipsoid.
	XMFLOAT3 eRadius;
	// data for collision response
	BOOL foundCollision;
	double nearestDistance;                   // nearest distance to hit
	XMFLOAT3 nearestIntersectionPoint;        // on sphere
	XMFLOAT3 nearestPolygonIntersectionPoint; // on polygon
};

void checkCollision() {
	VECTOR P1, P2, P3;    // Temporary variables holding the triangle in R3
	VECTOR eP1, eP2, eP3; // Temporary variables holding the triangle in eSpace

	for (int i = 0; i < numtriangle; i++) // Iterate trough the entire triangle pool
	{
		// I'm sorry for my hard coding, but fill the traingle with the data from the pool
		P1.set(*triangle_pool[i * 9], *triangle_pool[i * 9 + 1], *triangle_pool[i * 9 + 2]);     // First vertex
		P2.set(*triangle_pool[i * 9 + 3], *triangle_pool[i * 9 + 4], *triangle_pool[i * 9 + 5]); // Second vertex
		P3.set(*triangle_pool[i * 9 + 6], *triangle_pool[i * 9 + 7], *triangle_pool[i * 9 + 8]); // Third vertex

		// Transform to eSpace
		eP1 = P1 / collisionPackage.eRadius;
		eP2 = P2 / collisionPackage.eRadius;
		eP3 = P3 / collisionPackage.eRadius;

		checkTriangle(&collisionPackage, eP1, eP2, eP3);
	}
}

XMFLOAT3 collideWithWorld(XMFLOAT3 position, XMFLOAT3 velocity) {

	XMFLOAT3 final;

	float unitsPerMeter = 100.0f;
	float unitScale = unitsPerMeter / 100.0f;
	float veryCloseDistance = 0.005f * unitScale;

	// Max recursion depth for multi-surface sliding
	if (collisionRecursionDepth > 5) {
		collisionRecursionDepth = 0;
		return position;
	}

	VECTOR vel;
	vel.x = (float)velocity.x;
	vel.y = (float)velocity.y;
	vel.z = (float)velocity.z;

	VECTOR pos;
	pos.x = position.x;
	pos.y = position.y;
	pos.z = position.z;

	// Initialize CCD collision packet
	collisionPackage.velocity = vel;
	collisionPackage.normalizedVelocity = vel;
	collisionPackage.normalizedVelocity.normalize();
	collisionPackage.velocityLength = vel.length();
	collisionPackage.basePoint = pos;
	collisionPackage.foundCollision = false;
	collisionPackage.toi = 1.0f;              // CCD: no collision = full travel
	collisionPackage.nearestDistance = 10000000;
	collisionPackage.contactNormal.set(0, 0, 0);
	collisionPackage.realpos.x = pos.x * eRadius.x;
	collisionPackage.realpos.y = pos.y * eRadius.y;
	collisionPackage.realpos.z = pos.z * eRadius.z;

	// Broad-phase + narrow-phase CCD sweep
	ObjectCollision();

	// No collision: advance the full velocity
	if (collisionPackage.foundCollision == false) {
		final.x = pos.x + vel.x;
		final.y = pos.y + vel.y;
		final.z = pos.z + vel.z;
		collisionRecursionDepth = 0;
		return final;
	}

	// --- CCD TOI collision response ---
	float toi = collisionPackage.toi;
	
	// TOI == 0 means already touching / embedded — stop
	if (toi <= 0.0f) {
		final.x = pos.x;
		final.y = pos.y;
		final.z = pos.z;
		collisionRecursionDepth = 0;
		return final;
		if (toi < 0.0f) {
			toi = 0.0f;
		}
	}

	// Advance position to the TOI contact point
	VECTOR newSourcePoint = pos + vel * toi;

	// Use the contact normal computed during CCD detection
	VECTOR contactNormal = collisionPackage.contactNormal;

	// Push out slightly along contact normal to prevent re-collision
	VECTOR pushOut = contactNormal * veryCloseDistance;
	newSourcePoint = newSourcePoint + pushOut;

	// Remaining velocity after the TOI
	float remainingFraction = 1.0f - toi;
	VECTOR remainingVel = vel * remainingFraction;

	// Project remaining velocity onto the sliding plane (remove normal component)
	float normalComponent = remainingVel.dot(contactNormal);
	VECTOR slideVelocity = remainingVel - contactNormal * normalComponent;

	// Anti-ping-pong: handle crease between two collision planes
	if (collisionRecursionDepth == 0) {
		firstSlideNormal = contactNormal;
	} else {
		float normalDot = firstSlideNormal.x * contactNormal.x +
		                  firstSlideNormal.y * contactNormal.y +
		                  firstSlideNormal.z * contactNormal.z;
		if (normalDot < 0.99f) {
			// Two different collision planes — constrain to crease line
			VECTOR crease = firstSlideNormal.cross(contactNormal);
			float creaseLen = crease.length();
			if (creaseLen < 0.001f) {
				// Opposing parallel planes — stop movement
				final.x = newSourcePoint.x;
				final.y = newSourcePoint.y;
				final.z = newSourcePoint.z;
				collisionRecursionDepth = 0;
				return final;
			}
			crease.normalize();
			float d = slideVelocity.x * crease.x +
			          slideVelocity.y * crease.y +
			          slideVelocity.z * crease.z;
			slideVelocity = crease * d;
		}
	}

	// If remaining slide velocity is negligible, stop
	if (slideVelocity.length() < veryCloseDistance) {
		final.x = newSourcePoint.x;
		final.y = newSourcePoint.y;
		final.z = newSourcePoint.z;
		collisionRecursionDepth = 0;
		return final;
	}

	// Recurse with slide velocity
	collisionRecursionDepth++;

	XMFLOAT3 newP;
	XMFLOAT3 newV;

	newP.x = newSourcePoint.x;
	newP.y = newSourcePoint.y;
	newP.z = newSourcePoint.z;

	newV.x = slideVelocity.x;
	newV.y = slideVelocity.y;
	newV.z = slideVelocity.z;

	return collideWithWorld(newP, newV);
}

void ObjectCollision() {

	float centroidx;
	float centroidy;
	float centroidz;

	float qdist = 0;
	int i = 0;
	int count = 0;

	calcy = 0;
	lastmodely = -9999;
	foundcollision = FALSE;
	nearestDistance = -99;
	int vertcount = 0;
	int vertnum = 0;

	colPack2.foundCollision = false;
	colPack2.toi = 1.0f;
	colPack2.nearestDistance = 10000000;

	vertnum = verts_per_poly[vertcount];

	vertnum = 3;

	int test = endc / 3;

	for (i = 0; i < endc; i++) {
		if (count == 0 && src_collide[i] == 1) {

			mxc[0] = src_v[i].x;
			myc[0] = src_v[i].y;
			mzc[0] = src_v[i].z;

			mxc[1] = src_v[i + 1].x;
			myc[1] = src_v[i + 1].y;
			mzc[1] = src_v[i + 1].z;

			mxc[2] = src_v[i + 2].x;
			myc[2] = src_v[i + 2].y;
			mzc[2] = src_v[i + 2].z;

			//  3 2
			//  1 0

			centroidx = (mxc[0] + mxc[1] + mxc[2]) * QVALUE;
			centroidy = (myc[0] + myc[1] + myc[2]) * QVALUE;
			centroidz = (mzc[0] + mzc[1] + mzc[2]) * QVALUE;
			qdist = FastDistance(collisionPackage.realpos.x - centroidx,
			                     collisionPackage.realpos.y - centroidy,
			                     collisionPackage.realpos.z - centroidz);

			if (qdist < collisiondist) {
				calculate_block_location();
			}
			/*if (vertnum == 4)
			{
			    mxc[0] = src_v[i + 1].x;
			    myc[0] = src_v[i + 1].y;
			    mzc[0] = src_v[i + 1].z;

			    mxc[1] = src_v[i + 3].x;
			    myc[1] = src_v[i + 3].y;
			    mzc[1] = src_v[i + 3].z;

			    mxc[2] = src_v[i + 2].x;
			    myc[2] = src_v[i + 2].y;
			    mzc[2] = src_v[i + 2].z;


			    centroidx = (mxc[0] + mxc[1] + mxc[2]) * QVALUE;
			    centroidy = (myc[0] + myc[1] + myc[2]) * QVALUE;
			    ;
			    centroidz = (mzc[0] + mzc[1] + mzc[2]) * QVALUE;
			    qdist = FastDistance(collisionPackage.realpos.x - centroidx,
			        collisionPackage.realpos.y - centroidy,
			        collisionPackage.realpos.z - centroidz);

			    if (qdist < collisiondist)
			        calculate_block_location();
			}*/
		}
		count++;
		if (count > vertnum - 1) {
			count = 0;
			// vertcount++;
			// vertnum = verts_per_poly[vertcount];
			vertnum = 3;
		}
	}

	vertnum = 3;
	count = 0;

	count = 0;
	vertnum = 4;

	for (i = 0; i < countboundingbox; i++) {
		// Stop player missle from hitting bounding box.
		if (collideWithBoundingBox == 0 && boundingbox[i].monster == 1) {
			// player missles should not hit monster bounding box, but should hit 3ds bounding box.
		} else {
			if (count == 0) {
				mxc[0] = boundingbox[i].x;
				myc[0] = boundingbox[i].y;
				mzc[0] = boundingbox[i].z;

				mxc[1] = boundingbox[i + 1].x;
				myc[1] = boundingbox[i + 1].y;
				mzc[1] = boundingbox[i + 1].z;

				mxc[2] = boundingbox[i + 2].x;
				myc[2] = boundingbox[i + 2].y;
				mzc[2] = boundingbox[i + 2].z;

				//  3 2
				//  1 0

				centroidx = (mxc[0] + mxc[1] + mxc[2]) * QVALUE;
				centroidy = (myc[0] + myc[1] + myc[2]) * QVALUE;
				centroidz = (mzc[0] + mzc[1] + mzc[2]) * QVALUE;

				qdist = FastDistance(collisionPackage.realpos.x - centroidx,
				                     collisionPackage.realpos.y - centroidy,
				                     collisionPackage.realpos.z - centroidz);

				if (qdist < collisiondist + 200.0f)
					calculate_block_location();

				if (vertnum == 4) {
					mxc[0] = boundingbox[i + 1].x;
					myc[0] = boundingbox[i + 1].y;
					mzc[0] = boundingbox[i + 1].z;

					mxc[1] = boundingbox[i + 3].x;
					myc[1] = boundingbox[i + 3].y;
					mzc[1] = boundingbox[i + 3].z;

					mxc[2] = boundingbox[i + 2].x;
					myc[2] = boundingbox[i + 2].y;
					mzc[2] = boundingbox[i + 2].z;

					centroidx = (mxc[0] + mxc[1] + mxc[2]) * QVALUE;
					centroidy = (myc[0] + myc[1] + myc[2]) * QVALUE;

					centroidz = (mzc[0] + mzc[1] + mzc[2]) * QVALUE;
					qdist = FastDistance(collisionPackage.realpos.x - centroidx,
					                     collisionPackage.realpos.y - centroidy,
					                     collisionPackage.realpos.z - centroidz);

					if (qdist < collisiondist + 200.0f)
						calculate_block_location();
				}
			}
			count++;
			if (count > vertnum - 1) {
				count = 0;
			}
		}
	}
}

void calculate_block_location() {
	// plane data
	D3DVECTOR p1, p2, p3;

	int nohit = 0;

	p1.x = mxc[0] / eRadius.x;
	p1.y = myc[0] / eRadius.y;
	p1.z = mzc[0] / eRadius.z;

	p2.x = mxc[1] / eRadius.x;
	p2.y = myc[1] / eRadius.y;
	p2.z = mzc[1] / eRadius.z;

	p3.x = mxc[2] / eRadius.x;
	p3.y = myc[2] / eRadius.y;
	p3.z = mzc[2] / eRadius.z;

	// check embedded

	VECTOR pp1;
	VECTOR pp2;
	VECTOR pp3;

	pp1.x = p1.x;
	pp1.y = p1.y;
	pp1.z = p1.z;

	pp2.x = p2.x;
	pp2.y = p2.y;
	pp2.z = p2.z;

	pp3.x = p3.x;
	pp3.y = p3.y;
	pp3.z = p3.z;

	VECTOR vel;
	VECTOR pos;

	checkTriangle(&collisionPackage, pp1, pp2, pp3);

	return;
}

float FastDistance(float fx, float fy, float fz) {

	int temp;
	int x, y, z;

	x = (int)fabs(fx) * 1024;
	y = (int)fabs(fy) * 1024;
	z = (int)fabs(fz) * 1024;
	if (y < x) {
		temp = x;
		x = y;
		y = temp;
	}
	if (z < y) {
		temp = y;
		y = z;
		z = temp;
	}
	if (y < x) {
		temp = x;
		x = y;
		y = temp;
	}
	int dist = (z + 11 * (y >> 5) + (x >> 2));
	return ((float)(dist >> 10));
}
