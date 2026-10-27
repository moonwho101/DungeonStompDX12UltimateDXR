import { levelTriangles, levelRevision } from './collision.js';
import { monsters, doors, MonsterState } from './gameState.js';
import { activePickups, activeTeleporters, chests } from './logic.js';

let canvas = null;
let ctx = null;
let canvasSize = 0;
let lastDrawTime = 0;

const VIEW_RADIUS = 1600.0; // World distance visible on minimap radius
const VIEW_RADIUS_SQ = (VIEW_RADIUS * 1.2) * (VIEW_RADIUS * 1.2);
const REDRAW_INTERVAL_MS = 1000 / 30;

// The static level never changes between loads, so it is rasterised once into
// square tiles and then blitted. Per-frame cost no longer scales with triangle count.
const TILE_SIZE = 1000.0;
const TILE_PX = 200;
const TILE_SCALE = TILE_PX / TILE_SIZE;
const MAX_TILES = 96;
const MAX_BAKES_PER_FRAME = 4;
const FLOOR_COLOR = '#1f3050';
const WALL_COLOR = '#9fb3cf';

const buckets = new Map();
const tiles = new Map();
let bakedRevision = -1;
let frameCounter = 0;

function tileKey(tx, tz) {
    return (tx + 1024) * 4096 + (tz + 1024);
}

function bucketFor(tx, tz) {
    const key = tileKey(tx, tz);
    let bucket = buckets.get(key);
    if (!bucket) {
        bucket = { tx, tz, floors: [], walls: [] };
        buckets.set(key, bucket);
    }
    return bucket;
}

function rebuildBuckets() {
    buckets.clear();
    tiles.clear();
    bakedRevision = levelRevision;

    for (let i = 0; i < levelTriangles.length; i++) {
        const tri = levelTriangles[i];
        if (!tri.planar) continue;
        const ny = tri.n.y;
        const isFloor = ny > 0.6;
        if (!isFloor && Math.abs(ny) > 0.35) continue; // ceilings and slanted decor would only add clutter

        const minTx = Math.floor(tri.minX / TILE_SIZE);
        const maxTx = Math.floor(tri.maxX / TILE_SIZE);
        const minTz = Math.floor(tri.minZ / TILE_SIZE);
        const maxTz = Math.floor(tri.maxZ / TILE_SIZE);
        for (let tx = minTx; tx <= maxTx; tx++) {
            for (let tz = minTz; tz <= maxTz; tz++) {
                const bucket = bucketFor(tx, tz);
                (isFloor ? bucket.floors : bucket.walls).push(tri);
            }
        }
    }
}

function bakeTile(bucket) {
    const tile = document.createElement('canvas');
    tile.width = TILE_PX;
    tile.height = TILE_PX;
    const g = tile.getContext('2d');
    const ox = bucket.tx * TILE_SIZE;
    const oz = bucket.tz * TILE_SIZE;

    g.fillStyle = FLOOR_COLOR;
    g.strokeStyle = FLOOR_COLOR;
    g.lineWidth = 1;
    g.lineJoin = 'round';
    for (let i = 0; i < bucket.floors.length; i++) {
        const { a, b, c } = bucket.floors[i];
        g.beginPath();
        g.moveTo((a.x - ox) * TILE_SCALE, (a.z - oz) * TILE_SCALE);
        g.lineTo((b.x - ox) * TILE_SCALE, (b.z - oz) * TILE_SCALE);
        g.lineTo((c.x - ox) * TILE_SCALE, (c.z - oz) * TILE_SCALE);
        g.closePath();
        g.fill();
        g.stroke(); // hides hairline seams between neighbouring triangles
    }

    // A vertical wall triangle collapses to a line in plan view: keep its longest edge.
    g.strokeStyle = WALL_COLOR;
    g.lineWidth = 1.5;
    g.lineCap = 'round';
    g.beginPath();
    for (let i = 0; i < bucket.walls.length; i++) {
        const { a, b, c } = bucket.walls[i];
        const ab = (a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z);
        const bc = (b.x - c.x) * (b.x - c.x) + (b.z - c.z) * (b.z - c.z);
        const ca = (c.x - a.x) * (c.x - a.x) + (c.z - a.z) * (c.z - a.z);
        let p = a, q = b;
        if (bc >= ab && bc >= ca) { p = b; q = c; }
        else if (ca >= ab && ca >= bc) { p = c; q = a; }
        g.moveTo((p.x - ox) * TILE_SCALE, (p.z - oz) * TILE_SCALE);
        g.lineTo((q.x - ox) * TILE_SCALE, (q.z - oz) * TILE_SCALE);
    }
    g.stroke();

    return { canvas: tile, used: frameCounter };
}

