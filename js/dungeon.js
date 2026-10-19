import * as THREE from './lib/three.module.js';

// Procedural Dungeon Generator Port in JS
export class DungeonGeneratorJS {
  static generate(seed = 42, count = 120, startX = 5200, startZ = 2600) {
    let random = DungeonGeneratorJS.createPRNG(seed || Date.now());

    const OBJECTS = {
      'ROOM2': [{ pos: [0, -160], out: [0, -1] }, { pos: [0, 160], out: [0, 1] }],
      'crossroads': [{ pos: [80, 0], out: [0, -1] }, { pos: [80, 240], out: [0, 1] }, { pos: [-40, 120], out: [-1, 0] }, { pos: [200, 120], out: [1, 0] }],
      't_junction': [{ pos: [80, 0], out: [0, -1] }, { pos: [-40, 120], out: [-1, 0] }, { pos: [200, 120], out: [1, 0] }],
      'left_corner': [{ pos: [80, 0], out: [0, -1] }, { pos: [-40, 120], out: [-1, 0] }],
      'ROOM_SQUARE': [{ pos: [240, 0], out: [1, 0] }],
      'ROOMEDIUM': [{ pos: [-160, 0], out: [-1, 0] }, { pos: [160, 0], out: [1, 0] }]
    };

    const placed = [];
    const entities = [];

    placed.push({ name: 'ROOM2', x: startX, y: 0.0, z: startZ, rot: 0 });

    const openExits = [];
    for (const ext of OBJECTS['ROOM2']) {
      openExits.push({
        wx: startX + ext.pos[0],
        wy: 0.0,
        wz: startZ + ext.pos[1],
        wdx: ext.out[0],
        wdz: ext.out[1]
      });
    }

    const types = Object.keys(OBJECTS);

    for (let i = 0; i < count && openExits.length > 0; i++) {
      const exitIdx = Math.floor(random() * openExits.length);
      const O = openExits.splice(exitIdx, 1)[0];

      let placedNew = false;
      const shuffledTypes = [...types].sort(() => random() - 0.5);

      for (const candName of shuffledTypes) {
        if (placedNew) break;
        const candExits = OBJECTS[candName];

        for (let extIdx = 0; extIdx < candExits.length; extIdx++) {
          if (placedNew) break;
          const locExt = candExits[extIdx];
          const rots = [0, 90, 180, 270].sort(() => random() - 0.5);

          for (const ang of rots) {
            const rad = THREE.MathUtils.degToRad(ang);
            const rdx = Math.round(locExt.out[0] * Math.cos(rad) - locExt.out[1] * Math.sin(rad));
            const rdz = Math.round(locExt.out[0] * Math.sin(rad) + locExt.out[1] * Math.cos(rad));

            if (rdx === -O.wdx && rdz === -O.wdz) {
              const rx = locExt.pos[0] * Math.cos(rad) - locExt.pos[1] * Math.sin(rad);
              const rz = locExt.pos[0] * Math.sin(rad) + locExt.pos[1] * Math.cos(rad);

              const Ox = O.wx - rx;
              const Oy = O.wy;
              const Oz = O.wz - rz;

              placed.push({ name: candName, x: Ox, y: Oy, z: Oz, rot: ang });
              placedNew = true;

              if (random() < 0.3) {
                entities.push({ name: 'door55', x: O.wx, y: O.wy + 10, z: O.wz, rot: ang });
              }

              if (random() < 0.4) {
                const mobs = ['GOBLIN', 'OGRE', 'KNIGHT', 'SORCERER', 'TENTACLE', 'MUMMY', 'WRAITH'];
                const mobType = mobs[Math.floor(random() * mobs.length)];
                entities.push({ name: '!monster1', extra: [mobType], x: Ox, y: Oy + 10, z: Oz, rot: ang });
              } else if (random() < 0.3) {
                const items = ['POTION', 'COIN', 'SPELLBOOK'];
                const itemType = items[Math.floor(random() * items.length)];
                entities.push({ name: itemType, x: Ox, y: Oy + 10, z: Oz, rot: ang });
              }

              for (let k = 0; k < candExits.length; k++) {
                if (k === extIdx) continue;
                const other = candExits[k];
                const orx = other.pos[0] * Math.cos(rad) - other.pos[1] * Math.sin(rad);
                const orz = other.pos[0] * Math.sin(rad) + other.pos[1] * Math.cos(rad);
                const odx = Math.round(other.out[0] * Math.cos(rad) - other.out[1] * Math.sin(rad));
                const odz = Math.round(other.out[0] * Math.sin(rad) + other.out[1] * Math.cos(rad));

                openExits.push({ wx: Ox + orx, wy: Oy, wz: Oz + orz, wdx: odx, wdz: odz });
              }
              break;
            }
          }
        }
      }
    }

    return { spawnPoint: new THREE.Vector3(startX, 30, startZ), placed, entities };
  }

  static createPRNG(seed) {
    return function() {
      let t = seed += 0x6D2B79F5;
      t = Math.imul(t ^ (t >>> 15), t | 1);
      t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }
}

export class DungeonBuilder {
  constructor(scene) {
    this.scene = scene;
    this.colliders = [];
    this.lights = [];
    this.spawnPoint = new THREE.Vector3(5200, 30, 2600);

    this.createMaterials();
  }

  createMaterials() {
    this.materials = {
      floor: new THREE.MeshLambertMaterial({ color: 0x334455, side: THREE.DoubleSide }),
      wall: new THREE.MeshLambertMaterial({ color: 0x776655, side: THREE.DoubleSide }),
      ceiling: new THREE.MeshLambertMaterial({ color: 0x222233, side: THREE.DoubleSide }),
      arch: new THREE.MeshLambertMaterial({ color: 0x997755, side: THREE.DoubleSide })
    };
  }

