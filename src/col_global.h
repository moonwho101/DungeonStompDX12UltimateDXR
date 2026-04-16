#pragma warning(disable : 4244)
#include <math.h>

// Collision global header, include this in your application
// Output structure, contains all the information of the collision
struct CollisionTrace {
	bool foundCollision;
	float nearestDistance;
	float toi;                // CCD time of impact [0,1]
	float intersectionPoint[3];
	float contactNormal[3];   // Contact normal at collision point
	float finalPosition[3];
};
