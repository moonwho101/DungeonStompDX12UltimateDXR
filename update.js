import * as THREE from 'three';
import {
    scene, camera, controls, hudEl, disposeOwnedResources,
    emitParticle, createPickupFloatingText, createPickupParticleBurst,
    flashScreen, FLASH_PICKUP_HEALTH, FLASH_PICKUP_XP,
    updatePickupEffects, updateExplosionFlashes, releaseDynamicLight
} from './render.js';
import {
    monsters, doors, chests, levelObjects, activePickups, activeProjectiles,
    PROJECTILE_TYPES, PLAYER_GUNS, unlockedWeapons, spellCounts, WEAPON_DAMAGE,
    getCurrentGunIndex, setCurrentGunIndex, getCurrentGunMesh,
    MD2_ANIMATIONS, MonsterState, damagePlayer, healPlayer, addPlayerXp,
    updatePlayerHud, playSound, playPickupSound, getRandomSound, IMPACT_SOUNDS,
    triggerSpellExplosion, triggerMonsterSpellExplosion, checkPlayerAttackHit,
    updateWeaponMesh, playerWeaponCurrentAnim, playerWeaponAnimTimer,
    setPlayerWeaponAnimTimer, setPlayerWeaponCurrentAnim,
    isWeaponAttacking, setIsWeaponAttacking,
    hasWeaponHitThisSwing, setHasWeaponHitThisSwing, playerWeaponActiveInfluences
} from './logic.js';

// Movement & Physics Constants & State
export const CAPSULE_RADIUS = 28.0;
export const CAPSULE_HEIGHT = 105.0;
export const EYE_HEIGHT = 100.0;
export const GRAVITY = 1600.0;
export const JUMP_SPEED = 520.0;

export const MONSTER_CAPSULE_RADIUS = 24.0;
export const MONSTER_CAPSULE_HEIGHT = 80.0;
export const MONSTER_FOOT_OFFSET = 50.0;
export const STAND_ON_MAX_LIFT = 20.0;

export let isFlyMode = false;
export let isGrounded = false;
export let jumpRequested = false;

export function setIsGrounded(val) {
    isGrounded = val;
}

export function setJumpRequested(val) {
    jumpRequested = val;
}

export function toggleFlyMode(forceState) {
    if (typeof forceState === 'boolean') {
        isFlyMode = forceState;
    } else {
        isFlyMode = !isFlyMode;
    }
    velocity.set(0, 0, 0);
    updateModeDisplay();
}

export function updateModeDisplay() {
    const modeElem = document.getElementById('mode-display');
    if (modeElem) {
        modeElem.textContent = isFlyMode ? 'FLY MODE' : 'FPS (WALK)';
        modeElem.style.color = isFlyMode ? '#00ffff' : '#00ff88';
    }
}

export const velocity = new THREE.Vector3();
export const direction = new THREE.Vector3();
export const cameraQuaternion = new THREE.Quaternion();
export const worldDir = new THREE.Vector3();
export let footstepTimer = 0.0;

export function setFootstepTimer(val) {
    footstepTimer = val;
}

// Spatial Collision Structures
export const levelTriangles = [];
export const modelBoxes = [];
export const collisionGrid = new Map();
export const GRID_CELL_SIZE = 256;

const _triEdge1 = new THREE.Vector3();
const _triEdge2 = new THREE.Vector3();

export function makeCollisionTriangle(ax, ay, az, bx, by, bz, cx, cy, cz) {
    const a = new THREE.Vector3(ax, ay, az);
    const b = new THREE.Vector3(bx, by, bz);
    const c = new THREE.Vector3(cx, cy, cz);
    const n = new THREE.Vector3().crossVectors(_triEdge1.subVectors(b, a), _triEdge2.subVectors(c, a));
    const planar = n.lengthSq() > 1e-10;
    n.normalize();
    return {
        a, b, c, n, planar, stamp: 0,
        minX: Math.min(ax, bx, cx), maxX: Math.max(ax, bx, cx),
        minY: Math.min(ay, by, cy), maxY: Math.max(ay, by, cy),
        minZ: Math.min(az, bz, cz), maxZ: Math.max(az, bz, cz)
    };
}

export function computeTriangleBounds(triangles) {
    const bounds = { minX: Infinity, maxX: -Infinity, minY: Infinity, maxY: -Infinity, minZ: Infinity, maxZ: -Infinity };
    for (const tri of triangles) {
        bounds.minX = Math.min(bounds.minX, tri.minX);
        bounds.maxX = Math.max(bounds.maxX, tri.maxX);
        bounds.minY = Math.min(bounds.minY, tri.minY);
        bounds.maxY = Math.max(bounds.maxY, tri.maxY);
        bounds.minZ = Math.min(bounds.minZ, tri.minZ);
        bounds.maxZ = Math.max(bounds.maxZ, tri.maxZ);
    }
    return bounds;
}

export function getGridKey(cx, cy, cz) {
    return ((cx + 512) * 1024 + (cy + 512)) * 1024 + (cz + 512);
}

