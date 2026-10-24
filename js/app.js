import * as THREE from './lib/three.module.js';
import { Player } from './player.js';
import { DungeonBuilder, DungeonGeneratorJS } from './dungeon.js';
import { Door, Item, Projectile } from './entities.js';
import { Monster } from './monsters.js';
import { audio } from './audio.js';

class DungeonStompApp {
  constructor() {
    this.container = document.getElementById('canvas-container');

    // Three.js Core
    this.scene = new THREE.Scene();
    this.scene.background = new THREE.Color(0x1a1a2e);

    this.camera = new THREE.PerspectiveCamera(75, window.innerWidth / window.innerHeight, 0.1, 4000);
    this.scene.add(this.camera);

    this.renderer = new THREE.WebGLRenderer({ antialias: true });
    this.renderer.setSize(window.innerWidth, window.innerHeight);
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    this.container.appendChild(this.renderer.domElement);

    // Global Ambient & Torch Lighting
    this.ambientLight = new THREE.AmbientLight(0xffffff, 1.8);
    this.scene.add(this.ambientLight);

    const dirLight = new THREE.DirectionalLight(0xffffff, 1.2);
    dirLight.position.set(100, 300, 100);
    this.scene.add(dirLight);

    // Dungeon & Entities
    this.dungeonBuilder = new DungeonBuilder(this.scene);
    this.player = new Player(this.camera, this.scene);
    this.doors = [];
    this.items = [];
    this.monsters = [];
    this.projectiles = [];

    // HUD & UI Elements
    this.hpBar = document.getElementById('hp-bar');
    this.hpText = document.getElementById('hp-text');
    this.manaBar = document.getElementById('mana-bar');
    this.manaText = document.getElementById('mana-text');
    this.levelDisplay = document.getElementById('level-display');
    this.xpDisplay = document.getElementById('xp-display');
    this.weaponName = document.getElementById('weapon-name');
    this.combatLog = document.getElementById('combat-log');
    this.overlayScreen = document.getElementById('overlay-screen');
    this.generatorModal = document.getElementById('generator-modal');

    // Minimap Canvas
    this.minimapCanvas = document.getElementById('minimap');
    this.minimapCtx = this.minimapCanvas ? this.minimapCanvas.getContext('2d') : null;
    this.showMinimap = true;

    // Timing
    this.clock = new THREE.Clock();

    // Event Listeners
    this.setupEvents();

    // Player Callbacks
    this.player.onLog = (msg, type) => this.logMessage(msg, type);
    this.player.onAttack = (weapon) => this.handlePlayerAttack(weapon);

    // Load Objects.dat & level map
    this.initGame();

    // Start loop
    this.animate = this.animate.bind(this);
    requestAnimationFrame(this.animate);
  }

  async initGame() {
    await this.dungeonBuilder.loadDataFiles();
    await this.loadLevelMap('bin/level1.map');
  }

  setupEvents() {
    window.addEventListener('resize', () => {
      this.camera.aspect = window.innerWidth / window.innerHeight;
      this.camera.updateProjectionMatrix();
      this.renderer.setSize(window.innerWidth, window.innerHeight);
    });

    // Start Screen / PointerLock
    this.overlayScreen.addEventListener('click', () => {
      audio.init();
      this.overlayScreen.style.display = 'none';
      this.player.isPointerLocked = true;
      try {
        this.container.requestPointerLock();
      } catch (e) {}
    });

    document.addEventListener('pointerlockchange', () => {
      const locked = document.pointerLockElement === this.container;
      this.player.isPointerLocked = locked;
    });

    // F7 Generator Modal & Hotkeys
    window.addEventListener('keydown', (e) => {
      if (e.code === 'KeyL') {
        this.showMinimap = !this.showMinimap;
        if (this.minimapCanvas) this.minimapCanvas.style.display = this.showMinimap ? 'block' : 'none';
      }
      if (e.code === 'F7' || e.key === 'F7') {
        e.preventDefault();
        this.toggleGeneratorModal();
      }
      if (e.code === 'KeyE' || e.code === 'Space') {
        this.interactClosestDoor();
      }
    });

    document.getElementById('btn-close-modal').addEventListener('click', () => {
      this.toggleGeneratorModal(false);
    });

    document.getElementById('btn-generate').addEventListener('click', () => {
      const seedVal = parseInt(document.getElementById('input-seed').value, 10) || Math.floor(Math.random() * 10000);
      const countVal = parseInt(document.getElementById('input-count').value, 10) || 120;

      this.generateProceduralDungeon(seedVal, countVal);
      this.toggleGeneratorModal(false);
      this.logMessage(`Generated Procedural Dungeon (Seed: ${seedVal}, Tiles: ${countVal})!`, 'info');
    });
  }

