import * as THREE from './lib/three.module.js';
import { audio } from './audio.js';

export const MONSTER_TYPES = {
  GOBLIN: { name: 'Goblin', hp: 30, damage: 8, color: 0x33aa33, scale: 30, xp: 20 },
  OGRE: { name: 'Ogre', hp: 80, damage: 20, color: 0x886633, scale: 50, xp: 50 },
  KNIGHT: { name: 'Death Knight', hp: 100, damage: 25, color: 0x555577, scale: 45, xp: 75 },
  SORCERER: { name: 'Sorcerer', hp: 50, damage: 18, color: 0xaa33aa, scale: 40, xp: 60 },
  TENTACLE: { name: 'Tentacle', hp: 40, damage: 12, color: 0x228888, scale: 35, xp: 30 },
  DRAGON: { name: 'Dragon', hp: 200, damage: 45, color: 0xcc2222, scale: 70, xp: 200 },
  WRAITH: { name: 'Wraith', hp: 60, damage: 15, color: 0x88aaff, scale: 40, xp: 70 },
  COBRA: { name: 'Giant Cobra', hp: 35, damage: 10, color: 0x22aa55, scale: 30, xp: 25 },
  MUMMY: { name: 'Mummy', hp: 70, damage: 16, color: 0xccbb88, scale: 40, xp: 45 },
  WOLF: { name: 'Dire Wolf', hp: 45, damage: 14, color: 0x666666, scale: 35, xp: 35 }
};

export class Monster {
  constructor(typeKey, x, y, z, rotDeg = 0) {
    this.typeInfo = MONSTER_TYPES[typeKey] || MONSTER_TYPES.GOBLIN;
    this.name = this.typeInfo.name;
    this.maxHp = this.typeInfo.hp;
    this.hp = this.maxHp;
    this.damage = this.typeInfo.damage;
    this.xp = this.typeInfo.xp;

    this.group = new THREE.Group();
    this.group.position.set(x, y + this.typeInfo.scale / 2, z);

    this.isDead = false;
    this.attackCooldown = 0;
    this.speed = 80;

    this.buildMesh();
  }

  buildMesh() {
    const size = this.typeInfo.scale;

    // Body Billboard / Box
    const bodyGeom = new THREE.BoxGeometry(size * 0.7, size, size * 0.7);
    const bodyMat = new THREE.MeshStandardMaterial({
      color: this.typeInfo.color,
      roughness: 0.5,
      metalness: 0.2
    });
    this.bodyMesh = new THREE.Mesh(bodyGeom, bodyMat);
    this.group.add(this.bodyMesh);

    // Eyes
    const eyeGeom = new THREE.SphereGeometry(size * 0.1, 8, 8);
    const eyeMat = new THREE.MeshBasicMaterial({ color: 0xff0000 });

    const leftEye = new THREE.Mesh(eyeGeom, eyeMat);
    leftEye.position.set(-size * 0.2, size * 0.25, size * 0.35);
    this.group.add(leftEye);

    const rightEye = new THREE.Mesh(eyeGeom, eyeMat);
    rightEye.position.set(size * 0.2, size * 0.25, size * 0.35);
    this.group.add(rightEye);
  }

  takeDamage(amount, player, onLog) {
    if (this.isDead) return;

    this.hp -= amount;
    audio.playHit();

    if (onLog) {
      onLog(`Dealt ${Math.round(amount)} damage to ${this.name}!`, 'damage');
    }

    if (this.hp <= 0) {
      this.die(player, onLog);
    }
  }

  die(player, onLog) {
    this.isDead = true;
    this.group.rotation.x = Math.PI / 2;
    this.group.position.y = 5;

    if (player) {
      player.addXP(this.xp, onLog);
    }

    if (onLog) {
      onLog(`Killed ${this.name}! (+${this.xp} XP)`, 'heal');
    }
  }

  update(dt, player, onLog) {
    if (this.isDead) return;

    this.attackCooldown -= dt;

    const mPos = this.group.position;
    const pPos = player.position;

    const dist = mPos.distanceTo(pPos);

    // Look at player
    this.group.lookAt(pPos.x, mPos.y, pPos.z);

    // Steering / Pursuit
    if (dist > 40 && dist < 500) {
      const dir = pPos.clone().sub(mPos).normalize();
      mPos.addScaledVector(dir, this.speed * dt);
    }

    // Attack Player
    if (dist <= 50 && this.attackCooldown <= 0) {
      this.attackCooldown = 1.5;
      player.takeDamage(this.damage, onLog);
      audio.playHit();
    }
  }
}