export function buildCollisionGrid() {
    collisionGrid.clear();
    for (let i = 0; i < levelTriangles.length; i++) {
        const tri = levelTriangles[i];
        const minCx = Math.floor(tri.minX / GRID_CELL_SIZE);
        const maxCx = Math.floor(tri.maxX / GRID_CELL_SIZE);
        const minCy = Math.floor(tri.minY / GRID_CELL_SIZE);
        const maxCy = Math.floor(tri.maxY / GRID_CELL_SIZE);
        const minCz = Math.floor(tri.minZ / GRID_CELL_SIZE);
        const maxCz = Math.floor(tri.maxZ / GRID_CELL_SIZE);

        for (let cx = minCx; cx <= maxCx; cx++) {
            for (let cy = minCy; cy <= maxCy; cy++) {
                for (let cz = minCz; cz <= maxCz; cz++) {
                    const key = getGridKey(cx, cy, cz);
                    let cell = collisionGrid.get(key);
                    if (!cell) {
                        cell = [];
                        collisionGrid.set(key, cell);
                    }
                    cell.push(tri);
                }
            }
        }
    }
}

const _nearbyTriangles = [];
const _nearbyBoxes = [];
let _queryStamp = 0;

export function gatherNearbyTriangles(minX, maxX, minY, maxY, minZ, maxZ) {
    _nearbyTriangles.length = 0;
    const stamp = ++_queryStamp;

    const minCx = Math.floor(minX / GRID_CELL_SIZE);
    const maxCx = Math.floor(maxX / GRID_CELL_SIZE);
    const minCy = Math.floor(minY / GRID_CELL_SIZE);
    const maxCy = Math.floor(maxY / GRID_CELL_SIZE);
    const minCz = Math.floor(minZ / GRID_CELL_SIZE);
    const maxCz = Math.floor(maxZ / GRID_CELL_SIZE);

    for (let cx = minCx; cx <= maxCx; cx++) {
        for (let cy = minCy; cy <= maxCy; cy++) {
            for (let cz = minCz; cz <= maxCz; cz++) {
                const cell = collisionGrid.get(getGridKey(cx, cy, cz));
                if (!cell) continue;
                for (let i = 0; i < cell.length; i++) {
                    const tri = cell[i];
                    if (tri.stamp === stamp) continue;
                    tri.stamp = stamp;
                    if (tri.maxX < minX || tri.minX > maxX ||
                        tri.maxY < minY || tri.minY > maxY ||
                        tri.maxZ < minZ || tri.minZ > maxZ) {
                        continue;
                    }
                    _nearbyTriangles.push(tri);
                }
            }
        }
    }

    for (let d = 0; d < doors.length; d++) {
        const door = doors[d];
        const b = door.bounds;
        if (b.maxX < minX || b.minX > maxX || b.maxY < minY || b.minY > maxY || b.maxZ < minZ || b.minZ > maxZ) {
            continue;
        }
        const tris = door.triangles;
        for (let i = 0; i < tris.length; i++) {
            const tri = tris[i];
            if (tri.maxX < minX || tri.minX > maxX ||
                tri.maxY < minY || tri.minY > maxY ||
                tri.maxZ < minZ || tri.minZ > maxZ) {
                continue;
            }
            _nearbyTriangles.push(tri);
        }
    }
    return _nearbyTriangles;
}

export function gatherNearbyBoxes(minX, maxX, minY, maxY, minZ, maxZ) {
    _nearbyBoxes.length = 0;
    for (let b = 0; b < modelBoxes.length; b++) {
        const box = modelBoxes[b];
        if (box.isEmpty()) continue;
        if (box.max.x < minX || box.min.x > maxX ||
            box.max.y < minY || box.min.y > maxY ||
            box.max.z < minZ || box.min.z > maxZ) {
            continue;
        }
        _nearbyBoxes.push(box);
    }
    return _nearbyBoxes;
}

const _losRaySample = new THREE.Vector3();
const _losClosest = new THREE.Vector3();

export function hasLineOfSight(fromPos, toPos) {
    const dist = fromPos.distanceTo(toPos);
    if (dist <= 1.0) return true;

    const stepSize = 35.0;
    const steps = Math.max(1, Math.floor(dist / stepSize));

    for (let i = 1; i < steps; i++) {
        _losRaySample.lerpVectors(fromPos, toPos, i / steps);

        const tris = gatherNearbyTriangles(
            _losRaySample.x - 25, _losRaySample.x + 25,
            _losRaySample.y - 25, _losRaySample.y + 25,
            _losRaySample.z - 25, _losRaySample.z + 25
        );
        for (let t = 0; t < tris.length; t++) {
            const tri = tris[t];
            closestPointTrianglePoint(tri.a, tri.b, tri.c, _losRaySample, _losClosest);
            if (_losRaySample.distanceToSquared(_losClosest) <= 20 * 20) {
                return false;
            }
        }

        const boxes = gatherNearbyBoxes(
            _losRaySample.x - 25, _losRaySample.x + 25,
            _losRaySample.y - 25, _losRaySample.y + 25,
            _losRaySample.z - 25, _losRaySample.z + 25
        );
        for (let b = 0; b < boxes.length; b++) {
            if (boxes[b].containsPoint(_losRaySample)) {
                return false;
            }
        }
    }
    return true;
}

