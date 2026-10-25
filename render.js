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

    return { scene, camera, renderer };
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