  toggleGeneratorModal(show = null) {
    if (show === null) {
      show = this.generatorModal.style.display !== 'block';
    }
    this.generatorModal.style.display = show ? 'block' : 'none';
    if (show && document.pointerLockElement) {
      document.exitPointerLock();
    }
  }

  logMessage(msg, type = 'info') {
    if (!this.combatLog) return;
    const entry = document.createElement('div');
    entry.className = `log-entry ${type}`;
    entry.textContent = msg;
    this.combatLog.prepend(entry);

    while (this.combatLog.children.length > 20) {
      this.combatLog.removeChild(this.combatLog.lastChild);
    }
  }

  clearSceneEntities() {
    for (const d of this.doors) this.scene.remove(d.group);
    for (const m of this.monsters) this.scene.remove(m.group);
    for (const i of this.items) this.scene.remove(i.group);
    for (const p of this.projectiles) p.destroy();

    this.doors = [];
    this.monsters = [];
    this.items = [];
    this.projectiles = [];
  }

  async loadLevelMap(mapPath) {
    try {
      this.clearSceneEntities();

      const res = await fetch(mapPath);
      let mapText = '';
      if (res.ok) {
        mapText = await res.text();
      }

      const { spawnPoint, entities } = this.dungeonBuilder.buildFromMapData(mapText);
      this.player.position.copy(spawnPoint);

      this.spawnEntities(entities);
      this.logMessage(`Loaded dungeon map: ${mapPath}`, 'info');
    } catch (e) {
      console.error('Map loading error:', e);
    }
  }

  generateProceduralDungeon(seed, count) {
    this.clearSceneEntities();

    const { spawnPoint, placed, entities } = DungeonGeneratorJS.generate(seed, count);
    this.player.position.copy(spawnPoint);

    for (const tile of placed) {
      this.dungeonBuilder.buildDungeonTile(tile.name, tile.x, tile.y, tile.z, tile.rot);
    }

    this.spawnEntities(entities);
  }

  spawnEntities(entities) {
    for (const ent of entities) {
      if (ent.name.startsWith('door')) {
        const door = new Door(ent.x, ent.y, ent.z, ent.rot);
        this.doors.push(door);
        this.scene.add(door.group);
      } else if (ent.name === '!monster1') {
        const typeKey = (ent.extra && ent.extra[0]) ? ent.extra[0] : 'GOBLIN';
        const monster = new Monster(typeKey, ent.x, ent.y, ent.z, ent.rot);
        this.monsters.push(monster);
        this.scene.add(monster.group);
      } else if (['POTION', 'COIN', 'SPELLBOOK'].some(k => ent.name.includes(k))) {
        const item = new Item(ent.name, ent.x, ent.y, ent.z, ent.rot);
        this.items.push(item);
        this.scene.add(item.group);
      }
    }
  }

  interactClosestDoor() {
    const pPos = this.player.position;
    for (const door of this.doors) {
      const dPos = door.group.position;
      if (pPos.distanceToSquared(dPos) < 10000) {
        door.toggle((msg, type) => this.logMessage(msg, type));
        break;
      }
    }
  }