// Geometric Collision Functions
const _v10 = new THREE.Vector3();
const _v20 = new THREE.Vector3();
const _vp0 = new THREE.Vector3();
const _vp1 = new THREE.Vector3();
const _vp2 = new THREE.Vector3();
const _tempDiff = new THREE.Vector3();

export function closestPointSegmentPoint(a, b, p, target) {
    const ab = _v10.subVectors(b, a);
    const abLenSq = ab.lengthSq();
    if (abLenSq < 1e-8) {
        return target.copy(a);
    }
    const t = Math.max(0, Math.min(1, _vp0.subVectors(p, a).dot(ab) / abLenSq));
    return target.copy(a).addScaledVector(ab, t);
}

export function closestPointTrianglePoint(v0, v1, v2, p, target) {
    _v10.subVectors(v1, v0);
    _v20.subVectors(v2, v0);
    _vp0.subVectors(p, v0);

    const d1 = _v10.dot(_vp0);
    const d2 = _v20.dot(_vp0);
    if (d1 <= 0 && d2 <= 0) return target.copy(v0);

    _vp1.subVectors(p, v1);
    const d3 = _v10.dot(_vp1);
    const d4 = _v20.dot(_vp1);
    if (d3 >= 0 && d4 <= d3) return target.copy(v1);

    const vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0) {
        const v = d1 / (d1 - d3);
        return target.copy(v0).addScaledVector(_v10, v);
    }

    _vp2.subVectors(p, v2);
    const d5 = _v10.dot(_vp2);
    const d6 = _v20.dot(_vp2);
    if (d6 >= 0 && d5 <= d6) return target.copy(v2);

    const vb = d5 * d2 - d1 * d6;
    if (vb <= 0 && d2 >= 0 && d6 <= 0) {
        const w = d2 / (d2 - d6);
        return target.copy(v0).addScaledVector(_v20, w);
    }

    const va = d3 * d6 - d5 * d4;
    if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) {
        const w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return target.copy(v1).addScaledVector(_tempDiff.subVectors(v2, v1), w);
    }

    const denom = 1 / (va + vb + vc);
    const v = vb * denom;
    const w = vc * denom;
    return target.copy(v0).addScaledVector(_v10, v).addScaledVector(_v20, w);
}

const _u = new THREE.Vector3();
const _v = new THREE.Vector3();
const _w = new THREE.Vector3();

export function closestPointsSegmentSegment(a, b, c, d, target1, target2) {
    _u.subVectors(b, a);
    _v.subVectors(d, c);
    _w.subVectors(a, c);

    const aDot = _u.dot(_u);
    const bDot = _u.dot(_v);
    const cDot = _v.dot(_v);
    const dDot = _u.dot(_w);
    const eDot = _v.dot(_w);
    const D = aDot * cDot - bDot * bDot;

    let sN, sD = D;
    let tN, tD = D;

    if (D < 1e-8) {
        sN = 0.0;
        sD = 1.0;
        tN = eDot;
        tD = cDot;
    } else {
        sN = (bDot * eDot - cDot * dDot);
        tN = (aDot * eDot - bDot * dDot);
        if (sN < 0.0) {
            sN = 0.0;
            tN = eDot;
            tD = cDot;
        } else if (sN > sD) {
            sN = sD;
            tN = eDot + bDot;
            tD = cDot;
        }
    }

    if (tN < 0.0) {
        tN = 0.0;
        if (-dDot < 0.0) sN = 0.0;
        else if (-dDot > aDot) sN = sD;
        else {
            sN = -dDot;
            sD = aDot;
        }
    } else if (tN > tD) {
        tN = tD;
        if ((-dDot + bDot) < 0.0) sN = 0.0;
        else if ((-dDot + bDot) > aDot) sN = sD;
        else {
            sN = (-dDot + bDot);
            sD = aDot;
        }
    }

    const sc = Math.abs(sN) < 1e-8 ? 0.0 : sN / sD;
    const tc = Math.abs(tN) < 1e-8 ? 0.0 : tN / tD;

    target1.copy(a).addScaledVector(_u, sc);
    target2.copy(c).addScaledVector(_v, tc);
}

const _pCapA = new THREE.Vector3();
const _pCapB = new THREE.Vector3();
const _pTriA = new THREE.Vector3();
const _pTriB = new THREE.Vector3();
const _pSeg1 = new THREE.Vector3();
const _pSeg2 = new THREE.Vector3();

const _bestCapP = new THREE.Vector3();
const _bestTriP = new THREE.Vector3();
const _triPlaneN = new THREE.Vector3();
const _triPierceP = new THREE.Vector3();
const _triRes = { capP: _bestCapP, triP: _bestTriP, dist: 0, pierced: false, normal: _triPlaneN };