function evictTiles() {
    while (tiles.size > MAX_TILES) {
        let oldestKey = null;
        let oldest = Infinity;
        for (const [key, tile] of tiles) {
            if (tile.used < oldest) {
                oldest = tile.used;
                oldestKey = key;
            }
        }
        if (oldestKey === null) break;
        tiles.delete(oldestKey);
    }
}

function fitCanvas() {
    if (!canvas) return;
    const cssSize = canvas.clientWidth || 260;
    const size = Math.max(1, Math.round(cssSize * Math.min(window.devicePixelRatio || 1, 2)));
    if (canvas.width !== size || canvas.height !== size) {
        canvas.width = size;
        canvas.height = size;
    }
    canvasSize = size;
    lastDrawTime = 0;
}

export function initMinimap() {
    canvas = document.getElementById('minimap-canvas');
    if (canvas) {
        ctx = canvas.getContext('2d');
        fitCanvas();
        window.addEventListener('resize', fitCanvas);
    }
}

function drawDots(list, radius, scale, center, px, pz, color, filter) {
    ctx.fillStyle = color;
    ctx.beginPath();
    for (let i = 0; i < list.length; i++) {
        const item = list[i];
        if (filter && !filter(item)) continue;
        const dx = item.position.x - px;
        const dz = item.position.z - pz;
        if (dx * dx + dz * dz > VIEW_RADIUS_SQ) continue;
        const x = center + dx * scale;
        const y = center + dz * scale;
        ctx.moveTo(x + radius, y);
        ctx.arc(x, y, radius, 0, Math.PI * 2);
    }
    ctx.fill();
}

const isMonsterAggro = (m) => m.state === MonsterState.CHASE || m.state === MonsterState.ATTACK;
const isMonsterCalm = (m) => m.state !== MonsterState.DEATH && !isMonsterAggro(m);
const isHealthPickup = (p) => p.state === 'ACTIVE' && !!p.def && p.def.type === 'health';
const isOtherPickup = (p) => p.state === 'ACTIVE' && !(p.def && p.def.type === 'health');
const isClosedChest = (c) => !c.isOpen;

