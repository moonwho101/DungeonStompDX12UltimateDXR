import * as THREE from 'three';
import { PointerLockControls } from 'three/addons/controls/PointerLockControls.js';
import { DDSLoader } from 'three/addons/loaders/DDSLoader.js';
import { TextureManager, ObjectsDatParser, LevelMapParser, ModelListParser, OBJECT_VERT_STRIDE } from './parsers.js';
import { TDSLoader } from 'three/addons/loaders/TDSLoader.js';
import { MD2Loader } from 'three/addons/loaders/MD2Loader.js';

// Monster AI Finite State Machine & Animation Definitions
export const MonsterState = Object.freeze({
    IDLE: 'IDLE',
    ALERT: 'ALERT',
    CHASE: 'CHASE',
    ATTACK: 'ATTACK',
    HURT: 'HURT',
    DEATH: 'DEATH'
});

export const MD2_ANIMATIONS = Object.freeze({
    STAND:  { name: 'STAND',  start: 0,   end: 39,  fps: 9,  loop: true },
    RUN:    { name: 'RUN',    start: 40,  end: 45,  fps: 10, loop: true },
    ATTACK: { name: 'ATTACK', start: 46,  end: 53,  fps: 10, loop: false },
    PAIN1:  { name: 'PAIN1',  start: 54,  end: 57,  fps: 8,  loop: false },
    PAIN2:  { name: 'PAIN2',  start: 58,  end: 61,  fps: 8,  loop: false },
    PAIN3:  { name: 'PAIN3',  start: 62,  end: 65,  fps: 8,  loop: false },
    JUMP:   { name: 'JUMP',   start: 66,  end: 71,  fps: 10, loop: false },
    FLIP:   { name: 'FLIP',   start: 72,  end: 83,  fps: 10, loop: false },
    SALUTE: { name: 'SALUTE', start: 84,  end: 94,  fps: 9,  loop: false },
    TAUNT:  { name: 'TAUNT',  start: 95,  end: 111, fps: 9,  loop: false },
    WAVE:   { name: 'WAVE',   start: 112, end: 122, fps: 9,  loop: false },
    POINT:  { name: 'POINT',  start: 123, end: 134, fps: 9,  loop: false },
    DEATH1: { name: 'DEATH1', start: 178, end: 183, fps: 7,  loop: false },
    DEATH2: { name: 'DEATH2', start: 184, end: 189, fps: 7,  loop: false },
    DEATH3: { name: 'DEATH3', start: 190, end: 197, fps: 7,  loop: false }
});

export const DEFAULT_MONSTER_CONFIG = Object.freeze({
    moveSpeed: 165.0,
    turnRate: 6.0,          // radians per second
    aggroRadius: 800.0,     // detection distance to player
    deaggroRadius: 1200.0,  // distance to lose interest and return to idle
    attackRange: 85.0,      // melee attack trigger range
    stopDistance: 85.0,     // minimum distance to maintain when attacking/moving
    alertDuration: 0.6,     // time spent in ALERT before entering CHASE
    damage: 10,
    maxHealth: 100,
    sound: 'goblin.wav',
    animations: MD2_ANIMATIONS
});

export const MONSTER_CONFIGS = {
    KNIGHT: { ...DEFAULT_MONSTER_CONFIG, sound: 'knightyell.wav', moveSpeed: 150.0, aggroRadius: 850.0, damage: 15, maxHealth: 20 },
    GOBLIN: { ...DEFAULT_MONSTER_CONFIG, sound: 'goblin.wav', moveSpeed: 190.0, aggroRadius: 750.0, damage: 6, maxHealth: 18 },
    BAUUL: { ...DEFAULT_MONSTER_CONFIG, sound: 'baullspeak.wav', moveSpeed: 140.0, aggroRadius: 900.0, damage: 20, maxHealth: 25 },
    CORPSE: { ...DEFAULT_MONSTER_CONFIG, sound: 'corpse.wav', moveSpeed: 110.0, aggroRadius: 650.0, damage: 8, maxHealth: 15 },
    SORCERER: { ...DEFAULT_MONSTER_CONFIG, sound: 'sorcererspeak.wav', moveSpeed: 160.0, aggroRadius: 950.0, damage: 18, maxHealth: 20 },
    TENTACLE: { ...DEFAULT_MONSTER_CONFIG, sound: 'mouther.wav', moveSpeed: 120.0, aggroRadius: 700.0, damage: 5, maxHealth: 17 },
    WRAITH: { ...DEFAULT_MONSTER_CONFIG, sound: 'wraithspeak.wav', moveSpeed: 180.0, aggroRadius: 950.0, damage: 12, maxHealth: 28 },
    OGRE: { ...DEFAULT_MONSTER_CONFIG, sound: 'ogre.wav', moveSpeed: 130.0, aggroRadius: 800.0, damage: 25, maxHealth: 25 },
    PHANTOM: { ...DEFAULT_MONSTER_CONFIG, sound: 'alive.wav', moveSpeed: 170.0, aggroRadius: 900.0, damage: 16, maxHealth: 20 },
    OGRO: { ...DEFAULT_MONSTER_CONFIG, sound: 'orc.wav', moveSpeed: 135.0, aggroRadius: 800.0, damage: 14, maxHealth: 20 },
    SKELETON2: { ...DEFAULT_MONSTER_CONFIG, sound: 'Skeleton.wav', moveSpeed: 165.0, aggroRadius: 800.0, damage: 10, maxHealth: 15 },
    DEMONESS: { ...DEFAULT_MONSTER_CONFIG, sound: 'demonspeak.wav', moveSpeed: 175.0, aggroRadius: 950.0, damage: 22, maxHealth: 35 },
    FAERIE: { ...DEFAULT_MONSTER_CONFIG, sound: 'faeriespeak.wav', moveSpeed: 185.0, aggroRadius: 900.0, damage: 15, maxHealth: 30 },
    SLAVE: { ...DEFAULT_MONSTER_CONFIG, sound: 'dmspeak.wav', moveSpeed: 140.0, aggroRadius: 600.0, damage: 5, maxHealth: 5 },
    WOLF: { ...DEFAULT_MONSTER_CONFIG, sound: 'wolfhowl.wav', moveSpeed: 210.0, aggroRadius: 850.0, damage: 12, maxHealth: 20 },
    INSECT: { ...DEFAULT_MONSTER_CONFIG, sound: 'Drip2.wav', moveSpeed: 195.0, aggroRadius: 800.0, damage: 10, maxHealth: 20 },
    OLUVE: { ...DEFAULT_MONSTER_CONFIG, sound: 'ghoul.wav', moveSpeed: 150.0, aggroRadius: 700.0, damage: 6, maxHealth: 17 },
    MUMMY: { ...DEFAULT_MONSTER_CONFIG, sound: 'mummyspeak.wav', moveSpeed: 125.0, aggroRadius: 750.0, damage: 12, maxHealth: 18 },
    HYDRA: { ...DEFAULT_MONSTER_CONFIG, sound: 'hydraspeak.wav', moveSpeed: 145.0, aggroRadius: 850.0, damage: 16, maxHealth: 20 },
    NECROMANCER: { ...DEFAULT_MONSTER_CONFIG, sound: 'necrospeak.wav', moveSpeed: 160.0, aggroRadius: 1000.0, damage: 24, maxHealth: 35 },
    IMP: { ...DEFAULT_MONSTER_CONFIG, sound: 'impspeak.wav', moveSpeed: 180.0, aggroRadius: 800.0, damage: 10, maxHealth: 20 },
    DEMON: { ...DEFAULT_MONSTER_CONFIG, sound: 'demonpain.wav', moveSpeed: 165.0, aggroRadius: 950.0, damage: 20, maxHealth: 25 },
    FULIMO: { ...DEFAULT_MONSTER_CONFIG, sound: 'fulimospeak.wav', moveSpeed: 170.0, aggroRadius: 900.0, damage: 18, maxHealth: 25 },
    COBRA: { ...DEFAULT_MONSTER_CONFIG, sound: 'snakehiss.wav', moveSpeed: 190.0, aggroRadius: 800.0, damage: 14, maxHealth: 30 },
    DRAGON: { ...DEFAULT_MONSTER_CONFIG, sound: 'DRAGON.WAV', moveSpeed: 160.0, aggroRadius: 1000.0, damage: 28, maxHealth: 30 },
    CENTAUR: { ...DEFAULT_MONSTER_CONFIG, sound: 'tsword.wav', moveSpeed: 190.0, aggroRadius: 850.0, damage: 12, maxHealth: 16 },
    CRAZ: { ...DEFAULT_MONSTER_CONFIG, sound: 'effect1.wav', moveSpeed: 175.0, aggroRadius: 800.0, damage: 14, maxHealth: 20 },
    RIDER: { ...DEFAULT_MONSTER_CONFIG, sound: 'effect2.wav', moveSpeed: 180.0, aggroRadius: 900.0, damage: 20, maxHealth: 26 },
    OKTA: { ...DEFAULT_MONSTER_CONFIG, sound: 'effect3.wav', moveSpeed: 165.0, aggroRadius: 850.0, damage: 16, maxHealth: 25 },
    SORB: { ...DEFAULT_MONSTER_CONFIG, sound: 'sorb.wav', moveSpeed: 170.0, aggroRadius: 950.0, damage: 22, maxHealth: 29 }
};

export function getMonsterConfig(name) {
    if (!name) return DEFAULT_MONSTER_CONFIG;
    const clean = name.toUpperCase().trim();
    if (MONSTER_CONFIGS[clean]) return MONSTER_CONFIGS[clean];
    for (const key in MONSTER_CONFIGS) {
        if (clean.includes(key)) return MONSTER_CONFIGS[key];
    }
    return DEFAULT_MONSTER_CONFIG;
}

export class Monster {
    constructor(mesh, weaponMesh = null, config = DEFAULT_MONSTER_CONFIG, initialPosition, initialRotationY = 0) {
        this.mesh = mesh;
        this.weaponMesh = weaponMesh;
        this.config = { ...DEFAULT_MONSTER_CONFIG, ...config };

        this.position = initialPosition.clone();
        this.velocity = new THREE.Vector3(0, 0, 0);
        this.isGrounded = false;

        this.rotationY = initialRotationY;
        this.mesh.position.copy(this.position);
        this.mesh.rotation.y = this.rotationY - Math.PI / 2;

        if (this.weaponMesh) {
            this.weaponMesh.position.copy(this.position);
            this.weaponMesh.rotation.y = this.rotationY - Math.PI / 2;
        }

        this.state = MonsterState.IDLE;
        this.health = this.config.maxHealth;

        // Timers & State control
        this.alertTimer = 0.0;
        this.stateTimer = 0.0;
        this.attackCooldown = 0.0;
        this.hasDealtDamageThisAttack = false;

        // Animation state tracking
        this.currentAnim = this.config.animations.STAND;
        this.animTimer = 0.0;
        this.animComplete = false;

        // Cross-fading for smooth state transitions
        this.prevAnimFrames = null;
        this.fadeTimer = 0.0;
        this.fadeDuration = 0.15; // 150ms cross-fade

        // Active morph target influences (frame indices written last update), reused every frame
        this.activeFrames = [0, 0, 0, 0];
        this.activeFrameCount = 0;
        this.frameWeights = [0, 0, 0, 0];
        this.scratchPose = { frameA: 0, frameB: 0, alpha: 0 };

        // Bounding box for dynamic collision
        this.boundingBox = new THREE.Box3();
        this.updateBoundingBox();

        // Initial random noise timer for ambient growls
        this.noiseTimer = 3.0 + Math.random() * 8.0;

        // Apply initial animation pose
        this.setAnimation(this.config.animations.STAND, true);
        this.updateMorphTargets(0);
    }

    playMonsterNoise(distToPlayer) {
        if (distToPlayer > 1200.0) return;
        const soundFile = this.config.sound || 'goblin.wav';
        const volume = Math.max(0.05, (1.0 - (distToPlayer / 1200.0)) * 0.7);
        playSound(soundFile, volume);
    }

    updateBoundingBox() {
        if (!this.mesh) return;
        if (this.state === MonsterState.DEATH) {
            this.boundingBox.makeEmpty();
            return;
        }
        const curPose = this.getCurrentPose(this.scratchPose);
        computeMD2FrameBoundingBox(this.mesh, curPose.frameA, this.boundingBox);
    }

    setAnimation(animDef, forceReset = false) {
        if (!animDef) return;
        if (this.currentAnim === animDef && !forceReset) return;

        // Save current frame pose for smooth cross-fading
        if (!forceReset && this.currentAnim) {
            this.prevAnimFrames = this.getCurrentPose();
            this.fadeTimer = 0.0;
        } else {
            this.prevAnimFrames = null;
        }

        this.currentAnim = animDef;
        this.animTimer = 0.0;
        this.animComplete = false;
    }

    getCurrentPose(out = { frameA: 0, frameB: 0, alpha: 0 }) {
        const anim = this.currentAnim;
        const totalFrames = anim.end - anim.start + 1;
        const progress = this.animTimer * anim.fps;

        let frameOffset, nextOffset;
        if (anim.loop) {
            const frameIdx = progress % totalFrames;
            frameOffset = Math.floor(frameIdx);
            nextOffset = (frameOffset + 1) % totalFrames;
            out.alpha = frameIdx - frameOffset;
        } else {
            const frameIdx = Math.min(progress, totalFrames - 1);
            frameOffset = Math.floor(frameIdx);
            nextOffset = Math.min(frameOffset + 1, totalFrames - 1);
            out.alpha = frameIdx - frameOffset;
        }
        out.frameA = anim.start + frameOffset;
        out.frameB = anim.start + nextOffset;
        return out;
    }

    // Adds a blend weight for a frame, merging with an already queued entry for the same frame
    addFrameWeight(frame, weight) {
        const n = this.activeFrameCount;
        for (let k = 0; k < n; k++) {
            if (this.activeFrames[k] === frame) {
                this.frameWeights[k] += weight;
                return;
            }
        }
        this.activeFrames[n] = frame;
        this.frameWeights[n] = weight;
        this.activeFrameCount = n + 1;
    }

    updateMorphTargets(delta) {
        const mesh = this.mesh;
        const meshGeometry = mesh ? mesh.geometry : null;
        if (!meshGeometry || !meshGeometry.morphAttributes || !meshGeometry.morphAttributes.position) {
            return;
        }

        const influences = mesh.morphTargetInfluences;
        if (!influences) return;
        const wInfluences = this.weaponMesh ? this.weaponMesh.morphTargetInfluences : null;

        // Reset the influences written last update
        for (let k = 0; k < this.activeFrameCount; k++) {
            const idx = this.activeFrames[k];
            if (idx >= 0 && idx < influences.length) influences[idx] = 0;
            if (wInfluences && idx >= 0 && idx < wInfluences.length) wInfluences[idx] = 0;
        }
        this.activeFrameCount = 0;

        // Advance animation timer
        this.animTimer += delta;

        const anim = this.currentAnim;
        const totalFrames = anim.end - anim.start + 1;
        const totalDuration = totalFrames / anim.fps;

        if (!anim.loop && this.animTimer >= totalDuration) {
            this.animComplete = true;
        }

        const curPose = this.getCurrentPose(this.scratchPose);

        if (this.prevAnimFrames && this.fadeTimer < this.fadeDuration) {
            this.fadeTimer += delta;
            const blend = Math.min(1.0, this.fadeTimer / this.fadeDuration);
            const invBlend = 1.0 - blend;
            const prev = this.prevAnimFrames;

            this.addFrameWeight(prev.frameA, (1 - prev.alpha) * invBlend);
            this.addFrameWeight(prev.frameB, prev.alpha * invBlend);
            this.addFrameWeight(curPose.frameA, (1 - curPose.alpha) * blend);
            this.addFrameWeight(curPose.frameB, curPose.alpha * blend);
        } else {
            this.prevAnimFrames = null;
            this.addFrameWeight(curPose.frameA, 1 - curPose.alpha);
            if (curPose.alpha > 1e-4) {
                this.addFrameWeight(curPose.frameB, curPose.alpha);
            }
        }

        // Apply the weights to the mesh and the identical weights to its attached weapon
        let kept = 0;
        for (let k = 0; k < this.activeFrameCount; k++) {
            const frameIdx = this.activeFrames[k];
            const weight = this.frameWeights[k];
            if (frameIdx < 0 || weight <= 0.001) continue;
            if (frameIdx < influences.length) influences[frameIdx] = weight;
            if (wInfluences && frameIdx < wInfluences.length) wInfluences[frameIdx] = weight;
            this.activeFrames[kept++] = frameIdx;
        }
        this.activeFrameCount = kept;
    }

