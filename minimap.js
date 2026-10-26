import * as THREE from 'three';
import { levelTriangles } from './collision.js';
import { monsters, doors, MonsterState } from './gameState.js';
import { activePickups } from './logic.js';

let canvas = null;
let ctx = null;

const VIEW_RADIUS = 1600.0; // World distance visible on minimap radius

export function initMinimap() {
    canvas = document.getElementById('minimap-canvas');
    if (canvas) {
        ctx = canvas.getContext('2d');
    }
}

export function updateMinimap(playerPos, camera) {
    if (!canvas || !ctx) {
        initMinimap();
        if (!ctx) return;
    }

    const width = canvas.width;
    const height = canvas.height;
    const center = width / 2;
    const scale = center / VIEW_RADIUS; // Pixels per world unit

    // Clear background
    ctx.clearRect(0, 0, width, height);

    ctx.save();

    // Clip to circular viewport
    ctx.beginPath();
    ctx.arc(center, center, center - 2, 0, Math.PI * 2);
    ctx.clip();

    // Background fill & subtle grid
    ctx.fillStyle = '#0a0e17';
    ctx.fillRect(0, 0, width, height);

    ctx.strokeStyle = 'rgba(255, 255, 255, 0.04)';
    ctx.lineWidth = 1;
    const gridSize = 40;
    for (let x = 0; x <= width; x += gridSize) {
        ctx.beginPath();
        ctx.moveTo(x, 0);
        ctx.lineTo(x, height);
        ctx.stroke();
    }
    for (let y = 0; y <= height; y += gridSize) {
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(width, y);
        ctx.stroke();
    }

    const px = playerPos.x;
    const pz = playerPos.z;

    // Helper: Map world (x, z) to canvas coordinates relative to player
    function worldToMap(wx, wz) {
        const dx = wx - px;
        const dz = wz - pz;
        return {
            x: center + dx * scale,
            y: center + dz * scale
        };
    }

    // Render level walls / triangles
    ctx.strokeStyle = '#475569';
    ctx.lineWidth = 1.5;
    ctx.beginPath();

    const renderRadiusSq = (VIEW_RADIUS * 1.2) * (VIEW_RADIUS * 1.2);
    for (let i = 0; i < levelTriangles.length; i++) {
        const tri = levelTriangles[i];
        // Quick distance check against triangle centroid
        const cx = (tri.a.x + tri.b.x + tri.c.x) / 3;
        const cz = (tri.a.z + tri.b.z + tri.c.z) / 3;
        const dx = cx - px;
        const dz = cz - pz;
        if (dx * dx + dz * dz > renderRadiusSq) continue;

        const pA = worldToMap(tri.a.x, tri.a.z);
        const pB = worldToMap(tri.b.x, tri.b.z);
        const pC = worldToMap(tri.c.x, tri.c.z);

        ctx.moveTo(pA.x, pA.y);
        ctx.lineTo(pB.x, pB.y);
        ctx.lineTo(pC.x, pC.y);
        ctx.lineTo(pA.x, pA.y);
    }
    ctx.stroke();

    // Render doors
    ctx.strokeStyle = '#f59e0b';
    ctx.lineWidth = 3;
    for (let i = 0; i < doors.length; i++) {
        const door = doors[i];
        if (door.isOpen) continue; // Skip rendering fully open doors
        const dPos = door.position;
        const dx = dPos.x - px;
        const dz = dPos.z - pz;
        if (dx * dx + dz * dz > renderRadiusSq) continue;

        if (door.triangles && door.triangles.length > 0) {
            ctx.beginPath();
            for (const tri of door.triangles) {
                const pA = worldToMap(tri.a.x, tri.a.z);
                const pB = worldToMap(tri.b.x, tri.b.z);
                const pC = worldToMap(tri.c.x, tri.c.z);
                ctx.moveTo(pA.x, pA.y);
                ctx.lineTo(pB.x, pB.y);
                ctx.lineTo(pC.x, pC.y);
            }
            ctx.stroke();
        } else {
            const mP = worldToMap(dPos.x, dPos.z);
            ctx.fillStyle = '#f59e0b';
            ctx.beginPath();
            ctx.arc(mP.x, mP.y, 3, 0, Math.PI * 2);
            ctx.fill();
        }
    }

    // Render active pickups / items
    for (let i = 0; i < activePickups.length; i++) {
        const pickup = activePickups[i];
        if (pickup.state !== 'ACTIVE') continue;
        const pPos = pickup.position;
        const dx = pPos.x - px;
        const dz = pPos.z - pz;
        if (dx * dx + dz * dz > renderRadiusSq) continue;

        const mP = worldToMap(pPos.x, pPos.z);
        const isHealth = pickup.def && pickup.def.type === 'health';
        ctx.fillStyle = isHealth ? '#4ade80' : '#38bdf8';
        ctx.beginPath();
        ctx.arc(mP.x, mP.y, 2.5, 0, Math.PI * 2);
        ctx.fill();
    }

    // Render active monsters
    for (let i = 0; i < monsters.length; i++) {
        const monster = monsters[i];
        if (!monster || monster.state === MonsterState.DEATH) continue;

        const mPos = monster.position;
        const dx = mPos.x - px;
        const dz = mPos.z - pz;
        if (dx * dx + dz * dz > renderRadiusSq) continue;

        const mP = worldToMap(mPos.x, mPos.z);
        ctx.fillStyle = (monster.state === MonsterState.CHASE || monster.state === MonsterState.ATTACK)
            ? '#ef4444'
            : '#f87171';
        ctx.beginPath();
        ctx.arc(mP.x, mP.y, 3.5, 0, Math.PI * 2);
        ctx.fill();
    }

    // Render player marker & direction cone
    const lookDir = new THREE.Vector3();
    camera.getWorldDirection(lookDir);
    const yaw = Math.atan2(lookDir.x, lookDir.z);

    // Player direction FOV cone
    ctx.fillStyle = 'rgba(251, 191, 36, 0.18)';
    ctx.beginPath();
    ctx.moveTo(center, center);
    ctx.arc(center, center, 28, yaw - Math.PI / 2 - 0.4, yaw - Math.PI / 2 + 0.4);
    ctx.closePath();
    ctx.fill();

    // Player arrow marker
    ctx.fillStyle = '#f59e0b';
    ctx.strokeStyle = '#ffffff';
    ctx.lineWidth = 1;
    ctx.beginPath();
    const arrowLen = 8;
    ctx.moveTo(
        center + Math.sin(yaw) * arrowLen,
        center + Math.cos(yaw) * arrowLen
    );
    ctx.lineTo(
        center + Math.sin(yaw + 2.5) * (arrowLen * 0.7),
        center + Math.cos(yaw + 2.5) * (arrowLen * 0.7)
    );
    ctx.lineTo(
        center + Math.sin(yaw - 2.5) * (arrowLen * 0.7),
        center + Math.cos(yaw - 2.5) * (arrowLen * 0.7)
    );
    ctx.closePath();
    ctx.fill();
    ctx.stroke();

    // Compass indicator (North)
    ctx.fillStyle = '#ef4444';
    ctx.font = 'bold 9px sans-serif';
    ctx.textAlign = 'center';
    ctx.fillText('N', center, 14);

    ctx.restore();
}