  buildFromMapData(mapText) {
    this.colliders = [];
    this.lights = [];

    const lines = mapText.split('\n');
    let currentObject = null;
    let currentCoords = null;
    let currentRot = 0;

    const parsedObjects = [];
    const parsedLights = [];

    for (let line of lines) {
      line = line.trim();
      if (!line || line.startsWith('#')) continue;

      const tokens = line.split(/\s+/);
      const tag = tokens[0];

      if (tag === 'OBJECT') {
        currentObject = tokens[1];
      } else if (tag === 'CO_ORDINATES') {
        currentCoords = {
          x: parseFloat(tokens[1]),
          y: parseFloat(tokens[2]),
          z: parseFloat(tokens[3])
        };
      } else if (tag === 'ROT_ANGLE') {
        currentRot = parseFloat(tokens[1]);
        if (currentObject && currentCoords) {
          parsedObjects.push({
            name: currentObject,
            x: currentCoords.x,
            y: currentCoords.y,
            z: currentCoords.z,
            rot: currentRot,
            extra: tokens.slice(2)
          });
        }
      } else if (tag === 'LIGHT_SOURCE') {
        const posIdx = tokens.indexOf('POS');
        const colIdx = tokens.indexOf('COLOUR');

        if (posIdx !== -1 && colIdx !== -1) {
          parsedLights.push({
            x: parseFloat(tokens[posIdx + 1]),
            y: parseFloat(tokens[posIdx + 2]),
            z: parseFloat(tokens[posIdx + 3]),
            r: parseFloat(tokens[colIdx + 1]),
            g: parseFloat(tokens[colIdx + 2]),
            b: parseFloat(tokens[colIdx + 3])
          });
        }
      }
    }

    const entitiesToSpawn = [];

    for (const obj of parsedObjects) {
      if (obj.name === 'startpos') {
        this.spawnPoint.set(obj.x, obj.y + 30, obj.z);
      } else if (obj.name.startsWith('CORRIDOR') || obj.name.startsWith('ROOM') || obj.name.startsWith('CROSSING') || obj.name.startsWith('t_junction') || obj.name.startsWith('crossroads') || obj.name.startsWith('left_corner')) {
        this.buildDungeonTile(obj.name, obj.x, obj.y, obj.z, obj.rot);
      } else if (obj.name === 'torch' || obj.name === 'lamp_post') {
        this.addTorchLight(obj.x, obj.y, obj.z);
      } else {
        entitiesToSpawn.push(obj);
      }
    }

    for (const light of parsedLights) {
      this.addPointLight(light.x, light.y, light.z, light.r, light.g, light.b);
    }

    return { spawnPoint: this.spawnPoint, entities: entitiesToSpawn };
  }

  buildDungeonTile(type, x, y, z, rot) {
    const tileGroup = new THREE.Group();
    tileGroup.position.set(x, y, z);
    tileGroup.rotation.y = THREE.MathUtils.degToRad(rot);

    let width = 240;
    let length = 320;
    let height = 120;

    if (type.includes('ROOM_SQUARE')) {
      width = 480; length = 480;
    } else if (type.includes('ROOMEDIUM') || type.includes('ROOM2')) {
      width = 360; length = 360;
    }

    // Floor
    const floorGeo = new THREE.PlaneGeometry(width, length);
    const floor = new THREE.Mesh(floorGeo, this.materials.floor);
    floor.rotation.x = -Math.PI / 2;
    tileGroup.add(floor);

    // Ceiling
    const ceilingGeo = new THREE.PlaneGeometry(width, length);
    const ceiling = new THREE.Mesh(ceilingGeo, this.materials.ceiling);
    ceiling.position.y = height;
    ceiling.rotation.x = Math.PI / 2;
    tileGroup.add(ceiling);

    // Walls
    const wallThick = 12;
    const halfW = width / 2;

    this.addWallSegment(tileGroup, -halfW, height / 2, 0, wallThick, height, length);
    this.addWallSegment(tileGroup, halfW, height / 2, 0, wallThick, height, length);

    this.scene.add(tileGroup);
    tileGroup.updateMatrixWorld(true);

    // Store world bounding boxes for collision
    for (const child of tileGroup.children) {
      if (child.geometry instanceof THREE.BoxGeometry) {
        this.colliders.push(new THREE.Box3().setFromObject(child));
      }
    }
  }

  addWallSegment(parent, lx, ly, lz, tw, th, tl) {
    const wallGeo = new THREE.BoxGeometry(tw, th, tl);
    const wall = new THREE.Mesh(wallGeo, this.materials.wall);
    wall.position.set(lx, ly, lz);
    parent.add(wall);
  }

  addTorchLight(x, y, z) {
    const light = new THREE.PointLight(0xffaa44, 3.0, 500);
    light.position.set(x, y + 20, z);
    this.scene.add(light);
    this.lights.push(light);

    const geo = new THREE.CylinderGeometry(2, 4, 18);
    const mat = new THREE.MeshBasicMaterial({ color: 0xffaa22 });
    const mesh = new THREE.Mesh(geo, mat);
    mesh.position.set(x, y, z);
    this.scene.add(mesh);
  }

  addPointLight(x, y, z, r, g, b) {
    const color = new THREE.Color(r / 8, g / 8, b / 8);
    const light = new THREE.PointLight(color, 2.5, 450);
    light.position.set(x, y, z);
    this.scene.add(light);
    this.lights.push(light);
  }
}
