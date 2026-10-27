import * as THREE from 'three';
import { DDSLoader } from 'three/addons/loaders/DDSLoader.js';
import { TextureManager } from './parsers.js';

export let scene, camera, renderer, controls;

export function setControls(newControls) {
    controls = newControls;
}

export function initRenderer(container) {
    scene = new THREE.Scene();
    scene.background = new THREE.Color(0x1a1a24);

    camera = new THREE.PerspectiveCamera(75, window.innerWidth / window.innerHeight, 1, 3600);

    setupSceneLighting();

    renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'high-performance' });
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    renderer.setSize(window.innerWidth, window.innerHeight);
    container.appendChild(renderer.domElement);

    maxAnisotropy = Math.min(8, renderer.capabilities.getMaxAnisotropy());

    basePixelRatio = currentPixelRatio = renderer.getPixelRatio();

    return { scene, camera, renderer };
}

// Adaptive resolution: trade render scale for frame rate on slower GPUs, and
// cautiously win it back once the frame rate is comfortable again.
const MIN_PIXEL_RATIO = 0.6;
const PIXEL_RATIO_STEP = 0.85;
const SLOW_FRAME_MS = 22;
const FAST_FRAME_MS = 18;
const EVALUATE_EVERY_FRAMES = 45;
let basePixelRatio = 1;
let currentPixelRatio = 1;
let frameAverageMs = 16.7;
let framesSinceEvaluation = 0;
let lastRatioChange = 0;
let upgradeCooldownMs = 8000;

function applyPixelRatio(ratio) {
    currentPixelRatio = ratio;
    renderer.setPixelRatio(ratio);
    updateParticleScale();
    lastRatioChange = performance.now();
    frameAverageMs = (SLOW_FRAME_MS + FAST_FRAME_MS) / 2;
}

export function updateAdaptiveResolution(frameMs) {
    // Spikes from level loads or tab switches say nothing about GPU load.
    if (!renderer || frameMs > 80) return;

    frameAverageMs += (frameMs - frameAverageMs) * 0.05;
    if (++framesSinceEvaluation < EVALUATE_EVERY_FRAMES) return;
    framesSinceEvaluation = 0;

    if (frameAverageMs > SLOW_FRAME_MS && currentPixelRatio > MIN_PIXEL_RATIO) {
        applyPixelRatio(Math.max(MIN_PIXEL_RATIO, currentPixelRatio * PIXEL_RATIO_STEP));
        upgradeCooldownMs = Math.min(60000, upgradeCooldownMs * 2);
    } else if (frameAverageMs < FAST_FRAME_MS && currentPixelRatio < basePixelRatio &&
               performance.now() - lastRatioChange > upgradeCooldownMs) {
        applyPixelRatio(Math.min(basePixelRatio, currentPixelRatio / PIXEL_RATIO_STEP));
    }
}

// Distance Culling
export const RENDER_DISTANCE = 3000.0;

export function updateDistanceCulling(levelObjects, pos) {
    for (let i = 0; i < levelObjects.length; i++) {
        const obj = levelObjects[i];
        if (obj.mesh) {
            const reach = RENDER_DISTANCE + obj.radius;
            obj.mesh.visible = pos.distanceToSquared(obj.position) <= reach * reach;
        }
    }
}

// HUD elements cache
const hudElements = new Map();
export function hudEl(id) {
    let el = hudElements.get(id);
    if (!el) {
        el = document.getElementById(id);
        if (el) hudElements.set(id, el);
    }
    return el;
}

// Screen Flash Vignettes
export const FLASH_RED = 'radial-gradient(circle, rgba(255,0,0,0) 40%, rgba(255,0,0,0.8) 100%)';
export const FLASH_GREEN = 'radial-gradient(circle, rgba(34,197,94,0) 30%, rgba(34,197,94,0.5) 100%)';
export const FLASH_PICKUP_HEALTH = 'radial-gradient(circle, rgba(34,197,94,0) 40%, rgba(34,197,94,0.4) 100%)';
export const FLASH_PICKUP_XP = 'radial-gradient(circle, rgba(234,179,8,0) 40%, rgba(234,179,8,0.4) 100%)';
let flashBackground = FLASH_RED;
let flashTimeout = 0;