    // Randomly plays a monster ambient sound/growl periodically
    tickAmbientNoise(dt, distToPlayer) {
        this.noiseTimer -= dt;
        if (this.noiseTimer <= 0) {
            this.noiseTimer = 5.0 + Math.random() * 10.0;
            this.playMonsterNoise(distToPlayer);
        }
    }

    update(delta, playerPos, otherMonsters) {
        const dt = Math.min(delta, 0.1);

        const dx = playerPos.x - this.position.x;
        const dz = playerPos.z - this.position.z;
        const distSq = dx * dx + dz * dz;
        const distToPlayer = Math.sqrt(distSq);

        if (this.isGrounded) {
            // A settled corpse has nothing left to simulate
            if (this.state === MonsterState.DEATH && this.animComplete && !this.weaponMesh) return;

            // Idle monsters well outside their aggro radius only need their idle animation,
            // and only while they are close enough to be drawn
            if (this.state === MonsterState.IDLE && distSq > this.config.aggroRadius * this.config.aggroRadius) {
                this.tickAmbientNoise(dt, distToPlayer);
                if (distSq <= RENDER_DISTANCE * RENDER_DISTANCE) {
                    this.updateMorphTargets(dt);
                    this.updateBoundingBox();
                }
                return;
            }
        }

        // Sub-step vertical movement & gravity to prevent tunneling through floor geometry
        const maxSubStep = 12.0;
        const estDy = Math.abs(this.velocity.y * dt);
        const subSteps = Math.max(1, Math.ceil(estDy / maxSubStep));
        const subDt = dt / subSteps;

        for (let s = 0; s < subSteps; s++) {
            this.velocity.y -= GRAVITY * subDt;
            this.position.y += this.velocity.y * subDt;
            resolveMonsterCapsuleCollisions(this);
            if (this.isGrounded) break;
        }

        if (this.state === MonsterState.DEATH) {
            this.mesh.position.copy(this.position);
            if (this.weaponMesh) {
                scene.remove(this.weaponMesh);
                const idx = levelObjects.findIndex(o => o.mesh === this.weaponMesh);
                if (idx !== -1) {
                    levelObjects.splice(idx, 1);
                }
                this.weaponMesh = null;
            }
            this.updateMorphTargets(dt);
            this.updateBoundingBox();
            return;
        }

        this.stateTimer += dt;

        if (this.attackCooldown > 0) {
            this.attackCooldown = Math.max(0, this.attackCooldown - dt);
        }

        const targetAngle = Math.atan2(dx, dz);

        this.tickAmbientNoise(dt, distToPlayer);

        switch (this.state) {
            case MonsterState.IDLE:
                this.setAnimation(this.config.animations.STAND);
                if (distToPlayer <= this.config.aggroRadius) {
                    this.setState(MonsterState.ALERT);
                }
                break;

            case MonsterState.ALERT:
                this.setAnimation(this.config.animations.STAND);
                this.smoothTurn(targetAngle, dt);

                if (distToPlayer > this.config.deaggroRadius) {
                    this.setState(MonsterState.IDLE);
                } else if (distToPlayer <= this.config.attackRange) {
                    this.setState(MonsterState.ATTACK);
                } else if (this.stateTimer >= this.config.alertDuration) {
                    this.setState(MonsterState.CHASE);
                }
                break;

            case MonsterState.CHASE:
                this.smoothTurn(targetAngle, dt);

                if (distToPlayer <= this.config.attackRange) {
                    if (this.attackCooldown <= 0) {
                        this.setState(MonsterState.ATTACK);
                    } else {
                        // Recovering from the last swing: hold position facing the player
                        this.setAnimation(this.config.animations.STAND);
                    }
                } else if (distToPlayer > this.config.deaggroRadius) {
                    this.setState(MonsterState.IDLE);
                } else {
                    this.setAnimation(this.config.animations.RUN);
                    if (distToPlayer > this.config.stopDistance) {
                        this.moveForward(dt, otherMonsters, playerPos);
                    }
                }
                break;

            case MonsterState.ATTACK:
                this.setAnimation(this.config.animations.ATTACK);
                if (this.stateTimer < 0.2) {
                    this.smoothTurn(targetAngle, dt);
                }

                // Monster strikes player halfway through attack swing
                if (!this.hasDealtDamageThisAttack) {
                    const anim = this.currentAnim;
                    const totalFrames = anim.end - anim.start + 1;
                    const totalDuration = totalFrames / anim.fps;
                    if (this.animTimer >= totalDuration * 0.5) {
                        this.hasDealtDamageThisAttack = true;
                        if (distToPlayer <= this.config.attackRange + 20.0) {
                            damagePlayer(this.config.damage);
                        }
                    }
                }

                if (this.animComplete) {
                    this.attackCooldown = 0.5;
                    this.hasDealtDamageThisAttack = false;
                    this.setState(distToPlayer <= this.config.deaggroRadius ? MonsterState.CHASE : MonsterState.IDLE);
                }
                break;

            case MonsterState.HURT:
                if (this.animComplete) {
                    this.setState(MonsterState.CHASE);
                }
                break;
        }

        this.mesh.position.copy(this.position);
        this.mesh.rotation.y = this.rotationY - Math.PI / 2;

        if (this.weaponMesh) {
            this.weaponMesh.position.copy(this.position);
            this.weaponMesh.rotation.y = this.rotationY - Math.PI / 2;
        }

        this.updateMorphTargets(dt);
        this.updateBoundingBox();
    }

    setState(newState) {
        if (this.state === newState || this.state === MonsterState.DEATH) return;
        const oldState = this.state;
        this.state = newState;
        this.stateTimer = 0.0;
        if (newState === MonsterState.ATTACK) {
            this.hasDealtDamageThisAttack = false;
        }
        if ((oldState === MonsterState.IDLE) && (newState === MonsterState.ALERT || newState === MonsterState.CHASE)) {
            const playerPos = window.controls ? window.controls.getObject().position : null;
            if (playerPos) {
                const dist = this.position.distanceTo(playerPos);
                this.playMonsterNoise(dist);
            }
        }
    }

    smoothTurn(targetAngle, dt) {
        let diff = targetAngle - this.rotationY;
        while (diff > Math.PI) diff -= Math.PI * 2;
        while (diff < -Math.PI) diff += Math.PI * 2;

        const maxTurn = this.config.turnRate * dt;
        const turn = Math.max(-maxTurn, Math.min(maxTurn, diff));
        this.rotationY += turn;
    }

    moveForward(dt, otherMonsters, playerPos) {
        const moveDist = this.config.moveSpeed * dt;
        this.position.x += Math.sin(this.rotationY) * moveDist;
        this.position.z += Math.cos(this.rotationY) * moveDist;

        // Only this monster is pushed out of level geometry, other monsters and the
        // player. Nothing it bumps into is displaced, so it cannot shove a pack of
        // monsters (or the player) around.
        resolveMonsterCapsuleCollisions(this, otherMonsters, playerPos);
    }

    takeDamage(damage) {
        if (this.state === MonsterState.DEATH) return;
        this.health -= damage;
        if (this.health <= 0) {
            this.health = 0;
            this.setState(MonsterState.DEATH);
            this.boundingBox.makeEmpty();
            const deaths = [this.config.animations.DEATH1, this.config.animations.DEATH2, this.config.animations.DEATH3];
            const chosenDeath = deaths[Math.floor(Math.random() * deaths.length)];
            this.setAnimation(chosenDeath, true);

            const deathSounds = ['death1.wav', 'death2.wav', 'death3.wav', 'death4.wav', 'die.wav'];
            const chosenSound = deathSounds[Math.floor(Math.random() * deathSounds.length)];
            const playerPos = window.controls ? window.controls.getObject().position : null;
            if (playerPos) {
                const dist = this.position.distanceTo(playerPos);
                if (dist <= 1200.0) {
                    playSound(chosenSound, Math.max(0.05, (1.0 - (dist / 1200.0)) * 0.7));
                }
            }

            // Drop treasure item
            dropTreasure(this.position);

            // Award XP and kills to player
            const xpGained = (this.config.maxHealth || 20) * 2;
            awardPlayerXp(xpGained);
        } else {
            this.setState(MonsterState.HURT);
            const pains = [this.config.animations.PAIN1, this.config.animations.PAIN2, this.config.animations.PAIN3];
            const chosenPain = pains[Math.floor(Math.random() * pains.length)];
            this.setAnimation(chosenPain, true);
        }
    }
}

let camera, scene, renderer, controls;
let moveForward = false;
let moveBackward = false;
let moveLeft = false;
let moveRight = false;
let moveUp = false;
let moveDown = false;
let isSprinting = false;

let isFlyMode = false;
let isGrounded = false;
let jumpRequested = false;

let prevTime = performance.now();
const velocity = new THREE.Vector3();
const direction = new THREE.Vector3();
const cameraQuaternion = new THREE.Quaternion();
const worldDir = new THREE.Vector3();

// Treasure Drops List
export const TREASURE_DROPS = [
    'POTION', 'cheese1', 'GOBLET', 'pot1', 'pot2', 'pot3',
    'mushroom', 'COIN', 'SPELLBOOK', 'SCROLL-HEALING-',
    'SCROLL-MAGICMISSLE-', 'SCROLL-FIREBALL-', 'SCROLL-LIGHTNING-'
];

// Item Pickup Definitions & Rewards Table
export const PICKUP_DEFINITIONS = Object.freeze({
    'POTION': { type: 'health', amount: 10, label: '+10 HP' },
    'CHEESE1': { type: 'health', amount: 15, label: '+15 HP' },
    'POT1': { type: 'health', amount: 10, label: '+10 HP' },
    'POT2': { type: 'health', amount: 15, label: '+15 HP' },
    'POT3': { type: 'health', amount: 20, label: '+20 HP' },
    'MUSHROOM': { type: 'health', amount: 'full', label: 'FULL HP' },

    'GOBLET': { type: 'xp', amount: 50, label: '+50 XP' },
    'ARMOUR': { type: 'xp', amount: 10, label: '+10 XP' },
    'COIN': { type: 'xp', amount: 10, label: '+10 XP' },
    'SPELLBOOK': { type: 'xp', amount: 50, label: '+50 XP' },
    'SCROLL-HEALING-': { type: 'xp', amount: 10, label: '+10 XP' },
    'SCROLL-MAGICMISSLE-': { type: 'xp', amount: 20, label: '+20 XP' },
    'SCROLL-FIREBALL-': { type: 'xp', amount: 30, label: '+30 XP' },
    'SCROLL-LIGHTNING-': { type: 'xp', amount: 40, label: '+40 XP' }
});

export function getPickupDefinition(name) {
    if (!name) return null;
    const clean = name.toUpperCase().trim();
    if (PICKUP_DEFINITIONS[clean]) return PICKUP_DEFINITIONS[clean];

    // Check if clean name matches key with/without trailing dashes or numbers (e.g. SCROLL1, POTION01)
    const noDash = clean.replace(/-+$/, '');

    for (const key in PICKUP_DEFINITIONS) {
        const keyNoDash = key.replace(/-+$/, '');
        if (clean === key || noDash === keyNoDash || clean.startsWith(keyNoDash) || keyNoDash.startsWith(noDash)) {
            return PICKUP_DEFINITIONS[key];
        }
    }
    return null;
}

// Player Stats Tracking
export const playerStats = {
    health: 100,
    maxHealth: 100,
    level: 1,
    xp: 0,
    nextXp: 100,
    kills: 0
};

const hudElements = new Map();
function hudEl(id) {
    let el = hudElements.get(id);
    if (!el) {
        el = document.getElementById(id);
        if (el) hudElements.set(id, el);
    }
    return el;
}

// Full-screen vignette flash (damage, healing, pickups). Re-triggering restarts the fade.
const FLASH_RED = 'radial-gradient(circle, rgba(255,0,0,0) 40%, rgba(255,0,0,0.8) 100%)';
const FLASH_GREEN = 'radial-gradient(circle, rgba(34,197,94,0) 30%, rgba(34,197,94,0.5) 100%)';
const FLASH_PICKUP_HEALTH = 'radial-gradient(circle, rgba(34,197,94,0) 40%, rgba(34,197,94,0.4) 100%)';
const FLASH_PICKUP_XP = 'radial-gradient(circle, rgba(234,179,8,0) 40%, rgba(234,179,8,0.4) 100%)';
let flashBackground = FLASH_RED;
let flashTimeout = 0;

function flashScreen(background, opacity, holdMs) {
    const flashEl = hudEl('damage-flash');
    if (!flashEl) return;
    if (background !== flashBackground) {
        flashBackground = background;
        flashEl.style.background = background;
    }
    flashEl.style.opacity = String(opacity);
    clearTimeout(flashTimeout);
    flashTimeout = setTimeout(() => { flashEl.style.opacity = '0'; }, holdMs);
}

let playerDead = false;
let playerDeathTimer = 0.0;
let playerStartPos = null;
const PLAYER_RESPAWN_DELAY = 3.0;

export function damagePlayer(amount) {
    if (playerStats.health <= 0) return;
    playerStats.health = Math.max(0, playerStats.health - amount);

    playSound(getRandomSound(PLAYER_IMPACT_SOUNDS), 0.6);
    flashScreen(FLASH_RED, 0.6, 150);
    updatePlayerHud();

    if (playerStats.health <= 0) {
        killPlayer();
    }
}

function killPlayer() {
    playerDead = true;
    playerDeathTimer = PLAYER_RESPAWN_DELAY;
    moveForward = moveBackward = moveLeft = moveRight = moveUp = moveDown = false;
    isSprinting = false;
    jumpRequested = false;
    isWeaponAttacking = false;
    playSound('die.wav', 0.8);
    const deathEl = hudEl('death-screen');
    if (deathEl) deathEl.style.display = 'flex';
}

function respawnPlayer() {
    playerDead = false;
    playerStats.health = playerStats.maxHealth;
    const deathEl = hudEl('death-screen');
    if (deathEl) deathEl.style.display = 'none';

    if (controls && playerStartPos) {
        controls.getObject().position.set(playerStartPos.x, playerStartPos.y, playerStartPos.z);
    }
    velocity.set(0, 0, 0);
    updatePlayerHud();
}

export function healPlayer(amount) {
    if (playerStats.health <= 0) return;
    if (amount === 'full') {
        playerStats.health = playerStats.maxHealth;
    } else {
        playerStats.health = Math.min(playerStats.maxHealth, playerStats.health + Number(amount));
    }
    updatePlayerHud();
}

export function addPlayerXp(amount) {
    playerStats.xp += amount;
    while (playerStats.xp >= playerStats.nextXp) {
        playerStats.xp -= playerStats.nextXp;
        playerStats.level += 1;
        playerStats.nextXp = Math.floor(playerStats.nextXp * 1.5);
        playerStats.maxHealth += 20;
        playerStats.health = playerStats.maxHealth; // Full heal on level up
    }
    updatePlayerHud();
}

export function awardPlayerXp(amount) {
    playerStats.kills += 1;
    addPlayerXp(amount);
}