export function updateMinimap(playerPos, camera) {
    if (!canvas || !ctx) {
        initMinimap();
        if (!ctx) return;
    }

    const now = performance.now();
    if (now - lastDrawTime < REDRAW_INTERVAL_MS) return;
    lastDrawTime = now;
    frameCounter++;

    if (bakedRevision !== levelRevision) rebuildBuckets();

    const size = canvasSize;
    const center = size / 2;
    const scale = center / VIEW_RADIUS; // Pixels per world unit
    const px = playerPos.x;
    const pz = playerPos.z;

    ctx.fillStyle = '#0a0e17';
    ctx.fillRect(0, 0, size, size);

    // Level tiles
    const minTx = Math.floor((px - VIEW_RADIUS) / TILE_SIZE);
    const maxTx = Math.floor((px + VIEW_RADIUS) / TILE_SIZE);
    const minTz = Math.floor((pz - VIEW_RADIUS) / TILE_SIZE);
    const maxTz = Math.floor((pz + VIEW_RADIUS) / TILE_SIZE);
    const tileScreenSize = TILE_SIZE * scale + 0.75;
    let bakes = 0;
    ctx.imageSmoothingEnabled = true;
    for (let tx = minTx; tx <= maxTx; tx++) {
        for (let tz = minTz; tz <= maxTz; tz++) {
            const key = tileKey(tx, tz);
            let tile = tiles.get(key);
            if (!tile) {
                const bucket = buckets.get(key);
                if (!bucket || bakes >= MAX_BAKES_PER_FRAME) continue;
                tile = bakeTile(bucket);
                tiles.set(key, tile);
                bakes++;
            }
            tile.used = frameCounter;
            ctx.drawImage(tile.canvas, center + (tx * TILE_SIZE - px) * scale, center + (tz * TILE_SIZE - pz) * scale, tileScreenSize, tileScreenSize);
        }
    }
    if (tiles.size > MAX_TILES) evictTiles();

    const dot = size / 160;

    // Closed doors
    ctx.strokeStyle = '#f59e0b';
    ctx.lineWidth = 3 * dot;
    ctx.beginPath();
    for (let i = 0; i < doors.length; i++) {
        const door = doors[i];
        if (door.isOpen) continue;
        const dx = door.position.x - px;
        const dz = door.position.z - pz;
        if (dx * dx + dz * dz > VIEW_RADIUS_SQ) continue;

        const tris = door.triangles;
        for (let t = 0; t < tris.length; t++) {
            const { a, b, c } = tris[t];
            ctx.moveTo(center + (a.x - px) * scale, center + (a.z - pz) * scale);
            ctx.lineTo(center + (b.x - px) * scale, center + (b.z - pz) * scale);
            ctx.lineTo(center + (c.x - px) * scale, center + (c.z - pz) * scale);
        }
    }
    ctx.stroke();

    drawDots(chests, 3.5 * dot, scale, center, px, pz, '#facc15', isClosedChest);
    drawDots(activePickups, 2.5 * dot, scale, center, px, pz, '#4ade80', isHealthPickup);
    drawDots(activePickups, 2.5 * dot, scale, center, px, pz, '#38bdf8', isOtherPickup);
    drawDots(monsters, 3.5 * dot, scale, center, px, pz, '#f87171', isMonsterCalm);
    drawDots(monsters, 4.0 * dot, scale, center, px, pz, '#ef4444', isMonsterAggro);

    // Exit portals: pulse on the map, or point toward them from the rim when out of view.
    const pulse = 0.5 + 0.5 * Math.sin(now / 250);
    const rim = center - 10 * dot;
    ctx.fillStyle = '#38bdf8';
    ctx.strokeStyle = '#e0f2fe';
    ctx.lineWidth = 1.5 * dot;
    for (let i = 0; i < activeTeleporters.length; i++) {
        const tp = activeTeleporters[i];
        const dx = tp.position.x - px;
        const dz = tp.position.z - pz;
        const dist = Math.sqrt(dx * dx + dz * dz);
        if (dist * scale < rim) {
            const x = center + dx * scale;
            const y = center + dz * scale;
            ctx.beginPath();
            ctx.arc(x, y, (4 + 3 * pulse) * dot, 0, Math.PI * 2);
            ctx.stroke();
            ctx.beginPath();
            ctx.arc(x, y, 3 * dot, 0, Math.PI * 2);
            ctx.fill();
        } else {
            const ang = Math.atan2(dz, dx);
            const ex = center + Math.cos(ang) * rim;
            const ey = center + Math.sin(ang) * rim;
            ctx.beginPath();
            ctx.moveTo(ex + Math.cos(ang) * 6 * dot, ey + Math.sin(ang) * 6 * dot);
            ctx.lineTo(ex + Math.cos(ang + 2.4) * 5 * dot, ey + Math.sin(ang + 2.4) * 5 * dot);
            ctx.lineTo(ex + Math.cos(ang - 2.4) * 5 * dot, ey + Math.sin(ang - 2.4) * 5 * dot);
            ctx.closePath();
            ctx.globalAlpha = 0.6 + 0.4 * pulse;
            ctx.fill();
            ctx.globalAlpha = 1;
        }
    }

    // Player marker & view cone. Canvas angle: +X right (0 rad), +Z down (PI/2 rad).
    // The camera uses YXZ Euler order, so its forward vector follows directly from yaw/pitch.
    const yaw = camera.rotation.y;
    const cosPitch = Math.cos(camera.rotation.x);
    const angle = Math.atan2(-Math.cos(yaw) * cosPitch, -Math.sin(yaw) * cosPitch);

    ctx.fillStyle = 'rgba(251, 191, 36, 0.22)';
    ctx.beginPath();
    ctx.moveTo(center, center);
    ctx.arc(center, center, 28 * dot, angle - 0.4, angle + 0.4);
    ctx.closePath();
    ctx.fill();

    ctx.fillStyle = '#f59e0b';
    ctx.strokeStyle = '#ffffff';
    ctx.lineWidth = dot;
    const arrowLen = 8 * dot;
    ctx.beginPath();
    ctx.moveTo(center + Math.cos(angle) * arrowLen, center + Math.sin(angle) * arrowLen);
    ctx.lineTo(center + Math.cos(angle + 2.5) * arrowLen * 0.7, center + Math.sin(angle + 2.5) * arrowLen * 0.7);
    ctx.lineTo(center + Math.cos(angle - 2.5) * arrowLen * 0.7, center + Math.sin(angle - 2.5) * arrowLen * 0.7);
    ctx.closePath();
    ctx.fill();
    ctx.stroke();

    ctx.fillStyle = '#ef4444';
    ctx.font = `bold ${Math.round(9 * dot)}px sans-serif`;
    ctx.textAlign = 'center';
    ctx.fillText('N', center, 14 * dot);
}
