import * as THREE from 'three';
import { PointerLockControls } from 'three/addons/controls/PointerLockControls.js';
import {
    initRenderer, scene, camera, renderer, controls, setControls,
    hudEl, updateLighting, updateDistanceCulling, onWindowResize,
    normalMapsEnabled, setNormalMapsEnabled, textureManager,
    createPickupFloatingText
} from './render.js';
import {
    velocity, direction, cameraQuaternion, worldDir, isFlyMode,
    toggleFlyMode, updateModeDisplay, isGrounded, setIsGrounded,
    jumpRequested, setJumpRequested, footstepTimer, setFootstepTimer,
    GRAVITY, JUMP_SPEED, CAPSULE_RADIUS, resolveCapsuleCollisions,
    resolveMonsterCapsuleCollisions, modelBoxes, updateDoors,
    updatePickups, updateProjectiles, updatePlayerWeaponAnimation
} from './update.js';
import {
    MonsterState, MD2_ANIMATIONS, DEFAULT_MONSTER_CONFIG, MONSTER_CONFIGS,
    getMonsterConfig, Monster, monsters, TREASURE_DROPS, PICKUP_DEFINITIONS,
    getPickupDefinition, playerStats, damagePlayer, healPlayer, addPlayerXp,
    awardPlayerXp, updatePlayerHud, isInvincible, setIsInvincible,
    playerDead, playerDeathTimer, setPlayerDeathTimer, respawnPlayer,
    PLAYER_GUNS, unlockedWeapons, spellCounts, WEAPON_DAMAGE,
    getCurrentGunIndex, cycleWeapon, updateWeaponMesh, triggerWeaponAttack,
    openNearbyDoor, SWISH_SOUNDS, IMPACT_SOUNDS, PLAYER_IMPACT_SOUNDS,
    playSound, getRandomSound, preloadSounds, resumeAudio,
    loadGameWorld, levelObjects, activePickups, doors,
    playerWeaponCurrentAnim
} from './logic.js';

// Re-export public interfaces for backwards compatibility and external modules
export {
    MonsterState, MD2_ANIMATIONS, DEFAULT_MONSTER_CONFIG, MONSTER_CONFIGS,
    getMonsterConfig, Monster, TREASURE_DROPS, PICKUP_DEFINITIONS,
    getPickupDefinition, playerStats, damagePlayer, healPlayer,
    addPlayerXp, awardPlayerXp, updatePlayerHud, PLAYER_GUNS,
    unlockedWeapons, spellCounts, SWISH_SOUNDS, IMPACT_SOUNDS,
    PLAYER_IMPACT_SOUNDS, playSound, getRandomSound
};

let moveForward = false;
let moveBackward = false;
let moveLeft = false;
let moveRight = false;
let moveUp = false;
let moveDown = false;
let isSprinting = false;

let prevTime = performance.now();
const forwardVector = new THREE.Vector3();
const rightVector = new THREE.Vector3();
let lastPosDisplayTime = 0;