export function closestPointsCapsuleTriangle(capA, capB, tri) {
    const v0 = tri.a;
    const v1 = tri.b;
    const v2 = tri.c;
    _triRes.pierced = false;

    if (tri.planar) {
        _triPlaneN.copy(tri.n);
        const dA = _triPlaneN.dot(_tempDiff.subVectors(capA, v0));
        const dB = _triPlaneN.dot(_tempDiff.subVectors(capB, v0));
        if (dA * dB < 0) {
            const t = dA / (dA - dB);
            _triPierceP.lerpVectors(capA, capB, t);
            closestPointTrianglePoint(v0, v1, v2, _triPierceP, _pTriA);
            if (_pTriA.distanceToSquared(_triPierceP) < 1e-6) {
                const aNear = Math.abs(dA) <= Math.abs(dB);
                if ((aNear ? dA : dB) > 0) _triPlaneN.negate();
                _bestCapP.copy(_triPierceP);
                _bestTriP.copy(_triPierceP);
                _triRes.dist = -Math.min(Math.abs(dA), Math.abs(dB));
                _triRes.pierced = true;
                return _triRes;
            }
        }
    }

    let minDistSq = Infinity;

    closestPointTrianglePoint(v0, v1, v2, capA, _pTriA);
    closestPointSegmentPoint(capA, capB, _pTriA, _pCapA);
    let dSq = _pCapA.distanceToSquared(_pTriA);
    if (dSq < minDistSq) {
        minDistSq = dSq;
        _bestCapP.copy(_pCapA);
        _bestTriP.copy(_pTriA);
    }

    closestPointTrianglePoint(v0, v1, v2, capB, _pTriB);
    closestPointSegmentPoint(capA, capB, _pTriB, _pCapB);
    dSq = _pCapB.distanceToSquared(_pTriB);
    if (dSq < minDistSq) {
        minDistSq = dSq;
        _bestCapP.copy(_pCapB);
        _bestTriP.copy(_pTriB);
    }

    for (let i = 0; i < 3; i++) {
        const e0 = i === 0 ? v0 : (i === 1 ? v1 : v2);
        const e1 = i === 0 ? v1 : (i === 1 ? v2 : v0);
        closestPointsSegmentSegment(capA, capB, e0, e1, _pSeg1, _pSeg2);
        dSq = _pSeg1.distanceToSquared(_pSeg2);
        if (dSq < minDistSq) {
            minDistSq = dSq;
            _bestCapP.copy(_pSeg1);
            _bestTriP.copy(_pSeg2);
        }
    }

    _triRes.dist = Math.sqrt(minDistSq);
    return _triRes;
}

const _boxRes = { capP: new THREE.Vector3(), boxP: new THREE.Vector3(), dist: 0 };

export function closestPointsCapsuleBox(capA, capB, box) {
    const bx = Math.min(Math.max(capA.x, box.min.x), box.max.x);
    const bz = Math.min(Math.max(capA.z, box.min.z), box.max.z);

    const lo = Math.max(capA.y, box.min.y);
    const hi = Math.min(capB.y, box.max.y);
    let capY, boxY;
    if (lo <= hi) {
        capY = boxY = (lo + hi) * 0.5;
    } else if (capB.y < box.min.y) {
        capY = capB.y;
        boxY = box.min.y;
    } else {
        capY = capA.y;
        boxY = box.max.y;
    }

    _boxRes.capP.set(capA.x, capY, capA.z);
    _boxRes.boxP.set(bx, boxY, bz);
    _boxRes.dist = _boxRes.capP.distanceTo(_boxRes.boxP);
    return _boxRes;
}

const _capA = new THREE.Vector3();
const _capB = new THREE.Vector3();
const _bestNormal = new THREE.Vector3();
const _contactNormal = new THREE.Vector3();

export function cylinderContact(px, pz, baseY, topY, ox, oz, oBaseY, oTopY, sumRadius, standOn) {
    if (baseY >= oTopY || oBaseY >= topY) return 0;
    const dx = px - ox;
    const dz = pz - oz;
    const distSq = dx * dx + dz * dz;
    if (distSq >= sumRadius * sumRadius) return 0;
    if (standOn) {
        const lift = oTopY - baseY;
        if (lift <= STAND_ON_MAX_LIFT) {
            _contactNormal.set(0, 1, 0);
            return lift;
        }
    }
    if (distSq > 1e-8) {
        const dist = Math.sqrt(distSq);
        _contactNormal.set(dx / dist, 0, dz / dist);
        return sumRadius - dist;
    }
    _contactNormal.set(1, 0, 0);
    return sumRadius;
}

