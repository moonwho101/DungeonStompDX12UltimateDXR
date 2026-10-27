import * as THREE from 'three';
import { TDSLoader } from 'three/addons/loaders/TDSLoader.js';
import { MD2Loader } from 'three/addons/loaders/MD2Loader.js';
import { ObjectsDatParser, LevelMapParser, ModelListParser, OBJECT_VERT_STRIDE } from './parsers.js';
import { generateDungeon } from './generator.js';
import {
    scene, camera, controls, hudEl, flashScreen, FLASH_RED, FLASH_GREEN,
    getSharedMaterial, applyNormalMap, registerLamp, clearLamps, disposeOwnedResources,
    emitParticle, createPickupFloatingText, createTreasureDropEffect,
    acquireDynamicLight, EXPLOSION_FLASH_INTENSITY,
    explosionFlashes, haloTexture
} from './render.js';
import {
    hasLineOfSight, levelTriangles, modelBoxes, makeCollisionTriangle,
    computeTriangleBounds, buildCollisionGrid, resolveMonsterCapsuleCollisions, velocity
} from './collision.js';
import { MonsterState, monsters, doors } from './gameState.js';

// Parsers & Loaders
const objectsParser = new ObjectsDatParser();
const levelParser = new LevelMapParser();
export const modelListParser = new ModelListParser();

const md2Loader = new MD2Loader();
const tdsModelCache = new Map();
const md2GeometryCache = new Map();

// Monster Animation Definitions
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
    turnRate: 6.0,
    aggroRadius: 800.0,
    deaggroRadius: 1200.0,
    attackRange: 85.0,
    stopDistance: 85.0,
    alertDuration: 0.6,
    damage: 10,
    maxHealth: 100,
    painSound: 'impact.wav',
    deathSound: 'die.wav',
    weaponSound: 'swish4.wav',
    yellSound: 'goblin.wav',
    sound: 'goblin.wav',
    animations: MD2_ANIMATIONS
});


const speedMultiplier = 1.5;

