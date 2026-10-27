#include "d3dtypes.h"
#include "LoadWorld.hpp"
#include "world.hpp"
#include "GlobalSettings.hpp"
#include <string.h>
#include "DirectInput.hpp"
#include "GameLogic.hpp"
#include "Missle.hpp"
#include "FrameMove.hpp"
#include "CameraBob.hpp"
#include "col_local.h"

extern CollisionPacket collisionPackage;
extern XMFLOAT3 eTest;
extern int gravityon;
int movement = 1;
void PlayerJump(const FLOAT &fTimeKey);
void FindDoors(const FLOAT &fTimeKey);
void GameTimers(const FLOAT &fTimeKey);
double GameClockSeconds();
bool MovePlayer(const FLOAT &fTimeKey);
void StrifePlayer(FLOAT &fTimeKey, bool addVel);
void PlayerAnimation();
void CheckAngle();

extern CameraBob bobY;
int playercurrentmove = 0;

HRESULT FrameMove(double fTime, FLOAT fTimeKey) {
	float cameradist = 50.0f;
	float dt = fTimeKey;
	if (dt <= 0.0f) dt = 1.0f / 60.0f;
	if (dt > 0.1f) dt = 0.1f;

	GameTimers(dt);
	CheckAngle();
	GetItem();
	MonsterHit();
	FindDoors(dt);

	// Unified 3D Physics & Movement matching Three.js app.js / update.js
	if (gravityon == 0) {
		// Fly mode (Zero gravity)
		savevelocity.x *= powf(0.01f, dt);
		savevelocity.y *= powf(0.01f, dt);
		savevelocity.z *= powf(0.01f, dt);

		float moveSpeed = Controls.bSprint ? 400.0f : 200.0f;

		float pitchRad = fixangle(look_up_ang, 90) * k;
		float yawRad = angy * k;

		XMFLOAT3 camFwd = { sinf(pitchRad) * sinf(yawRad), cosf(pitchRad), sinf(pitchRad) * cosf(yawRad) };
		XMFLOAT3 camRight = { cosf(yawRad), 0.0f, -sinf(yawRad) };

		float inFwd = (Controls.bForward ? 1.0f : 0.0f) - (Controls.bBackward ? 1.0f : 0.0f);
		float inRight = (Controls.bStepRight ? 1.0f : 0.0f) - (Controls.bStepLeft ? 1.0f : 0.0f);

		XMFLOAT3 moveDir = {
			inFwd * camFwd.x + inRight * camRight.x,
			inFwd * camFwd.y,
			inFwd * camFwd.z + inRight * camRight.z
		};

		if (Controls.bUp) moveDir.y += 1.0f;
		if (Controls.bDown) moveDir.y -= 1.0f;

		float lenSq = moveDir.x * moveDir.x + moveDir.y * moveDir.y + moveDir.z * moveDir.z;
		if (lenSq > 0.0001f) {
			float len = sqrtf(lenSq);
			moveDir.x /= len; moveDir.y /= len; moveDir.z /= len;

			savevelocity.x += moveDir.x * moveSpeed * 8.0f * dt;
			savevelocity.y += moveDir.y * moveSpeed * 8.0f * dt;
			savevelocity.z += moveDir.z * moveSpeed * 8.0f * dt;
		}

		m_vEyePt.x += savevelocity.x * dt;
		m_vEyePt.y += savevelocity.y * dt;
		m_vEyePt.z += savevelocity.z * dt;

		lastcollide = 0;
		jump = 0;
		nojumpallow = 0;
		gravitytime = 0.0f;

		playermove = inFwd > 0.0f ? 1 : (inFwd < 0.0f ? 4 : 0);
		playermovestrife = inRight > 0.0f ? 7 : (inRight < 0.0f ? 6 : 0);
	} else {
		// Ground Mode (FPS Walk / Sprint / Jump with Gravity)
		savevelocity.x *= powf(0.001f, dt);
		savevelocity.z *= powf(0.001f, dt);

		savevelocity.y -= 1600.0f * dt; // GRAVITY = 1600.0f

		bool jumpRequested = Controls.bJump || Controls.missle;
		if (jumpRequested && lastcollide == 0) {
			savevelocity.y = 520.0f; // JUMP_SPEED = 520.0f
			jump = 1;
			lastcollide = 1;
			PlayWavSound(SoundID("jump1"), 80);
		}

		float yawRad = angy * k;
		XMFLOAT3 fwd = { sinf(yawRad), 0.0f, cosf(yawRad) };
		XMFLOAT3 right = { cosf(yawRad), 0.0f, -sinf(yawRad) };

		float inFwd = (Controls.bForward ? 1.0f : 0.0f) - (Controls.bBackward ? 1.0f : 0.0f);
		float inRight = (Controls.bStepRight ? 1.0f : 0.0f) - (Controls.bStepLeft ? 1.0f : 0.0f);

		XMFLOAT3 moveDir = {
			inFwd * fwd.x + inRight * right.x,
			0.0f,
			inFwd * fwd.z + inRight * right.z
		};

		float lenSq = moveDir.x * moveDir.x + moveDir.z * moveDir.z;
		float moveSpeed = Controls.bSprint ? 400.0f : 200.0f;

		if (lenSq > 0.0001f) {
			float len = sqrtf(lenSq);
			moveDir.x /= len;
			moveDir.z /= len;

			savevelocity.x += moveDir.x * moveSpeed * 10.0f * dt;
			savevelocity.z += moveDir.z * moveSpeed * 10.0f * dt;
		}

		// Sub-stepped physics integration & 3D capsule collision resolution
		float capsuleRadius = eRadius.x;
		float maxStep = capsuleRadius * 0.5f;
		float speedSq = savevelocity.x * savevelocity.x + savevelocity.y * savevelocity.y + savevelocity.z * savevelocity.z;
		float travel = sqrtf(speedSq) * dt;
		int steps = (int)ceilf(travel / maxStep);
		if (steps < 1) steps = 1;
		if (steps > 16) steps = 16;
		float stepDt = dt / (float)steps;

		bool grounded = false;
		for (int s = 0; s < steps; s++) {
			XMFLOAT3 stepVel = { savevelocity.x * stepDt, savevelocity.y * stepDt, savevelocity.z * stepDt };

			XMFLOAT3 newPos = collideWithWorld(RadiusDivide(m_vEyePt, eRadius), RadiusDivide(stepVel, eRadius));
			newPos = RadiusMultiply(newPos, eRadius);

			if (collisionPackage.foundCollision) {
				XMFLOAT3 norm = { eTest.x, eTest.y, eTest.z };
				float nLen = sqrtf(norm.x * norm.x + norm.y * norm.y + norm.z * norm.z);
				if (nLen > 1e-5f) {
					norm.x /= nLen; norm.y /= nLen; norm.z /= nLen;
					if (norm.y > 0.5f) {
						grounded = true;
					}
					float dot = savevelocity.x * norm.x + savevelocity.y * norm.y + savevelocity.z * norm.z;
					if (dot < 0.0f) {
						savevelocity.x -= dot * norm.x;
						savevelocity.y -= dot * norm.y;
						savevelocity.z -= dot * norm.z;
					}
				}
			}

			m_vEyePt = newPos;
		}

		if (grounded) {
			if (lastcollide == 1 && savevelocity.y <= 0.0f) {
				if (gravitytime >= 0.4f) {
					PlayWavSound(SoundID("jump_land"), 100);
					TriggerLandingDip(20.0f);
				}
			}
			lastcollide = 0;
			jump = 0;
			nojumpallow = 0;
			gravitytime = 0.0f;
			if (savevelocity.y < 0.0f) {
				savevelocity.y = 0.0f;
			}
		} else {
			lastcollide = 1;
			nojumpallow = 1;
			gravitytime += dt;
		}

		playermove = inFwd > 0.0f ? 1 : (inFwd < 0.0f ? 4 : 0);
		playermovestrife = inRight > 0.0f ? 7 : (inRight < 0.0f ? 6 : 0);
	}

	PlayerAnimation();

	savelastmove = playermove;
	savelaststrifemove = playermovestrife;

	if (look_up_ang < -89.3f)
		look_up_ang = -89.3f;

	if (look_up_ang > 89.3f)
		look_up_ang = 89.3f;

	float newangle = fixangle(look_up_ang, 90);

	// Strafe view-roll
	float rollTarget = 0.0f;
	if (playermovestrife == 6)
		rollTarget = 2.0f; // strafe left → lean left
	if (playermovestrife == 7)
		rollTarget = -2.0f; // strafe right → lean right

	float lerpRate = dt * 10.0f;
	if (lerpRate > 1.0f)
		lerpRate = 1.0f;
	look_roll_ang += (rollTarget - look_roll_ang) * lerpRate;

	m_vLookatPt.x = m_vEyePt.x + cameradist * sinf(newangle * k) * sinf(angy * k);
	m_vLookatPt.y = m_vEyePt.y + cameradist * cosf(newangle * k);
	m_vLookatPt.z = m_vEyePt.z + cameradist * sinf(newangle * k) * cosf(angy * k);

	playercurrentmove = playermove;
	playermove = 0;
	playermovestrife = 0;

	EyeTrue = m_vEyePt;
	EyeTrue.y = m_vEyePt.y + cameraheight;

	LookTrue = m_vLookatPt;
	LookTrue.y = m_vLookatPt.y + cameraheight;

	cameraf.x = LookTrue.x;
	cameraf.y = LookTrue.y;
	cameraf.z = LookTrue.z;

	return S_OK;
}