export function resolveCapsuleWorld(pos, footOffset, radius, height, vel, selfMonster, monsterList, playerPos) {
    const baseY = pos.y - footOffset;
    const velYPadding = vel ? Math.abs(vel.y * 0.2) : 0;
    const queryMinX = pos.x - radius - 30;
    const queryMaxX = pos.x + radius + 30;
    const queryMinY = baseY - 50 - velYPadding;
    const queryMaxY = baseY + height + 50 + velYPadding;
    const queryMinZ = pos.z - radius - 30;
    const queryMaxZ = pos.z + radius + 30;

    const nearbyTriangles = gatherNearbyTriangles(queryMinX, queryMaxX, queryMinY, queryMaxY, queryMinZ, queryMaxZ);
    const nearbyBoxes = gatherNearbyBoxes(queryMinX, queryMaxX, queryMinY, queryMaxY, queryMinZ, queryMaxZ);

    let grounded = false;
    const MAX_ITERATIONS = 6;
    for (let iter = 0; iter < MAX_ITERATIONS; iter++) {
        const curBaseY = pos.y - footOffset;
        _capA.set(pos.x, curBaseY + radius, pos.z);
        _capB.set(pos.x, curBaseY + height - radius, pos.z);

        let maxPenetration = 0;

        for (let t = 0; t < nearbyTriangles.length; t++) {
            const tri = nearbyTriangles[t];

            const res = closestPointsCapsuleTriangle(_capA, _capB, tri);
            if (res.dist >= radius) continue;
            const pen = radius - res.dist;
            if (pen <= maxPenetration) continue;

            if (res.pierced) {
                _contactNormal.copy(res.normal);
            } else {
                _contactNormal.subVectors(res.capP, res.triP);
                const faceNormal = tri.n;

                if (_contactNormal.lengthSq() > 1e-8) {
                    _contactNormal.normalize();
                } else {
                    _contactNormal.copy(faceNormal);
                    const midY = (_capA.y + _capB.y) * 0.5;
                    if (_contactNormal.x * (pos.x - res.triP.x) + _contactNormal.y * (midY - res.triP.y) +
                        _contactNormal.z * (pos.z - res.triP.z) < 0) {
                        _contactNormal.negate();
                    }
                }

                if (Math.abs(faceNormal.y) > 0.7 && Math.abs(_contactNormal.y) > 0.5) {
                    _contactNormal.set(0, _contactNormal.y > 0 ? 1 : -1, 0);
                }
            }

            maxPenetration = pen;
            _bestNormal.copy(_contactNormal);
        }

        for (let b = 0; b < nearbyBoxes.length; b++) {
            const box = nearbyBoxes[b];

            const res = closestPointsCapsuleBox(_capA, _capB, box);
            if (res.dist >= radius) continue;

            let pen;
            if (res.dist > 1e-4) {
                pen = radius - res.dist;
                _contactNormal.subVectors(res.capP, res.boxP).normalize();
            } else {
                const cx = res.capP.x;
                const cz = res.capP.z;
                const outMinX = cx + radius - box.min.x;
                const outMaxX = box.max.x - (cx - radius);
                const outMinZ = cz + radius - box.min.z;
                const outMaxZ = box.max.z - (cz - radius);
                const outUp = box.max.y - curBaseY;

                pen = outUp;
                _contactNormal.set(0, 1, 0);
                if (outMinX < pen) { pen = outMinX; _contactNormal.set(-1, 0, 0); }
                if (outMaxX < pen) { pen = outMaxX; _contactNormal.set(1, 0, 0); }
                if (outMinZ < pen) { pen = outMinZ; _contactNormal.set(0, 0, -1); }
                if (outMaxZ < pen) { pen = outMaxZ; _contactNormal.set(0, 0, 1); }
            }

            if (pen > maxPenetration) {
                maxPenetration = pen;
                _bestNormal.copy(_contactNormal);
            }
        }

        const topY = curBaseY + height;
        if (monsterList) {
            const sumRadius = radius + MONSTER_CAPSULE_RADIUS;
            for (let m = 0; m < monsterList.length; m++) {
                const other = monsterList[m];
                if (other === selfMonster || other.state === MonsterState.DEATH) continue;
                const oBaseY = other.position.y - MONSTER_FOOT_OFFSET;
                const pen = cylinderContact(pos.x, pos.z, curBaseY, topY,
                    other.position.x, other.position.z, oBaseY, oBaseY + MONSTER_CAPSULE_HEIGHT, sumRadius,
                    !selfMonster);
                if (pen > maxPenetration) {
                    maxPenetration = pen;
                    _bestNormal.copy(_contactNormal);
                }
            }
        }
        if (playerPos) {
            const oBaseY = playerPos.y - EYE_HEIGHT;
            const pen = cylinderContact(pos.x, pos.z, curBaseY, topY,
                playerPos.x, playerPos.z, oBaseY, oBaseY + CAPSULE_HEIGHT, radius + CAPSULE_RADIUS);
            if (pen > maxPenetration) {
                maxPenetration = pen;
                _bestNormal.copy(_contactNormal);
            }
        }

        if (maxPenetration <= 0) break;

        pos.addScaledVector(_bestNormal, maxPenetration + 0.01);

        if (_bestNormal.y > 0.5) {
            grounded = true;
        }

        if (vel) {
            const dotVel = vel.dot(_bestNormal);
            if (dotVel < 0) {
                vel.addScaledVector(_bestNormal, -dotVel);
            }
        }
    }

    return grounded;
}

