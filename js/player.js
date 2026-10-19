import * as THREE from './lib/three.module.js';
import { audio } from './audio.js';

export class Player {
  constructor(camera, scene) {
    this.camera = camera;
    this.scene = scene;

    this.position = new THREE.Vector3(5200, 30, 2600);
    this.rotation = new THREE.Euler(0, 0, 0, 'YXZ');
    this.velocity = new THREE.Vector3();

    this.hp = 100;
    this.maxHp = 100;
    this.mana = 50;
    this.maxMana = 50;
    this.xp = 0;
    this.level = 1;

    // Movement speeds matching Dungeon Stomp
    this.speed = 180.0;
    this.sprintMultiplier = 1.6;
    this.isSprinting = false;

    this.isGrounded = true;
    this.verticalVelocity = 0;
    this.gravity = -450;
    this.jumpForce = 150;

    // Weapons / Spells
    this.weapons = [
      { name: 'Broadsword', type: 'melee', damage: 18, cost: 0, cooldown: 0.4 },
      { name: 'Battleaxe', type: 'melee', damage: 28, cost: 0, cooldown: 0.6 },
      { name: 'Fireball Scroll', type: 'magic', damage: 35, cost: 10, cooldown: 0.5 },
      { name: 'Lightning Scroll', type: 'magic', damage: 45, cost: 15, cooldown: 0.5 },
      { name: 'Healing Potion', type: 'heal', healAmount: 40, cost: 0, cooldown: 1.0 }
    ];
    this.currentWeaponIndex = 0;
    this.attackTimer = 0;

    // Input state
    this.keys = {};
    this.isPointerLocked = false;

    // Camera bobbing & sway
    this.bobTimer = 0;
    this.swayY = 0;

    // First person weapon mesh group
    this.weaponGroup = new THREE.Group();
    this.camera.add(this.weaponGroup);
    this.createWeaponMesh();

    this.setupInputs();
  }

  get currentWeapon() {
    return this.weapons[this.currentWeaponIndex];
  }

  setupInputs() {
    window.addEventListener('keydown', (e) => {
      this.keys[e.code] = true;

      if (e.code === 'KeyQ') this.cycleWeapon(-1);
      if (e.code === 'KeyZ') this.cycleWeapon(1);
      if (e.code === 'Digit1') this.selectWeapon(0);
      if (e.code === 'Digit2') this.selectWeapon(1);
      if (e.code === 'Digit3') this.selectWeapon(2);
      if (e.code === 'Digit4') this.selectWeapon(3);
      if (e.code === 'Digit5') this.selectWeapon(4);
    });

    window.addEventListener('keyup', (e) => {
      this.keys[e.code] = false;
    });

    window.addEventListener('mousemove', (e) => {
      if (this.isPointerLocked) {
        const sensitivity = 0.0022;
        this.rotation.y -= e.movementX * sensitivity;
        this.rotation.x -= e.movementY * sensitivity;

        // Clamp pitch (-85deg to 85deg)
        const maxPitch = Math.PI / 2 - 0.08;
        this.rotation.x = Math.max(-maxPitch, Math.min(maxPitch, this.rotation.x));
      }
    });

    window.addEventListener('mousedown', (e) => {
      if (this.isPointerLocked && e.button === 0) {
        this.attack();
      }
    });
  }

  cycleWeapon(dir) {
    this.currentWeaponIndex = (this.currentWeaponIndex + dir + this.weapons.length) % this.weapons.length;
    this.updateWeaponMesh();
  }

  selectWeapon(idx) {
    if (idx >= 0 && idx < this.weapons.length) {
      this.currentWeaponIndex = idx;
      this.updateWeaponMesh();
    }
  }

  createWeaponMesh() {
    // Clear previous
    while (this.weaponGroup.children.length > 0) {
      this.weaponGroup.remove(this.weaponGroup.children[0]);
    }

    const weapon = this.currentWeapon;
    const group = new THREE.Group();

    if (weapon.type === 'melee') {
      // Blade
      const bladeGeo = new THREE.BoxGeometry(0.06, 0.8, 0.02);
      const bladeMat = new THREE.MeshStandardMaterial({ color: 0xcccccc, metalness: 0.8, roughness: 0.2 });
      const blade = new THREE.Mesh(bladeGeo, bladeMat);
      blade.position.set(0, 0.4, 0);

      // Guard
      const guardGeo = new THREE.BoxGeometry(0.25, 0.04, 0.05);
      const guardMat = new THREE.MeshStandardMaterial({ color: 0x997733, metalness: 0.6 });
      const guard = new THREE.Mesh(guardGeo, guardMat);
      guard.position.set(0, 0.02, 0);

      // Hilt
      const hiltGeo = new THREE.CylinderGeometry(0.025, 0.025, 0.18);
      const hiltMat = new THREE.MeshStandardMaterial({ color: 0x442211 });
      const hilt = new THREE.Mesh(hiltGeo, hiltMat);
      hilt.position.set(0, -0.09, 0);

      group.add(blade, guard, hilt);
      group.position.set(0.3, -0.3, -0.5);
      group.rotation.set(0.2, -0.2, -0.1);
    } else if (weapon.type === 'magic') {
      // Scroll / Wand
      const wandGeo = new THREE.CylinderGeometry(0.03, 0.02, 0.7);
      const wandMat = new THREE.MeshStandardMaterial({ color: 0x2266aa, emissive: 0x113366 });
      const wand = new THREE.Mesh(wandGeo, wandMat);
      wand.position.set(0, 0.35, 0);

      // Orb tip
      const orbGeo = new THREE.SphereGeometry(0.06, 12, 12);
      const orbMat = new THREE.MeshStandardMaterial({ color: 0x33ffff, emissive: 0x00ffff, emissiveIntensity: 0.8 });
      const orb = new THREE.Mesh(orbGeo, orbMat);
      orb.position.set(0, 0.7, 0);

      group.add(wand, orb);
      group.position.set(0.28, -0.25, -0.45);
      group.rotation.set(0.3, -0.3, 0);
    } else if (weapon.type === 'heal') {
      // Potion flask
      const bodyGeo = new THREE.SphereGeometry(0.1, 16, 16);
      const bodyMat = new THREE.MeshStandardMaterial({ color: 0xff2222, roughness: 0.1, transparent: true, opacity: 0.85 });
      const body = new THREE.Mesh(bodyGeo, bodyMat);

      const neckGeo = new THREE.CylinderGeometry(0.03, 0.03, 0.08);
      const neckMat = new THREE.MeshStandardMaterial({ color: 0xaaaaaa });
      const neck = new THREE.Mesh(neckGeo, neckMat);
      neck.position.set(0, 0.1, 0);

      group.add(body, neck);
      group.position.set(0.25, -0.25, -0.4);
    }

    this.weaponGroup.add(group);
    this.weaponMesh = group;
  }