export function updatePlayerHud() {
    const hpBar = hudEl('hud-hp-bar');
    const hpText = hudEl('hud-hp-text');
    const xpBar = hudEl('hud-xp-bar');
    const levelText = hudEl('hud-level-text');
    const killsText = hudEl('hud-kills-text');
    const gunText = hudEl('hud-gun-text');

    const hpPercent = Math.max(0, Math.min(100, (playerStats.health / playerStats.maxHealth) * 100));
    const xpPercent = Math.max(0, Math.min(100, (playerStats.xp / playerStats.nextXp) * 100));

    if (hpBar) hpBar.style.width = `${hpPercent}%`;
    if (hpText) hpText.textContent = `${Math.ceil(playerStats.health)} / ${playerStats.maxHealth}`;
    if (xpBar) xpBar.style.width = `${xpPercent}%`;
    if (levelText) levelText.textContent = `LVL ${playerStats.level}`;
    if (killsText) killsText.textContent = `${playerStats.kills}`;

    if (gunText && typeof PLAYER_GUNS !== 'undefined' && typeof currentGunIndex !== 'undefined') {
        const gunName = PLAYER_GUNS[currentGunIndex] || 'AXE';
        if (typeof spellCounts !== 'undefined' && spellCounts[gunName] !== undefined) {
            gunText.textContent = `${gunName} (${spellCounts[gunName]})`;
        } else {
            gunText.textContent = gunName;
        }
    }
}

// Level Objects & Distance Culling
const RENDER_DISTANCE = 3000.0;
const levelObjects = [];
const activePickups = [];
const monsters = [];
const doors = [];
const chests = [];

function addLevelObject(mesh, position, radius = 0) {
    const entry = { mesh, position, radius };
    levelObjects.push(entry);
    return entry;
}

function addPickup(mesh, position, name, def) {
    activePickups.push({
        mesh,
        position: position.clone(),
        originalY: position.y,
        name,
        def,
        state: 'ACTIVE',
        floatTimer: 0,
        baseScale: mesh.scale.x
    });
}

// Dungeon lighting (lamp_post LIGHT_SOURCE entries)
const LIGHT_RANGE = 1000.0;
const MAX_POINT_LIGHTS = 8;
const MAX_SPOT_LIGHTS = 4;
const MAX_DYNAMIC_LIGHTS = 4; // projectile glow and explosion flashes
const POINT_LIGHT_INTENSITY = 8500;
const SPOT_LIGHT_INTENSITY = 12000;
const LIGHT_DECAY = 1.5; // softer than inverse-square to avoid harsh hotspots next to lamps
const LIGHT_FADE_SPEED = 4.0;
const LIGHT_REFRESH_INTERVAL = 0.1;
const FLICKER_COLOUR_REF = 14.0; // flicker colours are authored on a 0..~14 scale
const SPOT_COLOUR_REF = 0.9;

const lamps = [];
const lightSlots = { flicker: [], spotlight: [] };
const dynamicLights = [];
let lightRefreshTimer = LIGHT_REFRESH_INTERVAL;
let haloTexture = null;
let coreTexture = null;

// Spatial Collision Structures
const levelTriangles = [];
const modelBoxes = [];
const collisionGrid = new Map();
const GRID_CELL_SIZE = 256;

// Player Capsule parameters
const CAPSULE_RADIUS = 28.0;
const CAPSULE_HEIGHT = 105.0;
const EYE_HEIGHT = 100.0;
const GRAVITY = 1600.0;
const JUMP_SPEED = 520.0;

const textureManager = new TextureManager();
const objectsParser = new ObjectsDatParser();
const levelParser = new LevelMapParser();
const modelListParser = new ModelListParser();

const ddsLoader = new DDSLoader();
const md2Loader = new MD2Loader();
const textureLoader = new THREE.TextureLoader();

const textureCache = new Map();
const normalMapCache = new Map();
const normalMapMaterials = new Set();
const sharedMaterials = new Map();
let normalMapsEnabled = true;
let maxAnisotropy = 1;
const tdsModelCache = new Map();
const md2GeometryCache = new Map();

// Player Guns list & Damage Table
export const PLAYER_GUNS = [
    'AXE',
    'FLAMESWORD',
    'BASTARDSWORD',
    'BATTLEAXE',
    'ICESWORD',
    'LIGHTNINGSWORD',
    'MORNINGSTAR',
    'SPIKEDFLAIL',
    'SPLITSWORD',
    'SUPERFLAMESWORD',
    'MAGIC MISSILE',
    'FIREBALL',
    'LIGHTNING',
    'HEALING'
];

export const unlockedWeapons = new Set([
    'AXE',
    'MAGIC MISSILE',
    'FIREBALL',
    'LIGHTNING',
    'HEALING'
]);

export const spellCounts = {
    'MAGIC MISSILE': 5,
    'FIREBALL': 5,
    'LIGHTNING': 5,
    'HEALING': 5
};
const WEAPON_DAMAGE = {
    'AXE': 20,
    'FLAMESWORD': 25,
    'BASTARDSWORD': 30,
    'BATTLEAXE': 35,
    'ICESWORD': 30,
    'LIGHTNINGSWORD': 35,
    'MORNINGSTAR': 25,
    'SPIKEDFLAIL': 25,
    'SPLITSWORD': 30,
    'SUPERFLAMESWORD': 40,
    'MAGIC MISSILE': 35,
    'FIREBALL': 50,
    'LIGHTNING': 45,
    'HEALING': 0
};
let currentGunIndex = 0;
let currentGunMesh = null;

let playerWeaponAnimTimer = 0.0;
let playerWeaponCurrentAnim = MD2_ANIMATIONS.STAND;
let playerWeaponActiveInfluences = new Set();
let isWeaponAttacking = false;
let hasWeaponHitThisSwing = false;

const defaultCanvas = document.createElement('canvas');
defaultCanvas.width = 128;
defaultCanvas.height = 128;
const ctx = defaultCanvas.getContext('2d');
ctx.fillStyle = '#666677';
ctx.fillRect(0, 0, 128, 128);
ctx.fillStyle = '#444455';
ctx.fillRect(0, 0, 64, 64);
ctx.fillRect(64, 64, 64, 64);

function createGlareTexture(kind) {
    const size = 256;
    const c = document.createElement('canvas');
    c.width = size;
    c.height = size;
    const g = c.getContext('2d');
    const half = size / 2;

    const radial = (stops) => {
        const grad = g.createRadialGradient(half, half, 0, half, half, half);
        for (const [offset, alpha] of stops) grad.addColorStop(offset, `rgba(255,255,255,${alpha})`);
        return grad;
    };

    if (kind === 'halo') {
        g.fillStyle = radial([[0, 0.55], [0.2, 0.32], [0.45, 0.12], [0.75, 0.03], [1, 0]]);
        g.fillRect(0, 0, size, size);
    } else {
        // Bright core with soft four-point streaks
        g.save();
        g.translate(half, half);
        g.scale(1, 0.05);
        g.translate(-half, -half);
        g.fillStyle = radial([[0, 0.9], [0.3, 0.35], [1, 0]]);
        g.fillRect(0, 0, size, size);
        g.restore();
        g.save();
        g.translate(half, half);
        g.scale(0.05, 1);
        g.translate(-half, -half);
        g.fillStyle = radial([[0, 0.7], [0.3, 0.25], [1, 0]]);
        g.fillRect(0, 0, size, size);
        g.restore();
        g.fillStyle = radial([[0, 1], [0.12, 0.9], [0.3, 0.3], [0.6, 0.05], [1, 0]]);
        g.fillRect(0, 0, size, size);
    }

    const tex = new THREE.CanvasTexture(c);
    tex.colorSpace = THREE.SRGBColorSpace;
    return tex;
}

function setupSceneLighting() {
    // Dim cool ambient so the dungeon stays moody and the lamps carry the scene
    const ambientLight = new THREE.AmbientLight(0x7a82a8, 0.9);
    scene.add(ambientLight);

    // Faint cool "moonlight" directional fill so unlit corridors remain readable
    const dirLight = new THREE.DirectionalLight(0xaab6ff, 0.9);
    dirLight.position.set(2500, 6000, 1500);
    scene.add(dirLight);

    scene.fog = new THREE.Fog(0x08080e, 1200, 3400);
    scene.background = new THREE.Color(0x08080e);

    // Fixed-size light pools keep the shader light count constant (no recompiles while walking)
    for (let i = 0; i < MAX_POINT_LIGHTS; i++) {
        const light = new THREE.PointLight(0xffffff, 0, LIGHT_RANGE, LIGHT_DECAY);
        scene.add(light);
        lightSlots.flicker.push({ light, lamp: null, fade: 0, releasing: false });
    }
    for (let i = 0; i < MAX_SPOT_LIGHTS; i++) {
        const light = new THREE.SpotLight(0xffffff, 0, LIGHT_RANGE, Math.PI / 3.2, 0.65, LIGHT_DECAY);
        scene.add(light);
        scene.add(light.target);
        lightSlots.spotlight.push({ light, lamp: null, fade: 0, releasing: false });
    }

    haloTexture = createGlareTexture('halo');
    coreTexture = createGlareTexture('core');

    // Spell lights come from a fixed pool too, so casting never changes the shader light count
    for (let i = 0; i < MAX_DYNAMIC_LIGHTS; i++) {
        const light = new THREE.PointLight(0xffffff, 0, 400, LIGHT_DECAY);
        scene.add(light);
        dynamicLights.push({ light, busy: false });
    }
}

function acquireDynamicLight(colorHex, intensity, distance) {
    for (const slot of dynamicLights) {
        if (slot.busy) continue;
        slot.busy = true;
        slot.light.color.setHex(colorHex);
        slot.light.intensity = intensity;
        slot.light.distance = distance;
        return slot;
    }
    return null;
}

function releaseDynamicLight(slot) {
    if (!slot) return;
    slot.busy = false;
    slot.light.intensity = 0;
}

function clearLamps() {
    for (const lamp of lamps) {
        scene.remove(lamp.halo);
        scene.remove(lamp.core);
        lamp.halo.material.dispose();
        lamp.core.material.dispose();
    }
    lamps.length = 0;
    for (const type of ['flicker', 'spotlight']) {
        for (const slot of lightSlots[type]) {
            slot.lamp = null;
            slot.fade = 0;
            slot.releasing = false;
            slot.light.intensity = 0;
        }
    }
}

function registerLamp(src) {
    const isSpot = src.type === 'spotlight';
    const ref = isSpot ? SPOT_COLOUR_REF : FLICKER_COLOUR_REF;
    const maxComp = Math.max(src.r, src.g, src.b);

    // Keep the authored hue at full saturation; the authored magnitude only sets brightness
    const color = new THREE.Color();
    if (maxComp > 1e-4) {
        color.setRGB(src.r / maxComp, src.g / maxComp, src.b / maxComp, THREE.SRGBColorSpace);
    } else {
        color.setRGB(1.0, 0.7, 0.4, THREE.SRGBColorSpace);
    }
    const brightness = THREE.MathUtils.clamp(maxComp / ref, 0.6, 1.15);

    const dir = new THREE.Vector3(src.dx, src.dy, src.dz);
    if (dir.lengthSq() < 1e-6) dir.set(0, -1, 0);
    dir.normalize();

    const coreColor = color.clone().lerp(new THREE.Color(1, 1, 1), 0.55);
    const halo = new THREE.Sprite(new THREE.SpriteMaterial({
        map: haloTexture, color, transparent: true, opacity: 0,
        blending: THREE.AdditiveBlending, depthWrite: false, fog: false
    }));
    const core = new THREE.Sprite(new THREE.SpriteMaterial({
        map: coreTexture, color: coreColor, transparent: true, opacity: 0,
        blending: THREE.AdditiveBlending, depthWrite: false, fog: false
    }));
    const pos = new THREE.Vector3(src.x, src.y + 40, src.z);
    halo.position.copy(pos);
    core.position.copy(pos);
    halo.visible = false;
    core.visible = false;
    halo.renderOrder = 10;
    core.renderOrder = 11;
    scene.add(halo);
    scene.add(core);

    lamps.push({
        type: isSpot ? 'spotlight' : 'flicker',
        pos, dir, color, brightness,
        phase: Math.random() * Math.PI * 2,
        haloSize: isSpot ? 170 : 340,
        coreSize: isSpot ? 55 : 120,
        halo, core,
        slot: null,
        dist: Infinity,
        flicker: 1
    });
}

function flickerValue(lamp, t) {
    const p = lamp.phase;
    if (lamp.type === 'spotlight') {
        return 0.97 + 0.03 * Math.sin(t * 2.0 + p);
    }
    return 0.86
        + 0.07 * Math.sin(t * 9.0 + p)
        + 0.045 * Math.sin(t * 23.0 + p * 1.7)
        + 0.03 * Math.sin(t * 47.0 + p * 0.6)
        + 0.06 * Math.sin(t * 3.1 + p) * Math.sin(t * 5.3 + p * 2.0);
}

function smoothstep01(edge0, edge1, x) {
    const k = THREE.MathUtils.clamp((x - edge0) / (edge1 - edge0), 0, 1);
    return k * k * (3 - 2 * k);
}

function refreshLightSlots(type, slots) {
    const nearest = [];
    for (const lamp of lamps) {
        if (lamp.type === type && lamp.dist <= LIGHT_RANGE) nearest.push(lamp);
    }
    nearest.sort((a, b) => a.dist - b.dist);
    const wanted = new Set(nearest.slice(0, slots.length));

    for (const slot of slots) {
        if (slot.lamp && !wanted.has(slot.lamp)) slot.releasing = true;
    }
    for (const lamp of wanted) {
        if (lamp.slot) {
            lamp.slot.releasing = false;
            continue;
        }
        const free = slots.find((s) => !s.lamp);
        if (free) {
            free.lamp = lamp;
            free.fade = 0;
            free.releasing = false;
            lamp.slot = free;
            free.light.color.copy(lamp.color);
            if (lamp.type === 'spotlight') {
                free.light.position.copy(lamp.pos);
                free.light.target.position.copy(lamp.pos).addScaledVector(lamp.dir, 100);
                free.light.target.updateMatrixWorld();
            }
        }
    }
}

// Lights the lamp_post objects closest to the player (within LIGHT_RANGE) with their authored light type/colour.
function updateLighting(t, delta, playerPos) {
    const rangeSq = LIGHT_RANGE * LIGHT_RANGE;
    for (const lamp of lamps) {
        const distSq = lamp.pos.distanceToSquared(playerPos);

        // Lamps out of range need no flicker or sprite work
        if (distSq > rangeSq) {
            lamp.dist = Infinity;
            if (lamp.halo.visible) {
                lamp.halo.visible = false;
                lamp.core.visible = false;
            }
            continue;
        }

        lamp.dist = Math.sqrt(distSq);
        lamp.flicker = flickerValue(lamp, t);

        // Glare sprites
        const fade = (1 - smoothstep01(LIGHT_RANGE * 0.6, LIGHT_RANGE, lamp.dist)) * smoothstep01(40, 160, lamp.dist);
        const f = lamp.flicker;
        const visible = fade > 0.01;
        lamp.halo.visible = visible;
        lamp.core.visible = visible;
        if (visible) {
            const haloScale = lamp.haloSize * (0.9 + 0.2 * f);
            const coreScale = lamp.coreSize * (0.85 + 0.3 * f);
            lamp.halo.scale.set(haloScale, haloScale, 1);
            lamp.core.scale.set(coreScale, coreScale, 1);
            lamp.halo.material.opacity = fade * Math.min(1, 0.45 + 0.5 * f) * lamp.brightness;
            lamp.core.material.opacity = fade * Math.min(1, 0.5 + 0.5 * f);
        }
    }

    lightRefreshTimer += delta;
    if (lightRefreshTimer >= LIGHT_REFRESH_INTERVAL) {
        lightRefreshTimer = 0;
        refreshLightSlots('flicker', lightSlots.flicker);
        refreshLightSlots('spotlight', lightSlots.spotlight);
    }

    for (const type of ['flicker', 'spotlight']) {
        const base = type === 'flicker' ? POINT_LIGHT_INTENSITY : SPOT_LIGHT_INTENSITY;
        for (const slot of lightSlots[type]) {
            const lamp = slot.lamp;
            if (!lamp) continue;

            slot.fade += (slot.releasing ? -1 : 1) * LIGHT_FADE_SPEED * delta;
            slot.fade = THREE.MathUtils.clamp(slot.fade, 0, 1);
            if (slot.releasing && slot.fade <= 0) {
                slot.light.intensity = 0;
                lamp.slot = null;
                slot.lamp = null;
                slot.releasing = false;
                continue;
            }

            const light = slot.light;
            if (type === 'flicker') {
                // Slight positional wobble makes the flame light dance across walls
                const p = lamp.phase;
                light.position.set(
                    lamp.pos.x + Math.sin(t * 11.0 + p) * 3,
                    lamp.pos.y + Math.sin(t * 17.0 + p * 1.3) * 4,
                    lamp.pos.z + Math.cos(t * 13.0 + p * 0.7) * 3
                );
            }
            light.intensity = base * lamp.brightness * lamp.flicker * slot.fade;
        }
    }
}