export function resolveMonsterCapsuleCollisions(monster, otherMonsters = null, playerPos = null) {
    if (!monster || !monster.position) return;
    monster.isGrounded = resolveCapsuleWorld(
        monster.position, MONSTER_FOOT_OFFSET, MONSTER_CAPSULE_RADIUS, MONSTER_CAPSULE_HEIGHT,
        monster.velocity, monster, otherMonsters, playerPos);
}

export function resolveCapsuleCollisions(camPos) {
    isGrounded = resolveCapsuleWorld(
        camPos, EYE_HEIGHT, CAPSULE_RADIUS, CAPSULE_HEIGHT,
        velocity, null, monsters, null);
}

// Tick Updates
const PICKUP_DISTANCE = 70.0;
const PICKUP_DISTANCE_SQ = PICKUP_DISTANCE * PICKUP_DISTANCE;

export function updatePickups(delta, playerPos) {
    updatePickupEffects(delta);

    for (let i = activePickups.length - 1; i >= 0; i--) {
        const pickup = activePickups[i];
        if (!pickup || !pickup.mesh) continue;

        if (pickup.state === 'ACTIVE') {
            const distSq = playerPos.distanceToSquared(pickup.position);

            if (distSq <= PICKUP_DISTANCE_SQ) {
                pickup.state = 'FLOATING';
                pickup.floatTimer = 0.0;

                const nameClean = (pickup.name || '').toUpperCase().trim();
                const def = pickup.def;
                const isHealth = def && def.type === 'health';

                let unlockedWeapon = null;
                for (const gun of PLAYER_GUNS) {
                    if (nameClean.includes(gun.toUpperCase()) || gun.toUpperCase().includes(nameClean)) {
                        unlockedWeapon = gun;
                        break;
                    }
                }
                if (!unlockedWeapon && def && def.type === 'weapon') {
                    unlockedWeapon = pickup.name;
                }

                let popupLabel = def ? def.label : `+${pickup.name}`;
                let colorHex = isHealth ? 0x22c55e : 0xeab308;
                let colorCss = isHealth ? '#4ade80' : '#facc15';

                if (unlockedWeapon) {
                    unlockedWeapons.add(unlockedWeapon);
                    popupLabel = `+${unlockedWeapon}`;
                    colorHex = 0x38bdf8;
                    colorCss = '#38bdf8';
                    const newIndex = PLAYER_GUNS.indexOf(unlockedWeapon);
                    if (newIndex !== -1) {
                        setCurrentGunIndex(newIndex);
                        updateWeaponMesh();
                    }
                } else if (nameClean.includes('SPELLBOOK')) {
                    spellCounts['MAGIC MISSILE'] += 5;
                    spellCounts['FIREBALL'] += 5;
                    spellCounts['LIGHTNING'] += 5;
                    spellCounts['HEALING'] += 5;
                    popupLabel = '+5 ALL SPELLS';
                    colorHex = 0xa855f7;
                    colorCss = '#c084fc';
                    updatePlayerHud();
                } else if (nameClean.includes('SCROLL-MAGICMISSLE') || nameClean.includes('MAGICMISSLE')) {
                    spellCounts['MAGIC MISSILE'] += 5;
                    popupLabel = '+5 MAGIC MISSILE';
                    colorHex = 0x38bdf8;
                    colorCss = '#38bdf8';
                    updatePlayerHud();
                } else if (nameClean.includes('SCROLL-FIREBALL') || nameClean.includes('FIREBALL')) {
                    spellCounts['FIREBALL'] += 5;
                    popupLabel = '+5 FIREBALL';
                    colorHex = 0xff5500;
                    colorCss = '#ff7700';
                    updatePlayerHud();
                } else if (nameClean.includes('SCROLL-LIGHTNING') || nameClean.includes('LIGHTNING')) {
                    spellCounts['LIGHTNING'] += 5;
                    popupLabel = '+5 LIGHTNING';
                    colorHex = 0x00ffff;
                    colorCss = '#00ffff';
                    updatePlayerHud();
                } else if (nameClean.includes('SCROLL-HEALING') || nameClean.includes('HEALING')) {
                    spellCounts['HEALING'] += 5;
                    popupLabel = '+5 HEALING';
                    colorHex = 0x22c55e;
                    colorCss = '#4ade80';
                    updatePlayerHud();
                } else if (isHealth) {
                    healPlayer(def.amount);
                } else if (def && def.amount) {
                    addPlayerXp(def.amount);
                }

                playPickupSound(isHealth);
                createPickupFloatingText(popupLabel, pickup.position, colorCss);
                createPickupParticleBurst(pickup.position, colorHex);

                flashScreen(isHealth ? FLASH_PICKUP_HEALTH : FLASH_PICKUP_XP, 0.5, 150);
            }
        } else if (pickup.state === 'FLOATING') {
            pickup.floatTimer += delta;
            const floatDuration = 0.7;
            const progress = pickup.floatTimer / floatDuration;

            pickup.mesh.position.y = pickup.originalY + (progress * progress) * 350.0;
            pickup.mesh.rotation.y += delta * 12.0;
            pickup.mesh.scale.setScalar(pickup.baseScale * Math.max(0, 1 - progress));

            if (progress >= 1.0) {
                scene.remove(pickup.mesh);
                disposeOwnedResources(pickup.mesh);

                const objIdx = levelObjects.findIndex(o => o.mesh === pickup.mesh);
                if (objIdx !== -1) {
                    levelObjects.splice(objIdx, 1);
                }

                activePickups.splice(i, 1);
            }
        }
    }
}