  updateWeaponMesh() {
    this.createWeaponMesh();
  }

  attack() {
    if (this.attackTimer > 0) return;

    const weapon = this.currentWeapon;
    this.attackTimer = weapon.cooldown;

    if (weapon.type === 'heal') {
      if (this.hp < this.maxHp) {
        this.hp = Math.min(this.maxHp, this.hp + weapon.healAmount);
        audio.playPickup();
        if (this.onLog) this.onLog(`Drank Healing Potion! Healed ${weapon.healAmount} HP.`, 'heal');
      } else {
        if (this.onLog) this.onLog(`Already at full health!`, 'info');
      }
      return;
    }

    if (weapon.type === 'magic') {
      if (this.mana < weapon.cost) {
        if (this.onLog) this.onLog(`Not enough Mana! (${this.mana}/${weapon.cost})`, 'info');
        return;
      }
      this.mana -= weapon.cost;
      audio.playCast();
    } else {
      audio.playSwing();
    }

    // Weapon swing animation
    if (this.weaponMesh) {
      this.weaponMesh.position.z -= 0.15;
      this.weaponMesh.rotation.x += 0.4;
      setTimeout(() => {
        if (this.weaponMesh) {
          this.weaponMesh.position.z += 0.15;
          this.weaponMesh.rotation.x -= 0.4;
        }
      }, 150);
    }

    // Trigger attack callback for game combat manager
    if (this.onAttack) {
      this.onAttack(weapon);
    }
  }

  update(dt, colliders = [], doors = []) {
    if (this.attackTimer > 0) {
      this.attackTimer -= dt;
    }

    // Mana passive regen
    if (this.mana < this.maxMana) {
      this.mana = Math.min(this.maxMana, this.mana + dt * 2.5);
    }

    // Movement calculation
    this.isSprinting = !!(this.keys['ShiftLeft'] || this.keys['ShiftRight']);
    const moveSpeed = this.speed * (this.isSprinting ? this.sprintMultiplier : 1.0);

    const moveVector = new THREE.Vector3();
    if (this.keys['KeyW']) moveVector.z -= 1;
    if (this.keys['KeyS']) moveVector.z += 1;
    if (this.keys['KeyA']) moveVector.x -= 1;
    if (this.keys['KeyD']) moveVector.x += 1;

    if (moveVector.lengthSq() > 0) {
      moveVector.normalize();

      // Rotate moveVector by Y rotation
      moveVector.applyAxisAngle(new THREE.Vector3(0, 1, 0), this.rotation.y);
      moveVector.multiplyScalar(moveSpeed * dt);

      // Collision checks with walls & closed doors
      this.moveWithCollision(moveVector, colliders, doors);

      // Camera Bobbing
      this.bobTimer += dt * (this.isSprinting ? 14 : 9);
      this.swayY = Math.sin(this.bobTimer) * (this.isSprinting ? 2.2 : 1.2);
    } else {
      this.swayY = Math.sin(Date.now() * 0.002) * 0.3; // subtle idle sway
    }

    // Jump & Gravity
    if ((this.keys['Space'] || this.keys['KeyE']) && this.isGrounded) {
      this.verticalVelocity = this.jumpForce;
      this.isGrounded = false;
    }

    if (!this.isGrounded) {
      this.verticalVelocity += this.gravity * dt;
      this.position.y += this.verticalVelocity * dt;

      if (this.position.y <= 30.0) { // Base floor height
        this.position.y = 30.0;
        this.verticalVelocity = 0;
        this.isGrounded = true;
      }
    }

    // Update camera transform
    this.camera.position.copy(this.position);
    this.camera.position.y += this.swayY;
    this.camera.quaternion.setFromEuler(this.rotation);
  }

  moveWithCollision(dir, colliders, doors) {
    const radius = 22.0;

    // Move X
    this.position.x += dir.x;
    if (this.checkCollisions(colliders, doors, radius)) {
      this.position.x -= dir.x;
    }

    // Move Z
    this.position.z += dir.z;
    if (this.checkCollisions(colliders, doors, radius)) {
      this.position.z -= dir.z;
    }
  }

  checkCollisions(colliders, doors, radius) {
    const pBox = new THREE.Box3().setFromCenterAndSize(
      this.position,
      new THREE.Vector3(radius * 2, 60, radius * 2)
    );

    for (const box of colliders) {
      if (box.intersectsBox(pBox)) return true;
    }

    for (const door of doors) {
      if (!door.isOpen && door.box && door.box.intersectsBox(pBox)) {
        return true;
      }
    }

    return false;
  }
}
