import * as THREE from 'three';
import { PointerLockControls } from 'three/addons/controls/PointerLockControls.js';
import { DDSLoader } from 'three/addons/loaders/DDSLoader.js';
import { TextureManager, ObjectsDatParser, LevelMapParser, ModelListParser } from './parsers.js';
import { TDSLoader } from 'three/addons/loaders/TDSLoader.js';

let camera, scene, renderer, controls;
let moveForward = false;
let moveBackward = false;
let moveLeft = false;
let moveRight = false;
let isSprinting = false;

let prevTime = performance.now();
const velocity = new THREE.Vector3();
const direction = new THREE.Vector3();

const textureManager = new TextureManager();
const objectsParser = new ObjectsDatParser();
const levelParser = new LevelMapParser();
const modelListParser = new ModelListParser();

const ddsLoader = new DDSLoader();
const tdsLoader = new TDSLoader();
const textureLoader = new THREE.TextureLoader();

const textureCache = new Map();
const tdsModelCache = new Map();

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
            case 'ShiftLeft':
            case 'ShiftRight':
                isSprinting = true;
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
                tdsModelCache.set(key, object3ds);
                return object3ds;
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

        // Check if item instance is a 3DS model (e.g. textureAlias === '-1' or name starts with '!')
        const is3DSItem = inst.textureAlias === '-1' || (inst.name && inst.name.startsWith('!') && inst.textureAlias === '-1');

        if (is3DSItem && inst.modelName) {
            const modelDef = modelListParser.getModelByName(inst.modelName);
            if (modelDef && (modelDef.format === '3DS' || modelDef.filepath.toLowerCase().endsWith('.3ds'))) {
                const template = await load3DSModel(modelDef);
                if (template) {
                    const model = template.clone(true);
                    const scaleFactor = modelDef.scale || 1.0;
                    model.scale.set(scaleFactor, scaleFactor, scaleFactor);

                    model.position.set(inst.x, inst.y, inst.z);

                    const angleRad = (inst.rot * Math.PI) / 180.0;
                    model.rotation.y = angleRad;

                    model.traverse((child) => {
                        if (child.isMesh && child.material) {
                            child.material.side = THREE.DoubleSide;
                        }
                    });

                    scene.add(model);
                    builtObjectCount++;
                    continue;
                }
            }
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

            for (const v of poly.verts) {
                const rx = v.x * cosA - v.z * sinA;
                const rz = v.x * sinA + v.z * cosA;

                const wx = rx + inst.x;
                const wy = v.y + inst.y;
                const wz = rz + inst.z;

                group.positions.push(wx, wy, wz);
                group.uvs.push(v.u, v.v);
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

    document.getElementById('obj-display').textContent = builtObjectCount;
    document.getElementById('light-display').textContent = lightCount;
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

function animate() {
    requestAnimationFrame(animate);

    const time = performance.now();
    let delta = (time - prevTime) / 1000;
    if (delta > 0.1) delta = 0.1;

    velocity.x -= velocity.x * 10.0 * delta;
    velocity.z -= velocity.z * 10.0 * delta;

    direction.z = Number(moveForward) - Number(moveBackward);
    direction.x = Number(moveRight) - Number(moveLeft);
    direction.normalize();

    const speed = isSprinting ? 1200.0 : 600.0;

    if (moveForward || moveBackward) velocity.z -= direction.z * speed * delta;
    if (moveLeft || moveRight) velocity.x -= direction.x * speed * delta;

    if (controls.isLocked) {
        controls.moveRight(-velocity.x * delta);
        controls.moveForward(-velocity.z * delta);
    } else if (moveForward || moveBackward || moveLeft || moveRight) {
        const obj = controls.getObject();
        obj.translateX(-velocity.x * delta);
        obj.translateZ(-velocity.z * delta);
    }

    prevTime = time;

    const pos = controls.getObject().position;
    document.getElementById('pos-display').textContent = `${Math.round(pos.x)}, ${Math.round(pos.y)}, ${Math.round(pos.z)}`;

    renderer.render(scene, camera);
}

window.addEventListener('DOMContentLoaded', init);
