#include "col_local.h"
#include "world.hpp"
#include "ImportMD2.hpp"
#include "Import3DS.hpp"
#include "LoadWorld.hpp"

extern int foundcollisiontrue;

bool getLowestRoot(float a, float b, float c, float maxR, float *root) {
	// Check if a solution exists
	float determinant = b * b - 4.0f * a * c;

	// If determinant is negative it means no solutions.
	if (determinant < 0.0f)
		return false;

	// calculate the two roots: (if determinant == 0 then
	// x1==x2 but let's disregard that slight optimization)
	float sqrtD = (float)sqrt(determinant);
	float r1 = (-b - sqrtD) / (2 * a);
	float r2 = (-b + sqrtD) / (2 * a);

	// Sort so x1 <= x2
	if (r1 > r2) {
		float temp = r2;
		r2 = r1;
		r1 = temp;
	}

	// Get lowest root:
	if (r1 > 0 && r1 < maxR) {
		*root = r1;
		return true;
	}

	// It is possible that we want x2 - this can happen
	// if x1 < 0
	if (r2 > 0 && r2 < maxR) {
		*root = r2;
		return true;
	}

	// No (valid) solutions
	return false;
}

typedef unsigned int uint32;
#define in(a) ((uint32 &)a)

bool checkPointInTriangle(VECTOR point, VECTOR pa, VECTOR pb, VECTOR pc) {
	VECTOR e10 = pb - pa;
	VECTOR e20 = pc - pa;

	float a = e10.dot(e10);
	float b = e10.dot(e20);
	float c = e20.dot(e20);
	float ac_bb = (a * c) - (b * b);

	VECTOR vp(point.x - pa.x, point.y - pa.y, point.z - pa.z);

	float d = vp.dot(e10);
	float e = vp.dot(e20);
	float x = (d * c) - (e * b);
	float y = (e * a) - (d * b);
	float z = x + y - ac_bb;
	return ((in(z) & ~(in(x) | in(y))) & 0x80000000);
}

