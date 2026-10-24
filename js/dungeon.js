import * as THREE from './lib/three.module.js';

export class TexturesDatParser {
  static parse(text) {
    const lines = text.split(/\r?\n/);
    const textureMap = new Map(); // alias -> filename
    const aliasToId = new Map();  // alias -> id

    let currentTextureFile = '';

    for (let line of lines) {
      line = line.trim();
      if (!line || line.startsWith('//') || line.startsWith('#')) continue;

      if (line.startsWith('END_FILE')) break;

      const tokens = line.split(/\s+/);
      if (tokens[0] === 'AddTexture') {
        currentTextureFile = tokens[1] ? tokens[1].replace(/\\/g, '/') : '';
      } else if (tokens[0] === 'Alias') {
        // Alias  1   die10r1           OPAQUE  WHOLE  default
        const id = parseInt(tokens[1], 10);
        const alias = tokens[2];
        if (alias) {
          textureMap.set(alias.toLowerCase(), currentTextureFile);
          aliasToId.set(alias.toLowerCase(), id);
        }
      }
    }

    return { textureMap, aliasToId };
  }
}

export class ObjectsDatParser {
  static parse(text) {
    const lines = text.split(/\r?\n/);
    const objectsByName = new Map();
    const objectsById = new Map();

    let currentObj = null;
    let currentTexture = 'default';
    let datScale = 1.0;

    let lineIdx = 0;

    const readTokens = () => {
      while (lineIdx < lines.length) {
        let l = lines[lineIdx++].trim();
        if (!l || l.startsWith('//') || l.startsWith('#')) continue;
        const commentIdx = l.indexOf('//');
        if (commentIdx !== -1) l = l.substring(0, commentIdx).trim();
        if (!l) continue;
        return l.split(/\s+/);
      }
      return null;
    };

    while (lineIdx < lines.length) {
      const tokens = readTokens();
      if (!tokens) break;

      const cmd = tokens[0].toUpperCase();

      if (cmd === 'OBJECT' || cmd === 'Q2M_OBJECT') {
        const id = parseInt(tokens[1], 10);
        const name = tokens[2] || `object_${id}`;

        currentObj = {
          id,
          name,
          scale: 1.0,
          triangles: [] // { positions: [v0, v1, v2], uvs: [uv0, uv1, uv2], texture: string }
        };

        datScale = 1.0;
        currentTexture = 'default';

        objectsByName.set(name, currentObj);
        objectsByName.set(name.toLowerCase(), currentObj);
        objectsById.set(id, currentObj);
      } else if (cmd === 'SCALE') {
        datScale = parseFloat(tokens[1]) || 1.0;
        if (currentObj) currentObj.scale = datScale;
      } else if (cmd === 'TEXTURE') {
        currentTexture = tokens[1] ? tokens[1].toLowerCase() : 'default';
      } else if (cmd === 'TRI') {
        if (!currentObj) continue;
        const v0 = parseVert(readTokens(), datScale);
        const v1 = parseVert(readTokens(), datScale);
        const v2 = parseVert(readTokens(), datScale);

        currentObj.triangles.push({
          positions: [v0.pos, v1.pos, v2.pos],
          uvs: [v0.uv || [0, 1], v1.uv || [0, 0], v2.uv || [1, 1]],
          texture: currentTexture
        });
      } else if (cmd === 'QUAD') {
        if (!currentObj) continue;
        const q0 = parseVert(readTokens(), datScale);
        const q1 = parseVert(readTokens(), datScale);
        const q2 = parseVert(readTokens(), datScale);
        const q3 = parseVert(readTokens(), datScale);

        const defaultUVs = [[0, 1], [0, 0], [1, 0], [1, 1]];

        // Triangle 1: q0, q1, q3
        currentObj.triangles.push({
          positions: [q0.pos, q1.pos, q3.pos],
          uvs: [q0.uv || defaultUVs[0], q1.uv || defaultUVs[1], q3.uv || defaultUVs[3]],
          texture: currentTexture
        });
        // Triangle 2: q2, q3, q1
        currentObj.triangles.push({
          positions: [q2.pos, q3.pos, q1.pos],
          uvs: [q2.uv || defaultUVs[2], q3.uv || defaultUVs[3], q1.uv || defaultUVs[1]],
          texture: currentTexture
        });
      } else if (cmd === 'TRITEX') {
        if (!currentObj) continue;
        const v0 = parseVertEx(readTokens(), datScale);
        const v1 = parseVertEx(readTokens(), datScale);
        const v2 = parseVertEx(readTokens(), datScale);

        currentObj.triangles.push({
          positions: [v0.pos, v1.pos, v2.pos],
          uvs: [v0.uv, v1.uv, v2.uv],
          texture: currentTexture
        });
      } else if (cmd === 'QUADTEX') {
        if (!currentObj) continue;
        const q0 = parseVertEx(readTokens(), datScale);
        const q1 = parseVertEx(readTokens(), datScale);
        const q2 = parseVertEx(readTokens(), datScale);
        const q3 = parseVertEx(readTokens(), datScale);

        // Triangle 1: q0, q1, q3
        currentObj.triangles.push({
          positions: [q0.pos, q1.pos, q3.pos],
          uvs: [q0.uv, q1.uv, q3.uv],
          texture: currentTexture
        });
        // Triangle 2: q2, q3, q1
        currentObj.triangles.push({
          positions: [q2.pos, q3.pos, q1.pos],
          uvs: [q2.uv, q3.uv, q1.uv],
          texture: currentTexture
        });
      } else if (cmd === 'TRI_STRIP') {
        if (!currentObj) continue;
        const count = parseInt(tokens[1], 10);
        const verts = [];
        for (let i = 0; i < count; i++) {
          verts.push(parseVertEx(readTokens(), datScale));
        }
        for (let i = 0; i < count - 2; i++) {
          if (i % 2 === 0) {
            currentObj.triangles.push({
              positions: [verts[i].pos, verts[i + 1].pos, verts[i + 2].pos],
              uvs: [verts[i].uv, verts[i + 1].uv, verts[i + 2].uv],
              texture: currentTexture
            });
          } else {
            currentObj.triangles.push({
              positions: [verts[i + 2].pos, verts[i + 1].pos, verts[i].pos],
              uvs: [verts[i + 2].uv, verts[i + 1].uv, verts[i].uv],
              texture: currentTexture
            });
          }
        }
      } else if (cmd === 'TRI_FAN') {
        if (!currentObj) continue;
        const count = parseInt(tokens[1], 10);
        const verts = [];
        for (let i = 0; i < count; i++) {
          verts.push(parseVertEx(readTokens(), datScale));
        }
        for (let i = 0; i < count - 2; i++) {
          currentObj.triangles.push({
            positions: [verts[0].pos, verts[i + 1].pos, verts[i + 2].pos],
            uvs: [verts[0].uv, verts[i + 1].uv, verts[i + 2].uv],
            texture: currentTexture
          });
        }
      } else if (cmd === 'END_FILE') {
        break;
      }
    }

    return { objectsByName, objectsById };
  }
}