export function updateDoors(delta) {
    const RISE_SPEED = 200.0;

    for (let i = 0; i < doors.length; i++) {
        const door = doors[i];
        if (!door.isOpening || door.isOpen) continue;

        let dy = RISE_SPEED * delta;
        if (door.currentUp + dy >= door.maxUp) {
            dy = door.maxUp - door.currentUp;
            door.currentUp = door.maxUp;
            door.isOpening = false;
            door.isOpen = true;
        } else {
            door.currentUp += dy;
        }

        if (dy > 0) {
            door.mesh.position.y += dy;

            if (door.levelObj) {
                door.levelObj.position.y += dy;
            }

            if (door.triangles) {
                for (let t = 0; t < door.triangles.length; t++) {
                    const tri = door.triangles[t];
                    tri.a.y += dy;
                    tri.b.y += dy;
                    tri.c.y += dy;
                    tri.minY += dy;
                    tri.maxY += dy;
                }
                door.bounds.minY += dy;
                door.bounds.maxY += dy;
            }
        }
    }
}

const _projNext = new THREE.Vector3();
const _projStep = new THREE.Vector3();
const _projSample = new THREE.Vector3();
const _projClosest = new THREE.Vector3();

export function removeProjectile(index) {
    const proj = activeProjectiles[index];
    scene.remove(proj.group);
    releaseDynamicLight(proj.lightSlot);
    activeProjectiles.splice(index, 1);
}

export function projectileHitsEnvironmentAt(p, radius) {
    const radiusSq = radius * radius;
    const tris = gatherNearbyTriangles(p.x - radius, p.x + radius, p.y - radius, p.y + radius, p.z - radius, p.z + radius);
    for (let t = 0; t < tris.length; t++) {
        const tri = tris[t];
        closestPointTrianglePoint(tri.a, tri.b, tri.c, p, _projClosest);
        if (p.distanceToSquared(_projClosest) <= radiusSq) return true;
    }

    const boxes = gatherNearbyBoxes(p.x - radius, p.x + radius, p.y - radius, p.y + radius, p.z - radius, p.z + radius);
    for (let b = 0; b < boxes.length; b++) {
        if (boxes[b].distanceToPoint(p) <= radius) return true;
    }
    return false;
}

export function monsterProjectileHitsAt(p, radius) {
    if (controls) {
        const playerPos = controls.getObject().position;
        const reach = radius + CAPSULE_RADIUS;
        if (p.distanceToSquared(playerPos) <= reach * reach) {
            return true;
        }
    }
    return projectileHitsEnvironmentAt(p, radius);
}

export function projectileHitsAt(p, radius) {
    const monsterReach = radius + MONSTER_CAPSULE_RADIUS;
    const monsterReachSq = monsterReach * monsterReach;
    for (let m = 0; m < monsters.length; m++) {
        const monster = monsters[m];
        if (monster.state === MonsterState.DEATH || monster.health <= 0) continue;
        if (p.distanceToSquared(monster.position) <= monsterReachSq) return true;
    }

    return projectileHitsEnvironmentAt(p, radius);
}

export function updateProjectiles(delta) {
    updateExplosionFlashes(delta);

    for (let i = activeProjectiles.length - 1; i >= 0; i--) {
        const proj = activeProjectiles[i];
        proj.lifeTimer += delta;

        if (proj.lifeTimer >= proj.maxLife) {
            if (proj.owner === 'monster') {
                triggerMonsterSpellExplosion(proj.position, proj.type, proj.damage, proj.aoeRadius, proj.colorHex);
            } else {
                triggerSpellExplosion(proj.position, proj.type, proj.damage, proj.aoeRadius, proj.colorHex);
            }
            removeProjectile(i);
            continue;
        }

        if (proj.owner === 'monster' && proj.type === 'MAGIC MISSILE' && controls) {
            const playerPos = controls.getObject().position;
            const targetPos = _projSample.copy(playerPos);
            targetPos.y += 20;
            const toPlayer = _projClosest.subVectors(targetPos, proj.position).normalize();
            proj.velocity.lerp(toPlayer.multiplyScalar(PROJECTILE_TYPES['MAGIC MISSILE'].speed * 0.88), delta * 2.5);
        }

        proj.velocity.y += proj.gravity * delta;
        _projStep.copy(proj.velocity).multiplyScalar(delta);
        _projNext.copy(proj.position).add(_projStep);

        const segments = Math.min(8, Math.max(1, Math.ceil(_projStep.length() / proj.radius)));
        let hasExploded = false;
        for (let s = 1; s <= segments; s++) {
            _projSample.copy(proj.position).addScaledVector(_projStep, s / segments);
            const isHit = (proj.owner === 'monster') ? monsterProjectileHitsAt(_projSample, proj.radius) : projectileHitsAt(_projSample, proj.radius);
            if (isHit) {
                hasExploded = true;
                _projNext.copy(_projSample);
                if (proj.owner === 'monster') {
                    triggerMonsterSpellExplosion(_projNext, proj.type, proj.damage, proj.aoeRadius, proj.colorHex);
                } else {
                    triggerSpellExplosion(_projNext, proj.type, proj.damage, proj.aoeRadius, proj.colorHex);
                }
                break;
            }
        }

        if (hasExploded) {
            removeProjectile(i);
            continue;
        }

        proj.position.copy(_projNext);
        proj.group.position.copy(proj.position);
        if (proj.lightSlot) proj.lightSlot.light.position.copy(proj.position);

        if (Math.random() < 0.7) {
            for (let k = 0; k < 4; k++) {
                emitParticle(
                    proj.position.x + (Math.random() - 0.5) * 10,
                    proj.position.y + 10 + (Math.random() - 0.5) * 10,
                    proj.position.z + (Math.random() - 0.5) * 10,
                    (Math.random() - 0.5) * 120,
                    50 + (Math.random() - 0.5) * 120,
                    (Math.random() - 0.5) * 120,
                    proj.colorHex, 8, 0.5);
            }
        }
    }
}