// CCD Time-of-Impact triangle test.
// Assumes: p1, p2, p3 are given in ellipsoid space (unit sphere).
// Computes the earliest TOI in [0,1] when the moving unit sphere
// first contacts the triangle, and records the contact normal.
void checkTriangle(CollisionPacket *colPackage, VECTOR p1, VECTOR p2, VECTOR p3) {
	// Make the plane containing this triangle.
	PLANE trianglePlane(p1, p2, p3);

	// Only test front-facing triangles
	if (trianglePlane.isFrontFacingTo(colPackage->normalizedVelocity)) {
		// --- Compute plane intersection interval [t0, t1] ---
		double t0, t1;
		bool embeddedInPlane = false;

		double signedDistToTrianglePlane = trianglePlane.signedDistanceTo(colPackage->basePoint);
		float normalDotVelocity = trianglePlane.normal.dot(colPackage->velocity);

		if (normalDotVelocity == 0.0f) {
			// Sphere travelling parallel to plane
			if (fabs(signedDistToTrianglePlane) >= 1.0f) {
				return; // No intersection possible
			} else {
				embeddedInPlane = true;
				t0 = 0.0;
				t1 = 1.0;
			}
		} else {
			t0 = (-1.0 - signedDistToTrianglePlane) / normalDotVelocity;
			t1 = (1.0 - signedDistToTrianglePlane) / normalDotVelocity;

			if (t0 > t1) {
				double temp = t1;
				t1 = t0;
				t0 = temp;
			}

			// Both outside [0,1] — no collision this frame
			if (t0 > 1.0f || t1 < 0.0f) {
				return;
			}

			if (t0 < 0.0) t0 = 0.0;
			if (t1 < 0.0) t1 = 0.0;
			if (t0 > 1.0) t0 = 1.0;
			if (t1 > 1.0) t1 = 1.0;
		}

		// --- CCD TOI: find earliest contact ---
		VECTOR collisionPoint;
		VECTOR collisionNormal;
		bool foundCollison = false;
		float t = 1.0;

		// Case 1: Face collision at t0
		if (!embeddedInPlane) {
			VECTOR planeIntersectionPoint = (colPackage->basePoint - trianglePlane.normal) + colPackage->velocity * t0;
			if (checkPointInTriangle(planeIntersectionPoint, p1, p2, p3)) {
				foundCollison = true;
				t = (float)t0;
				collisionPoint = planeIntersectionPoint;
				collisionNormal = trianglePlane.normal; // Face contact normal = plane normal
			}
		}

		// Case 2: Vertex and edge collisions (only if no face hit yet)
		if (foundCollison == false) {
			VECTOR velocity = colPackage->velocity;
			VECTOR base = colPackage->basePoint;
			float velocitySquaredLength = velocity.squaredLength();
			float a, b, c;
			float newT;

			// --- Vertex tests ---
			a = velocitySquaredLength;

			// P1
			b = 2.0f * (velocity.dot(base - p1));
			c = (p1 - base).squaredLength() - 1.0f;
			if (getLowestRoot(a, b, c, t, &newT)) {
				t = newT;
				foundCollison = true;
				collisionPoint = p1;
				// Vertex contact normal: sphere center → vertex direction at TOI
				VECTOR centerAtT = base + velocity * t;
				collisionNormal = centerAtT - p1;
				collisionNormal.normalize();
			}

			// P2
			b = 2.0f * (velocity.dot(base - p2));
			c = (p2 - base).squaredLength() - 1.0f;
			if (getLowestRoot(a, b, c, t, &newT)) {
				t = newT;
				foundCollison = true;
				collisionPoint = p2;
				VECTOR centerAtT = base + velocity * t;
				collisionNormal = centerAtT - p2;
				collisionNormal.normalize();
			}

			// P3
			b = 2.0f * (velocity.dot(base - p3));
			c = (p3 - base).squaredLength() - 1.0f;
			if (getLowestRoot(a, b, c, t, &newT)) {
				t = newT;
				foundCollison = true;
				collisionPoint = p3;
				VECTOR centerAtT = base + velocity * t;
				collisionNormal = centerAtT - p3;
				collisionNormal.normalize();
			}

			// --- Edge tests ---
			// Edge p1 -> p2
			VECTOR edge = p2 - p1;
			VECTOR baseToVertex = p1 - base;
			float edgeSquaredLength = edge.squaredLength();
			float edgeDotVelocity = edge.dot(velocity);
			float edgeDotBaseToVertex = edge.dot(baseToVertex);

			a = edgeSquaredLength * -velocitySquaredLength + edgeDotVelocity * edgeDotVelocity;
			b = edgeSquaredLength * (2 * velocity.dot(baseToVertex)) - 2.0f * edgeDotVelocity * edgeDotBaseToVertex;
			c = edgeSquaredLength * (1 - baseToVertex.squaredLength()) + edgeDotBaseToVertex * edgeDotBaseToVertex;

			if (getLowestRoot(a, b, c, t, &newT)) {
				float f = (edgeDotVelocity * newT - edgeDotBaseToVertex) / edgeSquaredLength;
				if (f >= 0.0f && f <= 1.0f) {
					t = newT;
					foundCollison = true;
					collisionPoint = p1 + edge * f;
					// Edge contact normal: sphere center → closest point on edge
					VECTOR centerAtT = base + velocity * t;
					collisionNormal = centerAtT - collisionPoint;
					collisionNormal.normalize();
				}
			}

			// Edge p2 -> p3
			edge = p3 - p2;
			baseToVertex = p2 - base;
			edgeSquaredLength = edge.squaredLength();
			edgeDotVelocity = edge.dot(velocity);
			edgeDotBaseToVertex = edge.dot(baseToVertex);
			a = edgeSquaredLength * -velocitySquaredLength + edgeDotVelocity * edgeDotVelocity;
			b = edgeSquaredLength * (2 * velocity.dot(baseToVertex)) - 2.0f * edgeDotVelocity * edgeDotBaseToVertex;
			c = edgeSquaredLength * (1 - baseToVertex.squaredLength()) + edgeDotBaseToVertex * edgeDotBaseToVertex;
			if (getLowestRoot(a, b, c, t, &newT)) {
				float f = (edgeDotVelocity * newT - edgeDotBaseToVertex) / edgeSquaredLength;
				if (f >= 0.0f && f <= 1.0f) {
					t = newT;
					foundCollison = true;
					collisionPoint = p2 + edge * f;
					VECTOR centerAtT = base + velocity * t;
					collisionNormal = centerAtT - collisionPoint;
					collisionNormal.normalize();
				}
			}

			// Edge p3 -> p1
			edge = p1 - p3;
			baseToVertex = p3 - base;
			edgeSquaredLength = edge.squaredLength();
			edgeDotVelocity = edge.dot(velocity);
			edgeDotBaseToVertex = edge.dot(baseToVertex);
			a = edgeSquaredLength * -velocitySquaredLength + edgeDotVelocity * edgeDotVelocity;
			b = edgeSquaredLength * (2 * velocity.dot(baseToVertex)) - 2.0f * edgeDotVelocity * edgeDotBaseToVertex;
			c = edgeSquaredLength * (1 - baseToVertex.squaredLength()) + edgeDotBaseToVertex * edgeDotBaseToVertex;
			if (getLowestRoot(a, b, c, t, &newT)) {
				float f = (edgeDotVelocity * newT - edgeDotBaseToVertex) / edgeSquaredLength;
				if (f >= 0.0f && f <= 1.0f) {
					t = newT;
					foundCollison = true;
					collisionPoint = p3 + edge * f;
					VECTOR centerAtT = base + velocity * t;
					collisionNormal = centerAtT - collisionPoint;
					collisionNormal.normalize();
				}
			}
		}

		// --- Record earliest TOI collision ---
		if (foundCollison == true) {
			// CCD: compare by TOI directly instead of distance
			if (t < colPackage->toi) {
				foundcollisiontrue = 1;
				colPackage->toi = t;
				colPackage->nearestDistance = t * colPackage->velocity.length();
				colPackage->intersectionPoint = collisionPoint;
				colPackage->contactNormal = collisionNormal;
				colPackage->foundCollision = true;
			}
		}
	} // if not backface
}