function parseVert(tokens, scale) {
  if (!tokens || tokens.length < 3) return { pos: [0, 0, 0], uv: [0, 0] };
  return {
    pos: [
      parseFloat(tokens[0]) * scale,
      parseFloat(tokens[1]) * scale,
      parseFloat(tokens[2]) * scale
    ],
    uv: tokens.length >= 5 ? [parseFloat(tokens[3]), parseFloat(tokens[4])] : null
  };
}

function parseVertEx(tokens, scale) {
  if (!tokens || tokens.length < 5) return { pos: [0, 0, 0], uv: [0, 0] };
  return {
    pos: [
      parseFloat(tokens[0]) * scale,
      parseFloat(tokens[1]) * scale,
      parseFloat(tokens[2]) * scale
    ],
    uv: [parseFloat(tokens[3]), parseFloat(tokens[4])]
  };
}

export class DungeonBuilder {
  constructor(scene) {
    this.scene = scene;
    this.dungeonGroup = new THREE.Group();
    this.scene.add(this.dungeonGroup);
    this.colliders = [];

    this.textureMap = new Map();
    this.objectsByName = new Map();
    this.materialCache = new Map();
    this.proceduralTextures = new Map();

    this.initProceduralTextures();
  }

  initProceduralTextures() {
    this.proceduralTextures.set('wall', this.createPatternTexture('#3a3a4e', '#222233', 'brick'));
    this.proceduralTextures.set('floor', this.createPatternTexture('#1e1e2c', '#12121c', 'tile'));
    this.proceduralTextures.set('ceiling', this.createPatternTexture('#2a2a3c', '#181826', 'wood'));
    this.proceduralTextures.set('wood', this.createPatternTexture('#5c3a21', '#3d2514', 'wood'));
    this.proceduralTextures.set('metal', this.createPatternTexture('#7f8c8d', '#34495e', 'metal'));
    this.proceduralTextures.set('default', this.createPatternTexture('#4a4a5a', '#2d2d3a', 'stone'));
  }