export const MONSTER_CONFIGS = {
    KNIGHT: { ...DEFAULT_MONSTER_CONFIG, painSound: 'impact5.wav', deathSound: 'Skeleton.wav', weaponSound: 'swish9.wav', yellSound: 'mouther.wav', sound: 'mouther.wav', moveSpeed: 150.0 * speedMultiplier, aggroRadius: 850.0, damage: 15, maxHealth: 20 },
    GOBLIN: { ...DEFAULT_MONSTER_CONFIG, painSound: 'impact.wav', deathSound: 'death3.wav', weaponSound: 'swish4.wav', yellSound: 'goblin.wav', sound: 'goblin.wav', moveSpeed: 190.0 * speedMultiplier, aggroRadius: 750.0, damage: 6, maxHealth: 18 },
    BAUUL: { ...DEFAULT_MONSTER_CONFIG, painSound: 'baullpain.wav', deathSound: 'baull.wav', weaponSound: 'swish1.wav', yellSound: 'baullspeak.wav', sound: 'baullspeak.wav', moveSpeed: 140.0 * speedMultiplier, aggroRadius: 900.0, damage: 20, maxHealth: 25, canCastSpells: true, spells: ['MAGIC MISSILE', 'FIREBALL', 'LIGHTNING'], attackRange: 620.0, stopDistance: 270.0, spellCooldown: 2.0 },
    CORPSE: { ...DEFAULT_MONSTER_CONFIG, painSound: 'impact.wav', deathSound: 'goblindeath.wav', weaponSound: 'swish9.wav', yellSound: 'ghoul.wav', sound: 'ghoul.wav', moveSpeed: 110.0 * speedMultiplier, aggroRadius: 650.0, damage: 8, maxHealth: 15 },
    SORCERER: { ...DEFAULT_MONSTER_CONFIG, painSound: 'sorcererpain.wav', deathSound: 'sorcererdie.wav', weaponSound: 'swish7.wav', yellSound: 'sorcererspeak.wav', sound: 'sorcererspeak.wav', moveSpeed: 160.0 * speedMultiplier, aggroRadius: 950.0, damage: 18, maxHealth: 20, canCastSpells: true, spells: ['MAGIC MISSILE', 'FIREBALL', 'LIGHTNING'], attackRange: 650.0, stopDistance: 260.0, spellCooldown: 1.6 },
    TENTACLE: { ...DEFAULT_MONSTER_CONFIG, painSound: 'impact5.wav', deathSound: 'Skeleton.wav', weaponSound: 'swish9.wav', yellSound: 'mouther.wav', sound: 'mouther.wav', moveSpeed: 120.0 * speedMultiplier, aggroRadius: 700.0, damage: 5, maxHealth: 17 },
    WRAITH: { ...DEFAULT_MONSTER_CONFIG, painSound: 'wraithpain.wav', deathSound: 'wraithdie.wav', weaponSound: 'swish4.wav', yellSound: 'wraithspeak.wav', sound: 'wraithspeak.wav', moveSpeed: 180.0 * speedMultiplier, aggroRadius: 950.0, damage: 12, maxHealth: 28, canCastSpells: true, spells: ['MAGIC MISSILE', 'FIREBALL', 'LIGHTNING'], attackRange: 600.0, stopDistance: 250.0, spellCooldown: 1.8 },
    OGRE: { ...DEFAULT_MONSTER_CONFIG, painSound: 'impact3.wav', deathSound: 'death1.wav', weaponSound: 'swish7.wav', yellSound: 'ogre.wav', sound: 'ogre.wav', moveSpeed: 130.0 * speedMultiplier, aggroRadius: 800.0, damage: 25, maxHealth: 25, canCastSpells: true, spells: ['MAGIC MISSILE', 'FIREBALL', 'LIGHTNING'], attackRange: 550.0, stopDistance: 220.0, spellCooldown: 2.2 },
    PHANTOM: { ...DEFAULT_MONSTER_CONFIG, painSound: 'impact4.wav', deathSound: 'death2.wav', weaponSound: 'swish7.wav', yellSound: 'baull.wav', sound: 'baull.wav', moveSpeed: 170.0 * speedMultiplier, aggroRadius: 900.0, damage: 16, maxHealth: 20, canCastSpells: true, spells: ['MAGIC MISSILE', 'FIREBALL', 'LIGHTNING'], attackRange: 600.0, stopDistance: 250.0, spellCooldown: 2.0 },
    OGRO: { ...DEFAULT_MONSTER_CONFIG, painSound: 'impact3.wav', deathSound: 'death1.wav', weaponSound: 'swish7.wav', yellSound: 'ogre.wav', sound: 'ogre.wav', moveSpeed: 135.0 * speedMultiplier, aggroRadius: 800.0, damage: 14, maxHealth: 20 },
    SKELETON2: { ...DEFAULT_MONSTER_CONFIG, painSound: 'impact5.wav', deathSound: 'Skeleton.wav', weaponSound: 'swish9.wav', yellSound: 'mouther.wav', sound: 'mouther.wav', moveSpeed: 165.0 * speedMultiplier, aggroRadius: 800.0, damage: 10, maxHealth: 15 },
    DEMONESS: { ...DEFAULT_MONSTER_CONFIG, painSound: 'impact3.wav', deathSound: 'death4.wav', weaponSound: 'swish7.wav', yellSound: 'ogre.wav', sound: 'ogre.wav', moveSpeed: 175.0 * speedMultiplier, aggroRadius: 950.0, damage: 22, maxHealth: 35, canCastSpells: true, spells: ['MAGIC MISSILE', 'FIREBALL', 'LIGHTNING'], attackRange: 680.0, stopDistance: 280.0, spellCooldown: 1.6 },
    FAERIE: { ...DEFAULT_MONSTER_CONFIG, painSound: 'faeriepain.wav', deathSound: 'faeriedie.wav', weaponSound: 'swish7.wav', yellSound: 'faeriespeak.wav', sound: 'faeriespeak.wav', moveSpeed: 185.0 * speedMultiplier, aggroRadius: 900.0, damage: 15, maxHealth: 30, canCastSpells: true, spells: ['MAGIC MISSILE', 'FIREBALL', 'LIGHTNING'], attackRange: 700.0, stopDistance: 300.0, spellCooldown: 1.4 },
    SLAVE: { ...DEFAULT_MONSTER_CONFIG, painSound: 'impact5.wav', deathSound: 'Skeleton.wav', weaponSound: 'swish9.wav', yellSound: 'mouther.wav', sound: 'mouther.wav', moveSpeed: 140.0 * speedMultiplier, aggroRadius: 600.0, damage: 5, maxHealth: 5 },
    WOLF: { ...DEFAULT_MONSTER_CONFIG, painSound: 'wolfpain.wav', deathSound: 'wolfdie.wav', weaponSound: 'swish4.wav', yellSound: 'wolfhowl.wav', sound: 'wolfhowl.wav', moveSpeed: 210.0 * speedMultiplier, aggroRadius: 850.0, damage: 12, maxHealth: 20 },
    INSECT: { ...DEFAULT_MONSTER_CONFIG, painSound: 'impact5.wav', deathSound: 'Skeleton.wav', weaponSound: 'swish9.wav', yellSound: 'mouther.wav', sound: 'mouther.wav', moveSpeed: 195.0 * speedMultiplier, aggroRadius: 800.0, damage: 10, maxHealth: 20 },
    OLUVE: { ...DEFAULT_MONSTER_CONFIG, painSound: 'impact5.wav', deathSound: 'Skeleton.wav', weaponSound: 'swish9.wav', yellSound: 'mouther.wav', sound: 'mouther.wav', moveSpeed: 150.0 * speedMultiplier, aggroRadius: 700.0, damage: 6, maxHealth: 17 },
    MUMMY: { ...DEFAULT_MONSTER_CONFIG, painSound: 'mummypain.wav', deathSound: 'mummydie.wav', weaponSound: 'swish7.wav', yellSound: 'mummyspeak.wav', sound: 'mummyspeak.wav', moveSpeed: 125.0 * speedMultiplier, aggroRadius: 750.0, damage: 12, maxHealth: 18 },
    HYDRA: { ...DEFAULT_MONSTER_CONFIG, painSound: 'hydrapain.wav', deathSound: 'hydradie.wav', weaponSound: 'swish1.wav', yellSound: 'hydraspeak.wav', sound: 'hydraspeak.wav', moveSpeed: 145.0 * speedMultiplier, aggroRadius: 850.0, damage: 16, maxHealth: 20 },
    NECROMANCER: { ...DEFAULT_MONSTER_CONFIG, painSound: 'necropain.wav', deathSound: 'necrodie.wav', weaponSound: 'swish5.wav', yellSound: 'necrospeak.wav', sound: 'necrospeak.wav', moveSpeed: 160.0 * speedMultiplier, aggroRadius: 1000.0, damage: 24, maxHealth: 35, canCastSpells: true, spells: ['MAGIC MISSILE', 'FIREBALL', 'LIGHTNING'], attackRange: 650.0, stopDistance: 280.0, spellCooldown: 1.8 },
    IMP: { ...DEFAULT_MONSTER_CONFIG, painSound: 'imppain.wav', deathSound: 'impdie.wav', weaponSound: 'swish1.wav', yellSound: 'impspeak.wav', sound: 'impspeak.wav', moveSpeed: 180.0 * speedMultiplier, aggroRadius: 800.0, damage: 10, maxHealth: 20 },
    DEMON: { ...DEFAULT_MONSTER_CONFIG, painSound: 'demonpain.wav', deathSound: 'demondie.wav', weaponSound: 'swish7.wav', yellSound: 'demonspeak.wav', sound: 'demonspeak.wav', moveSpeed: 165.0 * speedMultiplier, aggroRadius: 950.0, damage: 20, maxHealth: 25 },
    FULIMO: { ...DEFAULT_MONSTER_CONFIG, painSound: 'fulimopain.wav', deathSound: 'fulimodie.wav', weaponSound: 'swish7.wav', yellSound: 'fulimospeak.wav', sound: 'fulimospeak.wav', moveSpeed: 170.0 * speedMultiplier, aggroRadius: 900.0, damage: 18, maxHealth: 25 },
    COBRA: { ...DEFAULT_MONSTER_CONFIG, painSound: 'impact.wav', deathSound: 'snakedie.wav', weaponSound: 'swish7.wav', yellSound: 'snakehiss.wav', sound: 'snakehiss.wav', moveSpeed: 190.0 * speedMultiplier, aggroRadius: 800.0, damage: 14, maxHealth: 30 },
    DRAGON: { ...DEFAULT_MONSTER_CONFIG, painSound: 'fulimopain.wav', deathSound: 'fulimodie.wav', weaponSound: 'swish7.wav', yellSound: 'fulimospeak.wav', sound: 'fulimospeak.wav', moveSpeed: 160.0 * speedMultiplier, aggroRadius: 1000.0, damage: 28, maxHealth: 30 },
    CENTAUR: { ...DEFAULT_MONSTER_CONFIG, painSound: 'fulimopain.wav', deathSound: 'fulimodie.wav', weaponSound: 'swish7.wav', yellSound: 'fulimospeak.wav', sound: 'fulimospeak.wav', moveSpeed: 190.0 * speedMultiplier, aggroRadius: 850.0, damage: 12, maxHealth: 16 },
    CRAZ: { ...DEFAULT_MONSTER_CONFIG, painSound: 'impact.wav', deathSound: 'death3.wav', weaponSound: 'swish7.wav', yellSound: 'orc.wav', sound: 'orc.wav', moveSpeed: 175.0 * speedMultiplier, aggroRadius: 800.0, damage: 14, maxHealth: 20 },
    RIDER: { ...DEFAULT_MONSTER_CONFIG, painSound: 'fulimopain.wav', deathSound: 'fulimodie.wav', weaponSound: 'swish7.wav', yellSound: 'fulimospeak.wav', sound: 'fulimospeak.wav', moveSpeed: 180.0 * speedMultiplier, aggroRadius: 900.0, damage: 20, maxHealth: 26 },
    OKTA: { ...DEFAULT_MONSTER_CONFIG, painSound: 'fulimopain.wav', deathSound: 'fulimodie.wav', weaponSound: 'swish7.wav', yellSound: 'fulimospeak.wav', sound: 'fulimospeak.wav', moveSpeed: 165.0 * speedMultiplier, aggroRadius: 850.0, damage: 16, maxHealth: 25 },
    SORB: { ...DEFAULT_MONSTER_CONFIG, painSound: 'impact.wav', deathSound: 'sorbdie.wav', weaponSound: 'swish7.wav', yellSound: 'sorb.wav', sound: 'sorb.wav', moveSpeed: 170.0 * speedMultiplier, aggroRadius: 950.0, damage: 22, maxHealth: 29 }
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

function drawHealthBarCanvas(canvas, name, currentHp, maxHp) {
    const ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    ctx.fillStyle = 'rgba(15, 23, 42, 0.85)';
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.35)';
    ctx.lineWidth = 2;
    ctx.beginPath();
    if (ctx.roundRect) ctx.roundRect(8, 4, 240, 56, 8);
    else ctx.rect(8, 4, 240, 56);
    ctx.fill();
    ctx.stroke();

    ctx.fillStyle = '#ffffff';
    ctx.font = 'bold 18px sans-serif';
    ctx.textAlign = 'left';
    ctx.fillText((name || 'MONSTER').toUpperCase(), 18, 26);

    ctx.textAlign = 'right';
    ctx.fillStyle = '#cbd5e1';
    ctx.font = 'bold 16px sans-serif';
    ctx.fillText(`${Math.max(0, Math.ceil(currentHp))} / ${maxHp}`, 238, 26);

    const barX = 18;
    const barY = 34;
    const barW = 220;
    const barH = 16;

    ctx.fillStyle = 'rgba(30, 41, 59, 0.9)';
    ctx.beginPath();
    if (ctx.roundRect) ctx.roundRect(barX, barY, barW, barH, 4);
    else ctx.rect(barX, barY, barW, barH);
    ctx.fill();

    const pct = Math.max(0, Math.min(1, currentHp / maxHp));
    if (pct > 0) {
        let fillColor = '#22c55e';
        if (pct < 0.25) fillColor = '#ef4444';
        else if (pct < 0.55) fillColor = '#eab308';

        ctx.fillStyle = fillColor;
        ctx.beginPath();
        if (ctx.roundRect) ctx.roundRect(barX, barY, barW * pct, barH, 4);
        else ctx.rect(barX, barY, barW * pct, barH);
        ctx.fill();
    }
}