void ResetPlayerMotionState() {
	direction = 0;
	directionlast = 0;
	savelastmove = 0;
	savelaststrifemove = 0;
	playermove = 0;
	playermovestrife = 0;
	movespeed = 0.0f;
	movespeedold = 0.0f;
	movespeedsave = 0.0f;
	movetime = 0.0f;
	currentspeed = 0.0f;
	savevelocity = { 0.0f, 0.0f, 0.0f };
	saveoldvelocity = { 0.0f, 0.0f, 0.0f };
	gravityvector.x = gravityvector.y = gravityvector.z = 0.0f;
	gravityvectorold.x = gravityvectorold.y = gravityvectorold.z = 0.0f;
	totaldist = 0.0f;
	gravitydropcount = 0;
	lastcollide = 0;
	foundcollisiontrue = 0;
	gravitytime = 0.0f;
}

void CheckAngle() {
	if (angy >= 360)
		angy = angy - 360;

	if (angy < 0)
		angy += 360;
}

void PlayerJump(const FLOAT &fTimeKey) {
}

void PlayerAnimation() {
	if (player_list[trueplayernum].current_sequence != 2) {
		if ((playermove == 1 || playermove == 4) && movement == 0) {
			if (savelastmove != playermove && jump == 0) {
				SetPlayerAnimationSequence(trueplayernum, 1);
			}
			movement = 1;
		} else if (playermove == 0 && movement == 1) {
			if (savelastmove != playermove && jump == 0) {
				SetPlayerAnimationSequence(trueplayernum, 0);
			}
			movement = 0;
		}
	}
}

