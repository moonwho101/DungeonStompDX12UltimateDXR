import * as THREE from './lib/three.module.js';
import { audio } from './audio.js';

export class Door {
  constructor(x, y, z, rotDeg = 0) {
    this.group = new THREE.Group();
    this.group.position.set(x, y, z);
    this.group.rotation.y = THREE.MathUtils.degToRad(rotDeg);

    this.isOpen = false;
    this.animating = false;

    this.buildDoor();
  }

  buildDoor() {
    // Frame
    const frameGeom = new THREE.BoxGeometry(110, 150, 15);
    const frameMat = new THREE.MeshStandardMaterial({ color: 0x4a3b2c, roughness: 0.9 });
    const frame = new THREE.Mesh(frameGeom, frameMat);
    frame.position.y = 75;
    this.group.add(frame);

    // Door Panel
    const panelGeom = new THREE.BoxGeometry(90, 135, 8);
    const panelMat = new THREE.MeshStandardMaterial({ color: 0x6e4d28, roughness: 0.7 });
    this.panel = new THREE.Mesh(panelGeom, panelMat);
    this.panel.position.set(0, 70, 0);
    this.group.add(this.panel);
  }

  toggle(onLog) {
    this.isOpen = !this.isOpen;
    audio.playDoor();

    if (this.isOpen) {
      this.panel.position.y = 190;
      if (onLog) onLog('Door opened.', 'info');
    } else {
      this.panel.position.y = 70;
      if (onLog) onLog('Door closed.', 'info');
    }
  }
}

export class Item {
  constructor(type, x, y, z, rotDeg = 0) {
    this.type = type;
    this.group = new THREE.Group();
    this.group.position.set(x, y + 15, z);
    this.group.rotation.y = THREE.MathUtils.degToRad(rotDeg);

    this.isPickedUp = false;

    this.buildMesh();
  }

  buildMesh() {
    let geom, mat;

    if (this.type.includes('POTION')) {
      geom = new THREE.CylinderGeometry(5, 8, 16, 8);
      mat = new THREE.MeshStandardMaterial({ color: 0xff2222, roughness: 0.3 });
    } else if (this.type.includes('COIN')) {
      geom = new THREE.CylinderGeometry(8, 8, 3, 12);
      mat = new THREE.MeshStandardMaterial({ color: 0xffcc00, metalness: 0.8, roughness: 0.2 });
    } else {
      geom = new THREE.BoxGeometry(12, 16, 4);
      mat = new THREE.MeshStandardMaterial({ color: 0x2266ff, roughness: 0.5 });
    }

    this.mesh = new THREE.Mesh(geom, mat);
    this.group.add(this.mesh);
  }

  update(dt, player, onLog) {
    if (this.isPickedUp) return;

    this.mesh.rotation.y += dt * 2;

    const pPos = player.position;
    if (this.group.position.distanceToSquared(pPos) < 1600) {
      this.pickup(player, onLog);
    }
  }

  pickup(player, onLog) {
    this.isPickedUp = true;
    this.group.visible = false;
    audio.playPickup();

    if (this.type.includes('POTION')) {
      player.heal(30);
      if (onLog) onLog('Picked up Health Potion! (+30 HP)', 'heal');
    } else if (this.type.includes('COIN')) {
      player.addXP(10, onLog);
      if (onLog) onLog('Picked up Gold Coin! (+10 XP)', 'pickup');
    } else {
      player.restoreMana(25);
      if (onLog) onLog('Picked up Mana Scroll! (+25 Mana)', 'heal');
    }
  }
}

export class Projectile {
  constructor(scene, startPos, direction, speed = 500, damage = 35) {
    this.scene = scene;
    this.direction = direction.normalize();
    this.speed = speed;
    this.damage = damage;
    this.life = 3.0;
    this.isDead = false;

    const geom = new THREE.SphereGeometry(6, 8, 8);
    const mat = new THREE.MeshBasicMaterial({ color: 0x00ffff });
    this.mesh = new THREE.Mesh(geom, mat);
    this.mesh.position.copy(startPos);

    this.light = new THREE.PointLight(0x00ffff, 2, 100);
    this.mesh.add(this.light);

    this.scene.add(this.mesh);
  }

  update(dt, monsters, player, onLog) {
    if (this.isDead) return;

    this.life -= dt;
    if (this.life <= 0) {
      this.destroy();
      return;
    }

    const moveStep = this.direction.clone().multiplyScalar(this.speed * dt);
    this.mesh.position.add(moveStep);

    for (const m of monsters) {
      if (m.isDead) continue;
      if (this.mesh.position.distanceToSquared(m.group.position) < 2500) {
        m.takeDamage(this.damage, player, onLog);
        this.destroy();
        break;
      }
    }
  }

  destroy() {
    if (this.isDead) return;
    this.isDead = true;
    this.scene.remove(this.mesh);
  }
}