export class Monster {
    constructor(mesh, weaponMesh = null, config = DEFAULT_MONSTER_CONFIG, initialPosition, initialRotationY = 0) {
        this.mesh = mesh;
        this.weaponMesh = weaponMesh;
        this.config = { ...DEFAULT_MONSTER_CONFIG, ...config };
        this.name = (this.config.name || 'MONSTER').toUpperCase();

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

        this.alertTimer = 0.0;
        this.stateTimer = 0.0;
        this.attackCooldown = 0.0;
        this.hasDealtDamageThisAttack = false;

        this.currentAnim = this.config.animations.STAND;
        this.animTimer = 0.0;
        this.animComplete = false;

        this.prevAnimFrames = null;
        this.fadeTimer = 0.0;
        this.fadeDuration = 0.15;

        this.activeFrames = [0, 0, 0, 0];
        this.activeFrameCount = 0;
        this.frameWeights = [0, 0, 0, 0];
        this.scratchPose = { frameA: 0, frameB: 0, alpha: 0 };

        this.boundingBox = new THREE.Box3();
        this.updateBoundingBox();

        this.noiseTimer = 3.0 + Math.random() * 8.0;

        this.pendingSpell = null;
        this.strafeDir = (Math.random() < 0.5 ? 1 : -1);
        this.strafeTimer = 0.0;

        this.setAnimation(this.config.animations.STAND, true);
        this.updateMorphTargets(0);

        this.createHealthBar();
    }

    createHealthBar() {
        const canvas = document.createElement('canvas');
        canvas.width = 256;
        canvas.height = 64;

        const texture = new THREE.CanvasTexture(canvas);
        texture.minFilter = THREE.LinearFilter;

        const spriteMat = new THREE.SpriteMaterial({
            map: texture,
            transparent: true,
            depthTest: true,
            depthWrite: false
        });

        const sprite = new THREE.Sprite(spriteMat);
        sprite.scale.set(80, 20, 1);

        scene.add(sprite);

        this.hpCanvas = canvas;
        this.hpTexture = texture;
        this.hpSpriteMat = spriteMat;
        this.hpSprite = sprite;

        this.updateHealthBar();
    }

    updateHealthBar() {
        if (!this.hpCanvas || !this.hpTexture) return;
        drawHealthBarCanvas(this.hpCanvas, this.name, this.health, this.config.maxHealth);
        this.hpTexture.needsUpdate = true;
    }

    updateTacticalMovement(dt, distToPlayer, otherMonsters, playerPos) {
        this.strafeTimer -= dt;
        if (this.strafeTimer <= 0) {
            this.strafeDir = (Math.random() < 0.5 ? 1 : -1);
            this.strafeTimer = 1.0 + Math.random() * 1.5;
        }

        const dx = playerPos.x - this.position.x;
        const dz = playerPos.z - this.position.z;
        const len = Math.sqrt(dx * dx + dz * dz);
        if (len < 0.001) return;

        let moveX = (-dz / len) * this.strafeDir;
        let moveZ = (dx / len) * this.strafeDir;

        const stopDist = this.config.stopDistance || 250.0;
        if (distToPlayer < stopDist * 0.8) {
            moveX -= (dx / len) * 0.7;
            moveZ -= (dz / len) * 0.7;
        }

        const speed = this.config.moveSpeed * 0.65;
        this.position.x += moveX * speed * dt;
        this.position.z += moveZ * speed * dt;

        resolveMonsterCapsuleCollisions(this, otherMonsters, playerPos);
    }

