import * as THREE from 'three';
import { PointerLockControls } from 'three/addons/controls/PointerLockControls.js';
import { DDSLoader } from 'three/addons/loaders/DDSLoader.js';
import { TextureManager, ObjectsDatParser, LevelMapParser, ModelListParser } from './parsers.js';
import { TDSLoader } from 'three/addons/loaders/TDSLoader.js';
import { MD2Loader } from 'three/addons/loaders/MD2Loader.js';

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

    window.addEventListener('resize', onWindowResize);

    // Load Game Data & Build Map
    await loadGameWorld();

    // Initialize Active Weapon
    await updateWeaponMesh();

    window.camera = camera;
    window.controls = controls;
    window.scene = scene;

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
    const parts = filepath.split('/');
    if (parts.length < 4) return [filepath];

    const dirs0 = [parts[0], 'Models', 'models'];
    const dirs1 = [parts[1], 'Players', 'players'];

    const folder = parts[2];
    const capFolder = folder.charAt(0).toUpperCase() + folder.slice(1).toLowerCase();
    const dirs2 = [folder, folder.toLowerCase(), capFolder, folder.toUpperCase()];

    const file = parts[3];
    const dotIdx = file.lastIndexOf('.');
    const base = dotIdx !== -1 ? file.substring(0, dotIdx) : file;
    const ext = dotIdx !== -1 ? file.substring(dotIdx) : '';

    const capBase = base.charAt(0).toUpperCase() + base.slice(1).toLowerCase();
    const baseVars = [base, base.toLowerCase(), capBase, base.toUpperCase()];
    const extVars = [ext, ext.toLowerCase(), ext.toUpperCase(), '.md2', '.MD2', '.3ds', '.3DS'];

    const fileVars = [];
    for (const b of baseVars) {
        for (const e of extVars) {
            fileVars.push(b + e);
        }
    }

    const set = new Set();
    set.add(filepath);
    for (const d0 of dirs0) {
        for (const d1 of dirs1) {
            for (const d2 of dirs2) {
                for (const f of fileVars) {
                    set.add(`${d0}/${d1}/${d2}/${f}`);
                }
            }
        }
    }
    return Array.from(set);
}

function computeMD2FrameBoundingBox(mesh, frameIndex = 0) {
    const geometry = mesh.geometry;
    if (!geometry) return new THREE.Box3().setFromObject(mesh);

    mesh.updateMatrixWorld(true);

    const morphPositions = geometry.morphAttributes && geometry.morphAttributes.position;
    let posAttr = (morphPositions && morphPositions.length > frameIndex)
        ? morphPositions[frameIndex]
        : geometry.attributes.position;

    if (!posAttr) return new THREE.Box3().setFromObject(mesh);

    const box = new THREE.Box3();
    const v = new THREE.Vector3();
    const array = posAttr.array;

    for (let i = 0; i < array.length; i += 3) {
        v.set(array[i], array[i + 1], array[i + 2]);
        v.applyMatrix4(mesh.matrixWorld);
        box.expandByPoint(v);
    }

    return box;
}

async function loadMD2Geometry(modelDef) {
    if (!modelDef || !modelDef.filepath) return null;
    const key = modelDef.filepath.toLowerCase();
    if (md2GeometryCache.has(key)) {
        return md2GeometryCache.get(key);
    }

    const uniquePaths = generatePathVariations(modelDef.filepath);

    for (const path of uniquePaths) {
        try {
            const geometry = await new Promise((resolve, reject) => {
                md2Loader.load(path, resolve, undefined, reject);
            });

            if (geometry) {
                console.log(`Successfully loaded MD2 at ${path}`);
                md2GeometryCache.set(key, geometry);
                return geometry;
            }
        } catch (e) {
            // Try next variant
        }
    }

    console.warn(`Could not load MD2 model at ${modelDef.filepath}`);
    md2GeometryCache.set(key, null);
    return null;
}