  createPatternTexture(c1, c2, type = 'brick') {
    const canvas = document.createElement('canvas');
    canvas.width = 256;
    canvas.height = 256;
    const ctx = canvas.getContext('2d');

    ctx.fillStyle = c1;
    ctx.fillRect(0, 0, 256, 256);
    ctx.fillStyle = c2;

    if (type === 'brick') {
      for (let y = 0; y < 256; y += 32) {
        ctx.fillRect(0, y, 256, 2);
        const offset = (y / 32) % 2 === 0 ? 0 : 32;
        for (let x = offset; x < 256; x += 64) {
          ctx.fillRect(x, y, 2, 32);
        }
      }
    } else if (type === 'tile') {
      for (let i = 0; i < 256; i += 64) {
        ctx.fillRect(i, 0, 2, 256);
        ctx.fillRect(0, i, 256, 2);
      }
    } else {
      for (let i = 0; i < 500; i++) {
        const x = Math.random() * 256;
        const y = Math.random() * 256;
        const r = Math.random() * 3 + 1;
        ctx.beginPath();
        ctx.arc(x, y, r, 0, Math.PI * 2);
        ctx.fill();
      }
    }

    const texture = new THREE.CanvasTexture(canvas);
    texture.wrapS = THREE.RepeatWrapping;
    texture.wrapT = THREE.RepeatWrapping;
    return texture;
  }

  async loadDataFiles() {
    try {
      const texRes = await fetch('bin/textures.dat');
      if (texRes.ok) {
        const texText = await texRes.text();
        const { textureMap } = TexturesDatParser.parse(texText);
        this.textureMap = textureMap;
      }

      const objRes = await fetch('bin/Objects.dat');
      if (objRes.ok) {
        const objText = await objRes.text();
        const { objectsByName } = ObjectsDatParser.parse(objText);
        this.objectsByName = objectsByName;
      }
    } catch (e) {
      console.warn('Error loading Objects.dat or textures.dat:', e);
    }
  }

  getMaterialForTextureAlias(texAlias) {
    if (this.materialCache.has(texAlias)) {
      return this.materialCache.get(texAlias);
    }

    let texture = this.proceduralTextures.get('default');
    const aliasLower = (texAlias || '').toLowerCase();

    if (aliasLower.includes('wall') || aliasLower.includes('brick')) {
      texture = this.proceduralTextures.get('wall');
    } else if (aliasLower.includes('floor') || aliasLower.includes('tile')) {
      texture = this.proceduralTextures.get('floor');
    } else if (aliasLower.includes('ceiling') || aliasLower.includes('roof')) {
      texture = this.proceduralTextures.get('ceiling');
    } else if (aliasLower.includes('wood') || aliasLower.includes('door')) {
      texture = this.proceduralTextures.get('wood');
    } else if (aliasLower.includes('metal') || aliasLower.includes('iron')) {
      texture = this.proceduralTextures.get('metal');
    }

    const mat = new THREE.MeshStandardMaterial({
      map: texture,
      roughness: 0.8,
      metalness: 0.1,
      side: THREE.DoubleSide
    });

    this.materialCache.set(texAlias, mat);
    return mat;
  }

