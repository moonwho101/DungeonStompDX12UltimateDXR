#include "col_global.h"
#include <DirectXMath.h>
using namespace DirectX;

class VECTOR {
  public:
	float x, y, z;

	VECTOR();
	VECTOR(float x, float y, float z);

	void set(float x, float y, float z);
	void get(float **floatvector);

	VECTOR operator+(VECTOR const &V1);
	VECTOR operator-(VECTOR const &V1);
	VECTOR operator*(float const &c);
	VECTOR operator*(double const &c);
	VECTOR operator/(float const &c);
	float length();
	float squaredLength();
	void SetLength(float c);
	void normalize();
	float dot(VECTOR &V);
	VECTOR cross(VECTOR &V);
};

class PLANE {
  public:
	float equation[4];
	VECTOR origin;
	VECTOR normal;

	PLANE(VECTOR origin, VECTOR normal);
	PLANE(VECTOR p1, VECTOR p2, VECTOR p3);

	bool isFrontFacingTo(VECTOR direction);
	double signedDistanceTo(VECTOR point);
};

struct CollisionPacket {
	VECTOR R3Position;
	VECTOR R3Velocity;

	float eRadius;

	VECTOR basePoint;
	VECTOR velocity;
	VECTOR normalizedVelocity;
	float velocityLength;

	// CCD TOI results
	bool foundCollision;
	float toi;                // Time of impact [0,1] — fraction of velocity at first contact
	float nearestDistance;    // Legacy: derived from toi * velocityLength for compatibility
	VECTOR intersectionPoint; // Contact point in eSpace
	VECTOR contactNormal;     // Surface normal at contact point

	VECTOR sourcePoint;
	XMFLOAT3 realpos;
};