async function init() {
    const container = document.getElementById('canvas-container');

    // Scene setup
    scene = new THREE.Scene();
    scene.background = new THREE.Color(0x1a1a24);

    // Camera setup (far plane just past the fog so depth precision stays high)
    camera = new THREE.PerspectiveCamera(75, window.innerWidth / window.innerHeight, 1, 3600);

    // Ambient, directional and lamp light pools
    setupSceneLighting();

    // Renderer setup
    renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'high-performance' });
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    renderer.setSize(window.innerWidth, window.innerHeight);
    container.appendChild(renderer.domElement);
    maxAnisotropy = Math.min(8, renderer.capabilities.getMaxAnisotropy());

    // Pointer Lock Controls
    controls = new PointerLockControls(camera, document.body);

    const instructions = document.getElementById('instructions');
    instructions.addEventListener('click', () => {
        resumeAudio();
        controls.lock();
    });

    controls.addEventListener('lock', () => {
        instructions.style.display = 'none';
    });

    const releaseInputs = () => {
        moveForward = moveBackward = moveLeft = moveRight = moveUp = moveDown = false;
        isSprinting = false;
    };

    controls.addEventListener('unlock', () => {
        instructions.style.display = 'block';
        releaseInputs(); // keyup events are not delivered once focus leaves the page
    });
    window.addEventListener('blur', releaseInputs);

    scene.add(controls.getObject());

    // Keyboard Listeners
    const onKeyDown = function (event) {
        // Held keys must not retrigger toggles (fly mode, weapon cycling, normal maps)
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
                    jumpRequested = true;
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
                if (controls && controls.getObject()) {
                    createPickupFloatingText('ALL WEAPONS & SPELLS UNLOCKED', controls.getObject().position, '#facc15');
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
        if (event.button === 0 && controls && controls.isLocked) {
            triggerWeaponAttack();
        }
    });

    window.addEventListener('resize', onWindowResize);

    // Load Game Data & Build Map
    try {
        await loadGameWorld();
    } catch (e) {
        console.error('Failed to load the dungeon:', e);
        hudEl('instructions-prompt').textContent = 'Failed to load the dungeon. Check the console for details.';
        return;
    }

    // Initialize Active Weapon
    await updateWeaponMesh();

    preloadSounds([
        ...SWISH_SOUNDS, ...IMPACT_SOUNDS, ...PLAYER_IMPACT_SOUNDS,
        'spell1.wav', 'spell2.wav', 'spell3.wav', 'fireball.wav', 'explod1.wav', 'explod2.wav',
        'walk.wav', 'die.wav', 'death1.wav', 'death2.wav', 'death3.wav', 'death4.wav',
        ...monsters.map(m => m.config.sound)
    ]);

    window.camera = camera;
    window.renderer = renderer;
    window.controls = controls;
    window.scene = scene;
    window.monsters = monsters;
    window.modelBoxes = modelBoxes;
    window.resolveMonsterCapsuleCollisions = resolveMonsterCapsuleCollisions;
    window.doors = doors;
    window.openNearbyDoor = openNearbyDoor;
    window.activePickups = activePickups;
    window.playerStats = playerStats;
    window.damagePlayer = damagePlayer;
    Object.defineProperty(window, 'playerWeaponCurrentAnim', {
        get: () => playerWeaponCurrentAnim
    });

    prevTime = performance.now();
    hudEl('instructions-prompt').textContent = 'Click anywhere to Start & Lock Pointer';
    animate();
}

const fallbackTexture = new THREE.CanvasTexture(defaultCanvas);
fallbackTexture.wrapS = THREE.RepeatWrapping;
fallbackTexture.wrapT = THREE.RepeatWrapping;

function getTexture(texAlias, onLoadCallback) {
    if (!texAlias) return null;

    const existing = textureCache.get(texAlias);
    if (existing) {
        if (existing.loadedTex) {
            if (onLoadCallback) onLoadCallback(existing.loadedTex);
            return existing.loadedTex;
        }
        if (onLoadCallback) existing.callbacks.push(onLoadCallback);
        return fallbackTexture;
    }

    const cacheEntry = {
        loadedTex: null,
        isLoading: true,
        callbacks: onLoadCallback ? [onLoadCallback] : []
    };
    textureCache.set(texAlias, cacheEntry);

    // Image files are preferred over DDS when both exist (DDS is the last resort for a texture)
    const texPath = textureManager.getTexturePath(texAlias);
    const possiblePaths = new Set();
    if (texPath) {
        if (/\.dds$/i.test(texPath)) {
            const stem = texPath.replace(/\.dds$/i, '');
            possiblePaths.add(`Textures/${stem}.jpg`);
            possiblePaths.add(`Textures/${stem}.png`);
            possiblePaths.add(`Textures/${stem}.bmp`);
        }
        possiblePaths.add(`Textures/${texPath}`);
    }
    possiblePaths.add(`Textures/${texAlias}.jpg`);
    possiblePaths.add(`Textures/${texAlias}.png`);
    possiblePaths.add(`Textures/${texAlias}.bmp`);
    possiblePaths.add(`Textures/${texAlias}.dds`);

    loadTextureFromPaths([...possiblePaths], (loadedTex) => {
        cacheEntry.loadedTex = loadedTex;
        cacheEntry.isLoading = false;

        for (const cb of cacheEntry.callbacks) {
            cb(loadedTex);
        }
        cacheEntry.callbacks = [];
    });

    return fallbackTexture;
}

// Tries each path in order. Every texture is normalized to the same orientation:
// image-loader textures use flipY=true, but compressed DDS textures cannot be flipped on
// upload and keep the file's top row at v=0, so flip them through the UV transform instead.
function loadTextureFromPaths(paths, onLoad, index = 0) {
    if (index >= paths.length) return;
    const path = paths[index];
    const isDds = path.toLowerCase().endsWith('.dds');
    const loader = isDds ? ddsLoader : textureLoader;

    loader.load(
        path,
        (loadedTex) => {
            loadedTex.wrapS = THREE.RepeatWrapping;
            loadedTex.wrapT = THREE.RepeatWrapping;
            loadedTex.anisotropy = maxAnisotropy;
            if (isDds && !loadedTex.flipY) {
                loadedTex.repeat.y = -1;
                loadedTex.offset.y = 1;
            }
            onLoad(loadedTex);
        },
        undefined,
        () => loadTextureFromPaths(paths, onLoad, index + 1)
    );
}

// A texture "foo" has a normal map when a texture named "foo_nm" exists in textures.dat.
function getNormalMap(texAlias, onLoadCallback) {
    if (!texAlias) return;
    const nmAlias = `${texAlias.toLowerCase().trim()}_nm`;
    if (!textureManager.aliases.has(nmAlias)) return;

    if (normalMapCache.has(nmAlias)) {
        const cached = normalMapCache.get(nmAlias);
        if (cached.loadedTex) onLoadCallback(cached.loadedTex);
        else if (cached.isLoading) cached.callbacks.push(onLoadCallback);
        return;
    }

    const cacheEntry = { loadedTex: null, isLoading: true, callbacks: [onLoadCallback] };
    normalMapCache.set(nmAlias, cacheEntry);

    const texPath = textureManager.getTexturePath(nmAlias);
    const base = `Textures/${texPath.replace(/\.[^./]+$/, '')}`;
    const paths = [...new Set([`Textures/${texPath}`, `${base}.png`, `${base}.jpg`, `${base}.bmp`])];

    loadTextureFromPaths(paths, (loadedTex) => {
        // Normal maps hold data, not color
        loadedTex.colorSpace = THREE.NoColorSpace;
        cacheEntry.loadedTex = loadedTex;
        cacheEntry.isLoading = false;
        for (const cb of cacheEntry.callbacks) cb(loadedTex);
        cacheEntry.callbacks = [];
    });
}

// The normal maps are DirectX style (green channel points down). three.js builds the tangent
// frame from the UVs, so flipY=true textures (image files) need Y inverted, while DDS textures,
// which are flipped through the UV transform, are already in the right orientation.
function applyNormalMap(material, texAlias) {
    getNormalMap(texAlias, (normalTex) => {
        material.userData.normalMapTex = normalTex;
        normalMapMaterials.add(material);
        material.normalMap = normalMapsEnabled ? normalTex : null;
        material.normalScale.set(1, normalTex.flipY ? -1 : 1);
        material.needsUpdate = true;
    });
}

function setNormalMapsEnabled(enabled) {
    normalMapsEnabled = enabled;
    for (const material of normalMapMaterials) {
        material.normalMap = enabled ? material.userData.normalMapTex : null;
        material.needsUpdate = true;
    }
    const elem = document.getElementById('normalmap-display');
    if (elem) {
        elem.textContent = enabled ? 'ON' : 'OFF';
        elem.style.color = enabled ? '#00ff88' : '#ef4444';
    }
}

function generatePathVariations(filepath) {
    if (!filepath) return [];
    let clean = filepath.replace(/\\/g, '/');
    while (clean.startsWith('../') || clean.startsWith('./')) {
        clean = clean.substring(clean.indexOf('/') + 1);
    }
    if (clean.toLowerCase().startsWith('models/')) {
        clean = clean.substring(7);
    }

    const parts = clean.split('/');
    if (parts.length < 3) {
        return [
            `Models/${clean}`,
            `Models/${clean.toLowerCase()}`,
            `Models/${clean.toUpperCase()}`
        ];
    }

    const d1 = parts[0];
    const d2 = parts[1];
    const file = parts[2];

    const d2Cap = d2.charAt(0).toUpperCase() + d2.slice(1).toLowerCase();

    const dotIdx = file.lastIndexOf('.');
    const base = dotIdx !== -1 ? file.substring(0, dotIdx) : file;
    const ext = dotIdx !== -1 ? file.substring(dotIdx) : '';

    const paths = [
        `Models/${clean}`,
        `Models/Players/${d2Cap}/${base.toLowerCase()}${ext.toLowerCase()}`,
        `Models/Players/${d2Cap}/${base.toLowerCase()}${ext.toUpperCase()}`,
        `Models/Players/${d2.toLowerCase()}/${base.toLowerCase()}${ext.toLowerCase()}`,
        `Models/Players/${d2.toLowerCase()}/${base.toLowerCase()}${ext.toUpperCase()}`,
        `Models/Players/${d2.toLowerCase()}/${base}${ext}`,
        `Models/3ds/${d2Cap}/${base.toLowerCase()}${ext.toLowerCase()}`,
        `Models/3ds/${d2.toLowerCase()}/${base.toLowerCase()}${ext.toLowerCase()}`,
        // Last resort stand-in, only after every spelling of the real file has failed
        `Models/Players/MagicUser/WEAPONsorcerer.MD2`
    ];

    return [...new Set(paths)];
}

// Per-frame bounding box in the mesh's local space, computed once per frame and cached on the shared geometry
function getMD2FrameLocalBox(geometry, frameIndex) {
    let boxes = geometry.userData.frameBoxes;
    if (!boxes) boxes = geometry.userData.frameBoxes = [];

    let box = boxes[frameIndex];
    if (!box) {
        const morphPositions = geometry.morphAttributes && geometry.morphAttributes.position;
        const posAttr = (morphPositions && morphPositions.length > frameIndex)
            ? morphPositions[frameIndex]
            : geometry.attributes.position;
        box = boxes[frameIndex] = new THREE.Box3();
        if (posAttr) box.setFromBufferAttribute(posAttr);
    }
    return box;
}

function computeMD2FrameBoundingBox(mesh, frameIndex = 0, targetBox = null) {
    const box = targetBox || new THREE.Box3();
    const geometry = mesh.geometry;
    if (!geometry || !geometry.attributes.position) {
        box.makeEmpty();
        box.setFromObject(mesh);
        return box;
    }

    mesh.updateWorldMatrix(true, false);
    return box.copy(getMD2FrameLocalBox(geometry, frameIndex)).applyMatrix4(mesh.matrixWorld);
}

async function loadMD2Geometry(modelDef) {
    if (!modelDef || !modelDef.filepath) return null;
    const key = modelDef.filepath.toLowerCase();
    if (md2GeometryCache.has(key)) {
        return await md2GeometryCache.get(key);
    }

    const loadPromise = (async () => {
        const uniquePaths = generatePathVariations(modelDef.filepath);

        for (const path of uniquePaths) {
            try {
                const geometry = await new Promise((resolve, reject) => {
                    md2Loader.load(path, resolve, undefined, reject);
                });

                if (geometry) {
                    geometry.userData.shared = true;
                    return geometry;
                }
            } catch (e) {
                // Try next variant
            }
        }

        console.warn(`Could not load MD2 model at ${modelDef.filepath}`);
        return null;
    })();

    md2GeometryCache.set(key, loadPromise);
    return await loadPromise;
}

// 3DS textures are loaded by TDSLoader from files, so the texture name is only known once the
// image has loaded. Look for a "<name>_nm" texture at that point.
const tdsNormalMapChecked = new Set();
function applyNormalMapTo3DSMesh(child) {
    if (!child.isMesh) return;
    const materials = Array.isArray(child.material) ? child.material : [child.material];
    for (const material of materials) {
        const map = material && material.map;
        if (!map || tdsNormalMapChecked.has(material)) continue;
        tdsNormalMapChecked.add(material);

        const check = () => {
            const src = map.image && map.image.src;
            if (!src) return false;
            const file = decodeURIComponent(src.split('?')[0].split('/').pop());
            applyNormalMap(material, file.replace(/\.[^.]+$/, ''));
            return true;
        };

        if (!check()) {
            const prevOnUpdate = map.onUpdate;
            map.onUpdate = (...args) => {
                map.onUpdate = prevOnUpdate;
                if (prevOnUpdate) prevOnUpdate.apply(map, args);
                check();
            };
        }
    }
}

// Template meshes are cloned per placed instance, so every clone shares their geometry and materials
function markSharedMesh(child) {
    if (!child.isMesh) return;
    child.geometry.userData.shared = true;
    const materials = Array.isArray(child.material) ? child.material : [child.material];
    for (const material of materials) {
        material.userData.shared = true;
        material.side = THREE.DoubleSide;
    }
}

// Frees GPU resources of a removed object, skipping anything shared with other instances
function disposeOwnedResources(root) {
    root.traverse((child) => {
        if (!child.isMesh) return;
        if (child.geometry && !child.geometry.userData.shared) child.geometry.dispose();
        const materials = Array.isArray(child.material) ? child.material : [child.material];
        for (const material of materials) {
            if (material && !material.userData.shared) material.dispose();
        }
    });
}

// One Lambert material per texture alias, shared by all level geometry, models and weapons
function getSharedMaterial(texAlias) {
    const key = texAlias ? texAlias.toLowerCase().trim() : '';
    let material = sharedMaterials.get(key);
    if (material) return material;

    material = new THREE.MeshLambertMaterial({ side: THREE.DoubleSide });
    material.userData.shared = true;
    sharedMaterials.set(key, material);

    if (texAlias) {
        material.map = getTexture(texAlias, (loadedTex) => {
            material.map = loadedTex;
            material.needsUpdate = true;
        });
        applyNormalMap(material, texAlias);
    }
    return material;
}

