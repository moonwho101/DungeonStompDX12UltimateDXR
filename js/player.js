import * as THREE from './lib/three.module.js';
import { audio } from './audio.js';

export const WEAPONS = [
  { name: 'Broadsword', damage: 25, type: 'melee', cooldown: 0.5 },
  { name: 'Battleaxe', damage: 40, type: 'melee', cooldown: 0.8 },
  { name: 'Fireball Spell', damage: 35, type: 'magic', manaCost: 10, cooldown: 0.4 },
  { name: 'Lightning Scroll', damage: 60, type: 'magic', manaCost: 20, cooldown: 0.6 }
];

export class Player {
  constructor(camera, scene) {
    this.camera = camera;
    this.scene = scene;

    this.position = new THREE.Vector3(0, 50, 0);
    this.velocity = new THREE.Vector3();
    this.rotation = new THREE.Euler(0, 0, 0, 'YXZ');

    this.maxHp = 100;
    this.hp = 100;
    this.maxMana = 50;
    this.mana = 50;
    this.xp = 0;
    this.level = 1;

    this.currentWeaponIndex = 0;
    this.attackCooldown = 0;

    this.isPointerLocked = false;
    this.keys = {};

    this.moveSpeed = 250;
    this.sprintMult = 1.6;

    this.bobTimer = 0;

    this.setupWeaponMesh();
    this.setupInput();
  }

  get currentWeapon() {
    return WEAPONS[this.currentWeaponIndex];
  }

  setupWeaponMesh() {
    this.weaponGroup = new THREE.Group();

    const swordGeom = new THREE.BoxGeometry(4, 30, 4);
    const swordMat = new THREE.MeshStandardMaterial({ color: 0xcccccc, metalness: 0.8, roughness: 0.2 });
    this.weaponMesh = new THREE.Mesh(swordGeom, swordMat);
    this.weaponMesh.position.set(12, -10, -20);
    this.weaponMesh.rotation.z = -0.3;

    this.weaponGroup.add(this.weaponMesh);
    this.camera.add(this.weaponGroup);
  }

  setupInput() {
    window.addEventListener('keydown', (e) => {
      this.keys[e.code] = true;

      if (e.code === 'KeyQ') this.cycleWeapon(-1);
      if (e.code === 'KeyZ') this.cycleWeapon(1);
    });

    window.addEventListener('keyup', (e) => {
      this.keys[e.code] = false;
    });

    window.addEventListener('mousemove', (e) => {
      if (!this.isPointerLocked) return;

      const movementX = e.movementX || 0;
      const movementY = e.movementY || 0;

      this.rotation.y -= movementX * 0.002;
      this.rotation.x -= movementY * 0.002;

      this.rotation.x = Math.max(-Math.PI / 2.2, Math.min(Math.PI / 2.2, this.rotation.x));
    });

    window.addEventListener('mousedown', (e) => {
      if (!this.isPointerLocked) return;
      if (e.button === 0) {
        this.attack();
      }
    });
  }

  cycleWeapon(dir) {
    this.currentWeaponIndex = (this.currentWeaponIndex + dir + WEAPONS.length) % WEAPONS.length;
    if (this.onLog) {
      this.onLog(`Equipped ${this.currentWeapon.name}`, 'info');
    }
  }

  attack() {
    if (this.attackCooldown > 0) return;

    const weapon = this.currentWeapon;

    if (weapon.type === 'magic') {
      if (this.mana < weapon.manaCost) {
        if (this.onLog) this.onLog('Not enough mana!', 'damage');
        return;
      }
      this.mana -= weapon.manaCost;
      audio.playCast();
    } else {
      audio.playSwing();
    }

    this.attackCooldown = weapon.cooldown;

    // Swing animation
    this.weaponMesh.position.z = -12;
    setTimeout(() => {
      if (this.weaponMesh) this.weaponMesh.position.z = -20;
    }, 150);

    if (this.onAttack) {
      this.onAttack(weapon);
    }
  }

  takeDamage(amount, onLog) {
    this.hp -= amount;
    if (onLog) onLog(`Player took ${Math.round(amount)} damage!`, 'damage');

    if (this.hp <= 0) {
      this.hp = 0;
      if (onLog) onLog('You died! Respawning...', 'damage');
      this.position.set(0, 50, 0);
      this.hp = this.maxHp;
    }
  }

  heal(amount) {
    this.hp = Math.min(this.maxHp, this.hp + amount);
  }

  restoreMana(amount) {
    this.mana = Math.min(this.maxMana, this.mana + amount);
  }

  addXP(amount, onLog) {
    this.xp += amount;
    if (this.xp >= this.level * 100) {
      this.level++;
      this.maxHp += 20;
      this.hp = this.maxHp;
      this.maxMana += 10;
      this.mana = this.maxMana;
      if (onLog) onLog(`LEVEL UP! Advanced to Level ${this.level}!`, 'heal');
    }
  }

  update(dt, colliders, doors) {
    this.attackCooldown -= dt;

    // Mana Regeneration
    if (this.mana < this.maxMana) {
      this.mana = Math.min(this.maxMana, this.mana + dt * 2);
    }

    // Camera Orientation
    this.camera.rotation.copy(this.rotation);

    if (!this.isPointerLocked) return;

    // Movement Direction
    const forward = new THREE.Vector3(0, 0, -1).applyQuaternion(this.camera.quaternion);
    forward.y = 0;
    forward.normalize();

    const side = new THREE.Vector3(1, 0, 0).applyQuaternion(this.camera.quaternion);
    side.y = 0;
    side.normalize();

    let speed = this.moveSpeed;
    if (this.keys['ShiftLeft'] || this.keys['ShiftRight']) {
      speed *= this.sprintMult;
    }

    const moveVector = new THREE.Vector3();

    if (this.keys['KeyW']) moveVector.add(forward);
    if (this.keys['KeyS']) moveVector.sub(forward);
    if (this.keys['KeyD']) moveVector.add(side);
    if (this.keys['KeyA']) moveVector.sub(side);

    if (moveVector.lengthSq() > 0) {
      moveVector.normalize().multiplyScalar(speed * dt);

      // Simple collision checks
      const newPos = this.position.clone().add(moveVector);

      let canMove = true;
      if (colliders) {
        const playerRadius = 15;
        for (const box of colliders) {
          if (box.distanceToPoint(newPos) < playerRadius) {
            canMove = false;
            break;
          }
        }
      }

      if (canMove) {
        this.position.copy(newPos);
      }

      // Head Bobbing
      this.bobTimer += dt * 10;
      const bobY = Math.sin(this.bobTimer) * 3;
      this.camera.position.copy(this.position);
      this.camera.position.y += bobY;
    } else {
      this.camera.position.copy(this.position);
    }
  }
}