export function updatePlayerWeaponAnimation(delta, inputs) {
    const currentGunMesh = getCurrentGunMesh();
    if (!currentGunMesh) return;

    const geometry = currentGunMesh.geometry;
    if (!geometry || !geometry.morphAttributes || !geometry.morphAttributes.position) return;

    const influences = currentGunMesh.morphTargetInfluences;
    if (!influences) return;

    const { moveForward, moveBackward, moveLeft, moveRight } = inputs || {};

    let targetAnim = MD2_ANIMATIONS.STAND;

    if (isWeaponAttacking) {
        targetAnim = MD2_ANIMATIONS.ATTACK;
    } else if (!isGrounded && !isFlyMode) {
        targetAnim = MD2_ANIMATIONS.JUMP;
    } else if (moveForward || moveBackward || moveLeft || moveRight) {
        targetAnim = MD2_ANIMATIONS.RUN;
    } else {
        targetAnim = MD2_ANIMATIONS.STAND;
    }

    if (playerWeaponCurrentAnim !== targetAnim) {
        setPlayerWeaponCurrentAnim(targetAnim);
        setPlayerWeaponAnimTimer(0.0);
    }

    setPlayerWeaponAnimTimer(playerWeaponAnimTimer + delta);

    const anim = playerWeaponCurrentAnim;
    const totalFrames = anim.end - anim.start + 1;
    const totalDuration = totalFrames / anim.fps;

    if (isWeaponAttacking && playerWeaponCurrentAnim === MD2_ANIMATIONS.ATTACK) {
        if (!hasWeaponHitThisSwing && playerWeaponAnimTimer >= totalDuration * 0.5) {
            setHasWeaponHitThisSwing(true);
            checkPlayerAttackHit();
        }
    }

    if (!anim.loop && playerWeaponAnimTimer >= totalDuration) {
        if (isWeaponAttacking) {
            setIsWeaponAttacking(false);
            setHasWeaponHitThisSwing(false);
            setPlayerWeaponCurrentAnim((moveForward || moveBackward || moveLeft || moveRight)
                ? MD2_ANIMATIONS.RUN
                : MD2_ANIMATIONS.STAND);
            setPlayerWeaponAnimTimer(0.0);
        }
    }

    const curAnim = playerWeaponCurrentAnim;
    const curTotalFrames = curAnim.end - curAnim.start + 1;
    const progress = playerWeaponAnimTimer * curAnim.fps;

    let frameA, frameB, alpha;
    if (curAnim.loop) {
        const frameIdx = progress % curTotalFrames;
        frameA = curAnim.start + Math.floor(frameIdx);
        frameB = curAnim.start + ((Math.floor(frameIdx) + 1) % curTotalFrames);
        alpha = frameIdx - Math.floor(frameIdx);
    } else {
        const frameIdx = Math.min(progress, curTotalFrames - 1);
        frameA = curAnim.start + Math.floor(frameIdx);
        frameB = curAnim.start + Math.min(Math.floor(frameIdx) + 1, curTotalFrames - 1);
        alpha = frameIdx - Math.floor(frameIdx);
    }

    for (const idx of playerWeaponActiveInfluences) {
        if (idx >= 0 && idx < influences.length) {
            influences[idx] = 0;
        }
    }
    playerWeaponActiveInfluences.clear();

    if (frameA >= 0 && frameA < influences.length) {
        influences[frameA] = (1 - alpha);
        playerWeaponActiveInfluences.add(frameA);
    }
    if (frameB >= 0 && frameB < influences.length && alpha > 0.001) {
        influences[frameB] = (influences[frameB] || 0) + alpha;
        playerWeaponActiveInfluences.add(frameB);
    }
}