  handlePlayerAttack(weapon) {
    if (weapon.type === 'magic') {
      const pPos = this.player.position.clone();
      pPos.y += 10;
      const dir = new THREE.Vector3();
      this.camera.getWorldDirection(dir);

      const proj = new Projectile(this.scene, pPos, dir, 550, weapon.damage);
      this.projectiles.push(proj);
    } else if (weapon.type === 'melee') {
      const pPos = this.player.position;
      const pDir = new THREE.Vector3();
      this.camera.getWorldDirection(pDir);

      for (const monster of this.monsters) {
        if (monster.isDead) continue;
        const mPos = monster.group.position;
        const distSq = pPos.distanceToSquared(mPos);

        if (distSq < 14400) {
          const toMonster = mPos.clone().sub(pPos).normalize();
          const dot = pDir.dot(toMonster);
          if (dot > 0.5) {
            monster.takeDamage(weapon.damage, this.player, (msg, type) => this.logMessage(msg, type));
          }
        }
      }
    }
  }

  updateHUD() {
    const hpPct = Math.max(0, Math.min(100, (this.player.hp / this.player.maxHp) * 100));
    this.hpBar.style.width = `${hpPct}%`;
    this.hpText.textContent = `${Math.round(this.player.hp)} / ${this.player.maxHp}`;

    const manaPct = Math.max(0, Math.min(100, (this.player.mana / this.player.maxMana) * 100));
    this.manaBar.style.width = `${manaPct}%`;
    this.manaText.textContent = `${Math.round(this.player.mana)} / ${this.player.maxMana}`;

    this.levelDisplay.textContent = `Level: ${this.player.level}`;
    this.xpDisplay.textContent = `XP: ${this.player.xp}`;
    this.weaponName.textContent = this.player.currentWeapon.name;
  }

  renderMinimap() {
    if (!this.minimapCtx || !this.showMinimap) return;

    const ctx = this.minimapCtx;
    ctx.clearRect(0, 0, 160, 160);

    ctx.fillStyle = '#080810';
    ctx.fillRect(0, 0, 160, 160);

    const cx = 80;
    const cy = 80;
    const scale = 0.08;
    const pPos = this.player.position;

    // Draw Monsters
    for (const m of this.monsters) {
      if (m.isDead) continue;
      const mx = cx + (m.group.position.x - pPos.x) * scale;
      const my = cy + (m.group.position.z - pPos.z) * scale;

      if (mx >= 0 && mx <= 160 && my >= 0 && my <= 160) {
        ctx.fillStyle = '#ff3333';
        ctx.beginPath();
        ctx.arc(mx, my, 3, 0, Math.PI * 2);
        ctx.fill();
      }
    }

    // Draw Doors
    for (const d of this.doors) {
      const dx = cx + (d.group.position.x - pPos.x) * scale;
      const dy = cy + (d.group.position.z - pPos.z) * scale;

      if (dx >= 0 && dx <= 160 && dy >= 0 && dy <= 160) {
        ctx.fillStyle = d.isOpen ? '#33cc33' : '#e6a100';
        ctx.fillRect(dx - 3, dy - 3, 6, 6);
      }
    }

    // Draw Player Arrow
    ctx.save();
    ctx.translate(cx, cy);
    ctx.rotate(-this.player.rotation.y);

    ctx.fillStyle = '#00ffff';
    ctx.beginPath();
    ctx.moveTo(0, -6);
    ctx.lineTo(-4, 5);
    ctx.lineTo(4, 5);
    ctx.closePath();
    ctx.fill();

    ctx.restore();
  }

  animate() {
    requestAnimationFrame(this.animate);

    const dt = Math.min(this.clock.getDelta(), 0.1);

    this.player.update(dt, this.dungeonBuilder.colliders, this.doors);

    for (const m of this.monsters) {
      m.update(dt, this.player, (msg, type) => this.logMessage(msg, type));
    }

    for (const item of this.items) {
      item.update(dt, this.player, (msg, type) => this.logMessage(msg, type));
    }

    for (let i = this.projectiles.length - 1; i >= 0; i--) {
      const proj = this.projectiles[i];
      proj.update(dt, this.monsters, this.player, (msg, type) => this.logMessage(msg, type));
      if (proj.isDead) {
        this.projectiles.splice(i, 1);
      }
    }

    this.updateHUD();
    this.renderMinimap();

    this.renderer.render(this.scene, this.camera);
  }
}

window.addEventListener('DOMContentLoaded', () => {
  window.app = new DungeonStompApp();
});