async function load3DSModel(modelDef) {
    if (!modelDef || !modelDef.filepath) return null;
    const key = modelDef.filepath.toLowerCase();
    if (tdsModelCache.has(key)) {
        return await tdsModelCache.get(key);
    }

    const loadPromise = (async () => {
        const possiblePaths = [
            modelDef.filepath,
            modelDef.filepath.toLowerCase(),
            modelDef.filepath.replace(/^models\//i, 'Models/').replace(/\.3ds$/i, '.3DS'),
            modelDef.filepath.replace(/^textures\//i, 'Textures/').replace(/\.3ds$/i, '.3DS'),
            modelDef.filepath.replace(/^models\//i, 'Models/').replace(/\.3ds$/i, '.3ds'),
            modelDef.filepath.replace(/^textures\//i, 'Textures/').replace(/\.3ds$/i, '.3ds')
        ];
        const uniquePaths = [...new Set(possiblePaths)];

        for (const path of uniquePaths) {
            try {
                const lastSlash = path.lastIndexOf('/');
                const dir = lastSlash !== -1 ? path.substring(0, lastSlash + 1) : '';

                // TDSLoader resolves textures from its resource path while parsing, so models
                // loading concurrently each need their own loader instance
                const tdsLoader = new TDSLoader();
                tdsLoader.setResourcePath(dir);

                const object3ds = await new Promise((resolve, reject) => {
                    tdsLoader.load(path, resolve, undefined, reject);
                });

                if (object3ds) {
                    object3ds.traverse(applyNormalMapTo3DSMesh);
                    object3ds.traverse(markSharedMesh);
                    object3ds.rotation.set(-Math.PI / 2, 0, Math.PI / 2);
                    const wrapper = new THREE.Group();
                    wrapper.add(object3ds);
                    return wrapper;
                }
            } catch (e) {
                // Try next variant
            }
        }

        console.warn(`Could not load 3DS model at ${modelDef.filepath}`);
        return null;
    })();

    tdsModelCache.set(key, loadPromise);
    return await loadPromise;
}

const isModel3DS = (def) => def.format === '3DS' || def.filepath.toLowerCase().endsWith('.3ds');
const isModelMD2 = (def) => def.format === 'MD2' || def.filepath.toLowerCase().endsWith('.md2');

function getMonsterWeaponDef(modelDef) {
    if (!modelDef.monsterWeapon || modelDef.monsterWeapon.toUpperCase() === 'NONE') return null;
    return modelListParser.getModelByName(modelDef.monsterWeapon);
}

// Starts loading every distinct model the level uses at once; the build loop below then
// only awaits results that are already cached
function prefetchLevelModels(instances) {
    const seen = new Set();
    const loads = [];
    const request = (def) => {
        if (!def || seen.has(def)) return;
        seen.add(def);
        if (isModel3DS(def)) loads.push(load3DSModel(def));
        else if (isModelMD2(def)) loads.push(loadMD2Geometry(def));
    };
    for (const inst of instances) {
        if (!inst.modelName) continue;
        const def = modelListParser.getModelByName(inst.modelName);
        request(def);
        if (def) request(getMonsterWeaponDef(def));
    }
    for (const treasureName of TREASURE_DROPS) {
        const def = modelListParser.getModelByName(treasureName);
        request(def);
    }
    const openChestDef = modelListParser.getModelByName('cdooropenchest');
    if (openChestDef) {
        request(openChestDef);
    }
    return Promise.all(loads);
}

// Static level geometry is merged per texture into chunks of this size, so each chunk can be
// frustum/distance culled while thousands of tiny meshes collapse into a few draw calls
const STATIC_CHUNK_SIZE = 1024;

const _triEdge1 = new THREE.Vector3();
const _triEdge2 = new THREE.Vector3();

function makeCollisionTriangle(ax, ay, az, bx, by, bz, cx, cy, cz) {
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

function computeTriangleBounds(triangles) {
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

// Objects that never move do not need their matrices rebuilt every frame
function freezeStaticObject(root) {
    root.traverse((child) => {
        child.updateMatrix();
        child.matrixAutoUpdate = false;
    });
}

function createTexturedMesh(texAlias, positions, uvs) {
    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));
    geometry.setAttribute('uv', new THREE.Float32BufferAttribute(uvs, 2));
    geometry.computeVertexNormals();
    return new THREE.Mesh(geometry, getSharedMaterial(texAlias));
}

async function loadGameWorld() {
    const [, objectsMap, , level] = await Promise.all([
        textureManager.loadTexturesDat('bin/textures.dat'),
        objectsParser.parse('bin/Objects.dat'),
        modelListParser.parse('bin/modellist.dat'),
        levelParser.parse('bin/level1.map')
    ]);
    const { instances, startPos } = level;

    console.log(`Loaded ${instances.length} objects in level1.map. StartPos:`, startPos);

    await prefetchLevelModels(instances);

    // Reset level objects tracking
    levelObjects.length = 0;
    activePickups.length = 0;
    clearLamps();
    monsters.length = 0;
    levelTriangles.length = 0;
    modelBoxes.length = 0;
    doors.length = 0;
    chests.length = 0;

    // Set initial camera position
    playerStartPos = new THREE.Vector3(startPos.x, startPos.y, startPos.z);
    controls.getObject().position.copy(playerStartPos);

    // Look along level start direction
    const rotRad = (startPos.rot * Math.PI) / 180.0;
    camera.rotation.y = rotRad;

    const objectIdByName = new Map();
    for (const [id, obj] of objectsMap) {
        const name = obj.name && obj.name.toLowerCase().trim();
        if (name && !objectIdByName.has(name)) objectIdByName.set(name, id);
    }
    const getObjectId = (name) => (name ? objectIdByName.get(name.toLowerCase().trim()) ?? -1 : -1);


    // Merged static geometry: chunkKey -> { cx, cy, cz, textures: Map(alias -> { positions, uvs }) }
    const staticChunks = new Map();

    // Placeholder exclusion list: 6 (lamp_post), 35 (!monster1), 131 (startpos), 120 (text)
    const EXCLUDED_IDS = new Set([6, 35, 131, 120]);
    const EXCLUDED_NAMES = new Set(['lamp_post', '!monster1', 'startpos', 'text']);

    function isExcludedInstance(inst) {
        if (!inst) return true;
        if (inst.isStartPos) return true;
        if (inst.name) {
            const cleanName = inst.name.toLowerCase().trim();
            if (EXCLUDED_NAMES.has(cleanName)) return true;
        }
        return EXCLUDED_IDS.has(getObjectId(inst.name));
    }

    // Build map geometry
    let builtObjectCount = 0;
    let lightCount = 0;

    for (const inst of instances) {
        if (inst.isStartPos) continue;

        if (inst.light || inst.isLight) {
            lightCount++;
            registerLamp(inst.light || {
                type: 'flicker', x: inst.x, y: inst.y, z: inst.z,
                dx: 0, dy: -1, dz: 0, r: 10, g: 8, b: 5
            });
            continue;
        }

        // Check if item instance is a 3DS or MD2 model from modellist.dat FIRST
        // Placeholder objects (e.g. OBJECT !monster1) reference 3DS/MD2 models on their ROT_ANGLE line.
        // We MUST process and render the model, but NOT draw the placeholder geometry from Objects.dat.
        if (inst.modelName) {
            const modelDef = modelListParser.getModelByName(inst.modelName);
            if (modelDef) {
                const is3DSModel = isModel3DS(modelDef);
                const isMD2Model = isModelMD2(modelDef);

                if (is3DSModel) {
                    const template = await load3DSModel(modelDef);
                    if (template) {
                        const model = template.clone(true);
                        const scaleFactor = modelDef.scale || 1.0;
                        model.scale.set(scaleFactor, scaleFactor, scaleFactor);

                        model.position.set(inst.x, inst.y + 50, inst.z);

                        const angleRad = (inst.rot * Math.PI) / 180.0;
                        model.rotation.y = angleRad;

                        scene.add(model);
                        addLevelObject(model, new THREE.Vector3(inst.x, inst.y + 50, inst.z));
                        builtObjectCount++;

                        // Check if this 3DS model is a pickup item (checking modelDef.name, modelDef.filepath, inst.modelName, inst.name, or child mesh names)
                        let detectedPickupDef = getPickupDefinition(modelDef.name) || getPickupDefinition(inst.modelName) || getPickupDefinition(inst.name);
                        if (!detectedPickupDef && modelDef.filepath) {
                            const filename = modelDef.filepath.split('/').pop().replace(/\.[^.]+$/, '');
                            detectedPickupDef = getPickupDefinition(filename);
                        }
                        if (!detectedPickupDef) {
                            model.traverse(c => {
                                if (!detectedPickupDef && c.name) {
                                    detectedPickupDef = getPickupDefinition(c.name);
                                }
                            });
                        }

                        if (detectedPickupDef) {
                            addPickup(model, model.position, modelDef.name || inst.modelName || inst.name, detectedPickupDef);
                        } else {
                            model.updateMatrixWorld(true);
                            const box = new THREE.Box3().setFromObject(model);
                            if (!box.isEmpty()) {
                                modelBoxes.push(box);
                            }

                            const cleanModelName = (modelDef.name || inst.modelName || '').toLowerCase().trim();
                            if (cleanModelName === 'cdoorclosedchest' || cleanModelName === 'cdoorclosedchest.3ds') {
                                chests.push({
                                    model: model,
                                    position: model.position.clone(),
                                    rotationY: angleRad,
                                    scale: scaleFactor,
                                    isOpen: false,
                                    box: box
                                });
                            } else {
                                freezeStaticObject(model);
                            }
                        }
                        continue;
                    }
                } else if (isMD2Model) {
                    const geometry = await loadMD2Geometry(modelDef);
                    if (geometry) {
                        const scaleFactor = modelDef.scale || 1.0;
                        const texAlias = (inst.textureAlias && inst.textureAlias !== '-1') ? inst.textureAlias : modelDef.textureAlias;

                        const mesh = new THREE.Mesh(geometry, getSharedMaterial(texAlias));
                        mesh.scale.set(scaleFactor, scaleFactor, scaleFactor);
                        mesh.position.set(inst.x, inst.y + 50, inst.z);

                        const angleRad = (inst.rot * Math.PI) / 180.0;
                        mesh.rotation.y = angleRad - Math.PI / 2;

                        scene.add(mesh);
                        const meshEntry = addLevelObject(mesh, new THREE.Vector3(inst.x, inst.y + 50, inst.z));
                        builtObjectCount++;

                        if (modelDef.type === 'YOURGUN') {
                            // Display frame 87 for dungeon weapon models
                            if (mesh.morphTargetInfluences) {
                                for (let k = 0; k < mesh.morphTargetInfluences.length; k++) {
                                    mesh.morphTargetInfluences[k] = 0;
                                }
                                if (mesh.morphTargetInfluences.length > 87) {
                                    mesh.morphTargetInfluences[87] = 1.0;
                                } else if (mesh.morphTargetInfluences.length > 0) {
                                    mesh.morphTargetInfluences[0] = 1.0;
                                }
                            }

                            const pickupDefMD2 = getPickupDefinition(modelDef.name) || getPickupDefinition(inst.modelName) || getPickupDefinition(inst.name) || {
                                type: 'weapon',
                                amount: 50,
                                label: `+${modelDef.name}`
                            };

                            addPickup(mesh, mesh.position, modelDef.name || inst.modelName || inst.name, pickupDefMD2);
                            continue;
                        }

                        const pickupDefMD2 = getPickupDefinition(modelDef.name) || getPickupDefinition(inst.modelName) || getPickupDefinition(inst.name);
                        if (pickupDefMD2) {
                            addPickup(mesh, mesh.position, modelDef.name || inst.modelName || inst.name, pickupDefMD2);
                            continue; // an item is never also a monster
                        }

                        // Draw monster weapon if present
                        let weaponMesh = null;
                        let weaponEntry = null;
                        const weaponDef = getMonsterWeaponDef(modelDef);
                        if (weaponDef) {
                            if (isModelMD2(weaponDef)) {
                                const weaponGeom = await loadMD2Geometry(weaponDef);
                                if (weaponGeom) {
                                    const weaponScale = weaponDef.scale || scaleFactor;
                                    weaponMesh = new THREE.Mesh(weaponGeom, getSharedMaterial(weaponDef.textureAlias));
                                    weaponMesh.scale.set(weaponScale, weaponScale, weaponScale);
                                    weaponMesh.position.set(inst.x, inst.y + 50, inst.z);
                                    weaponMesh.rotation.y = angleRad - Math.PI / 2;
                                    scene.add(weaponMesh);
                                    weaponEntry = addLevelObject(weaponMesh, new THREE.Vector3(inst.x, inst.y + 50, inst.z));
                                }
                            } else if (isModel3DS(weaponDef)) {
                                const weaponTemplate = await load3DSModel(weaponDef);
                                if (weaponTemplate) {
                                    weaponMesh = weaponTemplate.clone(true);
                                    const weaponScale = weaponDef.scale || scaleFactor;
                                    weaponMesh.scale.set(weaponScale, weaponScale, weaponScale);
                                    weaponMesh.position.set(inst.x, inst.y + 50, inst.z);
                                    weaponMesh.rotation.y = angleRad;
                                    scene.add(weaponMesh);
                                    weaponEntry = addLevelObject(weaponMesh, new THREE.Vector3(inst.x, inst.y + 50, inst.z));
                                }
                            }
                        }

                        // Instantiate Monster state machine controller
                        const mConfig = getMonsterConfig(modelDef.name);
                        const posVec = new THREE.Vector3(inst.x, inst.y + 50, inst.z);
                        const monster = new Monster(mesh, weaponMesh, mConfig, posVec, angleRad);
                        monsters.push(monster);

                        // Distance culling follows the monster as it moves, not where it spawned
                        meshEntry.position = monster.position;
                        if (weaponEntry) weaponEntry.position = monster.position;
                        continue;
                    }
                }
            }
        }

        // Do not draw or collide with excluded placeholder objects from Objects.dat
        if (isExcludedInstance(inst)) {
            continue;
        }

        const objId = getObjectId(inst.name);
        const objDef = objectsMap.get(objId);

        if (!objDef) {
            continue;
        }

        builtObjectCount++;

        const isDoor55 = (inst.name && inst.name.toLowerCase().trim() === 'door55') ||
                         (objDef && objDef.name && objDef.name.toLowerCase().trim() === 'door55');
        const pickupDefObj = isDoor55 ? null : (getPickupDefinition(inst.name) || getPickupDefinition(objDef.name));
        const doorTriangles = [];

        const angleRad = (inst.rot * Math.PI) / 180.0;
        const cosA = Math.cos(angleRad);
        const sinA = Math.sin(angleRad);

        // Doors and pickups move, so they keep their own meshes. Pickups are built around their
        // own origin so they can spin and rise in place; doors are built in world space.
        // Everything else is merged into the shared static chunks.
        const isDynamic = isDoor55 || !!pickupDefObj;
        let texBuckets;
        if (isDynamic) {
            texBuckets = new Map();
        } else {
            const cx = Math.floor(inst.x / STATIC_CHUNK_SIZE);
            const cy = Math.floor(inst.y / STATIC_CHUNK_SIZE);
            const cz = Math.floor(inst.z / STATIC_CHUNK_SIZE);
            const chunkKey = `${cx},${cy},${cz}`;
            texBuckets = staticChunks.get(chunkKey);
            if (!texBuckets) {
                texBuckets = new Map();
                staticChunks.set(chunkKey, texBuckets);
            }
        }

        for (const poly of objDef.polys) {
            const texAlias = poly.texture || 'wall1';
            let bucket = texBuckets.get(texAlias);
            if (!bucket) {
                bucket = { positions: [], uvs: [] };
                texBuckets.set(texAlias, bucket);
            }

            const verts = poly.verts;
            const vertCount = verts.length / OBJECT_VERT_STRIDE;
            const world = new Array(vertCount * 3);

            for (let k = 0; k < vertCount; k++) {
                const o = k * OBJECT_VERT_STRIDE;
                const x = verts[o];
                const y = verts[o + 1];
                const z = verts[o + 2];

                const wx = x * cosA - z * sinA + inst.x;
                const wy = y + inst.y;
                const wz = x * sinA + z * cosA + inst.z;
                world[k * 3] = wx;
                world[k * 3 + 1] = wy;
                world[k * 3 + 2] = wz;

                if (pickupDefObj) {
                    bucket.positions.push(x, y, z);
                } else {
                    bucket.positions.push(wx, wy, wz);
                }
                bucket.uvs.push(verts[o + 3], verts[o + 4]);
            }

            // Collectable items are not solid; every polygon is already a triangle list
            if (pickupDefObj) continue;
            for (let k = 0; k + 2 < vertCount; k += 3) {
                const j = k * 3;
                const tri = makeCollisionTriangle(
                    world[j], world[j + 1], world[j + 2],
                    world[j + 3], world[j + 4], world[j + 5],
                    world[j + 6], world[j + 7], world[j + 8]);
                if (isDoor55) {
                    doorTriangles.push(tri);
                } else {
                    levelTriangles.push(tri);
                }
            }
        }

        if (!isDynamic) continue;

        const objGroup = new THREE.Group();
        for (const [texAlias, data] of texBuckets) {
            if (data.positions.length === 0) continue;
            objGroup.add(createTexturedMesh(texAlias, data.positions, data.uvs));
        }
        if (pickupDefObj) {
            objGroup.position.set(inst.x, inst.y, inst.z);
            objGroup.rotation.y = -angleRad;
        }

        scene.add(objGroup);
        const levelObj = addLevelObject(objGroup, new THREE.Vector3(inst.x, inst.y, inst.z));

        if (pickupDefObj) {
            addPickup(objGroup, levelObj.position, inst.name || objDef.name, pickupDefObj);
        }

        if (isDoor55) {
            doors.push({
                mesh: objGroup,
                position: new THREE.Vector3(inst.x, inst.y, inst.z),
                currentUp: 0,
                maxUp: 200.0,
                isOpening: false,
                isOpen: false,
                triangles: doorTriangles,
                bounds: computeTriangleBounds(doorTriangles),
                levelObj: levelObj
            });
        }
    }

    // One mesh per texture per chunk: far fewer draw calls, and each chunk culls on its own
    for (const texBuckets of staticChunks.values()) {
        for (const [texAlias, data] of texBuckets) {
            if (data.positions.length === 0) continue;
            const mesh = createTexturedMesh(texAlias, data.positions, data.uvs);
            mesh.geometry.computeBoundingSphere();
            mesh.matrixAutoUpdate = false;
            scene.add(mesh);
            const sphere = mesh.geometry.boundingSphere;
            addLevelObject(mesh, sphere.center.clone(), sphere.radius);
        }
    }

    // Build spatial grid for static level triangles
    buildCollisionGrid();

    hudEl('obj-display').textContent = builtObjectCount;
    hudEl('light-display').textContent = lightCount;
    updateModeDisplay();
}

function toggleFlyMode(forceState) {
    if (typeof forceState === 'boolean') {
        isFlyMode = forceState;
    } else {
        isFlyMode = !isFlyMode;
    }
    velocity.set(0, 0, 0);
    moveUp = false;
    moveDown = false;
    updateModeDisplay();
}

function updateModeDisplay() {
    const modeElem = document.getElementById('mode-display');
    if (modeElem) {
        modeElem.textContent = isFlyMode ? 'FLY MODE' : 'FPS (WALK)';
        modeElem.style.color = isFlyMode ? '#00ffff' : '#00ff88';
    }
}

function getGridKey(cx, cy, cz) {
    // Cells are offset to be non-negative (+/-512 cells = +/-131k world units) and packed into one number,
    // which avoids building a string key for every grid lookup
    return ((cx + 512) * 1024 + (cy + 512)) * 1024 + (cz + 512);
}

function buildCollisionGrid() {
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

// Collects the static triangles (grid) and moving door triangles whose bounds overlap the query box.
// Triangles spanning several grid cells are de-duplicated with a per-query stamp.
function gatherNearbyTriangles(minX, maxX, minY, maxY, minZ, maxZ) {
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

    // Doors move, so they stay out of the static grid and are tested directly
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

function gatherNearbyBoxes(minX, maxX, minY, maxY, minZ, maxZ) {
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

// Geometric Collision Math
const _v10 = new THREE.Vector3();
const _v20 = new THREE.Vector3();
const _vp0 = new THREE.Vector3();
const _vp1 = new THREE.Vector3();
const _vp2 = new THREE.Vector3();
const _tempDiff = new THREE.Vector3();

function closestPointSegmentPoint(a, b, p, target) {
    const ab = _v10.subVectors(b, a);
    const abLenSq = ab.lengthSq();
    if (abLenSq < 1e-8) {
        return target.copy(a);
    }
    const t = Math.max(0, Math.min(1, _vp0.subVectors(p, a).dot(ab) / abLenSq));
    return target.copy(a).addScaledVector(ab, t);
}

function closestPointTrianglePoint(v0, v1, v2, p, target) {
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

function closestPointsSegmentSegment(a, b, c, d, target1, target2) {
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

// Closest points between a capsule axis (capA-capB) and a triangle.
// If the axis passes straight through the triangle the closest points are not
// on its endpoints or edges, so that case is detected first and reported as a
// negative distance (how far the nearest axis end must travel to clear the plane).
function closestPointsCapsuleTriangle(capA, capB, tri) {
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
                // Leave through the side the far end is on, so the near end only has to cross the plane
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

// Capsule axes in this game are always vertical, so the closest points to an
// AABB can be solved exactly instead of sampling along the axis.
function closestPointsCapsuleBox(capA, capB, box) {
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

const MONSTER_CAPSULE_RADIUS = 24.0;
const MONSTER_CAPSULE_HEIGHT = 80.0;
const MONSTER_FOOT_OFFSET = 50.0; // monster.position sits this far above its feet
const STAND_ON_MAX_LIFT = 20.0;   // feet this close to a monster's head stand on it instead of being pushed sideways

const _capA = new THREE.Vector3();
const _capB = new THREE.Vector3();
const _bestNormal = new THREE.Vector3();
const _contactNormal = new THREE.Vector3();

// Penetration of the vertical capsule axis point (px, pz) against another
// upright cylinder. Writes the push-out direction to _contactNormal: horizontal,
// or straight up when `standOn` is set and the feet are just below the other
// body's top, so a player can jump over and land on monsters.
function cylinderContact(px, pz, baseY, topY, ox, oz, oBaseY, oTopY, sumRadius, standOn) {
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

// Shared capsule collide-and-slide for the player and monsters. Pushes `pos`
// out of level triangles, static model boxes and (optionally) other upright
// bodies, removing the velocity component into each contact. Only `pos` is
// ever moved, so a body never displaces what it touches. Returns true when
// standing on a walkable surface.
//   footOffset  - distance from pos.y down to the capsule's feet
//   selfMonster - monster being resolved (excluded from monsterList)
//   monsterList - monsters treated as immovable cylinders
//   playerPos   - player eye position treated as an immovable cylinder
function resolveCapsuleWorld(pos, footOffset, radius, height, vel, selfMonster, monsterList, playerPos) {
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
                    // Touching exactly: use the face normal, facing the capsule
                    _contactNormal.copy(faceNormal);
                    const midY = (_capA.y + _capB.y) * 0.5;
                    if (_contactNormal.x * (pos.x - res.triP.x) + _contactNormal.y * (midY - res.triP.y) +
                        _contactNormal.z * (pos.z - res.triP.z) < 0) {
                        _contactNormal.negate();
                    }
                }

                // Flat floors/ceilings push straight up/down so slight slopes don't make the
                // body creep sideways. Walls and steep faces keep their true contact normal,
                // otherwise a body beside a bar or ledge would be lifted onto it.
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
                // Axis is inside the box: leave through the nearest face (never downwards)
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

        // Dynamic bodies are immovable here: only this body gets pushed out
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

// Monster vs level geometry and static model boxes. Pass `otherMonsters` and
// `playerPos` (while the monster is walking) to also stop it from walking into
// them; those bodies are never moved by this call.
function resolveMonsterCapsuleCollisions(monster, otherMonsters = null, playerPos = null) {
    if (!monster || !monster.position) return;
    monster.isGrounded = resolveCapsuleWorld(
        monster.position, MONSTER_FOOT_OFFSET, MONSTER_CAPSULE_RADIUS, MONSTER_CAPSULE_HEIGHT,
        monster.velocity, monster, otherMonsters, playerPos);
}

// Player vs level geometry, static model boxes and monsters. Monsters are
// immovable here, so the player is never shoved by them (and vice versa).
function resolveCapsuleCollisions(camPos) {
    isGrounded = resolveCapsuleWorld(
        camPos, EYE_HEIGHT, CAPSULE_RADIUS, CAPSULE_HEIGHT,
        velocity, null, monsters, null);
}

let footstepTimer = 0.0;
const activeProjectiles = [];

function triggerWeaponAttack() {
    if (!controls || playerDead) return;
    const gunName = PLAYER_GUNS[currentGunIndex] || '';

    if (spellCounts[gunName] !== undefined) {
        if (spellCounts[gunName] <= 0) {
            playSound('buttonselect111.wav', 0.5);
            return;
        }
        spellCounts[gunName]--;
        updatePlayerHud();
    }

    isWeaponAttacking = true;
    hasWeaponHitThisSwing = false;
    playerWeaponAnimTimer = 0.0;
    playerWeaponCurrentAnim = MD2_ANIMATIONS.ATTACK;

    if (gunName === 'MAGIC MISSILE') {
        hasWeaponHitThisSwing = true; // Handled by projectile
        playSound('spell1.wav', 0.7);
        launchProjectile('MAGIC MISSILE');
    } else if (gunName === 'FIREBALL') {
        hasWeaponHitThisSwing = true; // Handled by projectile
        playSound('fireball.wav', 0.8);
        launchProjectile('FIREBALL');
    } else if (gunName === 'LIGHTNING') {
        hasWeaponHitThisSwing = true; // Handled by projectile
        playSound('spell3.wav', 0.8);
        launchProjectile('LIGHTNING');
    } else if (gunName === 'HEALING') {
        hasWeaponHitThisSwing = true; // Handled by healing spell
        playSound('spell2.wav', 0.8);
        castHealingSpell();
    } else {
        playSound(getRandomSound(SWISH_SOUNDS), 0.5);
    }
}

function castHealingSpell() {
    healPlayer(10);
    const playerPos = controls.getObject().position;
    createPickupFloatingText('+10 HP', playerPos, '#22c55e');

    // Healing ring / aura rising around the player
    for (let i = 0; i < 40; i++) {
        const angle = Math.random() * Math.PI * 2;
        const rad = 20 + Math.random() * 30;
        emitParticle(
            playerPos.x + Math.cos(angle) * rad,
            playerPos.y - 60 + Math.random() * 20,
            playerPos.z + Math.sin(angle) * rad,
            (Math.random() - 0.5) * 20,
            120 + Math.random() * 100,
            (Math.random() - 0.5) * 20,
            0x4ade80, 10, 1.0);
    }

    // Subtle green screen vignette flash
    flashScreen(FLASH_GREEN, 0.6, 200);
}

// Spell projectiles. Geometry and materials are shared per type, and the glow light comes from the pooled dynamic lights.
const PROJECTILE_TYPES = {
    'MAGIC MISSILE': {
        speed: 1300.0, gravity: 0, radius: 20.0, aoeRadius: 120.0, maxLife: 4.0,
        coreRadius: 12, coreColor: 0x38bdf8, coreOpacity: 0.9,
        haloColor: 0x00e5ff, haloOpacity: 0.8, haloSize: 70,
        lightColor: 0x00e5ff, lightIntensity: 6000, lightDistance: 350,
        colorHex: 0x38bdf8
    },
    'FIREBALL': {
        speed: 1050.0, gravity: -200.0, radius: 26.0, aoeRadius: 220.0, maxLife: 4.5,
        coreRadius: 18, coreColor: 0xff4500, coreOpacity: 0.95,
        haloColor: 0xffaa00, haloOpacity: 0.9, haloSize: 110,
        lightColor: 0xff6600, lightIntensity: 9000, lightDistance: 450,
        colorHex: 0xff5500
    },
    'LIGHTNING': {
        speed: 1600.0, gravity: 0, radius: 22.0, aoeRadius: 160.0, maxLife: 3.5,
        coreRadius: 14, coreColor: 0xffffff, coreOpacity: 0.95,
        haloColor: 0x00ffff, haloOpacity: 0.9, haloSize: 90,
        lightColor: 0x00ffff, lightIntensity: 8000, lightDistance: 400,
        colorHex: 0x00ffff
    }
};
const projectileResources = new Map();

function getProjectileResources(type) {
    let res = projectileResources.get(type);
    if (!res) {
        const def = PROJECTILE_TYPES[type];
        res = {
            coreGeometry: new THREE.SphereGeometry(def.coreRadius, 12, 8),
            coreMaterial: new THREE.MeshBasicMaterial({ color: def.coreColor, transparent: true, opacity: def.coreOpacity }),
            haloMaterial: new THREE.SpriteMaterial({
                map: haloTexture,
                color: def.haloColor,
                transparent: true,
                opacity: def.haloOpacity,
                blending: THREE.AdditiveBlending,
                depthWrite: false
            })
        };
        projectileResources.set(type, res);
    }
    return res;
}

function launchProjectile(type) {
    const def = PROJECTILE_TYPES[type];
    const res = getProjectileResources(type);

    const lookDir = new THREE.Vector3();
    camera.getWorldDirection(lookDir);

    // Spawn origin slightly in front of camera
    const spawnPos = controls.getObject().position.clone().addScaledVector(lookDir, 40);
    spawnPos.y -= 10;

    const group = new THREE.Group();
    group.position.copy(spawnPos);
    group.add(new THREE.Mesh(res.coreGeometry, res.coreMaterial));
    const haloSprite = new THREE.Sprite(res.haloMaterial);
    haloSprite.scale.set(def.haloSize, def.haloSize, 1);
    group.add(haloSprite);
    scene.add(group);

    const lightSlot = acquireDynamicLight(def.lightColor, def.lightIntensity, def.lightDistance);
    if (lightSlot) lightSlot.light.position.copy(spawnPos);

    activeProjectiles.push({
        type,
        group,
        lightSlot,
        position: spawnPos,
        velocity: lookDir.multiplyScalar(def.speed),
        gravity: def.gravity,
        radius: def.radius,
        aoeRadius: def.aoeRadius,
        damage: WEAPON_DAMAGE[type],
        lifeTimer: 0,
        maxLife: def.maxLife,
        colorHex: def.colorHex
    });
}

function triggerSpellExplosion(pos, type, damage, aoeRadius, colorHex) {
    const explosionSound = (type === 'FIREBALL') ? 'explod1.wav' : 'explod2.wav';
    playSound(explosionSound, 0.85);

    // Spawn explosion particle burst
    const particleCount = (type === 'FIREBALL') ? 60 : 40;
    for (let i = 0; i < particleCount; i++) {
        const theta = Math.random() * Math.PI * 2;
        const phi = Math.random() * Math.PI - Math.PI / 2;
        const speed = 120 + Math.random() * 250;
        emitParticle(
            pos.x, pos.y, pos.z,
            Math.cos(phi) * Math.cos(theta) * speed,
            Math.sin(phi) * speed,
            Math.cos(phi) * Math.sin(theta) * speed,
            colorHex || 0xffaa00, 14, 0.6);
    }

    // Bright flash light, faded out by updateExplosionFlashes
    const flashSlot = acquireDynamicLight(colorHex || 0xffaa00, EXPLOSION_FLASH_INTENSITY, aoeRadius * 2);
    if (flashSlot) {
        flashSlot.light.position.copy(pos);
        explosionFlashes.push({ slot: flashSlot, timer: 0 });
    }

    // Damage all monsters within AoE radius
    for (let i = 0; i < monsters.length; i++) {
        const monster = monsters[i];
        if (!monster || monster.state === MonsterState.DEATH || monster.health <= 0) continue;

        const dist = pos.distanceTo(monster.position);
        if (dist <= aoeRadius) {
            // Full damage at center, scaled slightly at edge
            const scale = Math.max(0.5, 1.0 - (dist / aoeRadius) * 0.5);
            const actualDamage = Math.round(damage * scale);
            monster.takeDamage(actualDamage);
            playSound(getRandomSound(IMPACT_SOUNDS), 0.7);
        }
    }
}

const EXPLOSION_FLASH_INTENSITY = 15000;
const EXPLOSION_FLASH_DURATION = 0.3;
const explosionFlashes = [];

function updateExplosionFlashes(delta) {
    for (let i = explosionFlashes.length - 1; i >= 0; i--) {
        const flash = explosionFlashes[i];
        flash.timer += delta;
        if (flash.timer >= EXPLOSION_FLASH_DURATION) {
            releaseDynamicLight(flash.slot);
            explosionFlashes.splice(i, 1);
        } else {
            flash.slot.light.intensity = EXPLOSION_FLASH_INTENSITY * (1 - flash.timer / EXPLOSION_FLASH_DURATION);
        }
    }
}

function removeProjectile(index) {
    const proj = activeProjectiles[index];
    scene.remove(proj.group);
    releaseDynamicLight(proj.lightSlot);
    activeProjectiles.splice(index, 1);
}

const _projNext = new THREE.Vector3();
const _projStep = new THREE.Vector3();
const _projSample = new THREE.Vector3();
const _projClosest = new THREE.Vector3();

// True when a projectile sphere at `p` touches a monster, level triangle or static model
function projectileHitsAt(p, radius) {
    const monsterReach = radius + MONSTER_CAPSULE_RADIUS;
    const monsterReachSq = monsterReach * monsterReach;
    for (let m = 0; m < monsters.length; m++) {
        const monster = monsters[m];
        if (monster.state === MonsterState.DEATH || monster.health <= 0) continue;
        if (p.distanceToSquared(monster.position) <= monsterReachSq) return true;
    }

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

function updateProjectiles(delta) {
    updateExplosionFlashes(delta);

    for (let i = activeProjectiles.length - 1; i >= 0; i--) {
        const proj = activeProjectiles[i];
        proj.lifeTimer += delta;

        if (proj.lifeTimer >= proj.maxLife) {
            triggerSpellExplosion(proj.position, proj.type, proj.damage, proj.aoeRadius, proj.colorHex);
            removeProjectile(i);
            continue;
        }

        // Apply physics movement
        proj.velocity.y += proj.gravity * delta;
        _projStep.copy(proj.velocity).multiplyScalar(delta);
        _projNext.copy(proj.position).add(_projStep);

        // Sample along the path so a fast projectile cannot jump through thin walls or monsters
        const segments = Math.min(8, Math.max(1, Math.ceil(_projStep.length() / proj.radius)));
        let hasExploded = false;
        for (let s = 1; s <= segments; s++) {
            _projSample.copy(proj.position).addScaledVector(_projStep, s / segments);
            if (projectileHitsAt(_projSample, proj.radius)) {
                hasExploded = true;
                _projNext.copy(_projSample);
                triggerSpellExplosion(_projNext, proj.type, proj.damage, proj.aoeRadius, proj.colorHex);
                break;
            }
        }

        if (hasExploded) {
            removeProjectile(i);
            continue;
        }

        // Update position and trail particles
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

function checkPlayerAttackHit() {
    if (!controls) return;
    const playerPos = controls.getObject().position;
    const playerDir = new THREE.Vector3();
    camera.getWorldDirection(playerDir);

    const ATTACK_HIT_RANGE = 140.0;
    let closestMonster = null;
    let closestDist = Infinity;

    const toMonster = new THREE.Vector3();

    for (let i = 0; i < monsters.length; i++) {
        const monster = monsters[i];
        if (!monster || monster.state === MonsterState.DEATH || monster.health <= 0) continue;

        toMonster.subVectors(monster.position, playerPos);
        const dist = toMonster.length();

        if (dist <= ATTACK_HIT_RANGE) {
            toMonster.normalize();
            const dot = playerDir.dot(toMonster);
            if (dot >= 0.5) { // Monster is in front of player
                if (dist < closestDist) {
                    closestDist = dist;
                    closestMonster = monster;
                }
            }
        }
    }

    if (closestMonster) {
        const gunName = PLAYER_GUNS[currentGunIndex] || '';
        const damage = WEAPON_DAMAGE[gunName] || 25;
        closestMonster.takeDamage(damage);
        playSound(getRandomSound(IMPACT_SOUNDS), 0.6);
    }
}

function updatePlayerWeaponAnimation(delta) {
    if (!currentGunMesh) return;

    const geometry = currentGunMesh.geometry;
    if (!geometry || !geometry.morphAttributes || !geometry.morphAttributes.position) return;

    const influences = currentGunMesh.morphTargetInfluences;
    if (!influences) return;

    // Determine target animation based on player state
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
        playerWeaponCurrentAnim = targetAnim;
        playerWeaponAnimTimer = 0.0;
    }

    playerWeaponAnimTimer += delta;

    const anim = playerWeaponCurrentAnim;
    const totalFrames = anim.end - anim.start + 1;
    const totalDuration = totalFrames / anim.fps;

    if (isWeaponAttacking && playerWeaponCurrentAnim === MD2_ANIMATIONS.ATTACK) {
        if (!hasWeaponHitThisSwing && playerWeaponAnimTimer >= totalDuration * 0.5) {
            hasWeaponHitThisSwing = true;
            checkPlayerAttackHit();
        }
    }

    if (!anim.loop && playerWeaponAnimTimer >= totalDuration) {
        if (isWeaponAttacking) {
            isWeaponAttacking = false;
            hasWeaponHitThisSwing = false;
            playerWeaponCurrentAnim = (moveForward || moveBackward || moveLeft || moveRight)
                ? MD2_ANIMATIONS.RUN
                : MD2_ANIMATIONS.STAND;
            playerWeaponAnimTimer = 0.0;
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

async function openChest(chest) {
    if (!chest || chest.isOpen) return;
    chest.isOpen = true;

    // Load cdooropenchest model
    const openDef = modelListParser.getModelByName('cdooropenchest');
    if (openDef) {
        const template = await load3DSModel(openDef);
        if (template) {
            const openModel = template.clone(true);
            const scaleFactor = openDef.scale || chest.scale || 1.0;
            openModel.scale.set(scaleFactor, scaleFactor, scaleFactor);
            openModel.position.copy(chest.position);
            openModel.rotation.y = chest.rotationY;

            // Remove old closed chest model from scene and levelObjects
            scene.remove(chest.model);
            const objIdx = levelObjects.findIndex(o => o.mesh === chest.model);
            if (objIdx !== -1) {
                levelObjects.splice(objIdx, 1);
            }
            disposeOwnedResources(chest.model);

            // Add new open chest model
            openModel.updateMatrixWorld(true);
            scene.add(openModel);
            addLevelObject(openModel, chest.position.clone());
            chest.model = openModel;

            // Update bounding box for collision
            const newBox = new THREE.Box3().setFromObject(openModel);
            if (!newBox.isEmpty()) {
                if (chest.box) {
                    chest.box.copy(newBox);
                } else {
                    modelBoxes.push(newBox);
                    chest.box = newBox;
                }
            }
        }
    }

    playSound('topen.wav', 0.8);

    // Force drop treasure from chest (100% chance)
    dropTreasure(chest.position, true);
}

function openNearbyDoor() {
    if (controls && !controls.isLocked) return;
    const playerPos = controls.getObject().position;
    const INTERACTION_DISTANCE = 250.0;
    const INTERACTION_DIST_SQ = INTERACTION_DISTANCE * INTERACTION_DISTANCE;

    let nearestDoor = null;
    let minBetween = INTERACTION_DIST_SQ;

    for (const door of doors) {
        if (door.isOpen || door.isOpening) continue;
        const distSq = playerPos.distanceToSquared(door.position);
        if (distSq <= minBetween) {
            minBetween = distSq;
            nearestDoor = door;
        }
    }

    if (nearestDoor) {
        nearestDoor.isOpening = true;
    }

    // Check for nearby closed chests
    let nearestChest = null;
    let minChestDist = INTERACTION_DIST_SQ;

    for (const chest of chests) {
        if (chest.isOpen) continue;
        const distSq = playerPos.distanceToSquared(chest.position);
        if (distSq <= minChestDist) {
            minChestDist = distSq;
            nearestChest = chest;
        }
    }

    if (nearestChest) {
        openChest(nearestChest);
    }
}

// Sound Manager Utility Functions & Sound Collections
export const SWISH_SOUNDS = [
    'swish1.wav', 'swish4.wav', 'swish5.wav',
    'swish7.wav', 'swish8.wav', 'swish9.wav', 'swing_02.wav'
];

export const IMPACT_SOUNDS = [
    'impact.wav', 'impact2.wav', 'impact3.wav', 'impact4.wav',
    'impact5.wav', 'impact6.wav', 'impact7.wav', 'impact8.wav',
    'impact10.wav'
];

export const PLAYER_IMPACT_SOUNDS = [
    'pimpact.wav', 'ouch.wav', 'pain1.wav'
];

// Sounds are fetched and decoded once, then played from memory (no per-play network/decode work)
const soundBuffers = new Map(); // path -> AudioBuffer | Promise<AudioBuffer|null> | null (failed)
const MAX_CONCURRENT_SOUNDS = 24;
const MAX_SOUND_LATENCY_MS = 750; // a sound that finishes loading later than this is no longer relevant
let activeSoundCount = 0;
let audioCtx = null;

function getAudioContext() {
    if (!audioCtx) {
        const AudioContextClass = window.AudioContext || window.webkitAudioContext;
        if (AudioContextClass) audioCtx = new AudioContextClass();
    }
    return audioCtx;
}

// Browsers keep audio suspended until a user gesture
function resumeAudio() {
    const ctx = getAudioContext();
    if (ctx && ctx.state === 'suspended') ctx.resume().catch(() => {});
}

function loadSoundBuffer(path) {
    const cached = soundBuffers.get(path);
    if (cached !== undefined) return cached;

    const ctx = getAudioContext();
    if (!ctx) return null;

    const pending = fetch(path)
        .then((resp) => {
            if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
            return resp.arrayBuffer();
        })
        .then((data) => ctx.decodeAudioData(data))
        .then((buffer) => {
            soundBuffers.set(path, buffer);
            return buffer;
        })
        .catch(() => {
            soundBuffers.set(path, null);
            return null;
        });
    soundBuffers.set(path, pending);
    return pending;
}

function startSound(buffer, volume) {
    if (activeSoundCount >= MAX_CONCURRENT_SOUNDS) return;
    const ctx = audioCtx;
    const source = ctx.createBufferSource();
    const gain = ctx.createGain();
    source.buffer = buffer;
    gain.gain.value = Math.max(0, Math.min(1, volume));
    source.connect(gain);
    gain.connect(ctx.destination);

    activeSoundCount++;
    source.onended = () => {
        activeSoundCount--;
        source.disconnect();
        gain.disconnect();
    };
    source.start();
}

export function playSound(filename, volume = 1.0) {
    if (!filename) return;
    try {
        const ctx = getAudioContext();
        if (!ctx || ctx.state !== 'running') return; // autoplay is blocked before user interaction

        const path = filename.startsWith('Sounds/') ? filename : `Sounds/${filename}`;
        const entry = loadSoundBuffer(path);
        if (!entry) return;
        if (entry instanceof AudioBuffer) {
            startSound(entry, volume);
        } else {
            const requestedAt = performance.now();
            entry.then((buffer) => {
                if (buffer && performance.now() - requestedAt <= MAX_SOUND_LATENCY_MS) startSound(buffer, volume);
            });
        }
    } catch (e) {
        // Audio is optional
    }
}

function preloadSounds(filenames) {
    for (const name of new Set(filenames)) {
        if (name) loadSoundBuffer(name.startsWith('Sounds/') ? name : `Sounds/${name}`);
    }
}

export function getRandomSound(soundArray) {
    if (!soundArray || soundArray.length === 0) return null;
    const index = Math.floor(Math.random() * soundArray.length);
    return soundArray[index];
}

function playPickupSound(isHealth = true) {
    try {
        const ctx = getAudioContext();
        if (!ctx) return;
        resumeAudio();
        const now = audioCtx.currentTime;
        const osc1 = audioCtx.createOscillator();
        const osc2 = audioCtx.createOscillator();
        const gain = audioCtx.createGain();

        osc1.type = isHealth ? 'sine' : 'triangle';
        osc2.type = 'sine';

        // Arpeggio chime notes
        const freq1 = isHealth ? 523.25 : 659.25; // C5 / E5
        const freq2 = isHealth ? 659.25 : 880.00; // E5 / A5
        const freq3 = isHealth ? 783.99 : 1046.50; // G5 / C6

        osc1.frequency.setValueAtTime(freq1, now);
        osc1.frequency.setValueAtTime(freq2, now + 0.08);
        osc1.frequency.setValueAtTime(freq3, now + 0.16);

        osc2.frequency.setValueAtTime(freq1 * 1.5, now);
        osc2.frequency.setValueAtTime(freq2 * 1.5, now + 0.08);
        osc2.frequency.setValueAtTime(freq3 * 1.5, now + 0.16);

        gain.gain.setValueAtTime(0.3, now);
        gain.gain.exponentialRampToValueAtTime(0.001, now + 0.45);

        osc1.connect(gain);
        osc2.connect(gain);
        gain.connect(audioCtx.destination);

        osc1.start(now);
        osc2.start(now);
        osc1.stop(now + 0.45);
        osc2.stop(now + 0.45);
    } catch (e) {
        // Audio playback error fallback
    }
}

// Visual Floating 3D Text & Particle Effects
const activeFloatingTexts = [];

const floatingTextTextures = new Map();

function getFloatingTextTexture(text, color) {
    const key = `${color}|${text}`;
    let texture = floatingTextTextures.get(key);
    if (texture) return texture;

    const canvas = document.createElement('canvas');
    canvas.width = 256;
    canvas.height = 128;
    const ctx = canvas.getContext('2d');

    ctx.font = '900 14px sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';

    ctx.strokeStyle = '#000000';
    ctx.lineWidth = 8;
    ctx.strokeText(text, 128, 64);

    ctx.fillStyle = color;
    ctx.fillText(text, 128, 64);

    texture = new THREE.CanvasTexture(canvas);
    floatingTextTextures.set(key, texture);
    return texture;
}

function createPickupFloatingText(text, pos, color = '#38bdf8') {
    // Each popup fades with its own opacity, but the label texture is shared
    const spriteMat = new THREE.SpriteMaterial({
        map: getFloatingTextTexture(text, color),
        transparent: true,
        opacity: 1.0,
        depthWrite: false,
        fog: false
    });

    const sprite = new THREE.Sprite(spriteMat);
    sprite.position.copy(pos).add(new THREE.Vector3(0, 30, 0));
    sprite.scale.set(120, 60, 1);
    scene.add(sprite);

    activeFloatingTexts.push({
        sprite,
        mat: spriteMat,
        timer: 0,
        duration: 1.2
    });
}

// All spell, pickup and trail particles live in one pooled Points object: no per-effect
// geometry or material allocation, and one draw call however many effects are active.
const MAX_PARTICLES = 3072;
const PARTICLE_GRAVITY = 180.0;
const particles = {
    positions: new Float32Array(MAX_PARTICLES * 3),
    colors: new Float32Array(MAX_PARTICLES * 3),
    sizes: new Float32Array(MAX_PARTICLES),
    velocities: new Float32Array(MAX_PARTICLES * 3),
    baseColors: new Float32Array(MAX_PARTICLES * 3),
    ages: new Float32Array(MAX_PARTICLES),
    lifetimes: new Float32Array(MAX_PARTICLES), // 0 marks a free slot
    next: 0,
    highWater: 0,
    alive: 0,
    points: null,
    material: null
};
const _particleColor = new THREE.Color();

function ensureParticleSystem() {
    if (particles.points) return;

    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.BufferAttribute(particles.positions, 3).setUsage(THREE.DynamicDrawUsage));
    geometry.setAttribute('color', new THREE.BufferAttribute(particles.colors, 3).setUsage(THREE.DynamicDrawUsage));
    geometry.setAttribute('size', new THREE.BufferAttribute(particles.sizes, 1).setUsage(THREE.DynamicDrawUsage));
    geometry.setDrawRange(0, 0);

    // Sizes attenuate with distance like PointsMaterial; additive blending lets colour fade stand in for opacity
    particles.material = new THREE.ShaderMaterial({
        uniforms: { scale: { value: window.innerHeight * 0.5 } },
        vertexShader: `
            attribute float size;
            attribute vec3 color;
            uniform float scale;
            varying vec3 vColor;
            void main() {
                vColor = color;
                vec4 mvPosition = modelViewMatrix * vec4(position, 1.0);
                gl_PointSize = size * (scale / -mvPosition.z);
                gl_Position = projectionMatrix * mvPosition;
            }`,
        fragmentShader: `
            varying vec3 vColor;
            void main() {
                gl_FragColor = vec4(vColor, 1.0);
            }`,
        transparent: true,
        blending: THREE.AdditiveBlending,
        depthWrite: false
    });

    particles.points = new THREE.Points(geometry, particles.material);
    particles.points.frustumCulled = false; // particles move every frame, the initial bounds would be wrong
    scene.add(particles.points);
    updateParticleScale();
}

function updateParticleScale() {
    if (particles.material) {
        particles.material.uniforms.scale.value = window.innerHeight * 0.5 * renderer.getPixelRatio();
    }
}

function emitParticle(x, y, z, vx, vy, vz, colorHex, size, life) {
    ensureParticleSystem();

    // Reuse free slots first; when the pool is full the oldest slot is recycled
    let slot = -1;
    for (let n = 0; n < MAX_PARTICLES; n++) {
        const candidate = (particles.next + n) % MAX_PARTICLES;
        if (particles.lifetimes[candidate] <= 0) {
            slot = candidate;
            break;
        }
    }
    if (slot === -1) {
        slot = particles.next;
        particles.alive--;
    }
    particles.next = (slot + 1) % MAX_PARTICLES;

    const i3 = slot * 3;
    particles.positions[i3] = x;
    particles.positions[i3 + 1] = y;
    particles.positions[i3 + 2] = z;
    particles.velocities[i3] = vx;
    particles.velocities[i3 + 1] = vy;
    particles.velocities[i3 + 2] = vz;
    _particleColor.setHex(colorHex);
    particles.baseColors[i3] = particles.colors[i3] = _particleColor.r;
    particles.baseColors[i3 + 1] = particles.colors[i3 + 1] = _particleColor.g;
    particles.baseColors[i3 + 2] = particles.colors[i3 + 2] = _particleColor.b;
    particles.sizes[slot] = size;
    particles.ages[slot] = 0;
    particles.lifetimes[slot] = life;

    particles.alive++;
    if (slot >= particles.highWater) particles.highWater = slot + 1;
}

function updateParticles(delta) {
    if (!particles.points) return;
    const geometry = particles.points.geometry;

    if (particles.alive <= 0) {
        if (particles.highWater !== 0) {
            particles.highWater = 0;
            geometry.setDrawRange(0, 0);
        }
        return;
    }

    for (let i = 0; i < particles.highWater; i++) {
        const life = particles.lifetimes[i];
        if (life <= 0) continue;

        const age = particles.ages[i] + delta;
        const i3 = i * 3;
        if (age >= life) {
            particles.lifetimes[i] = 0;
            particles.sizes[i] = 0;
            particles.alive--;
            continue;
        }
        particles.ages[i] = age;

        particles.positions[i3] += particles.velocities[i3] * delta;
        particles.positions[i3 + 1] += particles.velocities[i3 + 1] * delta;
        particles.positions[i3 + 2] += particles.velocities[i3 + 2] * delta;
        particles.velocities[i3 + 1] -= PARTICLE_GRAVITY * delta;

        const fade = 1 - age / life;
        particles.colors[i3] = particles.baseColors[i3] * fade;
        particles.colors[i3 + 1] = particles.baseColors[i3 + 1] * fade;
        particles.colors[i3 + 2] = particles.baseColors[i3 + 2] * fade;
    }

    geometry.setDrawRange(0, particles.highWater);
    geometry.attributes.position.needsUpdate = true;
    geometry.attributes.color.needsUpdate = true;
    geometry.attributes.size.needsUpdate = true;
}

function createTreasureDropEffect(pos) {
    for (let i = 0; i < 30; i++) {
        const theta = Math.random() * Math.PI * 2;
        const phi = Math.random() * Math.PI - Math.PI / 2;
        const speed = 60 + Math.random() * 140;
        emitParticle(
            pos.x, pos.y + 10, pos.z,
            Math.cos(phi) * Math.cos(theta) * speed,
            Math.sin(phi) * speed + 40,
            Math.cos(phi) * Math.sin(theta) * speed,
            0xfacc15, 10, 0.9);
    }
}

async function dropTreasure(position, forceDrop = false) {
    if (!position) return;
    if (!forceDrop && Math.random() >= 0.8) return; // 80% chance to drop treasure on monster kill
    const randomTreasure = TREASURE_DROPS[Math.floor(Math.random() * TREASURE_DROPS.length)];
    const modelDef = modelListParser.getModelByName(randomTreasure);
    if (!modelDef) return;

    const dropPos = position.clone();

    let model = null;
    if (isModel3DS(modelDef)) {
        const template = await load3DSModel(modelDef);
        if (template) {
            model = template.clone(true);
            const scaleFactor = modelDef.scale || 1.0;
            model.scale.set(scaleFactor, scaleFactor, scaleFactor);
            model.position.copy(dropPos);
            model.rotation.y = Math.random() * Math.PI * 2;
        }
    } else if (isModelMD2(modelDef)) {
        const geometry = await loadMD2Geometry(modelDef);
        if (geometry) {
            const scaleFactor = modelDef.scale || 1.0;
            const mesh = new THREE.Mesh(geometry, getSharedMaterial(modelDef.textureAlias));
            mesh.scale.set(scaleFactor, scaleFactor, scaleFactor);
            mesh.position.copy(dropPos);
            mesh.rotation.y = Math.random() * Math.PI * 2;
            model = mesh;
        }
    }

    if (!model) return;

    scene.add(model);
    addLevelObject(model, dropPos);

    const pickupDef = getPickupDefinition(randomTreasure) || getPickupDefinition(modelDef.name);
    if (pickupDef) {
        addPickup(model, dropPos, modelDef.name || randomTreasure, pickupDef);
    }

    const playerPos = window.controls ? window.controls.getObject().position : null;
    if (playerPos) {
        const dist = dropPos.distanceTo(playerPos);
        if (dist <= 1200.0) {
            playSound('treasure.wav', Math.max(0.1, (1.0 - (dist / 1200.0)) * 0.8));
        }
    } else {
        playSound('treasure.wav', 0.7);
    }

    createTreasureDropEffect(dropPos);
}

function createPickupParticleBurst(pos, colorHex = 0x38bdf8) {
    for (let i = 0; i < 24; i++) {
        const theta = Math.random() * Math.PI * 2;
        const phi = Math.random() * Math.PI - Math.PI / 2;
        const speed = 80 + Math.random() * 120;
        emitParticle(
            pos.x, pos.y + 10, pos.z,
            Math.cos(phi) * Math.cos(theta) * speed,
            Math.sin(phi) * speed + 50,
            Math.cos(phi) * Math.sin(theta) * speed,
            colorHex, 8, 0.8);
    }
}

function updatePickupEffects(delta) {
    // Update floating text popups
    for (let i = activeFloatingTexts.length - 1; i >= 0; i--) {
        const item = activeFloatingTexts[i];
        item.timer += delta;
        const progress = item.timer / item.duration;

        item.sprite.position.y += 30 * delta;
        item.mat.opacity = Math.max(0, 1 - progress);

        if (progress >= 1.0) {
            scene.remove(item.sprite);
            item.mat.dispose();
            activeFloatingTexts.splice(i, 1);
        }
    }

    updateParticles(delta);
}

// Pickup Proximity Detection & Ascension Animation
const PICKUP_DISTANCE = 70.0;
const PICKUP_DISTANCE_SQ = PICKUP_DISTANCE * PICKUP_DISTANCE;

function updatePickups(delta, playerPos) {
    updatePickupEffects(delta);

    for (let i = activePickups.length - 1; i >= 0; i--) {
        const pickup = activePickups[i];
        if (!pickup || !pickup.mesh) continue;

        if (pickup.state === 'ACTIVE') {
            const distSq = playerPos.distanceToSquared(pickup.position);

            if (distSq <= PICKUP_DISTANCE_SQ) {
                // Trigger reward
                pickup.state = 'FLOATING';
                pickup.floatTimer = 0.0;

                const nameClean = (pickup.name || '').toUpperCase().trim();
                const def = pickup.def;
                const isHealth = def && def.type === 'health';

                // Check for weapon unlock
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
                        currentGunIndex = newIndex;
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

                // Flash screen border subtly
                flashScreen(isHealth ? FLASH_PICKUP_HEALTH : FLASH_PICKUP_XP, 0.5, 150);
            }
        } else if (pickup.state === 'FLOATING') {
            pickup.floatTimer += delta;
            const floatDuration = 0.7; // 0.7 seconds float up animation
            const progress = pickup.floatTimer / floatDuration;

            // Float up into the sky rapidly
            pickup.mesh.position.y = pickup.originalY + (progress * progress) * 350.0;

            // Rotate smoothly as it floats up
            pickup.mesh.rotation.y += delta * 12.0;

            // Scale down to 0
            pickup.mesh.scale.setScalar(pickup.baseScale * Math.max(0, 1 - progress));

            if (progress >= 1.0) {
                // Remove mesh from scene and cleanup level tracking
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

function updateDoors(delta) {
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

            // Door triangles are not in the static grid, so moving them is just a position update
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

function cycleWeapon(dir) {
    let attempts = 0;
    do {
        currentGunIndex = (currentGunIndex + dir + PLAYER_GUNS.length) % PLAYER_GUNS.length;
        attempts++;
    } while (!unlockedWeapons.has(PLAYER_GUNS[currentGunIndex]) && attempts < PLAYER_GUNS.length);

    updateWeaponMesh();
}

let weaponLoadToken = 0;

async function updateWeaponMesh() {
    const gunName = PLAYER_GUNS[currentGunIndex];
    const loadToken = ++weaponLoadToken;
    updatePlayerHud();

    if (currentGunMesh) {
        camera.remove(currentGunMesh);
        currentGunMesh = null;
    }

    playerWeaponAnimTimer = 0.0;
    playerWeaponCurrentAnim = MD2_ANIMATIONS.STAND;
    playerWeaponActiveInfluences.clear();
    isWeaponAttacking = false;
    hasWeaponHitThisSwing = false;

    let modelDef = modelListParser.getModelByName(gunName);
    if (!modelDef && ['MAGIC MISSILE', 'FIREBALL', 'LIGHTNING', 'HEALING'].includes(gunName)) {
        modelDef = modelListParser.getModelByName('weaponsorcerer');
    }
    if (!modelDef) return;

    const geometry = await loadMD2Geometry(modelDef);
    // The player may have cycled weapons again while this one was loading
    if (!geometry || loadToken !== weaponLoadToken) return;

    const scaleFactor = modelDef.scale || 1.0;

    const mesh = new THREE.Mesh(geometry, getSharedMaterial(modelDef.textureAlias));
    mesh.scale.set(scaleFactor, scaleFactor, scaleFactor);
    //mesh.position.set(40, -20, 5);
    mesh.position.set(0, -25, 5);

    mesh.rotation.set(0, Math.PI / 2, 0);

    camera.add(mesh);
    currentGunMesh = mesh;
}

function onWindowResize() {
    camera.aspect = window.innerWidth / window.innerHeight;
    camera.updateProjectionMatrix();
    renderer.setSize(window.innerWidth, window.innerHeight);
    updateParticleScale();
}

const forwardVector = new THREE.Vector3();
const rightVector = new THREE.Vector3();
let lastPosDisplayTime = 0;

function animate() {
    requestAnimationFrame(animate);

    const time = performance.now();
    let delta = (time - prevTime) / 1000;
    if (delta > 0.1) delta = 0.1;

    const moveSpeed = isSprinting ? 400.0 : 200.0;

    if (isFlyMode) {
        // Fly Mode: Full 3D camera look-based movement
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
        // Modern FPS Mode: Pitch-independent horizontal movement on XZ plane
        velocity.x *= Math.pow(0.001, delta);
        velocity.z *= Math.pow(0.001, delta);

        // Apply gravity
        velocity.y -= GRAVITY * delta;

        // Handle jump
        if (jumpRequested) {
            if (isGrounded) {
                velocity.y = JUMP_SPEED;
                isGrounded = false;
            }
            jumpRequested = false;
        }

        // Compute forward and right vectors projected onto horizontal plane (XZ)
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

        // Sub-step so fast falls or sprints can't tunnel through thin walls and floors
        const maxStep = CAPSULE_RADIUS * 0.5;
        const travel = Math.max(Math.abs(velocity.x), Math.abs(velocity.y), Math.abs(velocity.z)) * delta;
        const steps = Math.min(16, Math.max(1, Math.ceil(travel / maxStep)));
        const stepDt = delta / steps;
        let grounded = false;
        for (let s = 0; s < steps; s++) {
            camPos.x += velocity.x * stepDt;
            camPos.y += velocity.y * stepDt;
            camPos.z += velocity.z * stepDt;

            // Collide and slide against dungeon geometry, model boxes and monsters
            resolveCapsuleCollisions(camPos);
            grounded = grounded || isGrounded;
        }
        isGrounded = grounded;

        // Play light footstep sounds when moving on the ground
        if (isGrounded && (moveForward || moveBackward || moveLeft || moveRight)) {
            const speedSq = velocity.x * velocity.x + velocity.z * velocity.z;
            if (speedSq > 400.0) { // Moving faster than threshold
                footstepTimer += delta * (isSprinting ? 1.5 : 1.0);
                if (footstepTimer >= 0.38) {
                    footstepTimer = 0.0;
                    playSound('walk.wav', 0.18);
                }
            } else {
                footstepTimer = 0.3;
            }
        } else {
            footstepTimer = 0.3;
        }
    }

    prevTime = time;

    const pos = controls.getObject().position;
    if (time - lastPosDisplayTime >= 100) {
        lastPosDisplayTime = time;
        hudEl('pos-display').textContent = `${Math.round(pos.x)}, ${Math.round(pos.y)}, ${Math.round(pos.z)}`;
    }

    if (playerDead) {
        playerDeathTimer -= delta;
        if (playerDeathTimer <= 0) respawnPlayer();
    }

    // Update monsters AI, movement, and animations
    const playerPos = pos;
    for (let i = 0; i < monsters.length; i++) {
        monsters[i].update(delta, playerPos, monsters);
    }

    // Update active player weapon animation
    updatePlayerWeaponAnimation(delta);

    // Update active magic spell projectiles
    updateProjectiles(delta);

    // Update item pickups detection and ascension animation
    updatePickups(delta, pos);

    // Update moving doors animation and collision
    updateDoors(delta);

    updateLighting(time / 1000, delta, pos);

    // Distance culling for level objects (merged chunks are culled by their bounding sphere)
    for (let i = 0; i < levelObjects.length; i++) {
        const obj = levelObjects[i];
        if (obj.mesh) {
            const reach = RENDER_DISTANCE + obj.radius;
            obj.mesh.visible = pos.distanceToSquared(obj.position) <= reach * reach;
        }
    }

    renderer.render(scene, camera);
}

if (document.readyState === 'loading') {
    window.addEventListener('DOMContentLoaded', init);
} else {
    init();
}