  buildMeshFromObjectData(objData) {
    if (!objData || !objData.triangles || objData.triangles.length === 0) {
      return null;
    }

    // Group triangles by texture
    const texGroups = new Map();
    for (const tri of objData.triangles) {
      const tex = tri.texture || 'default';
      if (!texGroups.has(tex)) texGroups.set(tex, []);
      texGroups.get(tex).push(tri);
    }

    const groupMesh = new THREE.Group();

    for (const [texAlias, tris] of texGroups.entries()) {
      const positions = [];
      const uvs = [];

      for (const tri of tris) {
        for (let i = 0; i < 3; i++) {
          positions.push(...tri.positions[i]);
          uvs.push(...tri.uvs[i]);
        }
      }

      const geom = new THREE.BufferGeometry();
      geom.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));
      geom.setAttribute('uv', new THREE.Float32BufferAttribute(uvs, 2));
      geom.computeVertexNormals();

      const mat = this.getMaterialForTextureAlias(texAlias);
      const mesh = new THREE.Mesh(geom, mat);
      groupMesh.add(mesh);
    }

    return groupMesh;
  }

  clear() {
    while (this.dungeonGroup.children.length > 0) {
      const child = this.dungeonGroup.children[0];
      this.dungeonGroup.remove(child);
    }
    this.colliders = [];
  }

  buildFromMapData(mapText) {
    this.clear();

    const lines = mapText.split(/\r?\n/);
    let spawnPoint = new THREE.Vector3(0, 50, 0);
    const entities = [];

    let currentObjName = '';
    let posX = 0, posY = 0, posZ = 0;
    let rotAngle = 0;
    let extraParams = [];

    const instantiateObject = () => {
      if (!currentObjName) return;

      const objData = this.objectsByName.get(currentObjName) || this.objectsByName.get(currentObjName.toLowerCase());

      if (currentObjName.toLowerCase() === 'startpos') {
        spawnPoint.set(posX, posY + 50, posZ);
        return;
      }

      if (currentObjName.startsWith('door') || currentObjName.startsWith('!monster') || ['POTION', 'COIN', 'SPELLBOOK'].some(k => currentObjName.includes(k))) {
        entities.push({ name: currentObjName, x: posX, y: posY, z: posZ, rot: rotAngle, extra: extraParams });
        return;
      }

      if (objData) {
        const meshGroup = this.buildMeshFromObjectData(objData);
        if (meshGroup) {
          meshGroup.position.set(posX, posY, posZ);
          meshGroup.rotation.y = THREE.MathUtils.degToRad(rotAngle);

          this.dungeonGroup.add(meshGroup);

          // Add bounding box collider
          const box = new THREE.Box3().setFromObject(meshGroup);
          if (!box.isEmpty()) {
            this.colliders.push(box);
          }
        }
      } else {
        // Fallback room tile if object definition not in Objects.dat
        this.buildFallbackTile(currentObjName, posX, posY, posZ, rotAngle);
      }
    };

    for (let line of lines) {
      line = line.trim();
      if (!line || line.startsWith('//') || line.startsWith('#')) continue;

      const tokens = line.split(/\s+/);
      const cmd = tokens[0].toUpperCase();

      if (cmd === 'OBJECT') {
        instantiateObject();
        currentObjName = tokens[1] || '';
        extraParams = [];
      } else if (cmd === 'CO_ORDINATES') {
        posX = parseFloat(tokens[1]) || 0;
        posY = parseFloat(tokens[2]) || 0;
        posZ = parseFloat(tokens[3]) || 0;
      } else if (cmd === 'ROT_ANGLE') {
        rotAngle = parseFloat(tokens[1]) || 0;
        extraParams = tokens.slice(2);
      }
    }

    instantiateObject(); // Final object

    return { spawnPoint, entities };
  }

  buildFallbackTile(name, x, y, z, rotDeg) {
    const group = new THREE.Group();
    group.position.set(x, y, z);
    group.rotation.y = THREE.MathUtils.degToRad(rotDeg);

    const size = 300;
    const wallHeight = 150;

    const floorMat = this.getMaterialForTextureAlias('floor');
    const wallMat = this.getMaterialForTextureAlias('wall');
    const ceilMat = this.getMaterialForTextureAlias('ceiling');

    // Floor
    const floor = new THREE.Mesh(new THREE.PlaneGeometry(size, size), floorMat);
    floor.rotation.x = -Math.PI / 2;
    group.add(floor);

    // Ceiling
    const ceil = new THREE.Mesh(new THREE.PlaneGeometry(size, size), ceilMat);
    ceil.position.y = wallHeight;
    ceil.rotation.x = Math.PI / 2;
    group.add(ceil);

    // Back Wall
    const wallBack = new THREE.Mesh(new THREE.PlaneGeometry(size, wallHeight), wallMat);
    wallBack.position.set(0, wallHeight / 2, -size / 2);
    group.add(wallBack);

    // Left Wall
    const wallLeft = new THREE.Mesh(new THREE.PlaneGeometry(size, wallHeight), wallMat);
    wallLeft.position.set(-size / 2, wallHeight / 2, 0);
    wallLeft.rotation.y = Math.PI / 2;
    group.add(wallLeft);

    // Right Wall
    const wallRight = new THREE.Mesh(new THREE.PlaneGeometry(size, wallHeight), wallMat);
    wallRight.position.set(size / 2, wallHeight / 2, 0);
    wallRight.rotation.y = -Math.PI / 2;
    group.add(wallRight);

    this.dungeonGroup.add(group);

    const box = new THREE.Box3().setFromObject(group);
    if (!box.isEmpty()) {
      this.colliders.push(box);
    }
  }

  buildDungeonTile(type, x, y, z, rotDeg) {
    this.buildFallbackTile(type, x, y, z, rotDeg);
  }
}