export function flashScreen(background, opacity, holdMs) {
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

// Texture & Material Management
export const textureManager = new TextureManager();
const ddsLoader = new DDSLoader();
const textureLoader = new THREE.TextureLoader();

export const textureCache = new Map();
export const normalMapCache = new Map();
export const normalMapMaterials = new Set();
export const sharedMaterials = new Map();
export let normalMapsEnabled = true;
export let maxAnisotropy = 1;

const defaultCanvas = document.createElement('canvas');
defaultCanvas.width = 128;
defaultCanvas.height = 128;
const ctx = defaultCanvas.getContext('2d');
ctx.fillStyle = '#666677';
ctx.fillRect(0, 0, 128, 128);
ctx.fillStyle = '#444455';
ctx.fillRect(0, 0, 64, 64);
ctx.fillRect(64, 64, 64, 64);

const fallbackTexture = new THREE.CanvasTexture(defaultCanvas);
fallbackTexture.wrapS = THREE.RepeatWrapping;
fallbackTexture.wrapT = THREE.RepeatWrapping;

export function getTexture(texAlias, onLoadCallback) {
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

export function loadTextureFromPaths(paths, onLoad, index = 0) {
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

export function getNormalMap(texAlias, onLoadCallback) {
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
        loadedTex.colorSpace = THREE.NoColorSpace;
        cacheEntry.loadedTex = loadedTex;
        cacheEntry.isLoading = false;
        for (const cb of cacheEntry.callbacks) cb(loadedTex);
        cacheEntry.callbacks = [];
    });
}

export function applyNormalMap(material, texAlias) {
    getNormalMap(texAlias, (normalTex) => {
        material.userData.normalMapTex = normalTex;
        normalMapMaterials.add(material);
        material.normalMap = normalMapsEnabled ? normalTex : null;
        material.normalScale.set(1, normalTex.flipY ? -1 : 1);
        material.needsUpdate = true;
    });
}

export function setNormalMapsEnabled(enabled) {
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

export function markSharedMesh(child) {
    if (!child.isMesh) return;
    child.geometry.userData.shared = true;
    const materials = Array.isArray(child.material) ? child.material : [child.material];
    for (const material of materials) {
        material.userData.shared = true;
        material.side = THREE.DoubleSide;
    }
}

export function disposeOwnedResources(root) {
    root.traverse((child) => {
        if (!child.isMesh) return;
        if (child.geometry && !child.geometry.userData.shared) child.geometry.dispose();
        const materials = Array.isArray(child.material) ? child.material : [child.material];
        for (const material of materials) {
            if (material && !material.userData.shared) material.dispose();
        }
    });
}

export function getSharedMaterial(texAlias) {
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

// Lighting System
const LIGHT_RANGE = 1000.0;
const MAX_POINT_LIGHTS = 8;
const MAX_SPOT_LIGHTS = 4;
const MAX_DYNAMIC_LIGHTS = 4;
const POINT_LIGHT_INTENSITY = 8500;
const SPOT_LIGHT_INTENSITY = 12000;
const LIGHT_DECAY = 1.5;
const LIGHT_FADE_SPEED = 4.0;
const LIGHT_REFRESH_INTERVAL = 0.1;
const FLICKER_COLOUR_REF = 14.0;
const SPOT_COLOUR_REF = 0.9;

export const lamps = [];
export const lightSlots = { flicker: [], spotlight: [] };
export const dynamicLights = [];
let lightRefreshTimer = LIGHT_REFRESH_INTERVAL;
export let haloTexture = null;
export let coreTexture = null;

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

export function setupSceneLighting() {
    const ambientLight = new THREE.AmbientLight(0x7a82a8, 0.9);
    scene.add(ambientLight);

    const dirLight = new THREE.DirectionalLight(0xaab6ff, 0.9);
    dirLight.position.set(2500, 6000, 1500);
    scene.add(dirLight);

    scene.fog = new THREE.Fog(0x08080e, 1200, 3400);
    scene.background = new THREE.Color(0x08080e);

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

    for (let i = 0; i < MAX_DYNAMIC_LIGHTS; i++) {
        const light = new THREE.PointLight(0xffffff, 0, 400, LIGHT_DECAY);
        scene.add(light);
        dynamicLights.push({ light, busy: false });
    }
}

export function acquireDynamicLight(colorHex, intensity, distance) {
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

export function releaseDynamicLight(slot) {
    if (!slot) return;
    slot.busy = false;
    slot.light.intensity = 0;
}

export function clearLamps() {
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

export function registerLamp(src) {
    const isSpot = src.type === 'spotlight';
    const ref = isSpot ? SPOT_COLOUR_REF : FLICKER_COLOUR_REF;
    const maxComp = Math.max(src.r, src.g, src.b);

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

export function updateLighting(t, delta, playerPos) {
    const rangeSq = LIGHT_RANGE * LIGHT_RANGE;
    for (const lamp of lamps) {
        const distSq = lamp.pos.distanceToSquared(playerPos);

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

// Particle System
const MAX_PARTICLES = 3072;
const PARTICLE_GRAVITY = 180.0;
export const particles = {
    positions: new Float32Array(MAX_PARTICLES * 3),
    colors: new Float32Array(MAX_PARTICLES * 3),
    sizes: new Float32Array(MAX_PARTICLES),
    velocities: new Float32Array(MAX_PARTICLES * 3),
    baseColors: new Float32Array(MAX_PARTICLES * 3),
    ages: new Float32Array(MAX_PARTICLES),
    lifetimes: new Float32Array(MAX_PARTICLES),
    next: 0,
    highWater: 0,
    alive: 0,
    points: null,
    material: null
};
const _particleColor = new THREE.Color();

export function ensureParticleSystem() {
    if (particles.points) return;

    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.BufferAttribute(particles.positions, 3).setUsage(THREE.DynamicDrawUsage));
    geometry.setAttribute('color', new THREE.BufferAttribute(particles.colors, 3).setUsage(THREE.DynamicDrawUsage));
    geometry.setAttribute('size', new THREE.BufferAttribute(particles.sizes, 1).setUsage(THREE.DynamicDrawUsage));
    geometry.setDrawRange(0, 0);

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
    particles.points.frustumCulled = false;
    scene.add(particles.points);
    updateParticleScale();
}

export function updateParticleScale() {
    if (particles.material && renderer) {
        particles.material.uniforms.scale.value = window.innerHeight * 0.5 * renderer.getPixelRatio();
    }
    if (blood.material && renderer) {
        blood.material.uniforms.scale.value = window.innerHeight * 0.5 * renderer.getPixelRatio();
    }
}

export function emitParticle(x, y, z, vx, vy, vz, colorHex, size, life) {
    ensureParticleSystem();

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

export function updateParticles(delta) {
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

// Blood System: alpha-blended droplets and mist (so it reads as blood rather than glow),
// plus pooled splat decals that stain the floor.
const MAX_BLOOD = 1792;
const BLOOD_GRAVITY = 620.0;
const BLOOD_PALETTE = [0x8b0000, 0xa80a0a, 0xc41212, 0x6a0808, 0x7d0d0d];
const MAX_BLOOD_DECALS = 56;
const BLOOD_DECAL_HOLD = 20.0;
const BLOOD_DECAL_FADE = 5.0;
const KIND_DROPLET = 0;
const KIND_MIST = 1;

const blood = {
    positions: new Float32Array(MAX_BLOOD * 3),
    velocities: new Float32Array(MAX_BLOOD * 3),
    colors: new Float32Array(MAX_BLOOD * 4),
    sizes: new Float32Array(MAX_BLOOD),
    softness: new Float32Array(MAX_BLOOD),
    baseSizes: new Float32Array(MAX_BLOOD),
    ages: new Float32Array(MAX_BLOOD),
    lifetimes: new Float32Array(MAX_BLOOD),
    gravity: new Float32Array(MAX_BLOOD),
    drag: new Float32Array(MAX_BLOOD),
    growth: new Float32Array(MAX_BLOOD),
    floorY: new Float32Array(MAX_BLOOD),
    trail: new Uint8Array(MAX_BLOOD),
    next: 0,
    highWater: 0,
    alive: 0,
    points: null,
    material: null,
    decals: [],
    decalNext: 0,
    decalTextures: [],
    decalGeometry: null
};
const _bloodColor = new THREE.Color();

function ensureBloodSystem() {
    if (blood.points) return;

    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.BufferAttribute(blood.positions, 3).setUsage(THREE.DynamicDrawUsage));
    geometry.setAttribute('aColor', new THREE.BufferAttribute(blood.colors, 4).setUsage(THREE.DynamicDrawUsage));
    geometry.setAttribute('size', new THREE.BufferAttribute(blood.sizes, 1).setUsage(THREE.DynamicDrawUsage));
    geometry.setAttribute('aSoft', new THREE.BufferAttribute(blood.softness, 1).setUsage(THREE.DynamicDrawUsage));
    geometry.setDrawRange(0, 0);

    blood.material = new THREE.ShaderMaterial({
        uniforms: { scale: { value: window.innerHeight * 0.5 } },
        vertexShader: `
            attribute float size;
            attribute vec4 aColor;
            attribute float aSoft;
            uniform float scale;
            varying vec4 vColor;
            varying float vSoft;
            void main() {
                vColor = aColor;
                vSoft = aSoft;
                vec4 mvPosition = modelViewMatrix * vec4(position, 1.0);
                gl_PointSize = clamp(size * (scale / -mvPosition.z), 1.0, 220.0);
                gl_Position = projectionMatrix * mvPosition;
            }`,
        fragmentShader: `
            varying vec4 vColor;
            varying float vSoft;
            void main() {
                vec2 p = gl_PointCoord * 2.0 - 1.0;
                float d = length(p);
                if (d > 1.0) discard;
                float crisp = smoothstep(1.0, 0.8, d);
                float puff = pow(1.0 - d, 1.6);
                float shape = mix(crisp, puff, vSoft);
                vec3 rgb = vColor.rgb * mix(mix(0.55, 1.0, smoothstep(1.0, 0.45, d)), 1.0, vSoft);
                float highlight = smoothstep(0.4, 0.0, length(p - vec2(-0.32, -0.32)));
                rgb += vec3(0.75, 0.35, 0.3) * highlight * (1.0 - vSoft) * vColor.a;
                gl_FragColor = vec4(rgb, vColor.a * shape);
            }`,
        transparent: true,
        depthWrite: false
    });

    blood.points = new THREE.Points(geometry, blood.material);
    blood.points.frustumCulled = false;
    blood.points.renderOrder = 2;
    scene.add(blood.points);
    updateParticleScale();
}

function createBloodDecalTexture(seed) {
    const size = 128;
    const canvas = document.createElement('canvas');
    canvas.width = canvas.height = size;
    const ctx = canvas.getContext('2d');
    const half = size / 2;
    let s = seed * 9301 + 49297;
    const rand = () => { s = (s * 9301 + 49297) % 233280; return s / 233280; };

    const blob = (x, y, rx, ry, rot, alpha) => {
        ctx.save();
        ctx.translate(x, y);
        ctx.rotate(rot);
        ctx.scale(1, ry / rx);
        const g = ctx.createRadialGradient(0, 0, 0, 0, 0, rx);
        g.addColorStop(0, `rgba(96,6,6,${alpha})`);
        g.addColorStop(0.7, `rgba(74,4,4,${alpha * 0.9})`);
        g.addColorStop(1, 'rgba(60,2,2,0)');
        ctx.fillStyle = g;
        ctx.beginPath();
        ctx.arc(0, 0, rx, 0, Math.PI * 2);
        ctx.fill();
        ctx.restore();
    };

    blob(half, half, 20 + rand() * 8, 16 + rand() * 8, rand() * Math.PI, 0.95);
    for (let i = 0; i < 5; i++) {
        const a = rand() * Math.PI * 2;
        const r = 10 + rand() * 12;
        blob(half + Math.cos(a) * r, half + Math.sin(a) * r, 8 + rand() * 8, 6 + rand() * 6, a, 0.9);
    }
    // Radial streaks and satellite droplets thrown outward from the impact.
    for (let i = 0; i < 16; i++) {
        const a = rand() * Math.PI * 2;
        const r = 26 + rand() * 32;
        const rad = 2 + rand() * (r > 48 ? 2.5 : 5);
        blob(half + Math.cos(a) * r, half + Math.sin(a) * r, rad * 2.2, rad, a, 0.95);
    }
    for (let i = 0; i < 5; i++) {
        const a = rand() * Math.PI * 2;
        const len = 18 + rand() * 20;
        for (let k = 0; k < 4; k++) {
            const r = 14 + k * (len / 4);
            const rad = 5 - k;
            blob(half + Math.cos(a) * r, half + Math.sin(a) * r, rad * 2, rad, a, 0.9);
        }
    }

    const texture = new THREE.CanvasTexture(canvas);
    texture.colorSpace = THREE.SRGBColorSpace;
    return texture;
}

function ensureBloodDecals() {
    if (blood.decals.length) return;
    blood.decalGeometry = new THREE.PlaneGeometry(1, 1);
    blood.decalGeometry.rotateX(-Math.PI / 2);
    for (let i = 0; i < 5; i++) blood.decalTextures.push(createBloodDecalTexture(i + 1));
    for (let i = 0; i < MAX_BLOOD_DECALS; i++) {
        const material = new THREE.MeshBasicMaterial({
            map: blood.decalTextures[i % blood.decalTextures.length],
            transparent: true,
            depthWrite: false,
            polygonOffset: true,
            polygonOffsetFactor: -4,
            polygonOffsetUnits: -4
        });
        const mesh = new THREE.Mesh(blood.decalGeometry, material);
        mesh.visible = false;
        mesh.frustumCulled = true;
        mesh.renderOrder = 1;
        scene.add(mesh);
        blood.decals.push({ mesh, material, active: false, age: 0, targetSize: 1, growTime: 0.15 });
    }
}

function spawnBloodDecal(x, floorY, z, size, growTime = 0.15, shade = 1.0) {
    ensureBloodDecals();
    const decal = blood.decals[blood.decalNext];
    blood.decalNext = (blood.decalNext + 1) % blood.decals.length;
    decal.active = true;
    decal.age = 0;
    decal.targetSize = size;
    decal.growTime = growTime;
    decal.mesh.visible = true;
    decal.mesh.position.set(x, floorY + 0.8, z);
    decal.mesh.rotation.y = Math.random() * Math.PI * 2;
    decal.mesh.scale.setScalar(size * 0.3);
    decal.material.map = blood.decalTextures[Math.floor(Math.random() * blood.decalTextures.length)];
    decal.material.color.setScalar(shade);
    decal.material.opacity = 1;
    decal.material.needsUpdate = true;
}

function emitBlood(x, y, z, vx, vy, vz, r, g, b, a, size, life, gravity, drag, growth, soft, floorY, trail) {
    ensureBloodSystem();

    let slot = -1;
    for (let n = 0; n < MAX_BLOOD; n++) {
        const candidate = (blood.next + n) % MAX_BLOOD;
        if (blood.lifetimes[candidate] <= 0) {
            slot = candidate;
            break;
        }
    }
    if (slot === -1) {
        slot = blood.next;
        blood.alive--;
    }
    blood.next = (slot + 1) % MAX_BLOOD;

    const i3 = slot * 3;
    const i4 = slot * 4;
    blood.positions[i3] = x;
    blood.positions[i3 + 1] = y;
    blood.positions[i3 + 2] = z;
    blood.velocities[i3] = vx;
    blood.velocities[i3 + 1] = vy;
    blood.velocities[i3 + 2] = vz;
    blood.colors[i4] = r;
    blood.colors[i4 + 1] = g;
    blood.colors[i4 + 2] = b;
    blood.colors[i4 + 3] = a;
    blood.sizes[slot] = blood.baseSizes[slot] = size;
    blood.softness[slot] = soft;
    blood.ages[slot] = 0;
    blood.lifetimes[slot] = life;
    blood.gravity[slot] = gravity;
    blood.drag[slot] = drag;
    blood.growth[slot] = growth;
    blood.floorY[slot] = floorY;
    blood.trail[slot] = trail ? 1 : 0;

    blood.alive++;
    if (slot >= blood.highWater) blood.highWater = slot + 1;
}

/**
 * Sprays blood from a point. `dir` is the direction the blow travels (attacker -> victim).
 * `severity` in [0,1] scales the amount; `killed` adds a heavy gush and a pooling puddle.
 */
export function createBloodSplatter(pos, dir, severity = 0.5, floorY = pos.y - 40, baseColorHex = 0x9a0a0a, killed = false) {
    ensureBloodSystem();
    const sev = Math.min(1, Math.max(0.15, severity));
    const boost = killed ? 2.0 : 1.0;
    _bloodColor.setHex(baseColorHex);
    const br = _bloodColor.r, bg = _bloodColor.g, bb = _bloodColor.b;

    const dx = dir.x, dz = dir.z;
    const flat = Math.hypot(dx, dz) || 1;
    const fx = dx / flat, fz = dz / flat;

    // Fine spray droplets
    const dropletCount = Math.round((18 + 46 * sev) * boost);
    for (let i = 0; i < dropletCount; i++) {
        const back = Math.random() < 0.22;
        const spread = 0.35 + Math.random() * 0.75;
        const speed = (110 + Math.random() * 260) * (0.7 + sev * 0.5) * (back ? 0.6 : 1);
        const sign = back ? -1 : 1;
        const vx = (fx * sign + (Math.random() - 0.5) * 2 * spread) * speed;
        const vz = (fz * sign + (Math.random() - 0.5) * 2 * spread) * speed;
        const vy = (Math.random() * 0.9 - 0.15) * speed * 0.9 + 40;
        const shade = 0.45 + Math.random() * 0.8;
        const big = Math.random() < 0.18;
        const size = big ? 2.6 + Math.random() * 2.4 : 1.1 + Math.random() * 2.0;
        emitBlood(
            pos.x + (Math.random() - 0.5) * 6, pos.y + (Math.random() - 0.5) * 8, pos.z + (Math.random() - 0.5) * 6,
            vx, vy, vz,
            Math.min(1, br * shade), Math.min(1, bg * shade), Math.min(1, bb * shade), 1.0,
            size, 1.6 + Math.random() * 0.8, BLOOD_GRAVITY, 0.25, 0, KIND_DROPLET, floorY, big && Math.random() < 0.6);
    }

    // Thick wet mist that billows and settles
    const mistCount = Math.round((7 + 9 * sev) * boost);
    for (let i = 0; i < mistCount; i++) {
        const speed = 40 + Math.random() * 150;
        const shade = 0.5 + Math.random() * 0.5;
        emitBlood(
            pos.x + (Math.random() - 0.5) * 8, pos.y + (Math.random() - 0.5) * 10, pos.z + (Math.random() - 0.5) * 8,
            (fx * 0.8 + (Math.random() - 0.5) * 1.4) * speed, (Math.random() - 0.1) * speed * 0.6, (fz * 0.8 + (Math.random() - 0.5) * 1.4) * speed,
            br * shade, bg * shade, bb * shade, 0.55 + Math.random() * 0.25,
            9 + Math.random() * 12 + sev * 6, 0.45 + Math.random() * 0.35, 40, 3.2, 1.4, KIND_MIST, floorY, false);
    }

    // Bright arterial fan on heavy hits and kills
    if (sev > 0.45 || killed) {
        const jets = killed ? 7 : 3;
        for (let j = 0; j < jets; j++) {
            const ang = Math.atan2(fz, fx) + (Math.random() - 0.5) * 2.2;
            const jx = Math.cos(ang), jz = Math.sin(ang);
            const jetSpeed = 260 + Math.random() * 220;
            const jetUp = 120 + Math.random() * 200;
            for (let k = 0; k < 7; k++) {
                const sp = jetSpeed * (0.8 + Math.random() * 0.4);
                emitBlood(
                    pos.x, pos.y, pos.z,
                    jx * sp, jetUp * (0.8 + Math.random() * 0.4), jz * sp,
                    Math.min(1, br * 1.25), bg * 0.9, bb * 0.9, 1.0,
                    2.2 + Math.random() * 1.8, 1.4 + Math.random() * 0.5, BLOOD_GRAVITY, 0.1, 0, KIND_DROPLET, floorY, true);
            }
        }
    }

    // Immediate stain at the victim's feet so each hit leaves a mark even before droplets land
    spawnBloodDecal(pos.x + fx * 14, floorY, pos.z + fz * 14, 26 + 34 * sev * boost, 0.2);
    if (killed) {
        spawnBloodDecal(pos.x, floorY, pos.z, 85 + Math.random() * 30, 5.0, 0.9);
        spawnBloodDecal(pos.x + fx * 30, floorY, pos.z + fz * 30, 55, 3.5, 0.95);
    }
}

function updateBloodDecals(delta) {
    for (let i = 0; i < blood.decals.length; i++) {
        const decal = blood.decals[i];
        if (!decal.active) continue;
        decal.age += delta;

        const grow = Math.min(1, decal.age / decal.growTime);
        const eased = 1 - Math.pow(1 - grow, 3);
        decal.mesh.scale.setScalar(decal.targetSize * (0.3 + 0.7 * eased));

        const fadeStart = BLOOD_DECAL_HOLD;
        if (decal.age > fadeStart) {
            const f = (decal.age - fadeStart) / BLOOD_DECAL_FADE;
            if (f >= 1) {
                decal.active = false;
                decal.mesh.visible = false;
                continue;
            }
            decal.material.opacity = 1 - f;
        }
    }
}

export function updateBlood(delta) {
    updateBloodDecals(delta);
    if (!blood.points) return;
    const geometry = blood.points.geometry;

    if (blood.alive <= 0) {
        if (blood.highWater !== 0) {
            blood.highWater = 0;
            geometry.setDrawRange(0, 0);
        }
        return;
    }

    for (let i = 0; i < blood.highWater; i++) {
        const life = blood.lifetimes[i];
        if (life <= 0) continue;

        const age = blood.ages[i] + delta;
        const i3 = i * 3;
        const i4 = i * 4;
        if (age >= life) {
            blood.lifetimes[i] = 0;
            blood.sizes[i] = 0;
            blood.colors[i4 + 3] = 0;
            blood.alive--;
            continue;
        }
        blood.ages[i] = age;
        const t = age / life;
        const isMist = blood.softness[i] > 0.5;

        const damping = Math.max(0, 1 - blood.drag[i] * delta);
        blood.velocities[i3] *= damping;
        blood.velocities[i3 + 2] *= damping;
        blood.velocities[i3 + 1] = blood.velocities[i3 + 1] * damping - blood.gravity[i] * delta;

        const px = blood.positions[i3] += blood.velocities[i3] * delta;
        const py = blood.positions[i3 + 1] += blood.velocities[i3 + 1] * delta;
        const pz = blood.positions[i3 + 2] += blood.velocities[i3 + 2] * delta;

        if (isMist) {
            blood.sizes[i] = blood.baseSizes[i] * (1 + blood.growth[i] * t);
            blood.colors[i4 + 3] *= 1 - Math.min(1, delta * 1.6 / (1 - t + 0.2));
        } else {
            if (py <= blood.floorY[i]) {
                const baseSize = blood.baseSizes[i];
                if (baseSize > 2.0 || Math.random() < 0.25) {
                    spawnBloodDecal(px, blood.floorY[i], pz, 7 + baseSize * 5 + Math.random() * 8, 0.1);
                }
                blood.lifetimes[i] = 0;
                blood.sizes[i] = 0;
                blood.colors[i4 + 3] = 0;
                blood.alive--;
                continue;
            }
            // Fast heavy droplets shed a short streak of fine droplets behind them
            if (blood.trail[i] && t < 0.4 && Math.random() < 0.55) {
                emitBlood(
                    px, py, pz,
                    blood.velocities[i3] * 0.15, blood.velocities[i3 + 1] * 0.15, blood.velocities[i3 + 2] * 0.15,
                    blood.colors[i4], blood.colors[i4 + 1], blood.colors[i4 + 2], 0.9,
                    blood.baseSizes[i] * 0.45, 0.35, BLOOD_GRAVITY * 0.4, 1.5, 0, KIND_DROPLET, blood.floorY[i], false);
            }
            if (t > 0.85) blood.colors[i4 + 3] = (1 - t) / 0.15;
        }
    }

    geometry.setDrawRange(0, blood.highWater);
    geometry.attributes.position.needsUpdate = true;
    geometry.attributes.aColor.needsUpdate = true;
    geometry.attributes.size.needsUpdate = true;
    geometry.attributes.aSoft.needsUpdate = true;
}

export function clearBlood() {
    blood.lifetimes.fill(0);
    blood.sizes.fill(0);
    blood.alive = 0;
    blood.highWater = 0;
    if (blood.points) {
        blood.points.geometry.setDrawRange(0, 0);
        blood.points.geometry.attributes.size.needsUpdate = true;
    }
    for (const decal of blood.decals) {
        decal.active = false;
        decal.mesh.visible = false;
    }
}

// 3D Floating Text
export const activeFloatingTexts = [];
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

export function createPickupFloatingText(text, pos, color = '#38bdf8') {
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

export function createTreasureDropEffect(pos) {
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

export function createPickupParticleBurst(pos, colorHex = 0x38bdf8) {
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

export function updatePickupEffects(delta) {
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
    updateBlood(delta);
}

// Explosions Fading Lights
export const EXPLOSION_FLASH_INTENSITY = 15000;
export const EXPLOSION_FLASH_DURATION = 0.3;
export const explosionFlashes = [];

export function updateExplosionFlashes(delta) {
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

export function onWindowResize() {
    if (!camera || !renderer) return;
    camera.aspect = window.innerWidth / window.innerHeight;
    camera.updateProjectionMatrix();
    renderer.setSize(window.innerWidth, window.innerHeight);
    updateParticleScale();
}
