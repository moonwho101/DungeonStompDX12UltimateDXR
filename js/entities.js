import * as THREE from './lib/three.module.js';
import { audio } from './audio.js';

export class Door {
  constructor(x, y, z, rot = 0) {
    this.isOpen = false;
    this.animating = false;

    this.group = new THREE.Group();
    this.group.position.set(x, y, z);
    this.group.rotation.y = THREE.MathUtils.degToRad(rot);

    // Frame
    const frameGeo = new THREE.BoxGeometry(110, 110, 12);
    const frameMat = new THREE.MeshStandardMaterial({ color: 0x443322, roughness: 0.8 });
    const frame = new THREE.Mesh(frameGeo, frameMat);
    frame.position.y = 55;

    // Door Panel
    const panelGeo = new THREE.BoxGeometry(90, 100, 8);
    const panelMat = new THREE.MeshStandardMaterial({ color: 0x664422, roughness: 0.6 });
    this.panel = new THREE.Mesh(panelGeo, panelMat);
    this.panel.position.set(0, 50, 0);
    this.panel.castShadow = true;

    this.group.add(frame, this.panel);

    // Bounding Box for player collision
    this.box = new THREE.Box3().setFromObject(this.group);
  }

  toggle(onLog) {
    if (this.animating) return;
    this.isOpen = !this.isOpen;
    this.animating = true;
    audio.playDoor();

    const targetY = this.isOpen ? 140 : 50;
    const startY = this.panel.position.y;
    const startTime = Date.now();
    const duration = 400;

    const animate = () => {
      const now = Date.now();
      const progress = Math.min(1, (now - startTime) / duration);
      this.panel.position.y = THREE.MathUtils.lerp(startY, targetY, progress);

      if (progress < 1) {
        requestAnimationFrame(animate);
      } else {
        this.animating = false;
      }
    };
    animate();

    if (onLog) {
      onLog(this.isOpen ? "Door opened." : "Door closed.", "info");
    }
  }
}

export class Item {
  constructor(type, x, y, z, rot = 0) {
    this.type = type;
    this.isPickedUp = false;

    this.group = new THREE.Group();
    this.group.position.set(x, y + 12, z);
    this.group.rotation.y = THREE.MathUtils.degToRad(rot);

    this.createMesh();
  }

  createMesh() {
    let geo, mat;
    if (this.type.includes('POTION')) {
      geo = new THREE.SphereGeometry(8, 12, 12);
      mat = new THREE.MeshStandardMaterial({ color: 0xff3333, emissive: 0xaa1111 });
    } else if (this.type.includes('COIN')) {
      geo = new THREE.CylinderGeometry(8, 8, 3, 12);
      mat = new THREE.MeshStandardMaterial({ color: 0xffd700, metalness: 0.9, roughness: 0.2 });
    } else if (this.type.includes('SPELLBOOK')) {
      geo = new THREE.BoxGeometry(14, 18, 4);
      mat = new THREE.MeshStandardMaterial({ color: 0x3344cc });
    } else {
      geo = new THREE.BoxGeometry(12, 12, 12);
      mat = new THREE.MeshStandardMaterial({ color: 0x88bbcc });
    }

    this.mesh = new THREE.Mesh(geo, mat);
    this.group.add(this.mesh);
  }

  update(dt, player, onLog) {
    if (this.isPickedUp) return;

    // Hover / Rotation animation
    this.group.rotation.y += dt * 1.5;

    const distSq = this.group.position.distanceToSquared(player.position);
    if (distSq < 1600) { // Pickup radius ~40 units
      this.isPickedUp = true;
      audio.playPickup();

      if (this.type.includes('POTION')) {
        player.hp = Math.min(player.maxHp, player.hp + 35);
        if (onLog) onLog("Picked up Health Potion (+35 HP)", "heal");
      } else if (this.type.includes('COIN')) {
        player.xp += 25;
        if (onLog) onLog("Picked up Gold Coins (+25 XP)", "pickup");
      } else if (this.type.includes('SPELLBOOK')) {
        player.mana = Math.min(player.maxMana, player.mana + 25);
        if (onLog) onLog("Picked up Ancient Spellbook (+25 Mana)", "pickup");
      } else {
        if (onLog) onLog(`Picked up ${this.type}`, "pickup");
      }

      this.group.visible = false;
    }
  }
}

export class Projectile {
  constructor(scene, startPos, dir, speed = 400, damage = 35) {
    this.scene = scene;
    this.position = startPos.clone();
    this.dir = dir.clone().normalize();
    this.speed = speed;
    this.damage = damage;
    this.isDead = false;
    this.lifetime = 2.5;

    const geo = new THREE.SphereGeometry(6, 8, 8);
    const mat = new THREE.MeshBasicMaterial({ color: 0xffaa00 });
    this.mesh = new THREE.Mesh(geo, mat);
    this.mesh.position.copy(this.position);

    this.light = new THREE.PointLight(0xff7700, 1.5, 120);
    this.mesh.add(this.light);

    this.scene.add(this.mesh);
  }

  update(dt, monsters, player, onLog) {
    if (this.isDead) return;

    this.lifetime -= dt;
    if (this.lifetime <= 0) {
      this.destroy();
      return;
    }

    const moveStep = this.dir.clone().multiplyScalar(this.speed * dt);
    this.position.add(moveStep);
    this.mesh.position.copy(this.position);

    // Collision with monsters
    for (const monster of monsters) {
      if (!monster.isDead && monster.group.position.distanceToSquared(this.position) < 1600) {
        monster.takeDamage(this.damage, player, onLog);
        this.destroy();
        return;
      }
    }
  }

  destroy() {
    this.isDead = true;
    if (this.mesh) {
      this.scene.remove(this.mesh);
    }
  }
}