void StrifePlayer(FLOAT &fTimeKey, bool addVel) {
}

bool MovePlayer(const FLOAT &fTimeKey) {
	return false;
}

void FindDoors(const FLOAT &fTimeKey) {
	// Find doors
	for (int q = 0; q < oblist_length; q++) {
		if (strstr(oblist[q].name, "door") != NULL) {
			// door
			float qdist = FastDistance(
			    m_vLookatPt.x - oblist[q].x,
			    m_vLookatPt.y - oblist[q].y,
			    m_vLookatPt.z - oblist[q].z);
			OpenDoor(q, qdist, fTimeKey);
		}
	}
}

void GameTimers(const FLOAT &fTimeKey) {
	static double lastTime = 0.0;
	static float elapsedTime = 0.0f;
	float kAnimationSpeed = 7.0f;

	double currentTime = GameClockSeconds();

	// The game clock restarts at 0 when a demo starts.
	if (currentTime < lastTime)
		lastTime = currentTime;

	// Calculate elapsed time in milliseconds
	elapsedTime = (float)((currentTime - lastTime) * 1000.0);

	// To find the current t we divide the elapsed time by the ratio of 1 second / our anim speed.
	// Since we aren't using 1 second as our t = 1, we need to divide the speed by 1000
	// milliseconds to get our new ratio, which is a 5th of a second.
	float t = elapsedTime / (1000.0f / kAnimationSpeed);
	gametimerAnimation = t;

	// If our elapsed time goes over a 5th of a second, we start over and go to the next key frame
	if (elapsedTime >= (1000.0f / kAnimationSpeed)) {
		// Animation Cycle
		maingameloop3 = 1;
		lastTime = currentTime;
	} else {
		maingameloop3 = 0;
	}

	if (maingameloop3) {
		AnimateCharacters();
	}

	gametimer2 = DSTimer();

	if ((gametimer2 - gametimerlast2) * time_factor >= 60.0f / 1000.0f) {
		// Torch & Teleport Cycle
		maingameloop2 = 1;
		gametimerlast2 = DSTimer();
	} else {
		maingameloop2 = 0;
	}

	gametimer = DSTimer();

	if ((gametimer - gametimerlast) * time_factor >= 40.0f / 1000) {
		// Rotation coins, keys, diamonds
		maingameloop = 1;
		gametimerlast = DSTimer();
	} else {
		maingameloop = 0;
	}

	fTimeKeysave = fTimeKey;
	elapsegametimersave = fTimeKey;
}