    playMonsterNoise(distToPlayer) {
        if (distToPlayer > 1200.0) return;
        const soundFile = this.config.yellSound || this.config.sound || 'goblin.wav';
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

        for (let k = 0; k < this.activeFrameCount; k++) {
            const idx = this.activeFrames[k];
            if (idx >= 0 && idx < influences.length) influences[idx] = 0;
            if (wInfluences && idx >= 0 && idx < wInfluences.length) wInfluences[idx] = 0;
        }
        this.activeFrameCount = 0;

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

        if (this.hpSprite) {
            if (this.state === MonsterState.DEATH || distToPlayer > 2200.0) {
                this.hpSprite.visible = false;
            } else {
                this.hpSprite.visible = true;
                const headOffsetY = (!this.boundingBox.isEmpty()) ? (this.boundingBox.max.y + 12.0) : (this.position.y + 75.0);
                this.hpSprite.position.set(this.position.x, headOffsetY, this.position.z);
            }
        }

        if (this.isGrounded) {
            if (this.state === MonsterState.DEATH && this.animComplete && !this.weaponMesh) return;

            if (this.state === MonsterState.IDLE && distSq > this.config.aggroRadius * this.config.aggroRadius) {
                this.tickAmbientNoise(dt, distToPlayer);
                if (distSq <= 3000.0 * 3000.0) {
                    this.updateMorphTargets(dt);
                    this.updateBoundingBox();
                }
                return;
            }
        }

        const maxSubStep = 12.0;
        const estDy = Math.abs(this.velocity.y * dt);
        const subSteps = Math.max(1, Math.ceil(estDy / maxSubStep));
        const subDt = dt / subSteps;

        for (let s = 0; s < subSteps; s++) {
            this.velocity.y -= 1600.0 * subDt;
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

        const eyePos = this.position.clone();
        eyePos.y += 40.0;
        const targetEyePos = playerPos.clone();
        targetEyePos.y += 20.0;
        const hasLOS = this.config.canCastSpells ? hasLineOfSight(eyePos, targetEyePos) : true;

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
                } else if (distToPlayer <= this.config.attackRange && hasLOS) {
                    this.setState(MonsterState.ATTACK);
                } else if (this.stateTimer >= this.config.alertDuration) {
                    this.setState(MonsterState.CHASE);
                }
                break;

            case MonsterState.CHASE:
                this.smoothTurn(targetAngle, dt);

                if (distToPlayer <= this.config.attackRange && hasLOS) {
                    if (this.attackCooldown <= 0) {
                        this.setState(MonsterState.ATTACK);
                    } else if (this.config.canCastSpells) {
                        this.setAnimation(this.config.animations.RUN);
                        this.updateTacticalMovement(dt, distToPlayer, otherMonsters, playerPos);
                    } else {
                        this.setAnimation(this.config.animations.STAND);
                    }
                } else if (distToPlayer > this.config.deaggroRadius) {
                    this.setState(MonsterState.IDLE);
                } else {
                    this.setAnimation(this.config.animations.RUN);
                    if (distToPlayer > this.config.stopDistance) {
                        this.moveForward(dt, otherMonsters, playerPos);
                    } else if (this.config.canCastSpells) {
                        this.updateTacticalMovement(dt, distToPlayer, otherMonsters, playerPos);
                    }
                }
                break;

            case MonsterState.ATTACK:
                this.setAnimation(this.config.animations.ATTACK);
                this.smoothTurn(targetAngle, dt);

                if (!this.hasDealtDamageThisAttack) {
                    const anim = this.currentAnim;
                    const totalFrames = anim.end - anim.start + 1;
                    const totalDuration = totalFrames / anim.fps;

                    if (this.config.canCastSpells && !this.isMeleeAttack) {
                        const spellColor = (this.pendingSpell === 'FIREBALL') ? 0xff5500 : ((this.pendingSpell === 'LIGHTNING') ? 0x00ffff : 0x38bdf8);
                        for (let p = 0; p < 3; p++) {
                            const offsetAngle = Math.random() * Math.PI * 2;
                            const rad = 15 + Math.random() * 20;
                            emitParticle(
                                this.position.x + Math.cos(offsetAngle) * rad,
                                this.position.y + 40 + (Math.random() - 0.5) * 20,
                                this.position.z + Math.sin(offsetAngle) * rad,
                                (Math.random() - 0.5) * 40,
                                40 + Math.random() * 60,
                                (Math.random() - 0.5) * 40,
                                spellColor, 10, 0.4
                            );
                        }

                        if (this.animTimer >= totalDuration * 0.5) {
                            this.hasDealtDamageThisAttack = true;
                            launchMonsterProjectile(this, this.pendingSpell || 'MAGIC MISSILE', playerPos);
                        }
                    } else {
                        if (this.animTimer >= totalDuration * 0.5) {
                            this.hasDealtDamageThisAttack = true;
                            const weaponSound = this.config.weaponSound || 'swish4.wav';
                            if (distToPlayer <= 1200.0) {
                                playSound(weaponSound, Math.max(0.05, (1.0 - (distToPlayer / 1200.0)) * 0.7));
                            }
                            if (distToPlayer <= 115.0 + 20.0) {
                                damagePlayer(this.config.damage);
                            }
                        }
                    }
                }

                if (this.animComplete) {
                    this.attackCooldown = this.config.spellCooldown || 0.5;
                    this.hasDealtDamageThisAttack = false;
                    this.pendingSpell = null;
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
            const playerPos = window.controls ? window.controls.getObject().position : null;
            const distToPlayer = playerPos ? this.position.distanceTo(playerPos) : 9999.0;
            const meleeCombatRange = 115.0;

            if (this.config.canCastSpells && distToPlayer <= meleeCombatRange) {
                this.isMeleeAttack = true;
                this.pendingSpell = null;
            } else if (this.config.canCastSpells && this.config.spells && this.config.spells.length > 0) {
                this.isMeleeAttack = false;
                this.pendingSpell = this.config.spells[Math.floor(Math.random() * this.config.spells.length)];
                if (this.pendingSpell === 'MAGIC MISSILE') playSound('spell1.wav', 0.8);
                else if (this.pendingSpell === 'FIREBALL') playSound('fireball.wav', 0.8);
                else if (this.pendingSpell === 'LIGHTNING') playSound('spell3.wav', 0.8);
            } else {
                this.isMeleeAttack = true;
                this.pendingSpell = null;
            }
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

        resolveMonsterCapsuleCollisions(this, otherMonsters, playerPos);
    }

    takeDamage(damage) {
        if (this.state === MonsterState.DEATH) return;
        this.health -= damage;
        this.updateHealthBar();

        const playerPos = window.controls ? window.controls.getObject().position : null;
        const dist = playerPos ? this.position.distanceTo(playerPos) : 9999.0;
        const volume = dist <= 1200.0 ? Math.max(0.05, (1.0 - (dist / 1200.0)) * 0.7) : 0;

        if (this.health <= 0) {
            this.health = 0;
            this.setState(MonsterState.DEATH);
            this.boundingBox.makeEmpty();
            const deaths = [this.config.animations.DEATH1, this.config.animations.DEATH2, this.config.animations.DEATH3];
            const chosenDeath = deaths[Math.floor(Math.random() * deaths.length)];
            this.setAnimation(chosenDeath, true);

            const deathSound = this.config.deathSound || 'die.wav';
            if (dist <= 1200.0) {
                playSound(deathSound, volume);
            }

            dropTreasure(this.position);

            const xpGained = (this.config.maxHealth || 20) * 2;
            awardPlayerXp(xpGained);
        } else {
            this.setState(MonsterState.HURT);
            const pains = [this.config.animations.PAIN1, this.config.animations.PAIN2, this.config.animations.PAIN3];
            const chosenPain = pains[Math.floor(Math.random() * pains.length)];
            this.setAnimation(chosenPain, true);

            const painSound = this.config.painSound || 'impact.wav';
            if (dist <= 1200.0) {
                playSound(painSound, volume);
            }
        }
    }
}

// Player Stats & State
export const playerStats = {
    health: 100,
    maxHealth: 100,
    level: 1,
    xp: 0,
    nextXp: 100,
    kills: 0
};

export let playerDead = false;
export let playerDeathTimer = 0.0;
export function setPlayerDeathTimer(val) { playerDeathTimer = val; }
export function setPlayerDead(val) { playerDead = val; }
export let playerStartPos = null;
export function setPlayerStartPos(pos) { playerStartPos = pos; }
export const PLAYER_RESPAWN_DELAY = 3.0;
export let isInvincible = false;
export function setIsInvincible(val) { isInvincible = val; }

export function damagePlayer(amount) {
    if (isInvincible) return;
    if (playerStats.health <= 0) return;
    playerStats.health = Math.max(0, playerStats.health - amount);

    playSound(getRandomSound(PLAYER_IMPACT_SOUNDS), 0.6);
    flashScreen(FLASH_RED, 0.6, 150);
    updatePlayerHud();

    if (playerStats.health <= 0) {
        killPlayer();
    }
}

export function killPlayer() {
    playerDead = true;
    playerDeathTimer = PLAYER_RESPAWN_DELAY;
    playSound('die.wav', 0.8);
    const deathEl = hudEl('death-screen');
    if (deathEl) deathEl.style.display = 'flex';
}

export function respawnPlayer() {
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
        playerStats.health = playerStats.maxHealth;
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

// Items, Pickups & Treasure Drops
export const TREASURE_DROPS = [
    'POTION', 'cheese1', 'GOBLET', 'pot1', 'pot2', 'pot3',
    'mushroom', 'COIN', 'SPELLBOOK', 'SCROLL-HEALING-',
    'SCROLL-MAGICMISSLE-', 'SCROLL-FIREBALL-', 'SCROLL-LIGHTNING-'
];

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

    const noDash = clean.replace(/-+$/, '');

    for (const key in PICKUP_DEFINITIONS) {
        const keyNoDash = key.replace(/-+$/, '');
        if (clean === key || noDash === keyNoDash || clean.startsWith(keyNoDash) || keyNoDash.startsWith(noDash)) {
            return PICKUP_DEFINITIONS[key];
        }
    }
    return null;
}

export const activePickups = [];

export function addPickup(mesh, position, name, def) {
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

export const WEAPON_DAMAGE = {
    'AXE': 5,
    'FLAMESWORD': 5,
    'BASTARDSWORD': 8,
    'BATTLEAXE': 10,
    'ICESWORD': 12,
    'LIGHTNINGSWORD': 14,
    'MORNINGSTAR': 16,
    'SPIKEDFLAIL': 18,
    'SPLITSWORD': 19,
    'SUPERFLAMESWORD': 20,
    'MAGIC MISSILE': 5,
    'FIREBALL': 10,
    'LIGHTNING': 15,
    'HEALING': 0
};

export let currentGunIndex = 0;
export function getCurrentGunIndex() { return currentGunIndex; }
export function setCurrentGunIndex(idx) { currentGunIndex = idx; }

export let currentGunMesh = null;
export function getCurrentGunMesh() { return currentGunMesh; }
export function setCurrentGunMesh(mesh) { currentGunMesh = mesh; }

// Exit Teleporter Portal Effects
export const activeTeleporters = [];

export function createExitTeleporterEffect(position) {
    const group = new THREE.Group();
    group.position.copy(position);

    // 1. Ethereal Light Beam Column
    const beamGeo = new THREE.CylinderGeometry(40, 50, 400, 16, 1, true);
    const beamMat = new THREE.MeshBasicMaterial({
        color: 0x38bdf8,
        transparent: true,
        opacity: 0.35,
        side: THREE.DoubleSide,
        depthWrite: false,
        blending: THREE.AdditiveBlending
    });
    const beamMesh = new THREE.Mesh(beamGeo, beamMat);
    beamMesh.position.y = 200;
    group.add(beamMesh);

    // Inner bright core beam
    const coreGeo = new THREE.CylinderGeometry(15, 20, 420, 16, 1, true);
    const coreMat = new THREE.MeshBasicMaterial({
        color: 0x818cf8,
        transparent: true,
        opacity: 0.6,
        side: THREE.DoubleSide,
        depthWrite: false,
        blending: THREE.AdditiveBlending
    });
    const coreMesh = new THREE.Mesh(coreGeo, coreMat);
    coreMesh.position.y = 210;
    group.add(coreMesh);

    // 2. Swirling Energy Rings
    const rings = [];
    const ringGeo = new THREE.RingGeometry(20, 65, 32);
    const ringColors = [0x38bdf8, 0x818cf8, 0xc084fc, 0x38bdf8];
    for (let i = 0; i < 4; i++) {
        const ringMat = new THREE.MeshBasicMaterial({
            color: ringColors[i],
            transparent: true,
            opacity: 0.7,
            side: THREE.DoubleSide,
            depthWrite: false,
            blending: THREE.AdditiveBlending
        });
        const ringMesh = new THREE.Mesh(ringGeo, ringMat);
        ringMesh.rotation.x = Math.PI / 2;
        ringMesh.position.y = 15 + i * 45;
        group.add(ringMesh);
        rings.push(ringMesh);
    }

    // 3. Dynamic Teleport Light
    const lightSlot = acquireDynamicLight(0x38bdf8, 12000, 600);
    if (lightSlot) {
        lightSlot.light.position.copy(position).add(new THREE.Vector3(0, 100, 0));
    }

    // Floating Teleport Title Banner
    createPickupFloatingText('DUNGEON EXIT PORTAL', position, '#38bdf8');

    scene.add(group);

    const teleporterObj = {
        group,
        beamMesh,
        coreMesh,
        rings,
        lightSlot,
        position: position.clone(),
        timer: 0
    };

    activeTeleporters.push(teleporterObj);
    return teleporterObj;
}

export function clearTeleporters() {
    for (const tp of activeTeleporters) {
        if (tp.group) scene.remove(tp.group);
        if (tp.lightSlot) releaseDynamicLight(tp.lightSlot);
    }
    activeTeleporters.length = 0;
}

export let playerWeaponAnimTimer = 0.0;
export function setPlayerWeaponAnimTimer(val) { playerWeaponAnimTimer = val; }

export let playerWeaponCurrentAnim = MD2_ANIMATIONS.STAND;
export function setPlayerWeaponCurrentAnim(anim) { playerWeaponCurrentAnim = anim; }

export const playerWeaponActiveInfluences = new Set();
export let isWeaponAttacking = false;
export function setIsWeaponAttacking(val) { isWeaponAttacking = val; }

export let hasWeaponHitThisSwing = false;
export function setHasWeaponHitThisSwing(val) { hasWeaponHitThisSwing = val; }

export function cycleWeapon(dir) {
    let attempts = 0;
    do {
        currentGunIndex = (currentGunIndex + dir + PLAYER_GUNS.length) % PLAYER_GUNS.length;
        attempts++;
    } while (!unlockedWeapons.has(PLAYER_GUNS[currentGunIndex]) && attempts < PLAYER_GUNS.length);

    updateWeaponMesh();
}

let weaponLoadToken = 0;

export async function updateWeaponMesh() {
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
    if (!geometry || loadToken !== weaponLoadToken) return;

    const scaleFactor = modelDef.scale || 1.0;

    const mesh = new THREE.Mesh(geometry, getSharedMaterial(modelDef.textureAlias));
    mesh.scale.set(scaleFactor, scaleFactor, scaleFactor);
    mesh.position.set(0, -25, 5);
    mesh.rotation.set(0, Math.PI / 2, 0);

    camera.add(mesh);
    currentGunMesh = mesh;
}

export function triggerWeaponAttack() {
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
        hasWeaponHitThisSwing = true;
        playSound('spell1.wav', 0.7);
        launchProjectile('MAGIC MISSILE');
    } else if (gunName === 'FIREBALL') {
        hasWeaponHitThisSwing = true;
        playSound('fireball.wav', 0.8);
        launchProjectile('FIREBALL');
    } else if (gunName === 'LIGHTNING') {
        hasWeaponHitThisSwing = true;
        playSound('spell3.wav', 0.8);
        launchProjectile('LIGHTNING');
    } else if (gunName === 'HEALING') {
        hasWeaponHitThisSwing = true;
        playSound('spell2.wav', 0.8);
        castHealingSpell();
    } else {
        playSound(getRandomSound(SWISH_SOUNDS), 0.5);
    }
}

export function castHealingSpell() {
    healPlayer(10);
    const playerPos = controls.getObject().position;
    createPickupFloatingText('+10 HP', playerPos, '#22c55e');

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

    flashScreen(FLASH_GREEN, 0.6, 200);
}

export function checkPlayerAttackHit() {
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
            if (dot >= 0.5) {
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

// Projectiles & Explosions
export const activeProjectiles = [];

export const PROJECTILE_TYPES = {
    'MAGIC MISSILE': {
        speed: 1300.0, gravity: 0, radius: 20.0, aoeRadius: 80.0, maxLife: 4.0,
        coreRadius: 7, coreColor: 0x38bdf8, coreOpacity: 0.9,
        haloColor: 0x00e5ff, haloOpacity: 0.8, haloSize: 70,
        lightColor: 0x00e5ff, lightIntensity: 6000, lightDistance: 350,
        colorHex: 0x38bdf8
    },
    'FIREBALL': {
        speed: 1050.0, gravity: -200.0, radius: 26.0, aoeRadius: 180.0, maxLife: 4.5,
        coreRadius: 7, coreColor: 0xff4500, coreOpacity: 0.95,
        haloColor: 0xffaa00, haloOpacity: 0.9, haloSize: 110,
        lightColor: 0xff6600, lightIntensity: 9000, lightDistance: 450,
        colorHex: 0xff5500
    },
    'LIGHTNING': {
        speed: 1600.0, gravity: 0, radius: 22.0, aoeRadius: 120.0, maxLife: 3.5,
        coreRadius: 7, coreColor: 0xffffff, coreOpacity: 0.95,
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

export function launchProjectile(type) {
    const def = PROJECTILE_TYPES[type];
    const res = getProjectileResources(type);

    const lookDir = new THREE.Vector3();
    camera.getWorldDirection(lookDir);

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
        owner: 'player',
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

export function launchMonsterProjectile(monster, type, targetPos) {
    const def = PROJECTILE_TYPES[type] || PROJECTILE_TYPES['MAGIC MISSILE'];
    const res = getProjectileResources(type);

    const spawnPos = monster.position.clone();
    spawnPos.y += 40;

    const targetEye = targetPos.clone();
    targetEye.y += 20;

    const dir = new THREE.Vector3().subVectors(targetEye, spawnPos).normalize();

    dir.x += (Math.random() - 0.5) * 0.08;
    dir.y += (Math.random() - 0.5) * 0.05;
    dir.z += (Math.random() - 0.5) * 0.08;
    dir.normalize();

    const group = new THREE.Group();
    group.position.copy(spawnPos);
    group.add(new THREE.Mesh(res.coreGeometry, res.coreMaterial));
    const haloSprite = new THREE.Sprite(res.haloMaterial);
    haloSprite.scale.set(def.haloSize, def.haloSize, 1);
    group.add(haloSprite);
    scene.add(group);

    const lightSlot = acquireDynamicLight(def.lightColor, def.lightIntensity, def.lightDistance);
    if (lightSlot) lightSlot.light.position.copy(spawnPos);

    const baseDmg = monster.config.damage || 15;
    const spellDmgMult = (type === 'FIREBALL') ? 1.25 : ((type === 'LIGHTNING') ? 1.15 : 1.0);
    const damage = Math.round(baseDmg * spellDmgMult);

    activeProjectiles.push({
        type,
        owner: 'monster',
        sourceMonster: monster,
        group,
        lightSlot,
        position: spawnPos,
        velocity: dir.multiplyScalar(def.speed * 0.88),
        gravity: (type === 'FIREBALL') ? -120.0 : 0,
        radius: def.radius,
        aoeRadius: def.aoeRadius,
        damage,
        lifeTimer: 0,
        maxLife: def.maxLife,
        colorHex: def.colorHex
    });
}

export function triggerSpellExplosion(pos, type, damage, aoeRadius, colorHex) {
    const explosionSound = (type === 'FIREBALL') ? 'explod1.wav' : 'explod2.wav';
    playSound(explosionSound, 0.85);

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

    const flashSlot = acquireDynamicLight(colorHex || 0xffaa00, EXPLOSION_FLASH_INTENSITY, aoeRadius * 2);
    if (flashSlot) {
        flashSlot.light.position.copy(pos);
        explosionFlashes.push({ slot: flashSlot, timer: 0 });
    }

    for (let i = 0; i < monsters.length; i++) {
        const monster = monsters[i];
        if (!monster || monster.state === MonsterState.DEATH || monster.health <= 0) continue;

        const dist = pos.distanceTo(monster.position);
        if (dist <= aoeRadius) {
            const scale = Math.max(0.5, 1.0 - (dist / aoeRadius) * 0.5);
            const actualDamage = Math.round(damage * scale);
            monster.takeDamage(actualDamage);
            playSound(getRandomSound(IMPACT_SOUNDS), 0.7);
        }
    }
}

export function triggerMonsterSpellExplosion(pos, type, damage, aoeRadius, colorHex) {
    const explosionSound = (type === 'FIREBALL') ? 'explod1.wav' : 'explod2.wav';
    playSound(explosionSound, 0.85);

    const particleCount = (type === 'FIREBALL') ? 50 : 35;
    for (let i = 0; i < particleCount; i++) {
        const theta = Math.random() * Math.PI * 2;
        const phi = Math.random() * Math.PI - Math.PI / 2;
        const speed = 100 + Math.random() * 220;
        emitParticle(
            pos.x, pos.y, pos.z,
            Math.cos(phi) * Math.cos(theta) * speed,
            Math.sin(phi) * speed,
            Math.cos(phi) * Math.sin(theta) * speed,
            colorHex || 0xffaa00, 12, 0.65);
    }

    const flashSlot = acquireDynamicLight(colorHex || 0xffaa00, EXPLOSION_FLASH_INTENSITY, aoeRadius * 2);
    if (flashSlot) {
        flashSlot.light.position.copy(pos);
        explosionFlashes.push({ slot: flashSlot, timer: 0 });
    }

    if (controls) {
        const playerPos = controls.getObject().position;
        const dist = pos.distanceTo(playerPos);
        if (dist <= aoeRadius) {
            const scale = Math.max(0.5, 1.0 - (dist / aoeRadius) * 0.5);
            const actualDamage = Math.round(damage * scale);
            damagePlayer(actualDamage);
            createPickupFloatingText(`-${actualDamage} HP`, playerPos, '#ef4444');
        }
    }
}

// Sound Manager Functions
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

const soundBuffers = new Map();
const MAX_CONCURRENT_SOUNDS = 24;
const MAX_SOUND_LATENCY_MS = 750;
let activeSoundCount = 0;
let audioCtx = null;

export function getAudioContext() {
    if (!audioCtx) {
        const AudioContextClass = window.AudioContext || window.webkitAudioContext;
        if (AudioContextClass) audioCtx = new AudioContextClass();
    }
    return audioCtx;
}

export function resumeAudio() {
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
        if (!ctx || ctx.state !== 'running') return;

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

export function preloadSounds(filenames) {
    for (const name of new Set(filenames)) {
        if (name) loadSoundBuffer(name.startsWith('Sounds/') ? name : `Sounds/${name}`);
    }
}

export function getRandomSound(soundArray) {
    if (!soundArray || soundArray.length === 0) return null;
    const index = Math.floor(Math.random() * soundArray.length);
    return soundArray[index];
}

export function playPickupSound(isHealth = true) {
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

        const freq1 = isHealth ? 523.25 : 659.25;
        const freq2 = isHealth ? 659.25 : 880.00;
        const freq3 = isHealth ? 783.99 : 1046.50;

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

// Doors & Chests & Level Objects
export const levelObjects = [];
export function addLevelObject(mesh, position, radius = 0) {
    const entry = { mesh, position, radius };
    levelObjects.push(entry);
    return entry;
}

export const chests = [];

export async function openChest(chest) {
    if (!chest || chest.isOpen) return;
    chest.isOpen = true;

    const openDef = modelListParser.getModelByName('cdooropenchest');
    if (openDef) {
        const template = await load3DSModel(openDef);
        if (template) {
            const openModel = template.clone(true);
            const scaleFactor = openDef.scale || chest.scale || 1.0;
            openModel.scale.set(scaleFactor, scaleFactor, scaleFactor);
            openModel.position.copy(chest.position);
            openModel.rotation.y = chest.rotationY;

            scene.remove(chest.model);
            const objIdx = levelObjects.findIndex(o => o.mesh === chest.model);
            if (objIdx !== -1) {
                levelObjects.splice(objIdx, 1);
            }
            disposeOwnedResources(chest.model);

            openModel.updateMatrixWorld(true);
            scene.add(openModel);
            addLevelObject(openModel, chest.position.clone());
            chest.model = openModel;

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

    chest.position.y += 40;
    dropTreasure(chest.position, true);
}

export function openNearbyDoor() {
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

export async function dropTreasure(position, forceDrop = false) {
    if (!position) return;
    if (!forceDrop && Math.random() >= 0.8) return;
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

// 3DS / MD2 Models Loading & Path Utilities
export function generatePathVariations(filepath) {
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
        `Models/Players/MagicUser/WEAPONsorcerer.MD2`
    ];

    return [...new Set(paths)];
}

export function getMD2FrameLocalBox(geometry, frameIndex) {
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

export function computeMD2FrameBoundingBox(mesh, frameIndex = 0, targetBox = null) {
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

export async function loadMD2Geometry(modelDef) {
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
                // Try next
            }
        }

        console.warn(`Could not load MD2 model at ${modelDef.filepath}`);
        return null;
    })();

    md2GeometryCache.set(key, loadPromise);
    return await loadPromise;
}

const tdsNormalMapChecked = new Set();
export function applyNormalMapTo3DSMesh(child) {
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

export function freezeStaticObject(root) {
    root.traverse((child) => {
        child.updateMatrix();
        child.matrixAutoUpdate = false;
    });
}

export async function load3DSModel(modelDef) {
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

                const tdsLoader = new TDSLoader();
                tdsLoader.setResourcePath(dir);

                const object3ds = await new Promise((resolve, reject) => {
                    tdsLoader.load(path, resolve, undefined, reject);
                });

                if (object3ds) {
                    object3ds.traverse(applyNormalMapTo3DSMesh);
                    object3ds.traverse((c) => {
                        if (c.isMesh) {
                            c.geometry.userData.shared = true;
                            const mats = Array.isArray(c.material) ? c.material : [c.material];
                            for (const m of mats) {
                                m.userData.shared = true;
                                m.side = THREE.DoubleSide;
                            }
                        }
                    });
                    object3ds.rotation.set(-Math.PI / 2, 0, Math.PI / 2);
                    const wrapper = new THREE.Group();
                    wrapper.add(object3ds);
                    return wrapper;
                }
            } catch (e) {
                // Try next
            }
        }

        console.warn(`Could not load 3DS model at ${modelDef.filepath}`);
        return null;
    })();

    tdsModelCache.set(key, loadPromise);
    return await loadPromise;
}

export const isModel3DS = (def) => def.format === '3DS' || def.filepath.toLowerCase().endsWith('.3ds');
export const isModelMD2 = (def) => def.format === 'MD2' || def.filepath.toLowerCase().endsWith('.md2');

export function getMonsterWeaponDef(modelDef) {
    if (!modelDef.monsterWeapon || modelDef.monsterWeapon.toUpperCase() === 'NONE') return null;
    return modelListParser.getModelByName(modelDef.monsterWeapon);
}

export function prefetchLevelModels(instances) {
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

const STATIC_CHUNK_SIZE = 1024;

export function createTexturedMesh(texAlias, positions, uvs) {
    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));
    geometry.setAttribute('uv', new THREE.Float32BufferAttribute(uvs, 2));
    geometry.computeVertexNormals();
    return new THREE.Mesh(geometry, getSharedMaterial(texAlias));
}

export function clearCurrentWorld() {
    for (const obj of levelObjects) {
        if (obj.mesh) {
            scene.remove(obj.mesh);
            disposeOwnedResources(obj.mesh);
        }
    }
    levelObjects.length = 0;

    for (const m of monsters) {
        if (m.mesh) scene.remove(m.mesh);
        if (m.weaponMesh) scene.remove(m.weaponMesh);
        if (m.hpSprite) {
            scene.remove(m.hpSprite);
            if (m.hpSpriteMat) m.hpSpriteMat.dispose();
            if (m.hpTexture) m.hpTexture.dispose();
        }
    }
    monsters.length = 0;

    for (const pickup of activePickups) {
        if (pickup.mesh) scene.remove(pickup.mesh);
    }
    activePickups.length = 0;

    for (const door of doors) {
        if (door.mesh) scene.remove(door.mesh);
    }
    doors.length = 0;

    for (const chest of chests) {
        if (chest.model) scene.remove(chest.model);
    }
    chests.length = 0;

    clearLamps();
    clearTeleporters();
    levelTriangles.length = 0;
    modelBoxes.length = 0;
}

export async function loadGameWorld(textureManager, overrideMapText = null) {
    clearCurrentWorld();

    let mapText = overrideMapText;
    if (!mapText) {
        try {
            mapText = localStorage.getItem('generatedLevel1Map');
        } catch (e) {}
    }

    const [, objectsMap] = await Promise.all([
        textureManager.loadTexturesDat('bin/textures.dat'),
        objectsParser.parse('bin/Objects.dat'),
        modelListParser.parse('bin/modellist.dat')
    ]);

    let level;
    if (mapText) {
        level = levelParser.parseText(mapText);
    } else {
        level = await levelParser.parse('bin/level1.map');
    }
    const { instances, startPos } = level;

    console.log(`Loaded ${instances.length} objects in level map. StartPos:`, startPos);

    await prefetchLevelModels(instances);

    playerStartPos = new THREE.Vector3(startPos.x, startPos.y, startPos.z);
    controls.getObject().position.copy(playerStartPos);

    const rotRad = (startPos.rot * Math.PI) / 180.0;
    camera.rotation.set(0, rotRad, 0, 'YXZ');

    const objectIdByName = new Map();
    for (const [id, obj] of objectsMap) {
        const name = obj.name && obj.name.toLowerCase().trim();
        if (name && !objectIdByName.has(name)) objectIdByName.set(name, id);
    }
    const getObjectId = (name) => (name ? objectIdByName.get(name.toLowerCase().trim()) ?? -1 : -1);

    const staticChunks = new Map();

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

        if (inst.modelName) {
            const cleanModelName = inst.modelName.toLowerCase().trim();
            if (cleanModelName === 'spiral') {
                const posVec = new THREE.Vector3(inst.x, inst.y + 40.0, inst.z);
                createExitTeleporterEffect(posVec);
                continue;
            }

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
                            continue;
                        }

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

                        const mConfig = { name: modelDef.name, ...getMonsterConfig(modelDef.name) };
                        const posVec = new THREE.Vector3(inst.x, inst.y + 50, inst.z);
                        const monster = new Monster(mesh, weaponMesh, mConfig, posVec, angleRad);
                        monsters.push(monster);

                        meshEntry.position = monster.position;
                        if (weaponEntry) weaponEntry.position = monster.position;
                        continue;
                    }
                }
            }
        }

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

    buildCollisionGrid();

    hudEl('obj-display').textContent = builtObjectCount;
    hudEl('light-display').textContent = lightCount;
}

export async function generateAndLoadNewLevel(textureManager) {
    console.log("Generating new dungeon level...");
    const newMapText = generateDungeon();
    try {
        localStorage.setItem('generatedLevel1Map', newMapText);
    } catch (e) {
        console.warn('Could not write to localStorage:', e);
    }

    await loadGameWorld(textureManager, newMapText);
    await updateWeaponMesh();

    if (controls) {
        velocity.set(0, 0, 0);
        if (playerStartPos) {
            controls.getObject().position.copy(playerStartPos);
        }
    }

    playerStats.health = playerStats.maxHealth;
    setPlayerDead(false);
    const deathEl = hudEl('death-screen');
    if (deathEl) deathEl.style.display = 'none';

    updatePlayerHud();

    playSound('spell3.wav', 0.8);

    if (controls && controls.getObject()) {
        createPickupFloatingText('NEW DUNGEON GENERATED!', controls.getObject().position, '#38bdf8');
    }
    flashScreen(FLASH_GREEN, 0.4, 200);
}
