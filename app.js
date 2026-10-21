import * as THREE from 'three';
import { PointerLockControls } from 'three/addons/controls/PointerLockControls.js';
import { DDSLoader } from 'three/addons/loaders/DDSLoader.js';
import { TextureManager, ObjectsDatParser, LevelMapParser, ModelListParser } from './parsers.js';
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
    stopDistance: 60.0,     // minimum distance to maintain when attacking/moving
    alertDuration: 0.6,     // time spent in ALERT before entering CHASE
    damage: 10,
    maxHealth: 100,
    animations: MD2_ANIMATIONS
});

export const MONSTER_CONFIGS = {
    GOBLIN: {
        ...DEFAULT_MONSTER_CONFIG,
        moveSpeed: 190.0,
        attackRange: 75.0,
        aggroRadius: 750.0,
        damage: 6,
        maxHealth: 30
    },
    KNIGHT: {
        ...DEFAULT_MONSTER_CONFIG,
        moveSpeed: 150.0,
        attackRange: 95.0,
        aggroRadius: 850.0,
        damage: 15,
        maxHealth: 120
    },
    BAUUL: {
        ...DEFAULT_MONSTER_CONFIG,
        moveSpeed: 140.0,
        attackRange: 110.0,
        aggroRadius: 900.0,
        damage: 20,
        maxHealth: 150
    },
    OGRE: {
        ...DEFAULT_MONSTER_CONFIG,
        moveSpeed: 130.0,
        attackRange: 120.0,
        aggroRadius: 800.0,
        damage: 25,
        maxHealth: 180
    },
    WRAITH: {
        ...DEFAULT_MONSTER_CONFIG,
        moveSpeed: 180.0,
        attackRange: 180.0,
        aggroRadius: 950.0,
        damage: 12,
        maxHealth: 80
    }
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

        // Animation state tracking
        this.currentAnim = this.config.animations.STAND;
        this.animTimer = 0.0;
        this.animComplete = false;

        // Cross-fading for smooth state transitions
        this.prevAnimFrames = null;
        this.fadeTimer = 0.0;
        this.fadeDuration = 0.15; // 150ms cross-fade

        // Active morph target influences set tracking
        this.activeInfluences = new Set();

        // Bounding box for dynamic collision
        this.boundingBox = new THREE.Box3();
        this.updateBoundingBox();

        // Apply initial animation pose
        this.setAnimation(this.config.animations.STAND, true);
        this.updateMorphTargets(0);
    }

    updateBoundingBox() {
        if (!this.mesh) return;
        if (this.state === MonsterState.DEATH) {
            this.boundingBox.makeEmpty();
            return;
        }
        const curPose = this.getCurrentPose();
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

    getCurrentPose() {
        const anim = this.currentAnim;
        const totalFrames = anim.end - anim.start + 1;
        const progress = this.animTimer * anim.fps;

        let frameA, frameB, alpha;
        if (anim.loop) {
            const frameIdx = progress % totalFrames;
            frameA = anim.start + Math.floor(frameIdx);
            frameB = anim.start + ((Math.floor(frameIdx) + 1) % totalFrames);
            alpha = frameIdx - Math.floor(frameIdx);
        } else {
            const frameIdx = Math.min(progress, totalFrames - 1);
            frameA = anim.start + Math.floor(frameIdx);
            frameB = anim.start + Math.min(Math.floor(frameIdx) + 1, totalFrames - 1);
            alpha = frameIdx - Math.floor(frameIdx);
        }

        return { frameA, frameB, alpha };
    }

    updateMorphTargets(delta) {
        const meshGeometry = this.mesh ? this.mesh.geometry : null;
        if (!meshGeometry || !meshGeometry.morphAttributes || !meshGeometry.morphAttributes.position) {
            return;
        }

        const influences = this.mesh.morphTargetInfluences;
        if (!influences) return;

        // Reset previously active morph influences to 0
        for (const idx of this.activeInfluences) {
            if (idx >= 0 && idx < influences.length) {
                influences[idx] = 0;
            }
        }
        this.activeInfluences.clear();

        // Advance animation timer
        this.animTimer += delta;

        const anim = this.currentAnim;
        const totalFrames = anim.end - anim.start + 1;
        const totalDuration = totalFrames / anim.fps;

        if (!anim.loop && this.animTimer >= totalDuration) {
            this.animComplete = true;
        }

        const curPose = this.getCurrentPose();
        const finalWeights = new Map();

        if (this.prevAnimFrames && this.fadeTimer < this.fadeDuration) {
            this.fadeTimer += delta;
            const blend = Math.min(1.0, this.fadeTimer / this.fadeDuration);
            const invBlend = 1.0 - blend;

            const pA = this.prevAnimFrames.frameA;
            const pB = this.prevAnimFrames.frameB;
            const pAlpha = this.prevAnimFrames.alpha;

            finalWeights.set(pA, (finalWeights.get(pA) || 0) + (1 - pAlpha) * invBlend);
            finalWeights.set(pB, (finalWeights.get(pB) || 0) + pAlpha * invBlend);

            const cA = curPose.frameA;
            const cB = curPose.frameB;
            const cAlpha = curPose.alpha;

            finalWeights.set(cA, (finalWeights.get(cA) || 0) + (1 - cAlpha) * blend);
            finalWeights.set(cB, (finalWeights.get(cB) || 0) + cAlpha * blend);
        } else {
            this.prevAnimFrames = null;
            const cA = curPose.frameA;
            const cB = curPose.frameB;
            const cAlpha = curPose.alpha;

            finalWeights.set(cA, (1 - cAlpha));
            if (cAlpha > 1e-4) {
                finalWeights.set(cB, (finalWeights.get(cB) || 0) + cAlpha);
            }
        }

        // Apply calculated weights to mesh influences
        for (const [frameIdx, weight] of finalWeights.entries()) {
            if (frameIdx >= 0 && frameIdx < influences.length && weight > 0.001) {
                influences[frameIdx] = weight;
                this.activeInfluences.add(frameIdx);
            }
        }

        // Apply identical morph target weights to attached weapon mesh if available
        if (this.weaponMesh && this.weaponMesh.morphTargetInfluences) {
            const wInfluences = this.weaponMesh.morphTargetInfluences;
            for (let i = 0; i < wInfluences.length; i++) {
                wInfluences[i] = 0;
            }
            for (const [frameIdx, weight] of finalWeights.entries()) {
                if (frameIdx >= 0 && frameIdx < wInfluences.length && weight > 0.001) {
                    wInfluences[frameIdx] = weight;
                }
            }
        }
    }

    update(delta, playerPos, otherMonsters) {
        const dt = Math.min(delta, 0.1);

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
                this.weaponMesh.position.copy(this.position);
            }
            this.updateMorphTargets(dt);
            this.updateBoundingBox();
            return;
        }

        this.stateTimer += dt;

        if (this.attackCooldown > 0) {
            this.attackCooldown = Math.max(0, this.attackCooldown - dt);
        }

        const dx = playerPos.x - this.position.x;
        const dz = playerPos.z - this.position.z;
        const distToPlayer = Math.sqrt(dx * dx + dz * dz);
        const targetAngle = Math.atan2(dx, dz);

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
                this.setAnimation(this.config.animations.RUN);
                this.smoothTurn(targetAngle, dt);

                if (distToPlayer <= this.config.attackRange) {
                    this.setState(MonsterState.ATTACK);
                } else if (distToPlayer > this.config.deaggroRadius) {
                    this.setState(MonsterState.IDLE);
                } else {
                    if (distToPlayer > this.config.stopDistance) {
                        this.moveForward(dt, otherMonsters);
                    }
                }
                break;

            case MonsterState.ATTACK:
                this.setAnimation(this.config.animations.ATTACK);
                if (this.stateTimer < 0.2) {
                    this.smoothTurn(targetAngle, dt);
                }

                if (this.animComplete) {
                    this.attackCooldown = 0.5;
                    if (distToPlayer <= this.config.attackRange) {
                        this.setState(MonsterState.CHASE);
                    } else if (distToPlayer <= this.config.deaggroRadius) {
                        this.setState(MonsterState.CHASE);
                    } else {
                        this.setState(MonsterState.IDLE);
                    }
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
        this.state = newState;
        this.stateTimer = 0.0;
    }

    smoothTurn(targetAngle, dt) {
        let diff = targetAngle - this.rotationY;
        while (diff > Math.PI) diff -= Math.PI * 2;
        while (diff < -Math.PI) diff += Math.PI * 2;

        const maxTurn = this.config.turnRate * dt;
        const turn = Math.max(-maxTurn, Math.min(maxTurn, diff));
        this.rotationY += turn;
    }

    moveForward(dt, otherMonsters) {
        const moveDist = this.config.moveSpeed * dt;
        const dirX = Math.sin(this.rotationY);
        const dirZ = Math.cos(this.rotationY);

        let newX = this.position.x + dirX * moveDist;
        let newZ = this.position.z + dirZ * moveDist;

        if (otherMonsters) {
            const minMonsterDist = 50.0;
            for (const other of otherMonsters) {
                if (other === this || other.state === MonsterState.DEATH) continue;
                const mdx = newX - other.position.x;
                const mdz = newZ - other.position.z;
                const mdistSq = mdx * mdx + mdz * mdz;
                if (mdistSq < minMonsterDist * minMonsterDist && mdistSq > 0.001) {
                    const mdist = Math.sqrt(mdistSq);
                    const push = (minMonsterDist - mdist) * 0.5;
                    newX += (mdx / mdist) * push;
                    newZ += (mdz / mdist) * push;
                }
            }
        }

        this.position.x = newX;
        this.position.z = newZ;

        // Resolve capsule collisions against level geometry and model bounding boxes
        resolveMonsterCapsuleCollisions(this);
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

// Level Objects & Distance Culling
const RENDER_DISTANCE = 3000.0;
const levelObjects = [];
const monsters = [];
const doors = [];

// Spatial Collision Structures
const levelTriangles = [];
const modelBoxes = [];
const collisionGrid = new Map();
const GRID_CELL_SIZE = 256;

// Player Capsule parameters
const CAPSULE_RADIUS = 28.0;
const CAPSULE_HEIGHT = 120.0;
const EYE_HEIGHT = 100.0;
const GRAVITY = 1600.0;
const JUMP_SPEED = 520.0;

const textureManager = new TextureManager();
const objectsParser = new ObjectsDatParser();
const levelParser = new LevelMapParser();
const modelListParser = new ModelListParser();

const ddsLoader = new DDSLoader();
const tdsLoader = new TDSLoader();
const md2Loader = new MD2Loader();
const textureLoader = new THREE.TextureLoader();

const textureCache = new Map();
const tdsModelCache = new Map();
const md2GeometryCache = new Map();

// Player Guns list
const PLAYER_GUNS = [
    'AXE',
    'FLAMESWORD',
    'BASTARDSWORD',
    'BATTLEAXE',
    'ICESWORD',
    'LIGHTNINGSWORD',
    'MORNINGSTAR',
    'SPIKEDFLAIL',
    'SPLITSWORD',
    'SUPERFLAMESWORD'
];
let currentGunIndex = 0;
let currentGunMesh = null;

let playerWeaponAnimTimer = 0.0;
let playerWeaponCurrentAnim = MD2_ANIMATIONS.STAND;
let playerWeaponActiveInfluences = new Set();
let isWeaponAttacking = false;

const defaultCanvas = document.createElement('canvas');
defaultCanvas.width = 128;
defaultCanvas.height = 128;
const ctx = defaultCanvas.getContext('2d');
ctx.fillStyle = '#666677';
ctx.fillRect(0, 0, 128, 128);
ctx.fillStyle = '#444455';
ctx.fillRect(0, 0, 64, 64);
ctx.fillRect(64, 64, 64, 64);

async function init() {
    const container = document.getElementById('canvas-container');

    // Scene setup
    scene = new THREE.Scene();
    scene.background = new THREE.Color(0x1a1a24);

    // Camera setup
    camera = new THREE.PerspectiveCamera(75, window.innerWidth / window.innerHeight, 1, 50000);

    // Ambient Lighting
    const ambientLight = new THREE.AmbientLight(0xffffff, 2.0);
    scene.add(ambientLight);

    // Directional Light for general visibility
    const dirLight = new THREE.DirectionalLight(0xffffff, 1.5);
    dirLight.position.set(5000, 10000, 5000);
    scene.add(dirLight);

    // Renderer setup
    renderer = new THREE.WebGLRenderer({ antialias: true });
    renderer.setPixelRatio(window.devicePixelRatio);
    renderer.setSize(window.innerWidth, window.innerHeight);
    container.appendChild(renderer.domElement);

    // Pointer Lock Controls
    controls = new PointerLockControls(camera, document.body);

    const instructions = document.getElementById('instructions');
    instructions.addEventListener('click', () => {
        controls.lock();
    });

    controls.addEventListener('lock', () => {
        instructions.style.display = 'none';
    });

    controls.addEventListener('unlock', () => {
        instructions.style.display = 'block';
    });

    scene.add(controls.getObject());

    // Keyboard Listeners
    const onKeyDown = function (event) {
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
    await loadGameWorld();

    // Initialize Active Weapon
    await updateWeaponMesh();

    window.camera = camera;
    window.controls = controls;
    window.scene = scene;
    window.monsters = monsters;
    window.modelBoxes = modelBoxes;
    window.resolveMonsterCapsuleCollisions = resolveMonsterCapsuleCollisions;
    window.doors = doors;
    window.openNearbyDoor = openNearbyDoor;
    Object.defineProperty(window, 'playerWeaponCurrentAnim', {
        get: () => playerWeaponCurrentAnim
    });

    prevTime = performance.now();
    animate();
}

function getTexture(texAlias, onLoadCallback) {
    if (!texAlias) return null;

    if (textureCache.has(texAlias)) {
        const cached = textureCache.get(texAlias);
        if (cached.loadedTex) {
            if (onLoadCallback) onLoadCallback(cached.loadedTex);
            return cached.loadedTex;
        } else if (cached.isLoading) {
            if (onLoadCallback) cached.callbacks.push(onLoadCallback);
            return cached.fallbackTex;
        }
    }

    const fallbackTex = new THREE.CanvasTexture(defaultCanvas);
    fallbackTex.wrapS = THREE.RepeatWrapping;
    fallbackTex.wrapT = THREE.RepeatWrapping;

    const cacheEntry = {
        loadedTex: null,
        fallbackTex: fallbackTex,
        isLoading: true,
        callbacks: onLoadCallback ? [onLoadCallback] : []
    };
    textureCache.set(texAlias, cacheEntry);

    const texPath = textureManager.getTexturePath(texAlias);

    let possiblePaths = [];
    if (texPath) {
        const ddsPath = `Textures/${texPath}`;
        const jpgPath = `Textures/${texPath.replace(/\.dds$/i, '.jpg')}`;
        const pngPath = `Textures/${texPath.replace(/\.dds$/i, '.png')}`;
        const bmpPath = `Textures/${texPath.replace(/\.dds$/i, '.bmp')}`;

        possiblePaths.push(jpgPath, pngPath, bmpPath, ddsPath);
    }

    const aliasJpg = `Textures/${texAlias}.jpg`;
    const aliasPng = `Textures/${texAlias}.png`;
    const aliasBmp = `Textures/${texAlias}.bmp`;
    const aliasDds = `Textures/${texAlias}.dds`;
    possiblePaths.push(aliasJpg, aliasPng, aliasBmp, aliasDds);

    possiblePaths = [...new Set(possiblePaths)];

    function tryLoad(index) {
        if (index >= possiblePaths.length) return;
        const path = possiblePaths[index];
        const loader = path.toLowerCase().endsWith('.dds') ? ddsLoader : textureLoader;

        loader.load(
            path,
            (loadedTex) => {
                loadedTex.wrapS = THREE.RepeatWrapping;
                loadedTex.wrapT = THREE.RepeatWrapping;
                cacheEntry.loadedTex = loadedTex;
                cacheEntry.isLoading = false;

                for (const cb of cacheEntry.callbacks) {
                    cb(loadedTex);
                }
                cacheEntry.callbacks = [];
            },
            undefined,
            () => {
                tryLoad(index + 1);
            }
        );
    }

    tryLoad(0);

    return fallbackTex;
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
        `Models/Players/${d2Cap}/${base.toLowerCase()}${ext.toLowerCase()}`,
        `Models/Players/${d2Cap}/${base.toLowerCase()}${ext.toUpperCase()}`,
        `Models/Players/${d2.toLowerCase()}/${base.toLowerCase()}${ext.toLowerCase()}`,
        `Models/Players/${d2.toLowerCase()}/${base.toLowerCase()}${ext.toUpperCase()}`,
        `Models/Players/${d2.toLowerCase()}/${base}${ext}`,
        `Models/3ds/${d2Cap}/${base.toLowerCase()}${ext.toLowerCase()}`,
        `Models/3ds/${d2.toLowerCase()}/${base.toLowerCase()}${ext.toLowerCase()}`
    ];

    return [...new Set(paths)];
}

const _tempMD2Vec = new THREE.Vector3();

function computeMD2FrameBoundingBox(mesh, frameIndex = 0, targetBox = null) {
    const box = targetBox || new THREE.Box3();
    box.makeEmpty();

    const geometry = mesh.geometry;
    if (!geometry) {
        box.setFromObject(mesh);
        return box;
    }

    mesh.updateMatrixWorld(true);

    const morphPositions = geometry.morphAttributes && geometry.morphAttributes.position;
    let posAttr = (morphPositions && morphPositions.length > frameIndex)
        ? morphPositions[frameIndex]
        : geometry.attributes.position;

    if (!posAttr) {
        box.setFromObject(mesh);
        return box;
    }

    const array = posAttr.array;
    for (let i = 0; i < array.length; i += 3) {
        _tempMD2Vec.set(array[i], array[i + 1], array[i + 2]);
        _tempMD2Vec.applyMatrix4(mesh.matrixWorld);
        box.expandByPoint(_tempMD2Vec);
    }

    return box;
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
                    console.log(`Successfully loaded MD2 at ${path}`);
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
                tdsLoader.setResourcePath(dir);

                const object3ds = await new Promise((resolve, reject) => {
                    tdsLoader.load(path, resolve, undefined, reject);
                });

                if (object3ds) {
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

async function loadGameWorld() {
    console.log('Loading textures.dat...');
    await textureManager.loadTexturesDat('bin/textures.dat');

    console.log('Parsing Objects.dat...');
    const objectsMap = await objectsParser.parse('bin/Objects.dat');

    console.log('Parsing modellist.dat...');
    await modelListParser.parse('bin/modellist.dat');

    console.log('Parsing level1.map...');
    const { instances, startPos } = await levelParser.parse('bin/level1.map');

    console.log(`Loaded ${instances.length} objects in level1.map. StartPos:`, startPos);

    // Reset level objects tracking
    levelObjects.length = 0;
    monsters.length = 0;
    levelTriangles.length = 0;
    modelBoxes.length = 0;
    doors.length = 0;

    // Set initial camera position
    controls.getObject().position.set(startPos.x, startPos.y, startPos.z);

    // Look along level start direction
    const rotRad = (startPos.rot * Math.PI) / 180.0;
    camera.rotation.y = rotRad;

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
        const objId = checkObjIdByName(inst.name, objectsMap);
        if (EXCLUDED_IDS.has(objId)) return true;
        return false;
    }

    // Build map geometry
    let builtObjectCount = 0;
    let lightCount = 0;

    for (const inst of instances) {
        if (inst.isStartPos) continue;

        if (inst.isLight) {
            lightCount++;
            const pointLight = new THREE.PointLight(0xffd29d, 3.0, 1500);
            pointLight.position.set(inst.x, inst.y + 100, inst.z);
            scene.add(pointLight);
            continue;
        }

        // Check if item instance is a 3DS or MD2 model from modellist.dat FIRST
        // Placeholder objects (e.g. OBJECT !monster1) reference 3DS/MD2 models on their ROT_ANGLE line.
        // We MUST process and render the model, but NOT draw the placeholder geometry from Objects.dat.
        if (inst.modelName) {
            const modelDef = modelListParser.getModelByName(inst.modelName);
            if (modelDef) {
                if (inst.textureAlias === '-1' || modelDef.format === '3DS' || modelDef.filepath.toLowerCase().endsWith('.3ds')) {
                    const template = await load3DSModel(modelDef);
                    if (template) {
                        const model = template.clone(true);
                        const scaleFactor = modelDef.scale || 1.0;
                        model.scale.set(scaleFactor, scaleFactor, scaleFactor);

                        model.position.set(inst.x, inst.y + 50, inst.z);

                        const angleRad = (inst.rot * Math.PI) / 180.0;
                        model.rotation.y = angleRad;

                        model.traverse((child) => {
                            if (child.isMesh && child.material) {
                                child.material.side = THREE.DoubleSide;
                            }
                        });

                        scene.add(model);
                        levelObjects.push({ mesh: model, position: new THREE.Vector3(inst.x, inst.y + 50, inst.z) });
                        builtObjectCount++;

                        // Add bounding box for 3DS model collision (must update matrixWorld first)
                        model.updateMatrixWorld(true);
                        const box = new THREE.Box3().setFromObject(model);
                        if (!box.isEmpty()) {
                            modelBoxes.push(box);
                        }
                        continue;
                    }
                } else if (modelDef.format === 'MD2' || modelDef.filepath.toLowerCase().endsWith('.md2')) {
                    const geometry = await loadMD2Geometry(modelDef);
                    if (geometry) {
                        const scaleFactor = modelDef.scale || 1.0;
                        const texAlias = (inst.textureAlias && inst.textureAlias !== '-1') ? inst.textureAlias : modelDef.textureAlias;

                        const material = new THREE.MeshLambertMaterial({
                            side: THREE.DoubleSide
                        });

                        if (texAlias) {
                            const tex = getTexture(texAlias, (loadedTex) => {
                                material.map = loadedTex;
                                material.needsUpdate = true;
                            });
                            material.map = tex;
                        }

                        const mesh = new THREE.Mesh(geometry, material);
                        mesh.scale.set(scaleFactor, scaleFactor, scaleFactor);
                        mesh.position.set(inst.x, inst.y + 50, inst.z);

                        const angleRad = (inst.rot * Math.PI) / 180.0;
                        mesh.rotation.y = angleRad - Math.PI / 2;

                        scene.add(mesh);
                        levelObjects.push({ mesh: mesh, position: new THREE.Vector3(inst.x, inst.y + 50, inst.z) });
                        builtObjectCount++;

                        // Draw monster weapon if present
                        let weaponMesh = null;
                        if (modelDef.monsterWeapon && modelDef.monsterWeapon.toUpperCase() !== 'NONE') {
                            const weaponDef = modelListParser.getModelByName(modelDef.monsterWeapon);
                            if (weaponDef) {
                                if (weaponDef.format === 'MD2' || weaponDef.filepath.toLowerCase().endsWith('.md2')) {
                                    const weaponGeom = await loadMD2Geometry(weaponDef);
                                    if (weaponGeom) {
                                        const weaponScale = weaponDef.scale || scaleFactor;
                                        const weaponTexAlias = weaponDef.textureAlias;
                                        const weaponMat = new THREE.MeshLambertMaterial({ side: THREE.DoubleSide });
                                        if (weaponTexAlias) {
                                            const wTex = getTexture(weaponTexAlias, (loadedTex) => {
                                                weaponMat.map = loadedTex;
                                                weaponMat.needsUpdate = true;
                                            });
                                            weaponMat.map = wTex;
                                        }
                                        weaponMesh = new THREE.Mesh(weaponGeom, weaponMat);
                                        weaponMesh.scale.set(weaponScale, weaponScale, weaponScale);
                                        weaponMesh.position.set(inst.x, inst.y + 50, inst.z);
                                        weaponMesh.rotation.y = angleRad - Math.PI / 2;
                                        scene.add(weaponMesh);
                                        levelObjects.push({ mesh: weaponMesh, position: new THREE.Vector3(inst.x, inst.y + 50, inst.z) });
                                    }
                                } else if (weaponDef.format === '3DS' || weaponDef.filepath.toLowerCase().endsWith('.3ds')) {
                                    const weaponTemplate = await load3DSModel(weaponDef);
                                    if (weaponTemplate) {
                                        weaponMesh = weaponTemplate.clone(true);
                                        const weaponScale = weaponDef.scale || scaleFactor;
                                        weaponMesh.scale.set(weaponScale, weaponScale, weaponScale);
                                        weaponMesh.position.set(inst.x, inst.y + 50, inst.z);
                                        weaponMesh.rotation.y = angleRad;
                                        weaponMesh.traverse((child) => {
                                            if (child.isMesh && child.material) {
                                                child.material.side = THREE.DoubleSide;
                                            }
                                        });
                                        scene.add(weaponMesh);
                                        levelObjects.push({ mesh: weaponMesh, position: new THREE.Vector3(inst.x, inst.y + 50, inst.z) });
                                    }
                                }
                            }
                        }

                        // Instantiate Monster state machine controller
                        const mConfig = getMonsterConfig(modelDef.name);
                        const posVec = new THREE.Vector3(inst.x, inst.y + 50, inst.z);
                        const monster = new Monster(mesh, weaponMesh, mConfig, posVec, angleRad);
                        monsters.push(monster);

                        // Register monster's dynamic bounding box for capsule collision
                        if (!monster.boundingBox.isEmpty()) {
                            modelBoxes.push(monster.boundingBox);
                        }
                        continue;
                    }
                }
            }
        }

        // Do not draw or collide with excluded placeholder objects from Objects.dat
        if (isExcludedInstance(inst)) {
            continue;
        }

        const objId = checkObjIdByName(inst.name, objectsMap);
        const objDef = objectsMap.get(objId);

        if (!objDef) {
            continue;
        }

        builtObjectCount++;

        const isDoor55 = (inst.name && inst.name.toLowerCase().trim() === 'door55') ||
                         (objDef && objDef.name && objDef.name.toLowerCase().trim() === 'door55');
        const doorTriangles = [];

        const angleRad = (inst.rot * Math.PI) / 180.0;
        const cosA = Math.cos(angleRad);
        const sinA = Math.sin(angleRad);

        const objGroup = new THREE.Group();

        const instTexMap = new Map();

        for (const poly of objDef.polys) {
            const texAlias = poly.texture || 'wall1';
            if (!instTexMap.has(texAlias)) {
                instTexMap.set(texAlias, { positions: [], uvs: [] });
            }

            const group = instTexMap.get(texAlias);

            const worldVerts = [];
            for (const v of poly.verts) {
                const rx = v.x * cosA - v.z * sinA;
                const rz = v.x * sinA + v.z * cosA;

                const wx = rx + inst.x;
                const wy = v.y + inst.y;
                const wz = rz + inst.z;

                group.positions.push(wx, wy, wz);
                group.uvs.push(v.u, v.v);

                worldVerts.push(new THREE.Vector3(wx, wy, wz));
            }

            for (let i = 1; i < worldVerts.length - 1; i++) {
                const a = worldVerts[0];
                const b = worldVerts[i];
                const c = worldVerts[i + 1];
                if (a && b && c) {
                    const tri = {
                        a, b, c,
                        minX: Math.min(a.x, b.x, c.x),
                        maxX: Math.max(a.x, b.x, c.x),
                        minY: Math.min(a.y, b.y, c.y),
                        maxY: Math.max(a.y, b.y, c.y),
                        minZ: Math.min(a.z, b.z, c.z),
                        maxZ: Math.max(a.z, b.z, c.z)
                    };
                    levelTriangles.push(tri);
                    if (isDoor55) {
                        doorTriangles.push(tri);
                    }
                }
            }
        }

        for (const [texAlias, data] of instTexMap.entries()) {
            if (data.positions.length === 0) continue;

            const geometry = new THREE.BufferGeometry();
            geometry.setAttribute('position', new THREE.Float32BufferAttribute(data.positions, 3));
            geometry.setAttribute('uv', new THREE.Float32BufferAttribute(data.uvs, 2));
            geometry.computeVertexNormals();

            const material = new THREE.MeshLambertMaterial({
                side: THREE.DoubleSide
            });

            const initialTexture = getTexture(texAlias, (loadedTex) => {
                material.map = loadedTex;
                material.needsUpdate = true;
            });
            material.map = initialTexture;

            const mesh = new THREE.Mesh(geometry, material);
            objGroup.add(mesh);
        }

        const levelObj = { mesh: objGroup, position: new THREE.Vector3(inst.x, inst.y, inst.z) };
        scene.add(objGroup);
        levelObjects.push(levelObj);

        if (isDoor55) {
            doors.push({
                mesh: objGroup,
                position: new THREE.Vector3(inst.x, inst.y, inst.z),
                currentUp: 0,
                maxUp: 200.0,
                isOpening: false,
                isOpen: false,
                triangles: doorTriangles,
                levelObj: levelObj
            });
        }
    }

    // Build spatial grid for static level triangles
    buildCollisionGrid();

    document.getElementById('obj-display').textContent = builtObjectCount;
    document.getElementById('light-display').textContent = lightCount;
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
    return `${cx},${cy},${cz}`;
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
const _triRes = { capP: _bestCapP, triP: _bestTriP, dist: 0 };

function closestPointsCapsuleTriangle(capA, capB, v0, v1, v2) {
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

    const edges = [[v0, v1], [v1, v2], [v2, v0]];
    for (let i = 0; i < 3; i++) {
        closestPointsSegmentSegment(capA, capB, edges[i][0], edges[i][1], _pSeg1, _pSeg2);
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

const _boxP = new THREE.Vector3();
const _segP = new THREE.Vector3();

function closestPointsCapsuleBox(capA, capB, box) {
    let minDistSq = Infinity;
    const bestCapP = new THREE.Vector3();
    const bestBoxP = new THREE.Vector3();

    const SAMPLES = 5;
    for (let i = 0; i <= SAMPLES; i++) {
        const t = i / SAMPLES;
        _segP.lerpVectors(capA, capB, t);
        box.clampPoint(_segP, _boxP);
        const dSq = _segP.distanceToSquared(_boxP);
        if (dSq < minDistSq) {
            minDistSq = dSq;
            bestCapP.copy(_segP);
            bestBoxP.copy(_boxP);
        }
    }

    return { capP: bestCapP, boxP: bestBoxP, dist: Math.sqrt(minDistSq) };
}

const MONSTER_CAPSULE_RADIUS = 24.0;
const MONSTER_CAPSULE_HEIGHT = 80.0;
const _mCapA = new THREE.Vector3();
const _mCapB = new THREE.Vector3();

const _capA = new THREE.Vector3();
const _capB = new THREE.Vector3();
const _normal = new THREE.Vector3();
const _faceNormal = new THREE.Vector3();
const _edge1 = new THREE.Vector3();
const _edge2 = new THREE.Vector3();

function resolveMonsterCapsuleCollisions(monster) {
    if (!monster || !monster.position) return;
    monster.isGrounded = false;
    const mPos = monster.position;

    const basePosY = mPos.y;

    const velYPadding = Math.abs(monster.velocity ? monster.velocity.y * 0.2 : 0);
    const queryMinX = mPos.x - MONSTER_CAPSULE_RADIUS - 30;
    const queryMaxX = mPos.x + MONSTER_CAPSULE_RADIUS + 30;
    const queryMinY = basePosY - 50 - velYPadding;
    const queryMaxY = basePosY + MONSTER_CAPSULE_HEIGHT + 50 + velYPadding;
    const queryMinZ = mPos.z - MONSTER_CAPSULE_RADIUS - 30;
    const queryMaxZ = mPos.z + MONSTER_CAPSULE_RADIUS + 30;

    const minCx = Math.floor(queryMinX / GRID_CELL_SIZE);
    const maxCx = Math.floor(queryMaxX / GRID_CELL_SIZE);
    const minCy = Math.floor(queryMinY / GRID_CELL_SIZE);
    const maxCy = Math.floor(queryMaxY / GRID_CELL_SIZE);
    const minCz = Math.floor(queryMinZ / GRID_CELL_SIZE);
    const maxCz = Math.floor(queryMaxZ / GRID_CELL_SIZE);

    const nearbyTriangles = new Set();
    for (let cx = minCx; cx <= maxCx; cx++) {
        for (let cy = minCy; cy <= maxCy; cy++) {
            for (let cz = minCz; cz <= maxCz; cz++) {
                const cell = collisionGrid.get(getGridKey(cx, cy, cz));
                if (cell) {
                    for (let i = 0; i < cell.length; i++) {
                        nearbyTriangles.add(cell[i]);
                    }
                }
            }
        }
    }

    const MAX_ITERATIONS = 4;
    for (let iter = 0; iter < MAX_ITERATIONS; iter++) {
        _mCapA.set(mPos.x, mPos.y - 50.0 + MONSTER_CAPSULE_RADIUS, mPos.z);
        _mCapB.set(mPos.x, mPos.y - 50.0 + MONSTER_CAPSULE_HEIGHT - MONSTER_CAPSULE_RADIUS, mPos.z);

        let maxPenetration = 0;
        let bestNormal = null;

        for (const tri of nearbyTriangles) {
            if (tri.maxX < queryMinX || tri.minX > queryMaxX ||
                tri.maxY < queryMinY || tri.minY > queryMaxY ||
                tri.maxZ < queryMinZ || tri.minZ > queryMaxZ) {
                continue;
            }

            const res = closestPointsCapsuleTriangle(_mCapA, _mCapB, tri.a, tri.b, tri.c);
            if (res.dist < MONSTER_CAPSULE_RADIUS) {
                const pen = MONSTER_CAPSULE_RADIUS - res.dist;
                _normal.subVectors(res.capP, res.triP);

                _edge1.subVectors(tri.b, tri.a);
                _edge2.subVectors(tri.c, tri.a);
                _faceNormal.crossVectors(_edge1, _edge2).normalize();

                if (_normal.lengthSq() > 1e-8) {
                    _normal.normalize();
                } else {
                    _normal.copy(_faceNormal);
                }

                if (_faceNormal.y > 0.3) {
                    if (res.capP.y < res.triP.y || _normal.y < 0) {
                        _normal.set(0, 1, 0);
                    }
                } else if (_faceNormal.y < -0.3) {
                    if (res.capP.y > res.triP.y || _normal.y > 0) {
                        _normal.set(0, -1, 0);
                    }
                }

                if (pen > maxPenetration) {
                    maxPenetration = pen;
                    bestNormal = _normal.clone();
                }
            }
        }

        for (let b = 0; b < modelBoxes.length; b++) {
            const box = modelBoxes[b];
            if (box === monster.boundingBox || box.isEmpty()) continue;
            if (box.max.x < queryMinX || box.min.x > queryMaxX ||
                box.max.y < queryMinY || box.min.y > queryMaxY ||
                box.max.z < queryMinZ || box.min.z > queryMaxZ) {
                continue;
            }

            const res = closestPointsCapsuleBox(_mCapA, _mCapB, box);
            if (res.dist < MONSTER_CAPSULE_RADIUS) {
                const pen = MONSTER_CAPSULE_RADIUS - res.dist;
                _normal.subVectors(res.capP, res.boxP);
                if (_normal.lengthSq() > 1e-8) {
                    _normal.normalize();
                } else {
                    const boxCenter = _tempDiff.addVectors(box.min, box.max).multiplyScalar(0.5);
                    _normal.subVectors(res.capP, boxCenter);
                    if (res.capP.y >= box.max.y - 15) {
                        _normal.set(0, 1, 0);
                    } else {
                        _normal.y = 0;
                        if (_normal.lengthSq() > 1e-8) {
                            _normal.normalize();
                        } else {
                            _normal.set(0, 1, 0);
                        }
                    }
                }

                if (pen > maxPenetration) {
                    maxPenetration = pen;
                    bestNormal = _normal.clone();
                }
            }
        }

        if (maxPenetration > 0 && bestNormal) {
            mPos.addScaledVector(bestNormal, maxPenetration + 0.01);

            if (bestNormal.y > 0.5) {
                monster.isGrounded = true;
            }

            if (monster.velocity) {
                const dotVel = monster.velocity.dot(bestNormal);
                if (dotVel < 0) {
                    monster.velocity.addScaledVector(bestNormal, -dotVel);
                }
            }
        } else {
            break;
        }
    }
}

function resolveCapsuleCollisions(camPos) {
    isGrounded = false;
    const basePosY = camPos.y - EYE_HEIGHT;

    const velYPadding = Math.abs(velocity.y * 0.2);
    const queryMinX = camPos.x - CAPSULE_RADIUS - 30;
    const queryMaxX = camPos.x + CAPSULE_RADIUS + 30;
    const queryMinY = basePosY - 50 - velYPadding;
    const queryMaxY = basePosY + CAPSULE_HEIGHT + 50 + velYPadding;
    const queryMinZ = camPos.z - CAPSULE_RADIUS - 30;
    const queryMaxZ = camPos.z + CAPSULE_RADIUS + 30;

    const minCx = Math.floor(queryMinX / GRID_CELL_SIZE);
    const maxCx = Math.floor(queryMaxX / GRID_CELL_SIZE);
    const minCy = Math.floor(queryMinY / GRID_CELL_SIZE);
    const maxCy = Math.floor(queryMaxY / GRID_CELL_SIZE);
    const minCz = Math.floor(queryMinZ / GRID_CELL_SIZE);
    const maxCz = Math.floor(queryMaxZ / GRID_CELL_SIZE);

    const nearbyTriangles = new Set();
    for (let cx = minCx; cx <= maxCx; cx++) {
        for (let cy = minCy; cy <= maxCy; cy++) {
            for (let cz = minCz; cz <= maxCz; cz++) {
                const cell = collisionGrid.get(getGridKey(cx, cy, cz));
                if (cell) {
                    for (let i = 0; i < cell.length; i++) {
                        nearbyTriangles.add(cell[i]);
                    }
                }
            }
        }
    }

    const MAX_ITERATIONS = 4;
    for (let iter = 0; iter < MAX_ITERATIONS; iter++) {
        const curBaseY = camPos.y - EYE_HEIGHT;
        _capA.set(camPos.x, curBaseY + CAPSULE_RADIUS, camPos.z);
        _capB.set(camPos.x, curBaseY + CAPSULE_HEIGHT - CAPSULE_RADIUS, camPos.z);

        let maxPenetration = 0;
        let bestNormal = null;

        for (const tri of nearbyTriangles) {
            if (tri.maxX < queryMinX || tri.minX > queryMaxX ||
                tri.maxY < queryMinY || tri.minY > queryMaxY ||
                tri.maxZ < queryMinZ || tri.minZ > queryMaxZ) {
                continue;
            }

            const res = closestPointsCapsuleTriangle(_capA, _capB, tri.a, tri.b, tri.c);
            if (res.dist < CAPSULE_RADIUS) {
                const pen = CAPSULE_RADIUS - res.dist;
                _normal.subVectors(res.capP, res.triP);

                _edge1.subVectors(tri.b, tri.a);
                _edge2.subVectors(tri.c, tri.a);
                _faceNormal.crossVectors(_edge1, _edge2).normalize();

                if (_normal.lengthSq() > 1e-8) {
                    _normal.normalize();
                } else {
                    _normal.copy(_faceNormal);
                }

                if (_faceNormal.y > 0.3) {
                    if (res.capP.y < res.triP.y || _normal.y < 0) {
                        _normal.set(0, 1, 0);
                    }
                } else if (_faceNormal.y < -0.3) {
                    if (res.capP.y > res.triP.y || _normal.y > 0) {
                        _normal.set(0, -1, 0);
                    }
                }

                if (pen > maxPenetration) {
                    maxPenetration = pen;
                    bestNormal = _normal.clone();
                }
            }
        }

        for (let b = 0; b < modelBoxes.length; b++) {
            const box = modelBoxes[b];
            if (box.isEmpty()) continue;
            if (box.max.x < queryMinX || box.min.x > queryMaxX ||
                box.max.y < queryMinY || box.min.y > queryMaxY ||
                box.max.z < queryMinZ || box.min.z > queryMaxZ) {
                continue;
            }

            const res = closestPointsCapsuleBox(_capA, _capB, box);
            if (res.dist < CAPSULE_RADIUS) {
                const pen = CAPSULE_RADIUS - res.dist;
                _normal.subVectors(res.capP, res.boxP);
                if (_normal.lengthSq() > 1e-8) {
                    _normal.normalize();
                } else {
                    const boxCenter = _tempDiff.addVectors(box.min, box.max).multiplyScalar(0.5);
                    _normal.subVectors(res.capP, boxCenter);
                    if (res.capP.y >= box.max.y - 15) {
                        _normal.set(0, 1, 0);
                    } else {
                        _normal.y = 0;
                        if (_normal.lengthSq() > 1e-8) {
                            _normal.normalize();
                        } else {
                            _normal.set(0, 1, 0);
                        }
                    }
                }

                if (pen > maxPenetration) {
                    maxPenetration = pen;
                    bestNormal = _normal.clone();
                }
            }
        }

        if (maxPenetration > 0 && bestNormal) {
            camPos.addScaledVector(bestNormal, maxPenetration + 0.01);

            if (bestNormal.y > 0.5) {
                isGrounded = true;
            }

            const dotVel = velocity.dot(bestNormal);
            if (dotVel < 0) {
                velocity.addScaledVector(bestNormal, -dotVel);
            }
        } else {
            break;
        }
    }
}

function triggerWeaponAttack() {
    isWeaponAttacking = true;
    playerWeaponAnimTimer = 0.0;
    playerWeaponCurrentAnim = MD2_ANIMATIONS.ATTACK;
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

    if (!anim.loop && playerWeaponAnimTimer >= totalDuration) {
        if (isWeaponAttacking) {
            isWeaponAttacking = false;
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
}

function updateDoors(delta) {
    let gridNeedsUpdate = false;
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
            }

            gridNeedsUpdate = true;
        }
    }

    if (gridNeedsUpdate) {
        buildCollisionGrid();
    }
}

function cycleWeapon(dir) {
    currentGunIndex = (currentGunIndex + dir + PLAYER_GUNS.length) % PLAYER_GUNS.length;
    updateWeaponMesh();
}

async function updateWeaponMesh() {
    const gunName = PLAYER_GUNS[currentGunIndex];
    const gunDisplay = document.getElementById('gun-display');
    if (gunDisplay) {
        gunDisplay.textContent = gunName;
    }

    if (currentGunMesh) {
        camera.remove(currentGunMesh);
        currentGunMesh = null;
    }

    playerWeaponAnimTimer = 0.0;
    playerWeaponCurrentAnim = MD2_ANIMATIONS.STAND;
    playerWeaponActiveInfluences.clear();
    isWeaponAttacking = false;

    const modelDef = modelListParser.getModelByName(gunName);
    if (!modelDef) return;

    const geometry = await loadMD2Geometry(modelDef);
    if (!geometry) return;

    const scaleFactor = modelDef.scale || 1.0;
    const texAlias = modelDef.textureAlias;

    const material = new THREE.MeshLambertMaterial({
        side: THREE.DoubleSide
    });

    if (texAlias) {
        const tex = getTexture(texAlias, (loadedTex) => {
            material.map = loadedTex;
            material.needsUpdate = true;
        });
        material.map = tex;
    }

    const mesh = new THREE.Mesh(geometry, material);
    mesh.scale.set(scaleFactor, scaleFactor, scaleFactor);
    //mesh.position.set(40, -20, 5);
    mesh.position.set(0, -25, 5);

    mesh.rotation.set(0, Math.PI / 2, 0);

    camera.add(mesh);
    currentGunMesh = mesh;
}

function checkObjIdByName(name, objectsMap) {
    if (!name) return -1;
    const cleanName = name.toLowerCase().trim();
    for (const [id, obj] of objectsMap.entries()) {
        if (obj.name && obj.name.toLowerCase().trim() === cleanName) {
            return id;
        }
    }
    return -1;
}

function onWindowResize() {
    camera.aspect = window.innerWidth / window.innerHeight;
    camera.updateProjectionMatrix();
    renderer.setSize(window.innerWidth, window.innerHeight);
}

const forwardVector = new THREE.Vector3();
const rightVector = new THREE.Vector3();

function animate() {
    requestAnimationFrame(animate);

    const time = performance.now();
    let delta = (time - prevTime) / 1000;
    if (delta > 0.1) delta = 0.1;

    const moveSpeed = isSprinting ? 1200.0 : 650.0;

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
        camPos.x += velocity.x * delta;
        camPos.y += velocity.y * delta;
        camPos.z += velocity.z * delta;

        // Collide and slide against dungeon geometry and model boxes
        resolveCapsuleCollisions(camPos);
    }

    prevTime = time;

    const pos = controls.getObject().position;
    document.getElementById('pos-display').textContent = `${Math.round(pos.x)}, ${Math.round(pos.y)}, ${Math.round(pos.z)}`;

    // Update monsters AI, movement, and animations
    const playerPos = controls.getObject().position;
    for (let i = 0; i < monsters.length; i++) {
        monsters[i].update(delta, playerPos, monsters);
    }

    // Update active player weapon animation
    updatePlayerWeaponAnimation(delta);

    // Update moving doors animation and collision
    updateDoors(delta);

    // Distance culling for level objects
    const renderDistSq = RENDER_DISTANCE * RENDER_DISTANCE;
    for (let i = 0; i < levelObjects.length; i++) {
        const obj = levelObjects[i];
        if (obj.mesh) {
            const distSq = pos.distanceToSquared(obj.position);
            obj.mesh.visible = (distSq <= renderDistSq);
        }
    }

    renderer.render(scene, camera);
}

window.addEventListener('DOMContentLoaded', init);
