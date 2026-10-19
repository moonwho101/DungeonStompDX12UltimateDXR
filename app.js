import * as THREE from 'three';
import { PointerLockControls } from 'three/addons/controls/PointerLockControls.js';
import { TextureManager, ObjectsDatParser, LevelMapParser } from './parsers.js';

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

const textureCache = new Map();

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

    prevTime = performance.now();
    animate();
}

function getTexture(texAlias) {
    if (textureCache.has(texAlias)) {
        return textureCache.get(texAlias);
    }

    const texPath = textureManager.getTexturePath(texAlias);

    const loader = new THREE.TextureLoader();

    // Default canvas texture
    const canvas = document.createElement('canvas');
    canvas.width = 128;
    canvas.height = 128;
    const ctx = canvas.getContext('2d');
    ctx.fillStyle = '#666677';
    ctx.fillRect(0, 0, 128, 128);
    ctx.fillStyle = '#444455';
    ctx.fillRect(0, 0, 64, 64);
    ctx.fillRect(64, 64, 64, 64);

    const fallbackTex = new THREE.CanvasTexture(canvas);
    fallbackTex.wrapS = THREE.RepeatWrapping;
    fallbackTex.wrapT = THREE.RepeatWrapping;

    let possiblePaths = [
        `Textures/${texPath}`,
        `Textures/${texPath.replace(/\.dds$/i, '.png')}`,
        `Textures/${texPath.replace(/\.dds$/i, '.jpg')}`,
        `Textures/${texPath.replace(/\.dds$/i, '.bmp')}`,
        `Textures/${texAlias}.png`,
        `Textures/${texAlias}.jpg`,
        `Textures/${texAlias}.bmp`
    ];

    let texture = fallbackTex;

    function tryLoad(index) {
        if (index >= possiblePaths.length) return;
        loader.load(
            possiblePaths[index],
            (loadedTex) => {
                loadedTex.wrapS = THREE.RepeatWrapping;
                loadedTex.wrapT = THREE.RepeatWrapping;
                texture.image = loadedTex.image;
                texture.needsUpdate = true;
            },
            undefined,
            () => {
                tryLoad(index + 1);
            }
        );
    }

    tryLoad(0);

    textureCache.set(texAlias, texture);
    return texture;
}

async function loadGameWorld() {
    console.log('Loading textures.dat...');
    await textureManager.loadTexturesDat('bin/textures.dat');

    console.log('Parsing Objects.dat...');
    const objectsMap = await objectsParser.parse('bin/Objects.dat');

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

        const objId = checkObjIdByName(inst.name, objectsMap);
        const objDef = objectsMap.get(objId);

        if (inst.isLight) {
            lightCount++;
            const pointLight = new THREE.PointLight(0xffd29d, 3.0, 1500);
            pointLight.position.set(inst.x, inst.y + 100, inst.z);
            scene.add(pointLight);
            continue;
        }

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

        const texture = getTexture(texAlias);
        const material = new THREE.MeshLambertMaterial({
            map: texture,
            side: THREE.DoubleSide
        });

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