export class DungeonGeneratorJS {
  static generate(seed = 1234, targetTileCount = 100) {
    const placed = [];
    const entities = [];
    const tileSize = 300;

    let spawnPoint = new THREE.Vector3(0, 50, 0);

    placed.push({ name: 'ROOM2', x: 0, y: 0, z: 0, rot: 0 });

    const directions = [
      { dx: 1, dz: 0, rot: 90 },
      { dx: -1, dz: 0, rot: 270 },
      { dx: 0, dz: 1, rot: 0 },
      { dx: 0, dz: -1, rot: 180 }
    ];

    const occupied = new Set(['0,0']);
    const tileTypes = ['ROOM2', 'CORRIDOR01', 'WALL01'];

    let currX = 0, currZ = 0;

    for (let i = 1; i < targetTileCount; i++) {
      const dir = directions[Math.floor(Math.random() * directions.length)];
      currX += dir.dx;
      currZ += dir.dz;

      const key = `${currX},${currZ}`;
      if (!occupied.has(key)) {
        occupied.add(key);
        const tileName = tileTypes[Math.floor(Math.random() * tileTypes.length)];
        const posX = currX * tileSize;
        const posZ = currZ * tileSize;

        placed.push({ name: tileName, x: posX, y: 0, z: posZ, rot: dir.rot });

        if (Math.random() < 0.2) {
          entities.push({ name: 'door55', x: posX, y: 0, z: posZ, rot: dir.rot });
        }
        if (Math.random() < 0.3) {
          entities.push({ name: '!monster1', x: posX, y: 50, z: posZ, rot: 0, extra: ['GOBLIN'] });
        }
        if (Math.random() < 0.25) {
          entities.push({ name: 'POTION', x: posX, y: 20, z: posZ, rot: 0 });
        }
      }
    }

    return { spawnPoint, placed, entities };
  }
}