async function load3DSModel(modelDef) {
    if (!modelDef || !modelDef.filepath) return null;
    const key = modelDef.filepath.toLowerCase();
    if (tdsModelCache.has(key)) {
        return tdsModelCache.get(key);
    }

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
                // 3DS is Z-up: tilt -90deg about X to Y-up. Z rotation is applied first, so it
                // spins the model about its own up axis. The wrapper keeps the root free for
                // per-instance scale/position/yaw.
                object3ds.rotation.set(-Math.PI / 2, 0, Math.PI / 2);
                const wrapper = new THREE.Group();
                wrapper.add(object3ds);
                tdsModelCache.set(key, wrapper);
                return wrapper;
            }
        } catch (e) {
            // Try next variant
        }
    }

    console.warn(`Could not load 3DS model at ${modelDef.filepath}`);
    tdsModelCache.set(key, null);
    return null;
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

    const texturePolysMap = new Map();

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
                        builtObjectCount++;

                        // Add bounding box for 3DS model collision
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
                        mesh.rotation.y = angleRad;

                        scene.add(mesh);
                        builtObjectCount++;

                        // Draw monster weapon if present
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
                                        const weaponMesh = new THREE.Mesh(weaponGeom, weaponMat);
                                        weaponMesh.scale.set(weaponScale, weaponScale, weaponScale);
                                        weaponMesh.position.set(inst.x, inst.y + 50, inst.z);
                                        weaponMesh.rotation.y = angleRad;
                                        scene.add(weaponMesh);
                                    }
                                } else if (weaponDef.format === '3DS' || weaponDef.filepath.toLowerCase().endsWith('.3ds')) {
                                    const weaponTemplate = await load3DSModel(weaponDef);
                                    if (weaponTemplate) {
                                        const weaponModel = weaponTemplate.clone(true);
                                        const weaponScale = weaponDef.scale || scaleFactor;
                                        weaponModel.scale.set(weaponScale, weaponScale, weaponScale);
                                        weaponModel.position.set(inst.x, inst.y + 50, inst.z);
                                        weaponModel.rotation.y = angleRad;
                                        weaponModel.traverse((child) => {
                                            if (child.isMesh && child.material) {
                                                child.material.side = THREE.DoubleSide;
                                            }
                                        });
                                        scene.add(weaponModel);
                                    }
                                }
                            }
                        }

                        // Add bounding box for MD2 model collision based on current/first frame geometry
                        const box = computeMD2FrameBoundingBox(mesh, 0);
                        if (!box.isEmpty()) {
                            modelBoxes.push(box);
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

        const angleRad = (inst.rot * Math.PI) / 180.0;
        const cosA = Math.cos(angleRad);
        const sinA = Math.sin(angleRad);

        for (const poly of objDef.polys) {
            const texAlias = poly.texture || 'wall1';
            if (!texturePolysMap.has(texAlias)) {
                texturePolysMap.set(texAlias, { positions: [], uvs: [] });
            }

            const group = texturePolysMap.get(texAlias);

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
                    levelTriangles.push({
                        a, b, c,
                        minX: Math.min(a.x, b.x, c.x),
                        maxX: Math.max(a.x, b.x, c.x),
                        minY: Math.min(a.y, b.y, c.y),
                        maxY: Math.max(a.y, b.y, c.y),
                        minZ: Math.min(a.z, b.z, c.z),
                        maxZ: Math.max(a.z, b.z, c.z)
                    });
                }
            }
        }
    }

    for (const [texAlias, data] of texturePolysMap.entries()) {
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
        scene.add(mesh);
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

const _capA = new THREE.Vector3();
const _capB = new THREE.Vector3();
const _normal = new THREE.Vector3();
const _faceNormal = new THREE.Vector3();
const _edge1 = new THREE.Vector3();
const _edge2 = new THREE.Vector3();

function resolveCapsuleCollisions(camPos) {
    isGrounded = false;
    const basePosY = camPos.y - EYE_HEIGHT;

    const queryMinX = camPos.x - CAPSULE_RADIUS - 10;
    const queryMaxX = camPos.x + CAPSULE_RADIUS + 10;
    const queryMinY = basePosY - 10;
    const queryMaxY = basePosY + CAPSULE_HEIGHT + 10;
    const queryMinZ = camPos.z - CAPSULE_RADIUS - 10;
    const queryMaxZ = camPos.z + CAPSULE_RADIUS + 10;

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
                if (_normal.lengthSq() > 1e-8) {
                    _normal.normalize();
                } else {
                    _edge1.subVectors(tri.b, tri.a);
                    _edge2.subVectors(tri.c, tri.a);
                    _normal.crossVectors(_edge1, _edge2).normalize();
                }

                if (pen > maxPenetration) {
                    maxPenetration = pen;
                    bestNormal = _normal.clone();
                }
            }
        }

        for (let b = 0; b < modelBoxes.length; b++) {
            const box = modelBoxes[b];
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
                    _normal.set(0, 1, 0);
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
    mesh.position.set(12, -12, -25);
    mesh.rotation.set(0, 0, 0);

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

    renderer.render(scene, camera);
}

window.addEventListener('DOMContentLoaded', init);