async function init() {
    const container = document.getElementById('canvas-container');

    initRenderer(container);

    const newControls = new PointerLockControls(camera, document.body);
    setControls(newControls);

    const instructions = document.getElementById('instructions');
    instructions.addEventListener('click', () => {
        resumeAudio();
        newControls.lock();
    });

    newControls.addEventListener('lock', () => {
        instructions.style.display = 'none';
    });

    const releaseInputs = () => {
        moveForward = moveBackward = moveLeft = moveRight = moveUp = moveDown = false;
        isSprinting = false;
    };

    newControls.addEventListener('unlock', () => {
        instructions.style.display = 'block';
        releaseInputs();
    });
    window.addEventListener('blur', releaseInputs);

    scene.add(newControls.getObject());

    const onKeyDown = function (event) {
        if (event.repeat || playerDead) return;
        switch (event.code) {
            case 'ArrowUp':
            case 'KeyW':
                moveForward = true;
                break;
            case 'ArrowLeft':
            case 'KeyA':
                moveLeft = true;
                break;
            case 'ArrowDown':
            case 'KeyS':
                moveBackward = true;
                break;
            case 'ArrowRight':
            case 'KeyD':
                moveRight = true;
                break;
            case 'Space':
                if (isFlyMode) {
                    moveUp = true;
                } else {
                    setJumpRequested(true);
                }
                break;
            case 'KeyC':
            case 'ControlLeft':
            case 'ControlRight':
                if (isFlyMode) {
                    moveDown = true;
                }
                break;
            case 'KeyF':
            case 'KeyV':
                toggleFlyMode();
                break;
            case 'ShiftLeft':
            case 'ShiftRight':
                isSprinting = true;
                break;
            case 'KeyQ':
                cycleWeapon(1);
                break;
            case 'KeyZ':
                cycleWeapon(-1);
                break;
            case 'KeyE':
                openNearbyDoor();
                break;
            case 'KeyN':
                setNormalMapsEnabled(!normalMapsEnabled);
                break;
            case 'KeyK':
            case 'KeyP':
                PLAYER_GUNS.forEach(g => unlockedWeapons.add(g));
                for (const spell in spellCounts) {
                    spellCounts[spell] = Math.max(99, spellCounts[spell] + 50);
                }
                updatePlayerHud();
                if (newControls && newControls.getObject()) {
                    createPickupFloatingText('ALL WEAPONS & SPELLS UNLOCKED', newControls.getObject().position, '#facc15');
                }
                playSound('treasure.wav', 0.8);
                break;
            case 'KeyI':
                setIsInvincible(!isInvincible);
                if (newControls && newControls.getObject()) {
                    const statusText = isInvincible ? 'GOD MODE: ON (INVINCIBLE)' : 'GOD MODE: OFF';
                    const statusColor = isInvincible ? '#22c55e' : '#ef4444';
                    createPickupFloatingText(statusText, newControls.getObject().position, statusColor);
                }
                playSound('treasure.wav', 0.8);
                break;
        }
    };

    const onKeyUp = function (event) {
        switch (event.code) {
            case 'ArrowUp':
            case 'KeyW':
                moveForward = false;
                break;
            case 'ArrowLeft':
            case 'KeyA':
                moveLeft = false;
                break;
            case 'ArrowDown':
            case 'KeyS':
                moveBackward = false;
                break;
            case 'ArrowRight':
            case 'KeyD':
                moveRight = false;
                break;
            case 'Space':
                moveUp = false;
                break;
            case 'KeyC':
            case 'ControlLeft':
            case 'ControlRight':
                moveDown = false;
                break;
            case 'ShiftLeft':
            case 'ShiftRight':
                isSprinting = false;
                break;
        }
    };

    document.addEventListener('keydown', onKeyDown);
    document.addEventListener('keyup', onKeyUp);

    document.addEventListener('mousedown', (event) => {
        if (event.button === 0 && newControls && newControls.isLocked) {
            triggerWeaponAttack();
        }
    });

    window.addEventListener('resize', onWindowResize);

    try {
        await loadGameWorld(textureManager);
    } catch (e) {
        console.error('Failed to load the dungeon:', e);
        hudEl('instructions-prompt').textContent = 'Failed to load the dungeon. Check the console for details.';
        return;
    }

    await updateWeaponMesh();

    preloadSounds([
        ...SWISH_SOUNDS, ...IMPACT_SOUNDS, ...PLAYER_IMPACT_SOUNDS,
        'spell1.wav', 'spell2.wav', 'spell3.wav', 'fireball.wav', 'explod1.wav', 'explod2.wav',
        'walk.wav', 'die.wav', 'death1.wav', 'death2.wav', 'death3.wav', 'death4.wav',
        ...monsters.map(m => m.config.sound)
    ]);

    window.camera = camera;
    window.renderer = renderer;
    window.controls = newControls;
    window.scene = scene;
    window.monsters = monsters;
    window.modelBoxes = modelBoxes;
    window.resolveMonsterCapsuleCollisions = resolveMonsterCapsuleCollisions;
    window.doors = doors;
    window.openNearbyDoor = openNearbyDoor;
    window.activePickups = activePickups;
    window.playerStats = playerStats;
    window.damagePlayer = damagePlayer;
    window.playSound = playSound;
    window.SWISH_SOUNDS = SWISH_SOUNDS;
    window.IMPACT_SOUNDS = IMPACT_SOUNDS;
    window.PLAYER_IMPACT_SOUNDS = PLAYER_IMPACT_SOUNDS;
    Object.defineProperty(window, 'playerWeaponCurrentAnim', {
        get: () => playerWeaponCurrentAnim
    });

    prevTime = performance.now();
    hudEl('instructions-prompt').textContent = 'Click anywhere to Start & Lock Pointer';
    animate();
}

