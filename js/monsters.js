import * as THREE from './lib/three.module.js';
import { audio } from './audio.js';

export const MONSTER_TYPES = {
  GOBLIN:     { hp: 35,  damage: 8,  speed: 65, color: 0x338833, scale: 0.8 },
  OGRE:       { hp: 80,  damage: 22, speed: 45, color: 0xaa5522, scale: 1.4 },
  KNIGHT:     { hp: 100, damage: 26, speed: 60, color: 0x8888aa, scale: 1.2 },
  SORCERER:   { hp: 60,  damage: 30, speed: 50, color: 0xaa22aa, scale: 1.0, ranged: true },
  TENTACLE:   { hp: 45,  damage: 14, speed: 40, color: 0x228888, scale: 0.9 },
  WRAITH:     { hp: 90,  damage: 25, speed: 70, color: 0x4444ff, scale: 1.1 },
  DRAGON:     { hp: 200, damage: 45, speed: 55, color: 0xcc2222, scale: 2.0 },
  COBRA:      { hp: 30,  damage: 10, speed: 75, color: 0x88aa22, scale: 0.7 },
  MUMMY:      { hp: 70,  damage: 18, speed: 38, color: 0xaaaa66, scale: 1.1 },
  WOLF:       { hp: 40,  damage: 12, speed: 85, color: 0x666666, scale: 0.8 }
};

export class Monster {
  constructor(typeKey, x, y, z, rot = 0) {
    this.typeKey = typeKey.toUpperCase();
    this.data = MONSTER_TYPES[this.typeKey] || MONSTER_TYPES.GOBLIN;

    this.hp = this.data.hp;
    this.maxHp = this.data.hp;
    this.speed = this.data.speed;
    this.damage = this.data.damage;
    this.isDead = false;

    this.attackTimer = 0;
    this.attackInterval = 1.2;

    this.group = new THREE.Group();
    this.group.position.set(x, y, z);
    this.group.rotation.y = THREE.MathUtils.degToRad(rot);

    this.createMesh();
  }

  createMesh() {
    const s = this.data.scale;

    // Body Mesh
    const bodyGeo = new THREE.CylinderGeometry(12 * s, 14 * s, 45 * s, 8);
    const bodyMat = new THREE.MeshStandardMaterial({
      color: this.data.color,
      roughness: 0.6,
      metalness: 0.2
    });
    this.body = new THREE.Mesh(bodyGeo, bodyMat);
    this.body.position.y = (45 * s) / 2;
    this.body.castShadow = true;
    this.body.receiveShadow = true;

    // Eyes
    const eyeGeo = new THREE.SphereGeometry(3 * s, 6, 6);
    const eyeMat = new THREE.MeshBasicMaterial({ color: 0xff0000 });

    const eyeL = new THREE.Mesh(eyeGeo, eyeMat);
    eyeL.position.set(-5 * s, 15 * s, -12 * s);

    const eyeR = new THREE.Mesh(eyeGeo, eyeMat);
    eyeR.position.set(5 * s, 15 * s, -12 * s);

    this.group.add(this.body, eyeL, eyeR);
  }

  update(dt, player, onLog) {
    if (this.isDead) return;

    if (this.attackTimer > 0) {
      this.attackTimer -= dt;
    }

    const mPos = this.group.position;
    const pPos = player.position;

    const dx = pPos.x - mPos.x;
    const dz = pPos.z - mPos.z;
    const distSq = dx * dx + dz * dz;
    const dist = Math.sqrt(distSq);

    // Perception radius
    const sightRange = 480;
    const attackRange = this.data.ranged ? 280 : 42;

    if (dist < sightRange && dist > 5) {
      // Face player
      const targetAngle = Math.atan2(dx, dz);
      this.group.rotation.y = THREE.MathUtils.lerp(this.group.rotation.y, targetAngle, dt * 5);

      // Seek movement if not in attack range
      if (dist > attackRange) {
        const moveDist = this.speed * dt;
        mPos.x += (dx / dist) * moveDist;
        mPos.z += (dz / dist) * moveDist;
      } else {
        // Attack
        if (this.attackTimer <= 0) {
          this.attackTimer = this.attackInterval;
          this.attackPlayer(player, onLog);
        }
      }
    }
  }

  attackPlayer(player, onLog) {
    const rawDmg = Math.round(this.damage * (0.8 + Math.random() * 0.4));
    player.hp = Math.max(0, player.hp - rawDmg);
    audio.playHit();

    if (onLog) {
      onLog(`${this.typeKey} hits you for ${rawDmg} damage!`, 'damage');
    }

    // Visual strike animation
    this.group.position.y += 8;
    setTimeout(() => {
      this.group.position.y -= 8;
    }, 120);
  }

  takeDamage(amount, player, onLog) {
    if (this.isDead) return;

    this.hp -= amount;
    audio.playHit();

    if (onLog) {
      onLog(`You hit ${this.typeKey} for ${amount} damage! (${Math.max(0, this.hp)} HP)`, 'info');
    }

    // Hit flash / nudge
    if (this.body) {
      const origColor = this.body.material.color.getHex();
      this.body.material.color.setHex(0xffffff);
      setTimeout(() => {
        if (this.body && this.body.material) {
          this.body.material.color.setHex(origColor);
        }
      }, 100);
    }

    if (this.hp <= 0) {
      this.die(player, onLog);
    }
  }

  die(player, onLog) {
    this.isDead = true;
    const xpGained = Math.round(this.maxHp * 0.8);
    player.xp += xpGained;

    if (onLog) {
      onLog(`${this.typeKey} defeated! +${xpGained} XP`, 'pickup');
    }

    // Collapse to floor
    this.group.rotation.x = Math.PI / 2;
    this.group.position.y = 8;
  }
}
