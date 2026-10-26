import * as THREE from 'three';
import {
    scene, controls, disposeOwnedResources,
    emitParticle, createPickupFloatingText, createPickupParticleBurst,
    flashScreen, FLASH_PICKUP_HEALTH, FLASH_PICKUP_XP,
    updatePickupEffects, updateExplosionFlashes, releaseDynamicLight
} from './render.js';
import { monsters, doors, MonsterState } from './gameState.js';
import {
    velocity, CAPSULE_RADIUS, MONSTER_CAPSULE_RADIUS,
    gatherNearbyTriangles, gatherNearbyBoxes, closestPointTrianglePoint
} from './collision.js';
import {
    levelObjects, activePickups, activeProjectiles,
    PROJECTILE_TYPES, PLAYER_GUNS, unlockedWeapons, spellCounts,
    setCurrentGunIndex, getCurrentGunMesh,
    MD2_ANIMATIONS, healPlayer, addPlayerXp,
    updatePlayerHud, playPickupSound,
    triggerSpellExplosion, triggerMonsterSpellExplosion, checkPlayerAttackHit,
    updateWeaponMesh, playerWeaponCurrentAnim, playerWeaponAnimTimer,
    setPlayerWeaponAnimTimer, setPlayerWeaponCurrentAnim,
    isWeaponAttacking, setIsWeaponAttacking,
    hasWeaponHitThisSwing, setHasWeaponHitThisSwing, playerWeaponActiveInfluences
} from './logic.js';

// Player Movement State
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

export const direction = new THREE.Vector3();
export const cameraQuaternion = new THREE.Quaternion();
export const worldDir = new THREE.Vector3();
export let footstepTimer = 0.0;

export function setFootstepTimer(val) {
    footstepTimer = val;
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

                let popupLabel = def ? def.label : `+${pickup.name}`;
                let colorHex = isHealth ? 0x22c55e : 0xeab308;
                let colorCss = isHealth ? '#4ade80' : '#facc15';

                // Check spell scrolls / spellbook first
                let spellScrollName = null;
                if (nameClean.includes('SPELLBOOK')) {
                    spellCounts['MAGIC MISSILE'] += 5;
                    spellCounts['FIREBALL'] += 5;
                    spellCounts['LIGHTNING'] += 5;
                    spellCounts['HEALING'] += 5;
                    popupLabel = '+5 ALL SPELLS';
                    colorHex = 0xa855f7;
                    colorCss = '#c084fc';
                    updatePlayerHud();
                } else if (nameClean.includes('SCROLL-MAGICMISSLE') || nameClean.includes('MAGICMISSLE') || nameClean === 'MAGIC MISSILE') {
                    spellScrollName = 'MAGIC MISSILE';
                    colorHex = 0x38bdf8;
                    colorCss = '#38bdf8';
                } else if (nameClean.includes('SCROLL-FIREBALL') || nameClean.includes('FIREBALL')) {
                    spellScrollName = 'FIREBALL';
                    colorHex = 0xff5500;
                    colorCss = '#ff7700';
                } else if (nameClean.includes('SCROLL-LIGHTNING') || nameClean.includes('LIGHTNING')) {
                    spellScrollName = 'LIGHTNING';
                    colorHex = 0x00ffff;
                    colorCss = '#00ffff';
                } else if (nameClean.includes('SCROLL-HEALING') || nameClean.includes('HEALING')) {
                    spellScrollName = 'HEALING';
                    colorHex = 0x22c55e;
                    colorCss = '#4ade80';
                }

                if (spellScrollName) {
                    const prevCount = spellCounts[spellScrollName] || 0;
                    spellCounts[spellScrollName] = prevCount + 5;
                    popupLabel = `+5 ${spellScrollName}`;
                    updatePlayerHud();

                    // Only switch to spell if its slot was at 0
                    if (prevCount === 0) {
                        const newIndex = PLAYER_GUNS.indexOf(spellScrollName);
                        if (newIndex !== -1) {
                            setCurrentGunIndex(newIndex);
                            updateWeaponMesh();
                        }
                    }
                } else if (!nameClean.includes('SPELLBOOK')) {
                    // Check physical weapon pickups (excluding spell slots)
                    let unlockedWeapon = null;
                    for (const gun of PLAYER_GUNS) {
                        if (spellCounts[gun] !== undefined) continue;

                        const gunUpper = gun.toUpperCase();
                        if (gunUpper === nameClean) {
                            unlockedWeapon = gun;
                            break;
                        }
                        if (nameClean && (nameClean.includes(gunUpper) || gunUpper.includes(nameClean))) {
                            if (!unlockedWeapon || gun.length > unlockedWeapon.length) {
                                unlockedWeapon = gun;
                            }
                        }
                    }
                    if (!unlockedWeapon && def && def.type === 'weapon') {
                        unlockedWeapon = pickup.name;
                    }

                    if (unlockedWeapon) {
                        const wasUnlocked = unlockedWeapons.has(unlockedWeapon);
                        unlockedWeapons.add(unlockedWeapon);
                        popupLabel = `+${unlockedWeapon}`;
                        colorHex = 0x38bdf8;
                        colorCss = '#38bdf8';

                        // Only switch to weapon if it's the first time the slot is activated
                        if (!wasUnlocked) {
                            const newIndex = PLAYER_GUNS.indexOf(unlockedWeapon);
                            if (newIndex !== -1) {
                                setCurrentGunIndex(newIndex);
                                updateWeaponMesh();
                            }
                        }
                    } else if (isHealth) {
                        healPlayer(def.amount);
                    } else if (def && def.amount) {
                        addPlayerXp(def.amount);
                    }
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