function animate() {
    requestAnimationFrame(animate);

    const time = performance.now();
    let delta = (time - prevTime) / 1000;
    if (delta > 0.1) delta = 0.1;

    const moveSpeed = isSprinting ? 400.0 : 200.0;

    if (isFlyMode) {
        velocity.x *= Math.pow(0.01, delta);
        velocity.y *= Math.pow(0.01, delta);
        velocity.z *= Math.pow(0.01, delta);

        direction.set(
            Number(moveRight) - Number(moveLeft),
            0,
            Number(moveBackward) - Number(moveForward)
        );

        if (direction.lengthSq() > 0) {
            direction.normalize();
            camera.getWorldQuaternion(cameraQuaternion);
            worldDir.copy(direction).applyQuaternion(cameraQuaternion);
            velocity.x += worldDir.x * moveSpeed * 8.0 * delta;
            velocity.y += worldDir.y * moveSpeed * 8.0 * delta;
            velocity.z += worldDir.z * moveSpeed * 8.0 * delta;
        }

        if (moveUp) {
            velocity.y += moveSpeed * 8.0 * delta;
        }
        if (moveDown) {
            velocity.y -= moveSpeed * 8.0 * delta;
        }

        controls.getObject().position.x += velocity.x * delta;
        controls.getObject().position.y += velocity.y * delta;
        controls.getObject().position.z += velocity.z * delta;
    } else {
        velocity.x *= Math.pow(0.001, delta);
        velocity.z *= Math.pow(0.001, delta);

        velocity.y -= GRAVITY * delta;

        if (jumpRequested) {
            if (isGrounded) {
                velocity.y = JUMP_SPEED;
                setIsGrounded(false);
            }
            setJumpRequested(false);
        }

        camera.getWorldQuaternion(cameraQuaternion);
        forwardVector.set(0, 0, -1).applyQuaternion(cameraQuaternion);
        forwardVector.y = 0;
        if (forwardVector.lengthSq() > 1e-6) {
            forwardVector.normalize();
        } else {
            forwardVector.set(0, 0, -1);
        }

        rightVector.set(1, 0, 0).applyQuaternion(cameraQuaternion);
        rightVector.y = 0;
        if (rightVector.lengthSq() > 1e-6) {
            rightVector.normalize();
        } else {
            rightVector.set(1, 0, 0);
        }

        const inputForward = Number(moveForward) - Number(moveBackward);
        const inputRight = Number(moveRight) - Number(moveLeft);

        worldDir.set(0, 0, 0);
        if (inputForward !== 0) {
            worldDir.addScaledVector(forwardVector, inputForward);
        }
        if (inputRight !== 0) {
            worldDir.addScaledVector(rightVector, inputRight);
        }

        if (worldDir.lengthSq() > 0) {
            worldDir.normalize();
            velocity.x += worldDir.x * moveSpeed * 10.0 * delta;
            velocity.z += worldDir.z * moveSpeed * 10.0 * delta;
        }

        const camPos = controls.getObject().position;

        const maxStep = CAPSULE_RADIUS * 0.5;
        const travel = Math.max(Math.abs(velocity.x), Math.abs(velocity.y), Math.abs(velocity.z)) * delta;
        const steps = Math.min(16, Math.max(1, Math.ceil(travel / maxStep)));
        const stepDt = delta / steps;
        let grounded = false;
        for (let s = 0; s < steps; s++) {
            camPos.x += velocity.x * stepDt;
            camPos.y += velocity.y * stepDt;
            camPos.z += velocity.z * stepDt;

            resolveCapsuleCollisions(camPos);
            grounded = grounded || isGrounded;
        }
        setIsGrounded(grounded);

        if (isGrounded && (moveForward || moveBackward || moveLeft || moveRight)) {
            const speedSq = velocity.x * velocity.x + velocity.z * velocity.z;
            if (speedSq > 400.0) {
                let curFootstepTimer = footstepTimer + delta * (isSprinting ? 1.5 : 1.0);
                if (curFootstepTimer >= 0.38) {
                    curFootstepTimer = 0.0;
                    playSound('walk.wav', 0.18);
                }
                setFootstepTimer(curFootstepTimer);
            } else {
                setFootstepTimer(0.3);
            }
        } else {
            setFootstepTimer(0.3);
        }
    }

    prevTime = time;

    const pos = controls.getObject().position;
    if (time - lastPosDisplayTime >= 100) {
        lastPosDisplayTime = time;
        hudEl('pos-display').textContent = `${Math.round(pos.x)}, ${Math.round(pos.y)}, ${Math.round(pos.z)}`;
    }

    if (playerDead) {
        setPlayerDeathTimer(playerDeathTimer - delta);
        if (playerDeathTimer <= 0) respawnPlayer();
    }

    const playerPos = pos;
    for (let i = 0; i < monsters.length; i++) {
        monsters[i].update(delta, playerPos, monsters);
    }

    updatePlayerWeaponAnimation(delta, { moveForward, moveBackward, moveLeft, moveRight });

    updateProjectiles(delta);

    updatePickups(delta, pos);

    updateDoors(delta);

    updateLighting(time / 1000, delta, pos);

    updateDistanceCulling(levelObjects, pos);

    renderer.render(scene, camera);
}

if (document.readyState === 'loading') {
    window.addEventListener('DOMContentLoaded', init);
} else {
    init();
}
