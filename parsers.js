// Parser for textures.dat, Objects.dat, and level1.map for Dungeon Stomp Three.js

export class TextureManager {
    constructor() {
        this.aliases = new Map(); // alias_name -> relative path from Textures/
    }

    async loadTexturesDat(url) {
        try {
            const resp = await fetch(url);
            if (!resp.ok) {
                console.warn(`HTTP error ${resp.status} loading ${url}`);
                return;
            }
            const text = await resp.text();
            const lines = text.split(/\r?\n/);

            let currentTexPath = '';
            for (let line of lines) {
                line = line.trim();
                if (!line || line.startsWith('//')) continue;

                const parts = line.split(/\s+/);
                if (parts[0] === 'AddTexture') {
                    currentTexPath = parts[1] || '';
                    // Clean path Windows separators and leading ..\
                    currentTexPath = currentTexPath.replace(/\\/g, '/');
                    if (currentTexPath.toLowerCase().startsWith('../textures/')) {
                        currentTexPath = currentTexPath.substring(12);
                    } else if (currentTexPath.toLowerCase().startsWith('textures/')) {
                        currentTexPath = currentTexPath.substring(9);
                    }
                } else if (parts[0] === 'Alias') {
                    // Alias <id> <alias_name> <blend> ...
                    const aliasName = parts[2];
                    if (aliasName) {
                        this.aliases.set(aliasName.toLowerCase(), currentTexPath);
                    }
                }
            }
        } catch (e) {
            console.error('Failed to load textures.dat:', e);
        }
    }

    getTexturePath(alias) {
        if (!alias) return null;
        const cleanAlias = alias.toLowerCase().trim();
        if (this.aliases.has(cleanAlias)) {
            return this.aliases.get(cleanAlias);
        }
        return cleanAlias + '.bmp';
    }
}

const TRI_UVS = [0, 0, 0, 1, 1, 0];
const QUAD_UVS = [0, 0, 0, 1, 1, 1, 1, 0];
// A quad is emitted as two triangles: corners (0,1,3) and (2,3,1)
const QUAD_ORDER = [0, 1, 3, 2, 3, 1];

export const OBJECT_VERT_STRIDE = 5; // x, y, z, u, v

export class ObjectsDatParser {
    constructor() {
        // object_id -> { id, name, polys: [{ texture, verts }] }
        // `verts` is a flat Float32Array of triangle-list vertices (x, y, z, u, v), a multiple of 3 vertices long.
        this.objects = new Map();
    }

    async parse(url) {
        const resp = await fetch(url);
        if (!resp.ok) throw new Error(`HTTP error ${resp.status} loading ${url}`);
        const text = await resp.text();
        const tokens = text.trim().split(/\s+/);
        const count = tokens.length;
        this.objects.clear();

        let i = 0;
        let currentObject = null;
        let currentTexture = 'wall1';

        const addPoly = (corners, order) => {
            const verts = new Float32Array(order.length * OBJECT_VERT_STRIDE);
            for (let k = 0; k < order.length; k++) {
                const src = order[k] * OBJECT_VERT_STRIDE;
                const dst = k * OBJECT_VERT_STRIDE;
                for (let c = 0; c < OBJECT_VERT_STRIDE; c++) verts[dst + c] = corners[src + c];
            }
            currentObject.polys.push({ texture: currentTexture, verts });
        };

        // Reads `n` corners; each has xyz and either given uv (textured) or the preset uv table
        const readCorners = (n, textured, presetUvs) => {
            const corners = new Array(n * OBJECT_VERT_STRIDE);
            for (let v = 0; v < n; v++) {
                const o = v * OBJECT_VERT_STRIDE;
                corners[o] = parseFloat(tokens[i++]);
                corners[o + 1] = parseFloat(tokens[i++]);
                corners[o + 2] = parseFloat(tokens[i++]);
                if (textured) {
                    corners[o + 3] = parseFloat(tokens[i++]);
                    corners[o + 4] = 1.0 - parseFloat(tokens[i++]);
                } else {
                    corners[o + 3] = presetUvs[v * 2];
                    corners[o + 4] = presetUvs[v * 2 + 1];
                }
            }
            return corners;
        };

        while (i < count) {
            const tok = tokens[i];

            if (tok === 'OBJECT' || tok === 'Q2M_OBJECT') {
                i++;
                const objId = parseInt(tokens[i++], 10);
                const objName = tokens[i++];
                currentObject = {
                    id: objId,
                    name: objName,
                    polys: []
                };
                this.objects.set(objId, currentObject);
                continue;
            }

            if (!currentObject) {
                i++;
                continue;
            }

            switch (tok) {
                case 'TEXTURE':
                    i++;
                    currentTexture = tokens[i++];
                    break;
                case 'TYPE':
                case 'SCALE':
                case 'SHADOW':
                case 'CONNECTION':
                    i += 2; // keyword and its single parameter
                    break;
                case 'TRI':
                    i++;
                    addPoly(readCorners(3, false, TRI_UVS), [0, 1, 2]);
                    break;
                case 'QUAD':
                    i++;
                    addPoly(readCorners(4, false, QUAD_UVS), QUAD_ORDER);
                    break;
                case 'TRITEX':
                    i++;
                    addPoly(readCorners(3, true), [0, 1, 2]);
                    break;
                case 'QUADTEX':
                    i++;
                    addPoly(readCorners(4, true), QUAD_ORDER);
                    break;
                default:
                    i++;
            }
        }

        return this.objects;
    }
}

export class ModelListParser {
    constructor() {
        this.modelsByName = new Map();
        this.modelsById = new Map();
    }

    async parse(url) {
        try {
            const resp = await fetch(url);
            if (!resp.ok) throw new Error(`HTTP error ${resp.status} loading ${url}`);
            const text = await resp.text();
            const lines = text.split(/\r?\n/);
            this.modelsByName.clear();
            this.modelsById.clear();

            for (let line of lines) {
                line = line.trim();
                if (!line || line.startsWith('FILENAME') || line.startsWith('END_FILE') || line.startsWith('//')) {
                    continue;
                }

                const tokens = line.split(/\s+/);
                if (tokens.length < 5) continue;

                const typeCmd = tokens[0].toUpperCase();
                if (typeCmd !== 'PLAYER' && typeCmd !== 'YOURGUN') continue;

                // Index the keywords once (first occurrence wins) instead of scanning the line per keyword
                const upper = tokens.map(t => t.toUpperCase());
                const idxOf = (kw) => upper.indexOf(kw);

                let format = '';
                for (let j = 1; j < Math.min(5, tokens.length); j++) {
                    if (upper[j] === '3DS' || upper[j] === 'MD2') {
                        format = upper[j];
                        break;
                    }
                }

                let modelId = -1;
                let filePath = '';
                const modelIdIdx = idxOf('MODEL_ID');
                if (modelIdIdx !== -1 && modelIdIdx + 2 < tokens.length) {
                    modelId = parseInt(tokens[modelIdIdx + 1], 10);
                    filePath = tokens[modelIdIdx + 2];
                }

                let scale = 1.0;
                const scaleIdx = idxOf('SCALE');
                if (scaleIdx !== -1 && scaleIdx + 1 < tokens.length) {
                    scale = parseFloat(tokens[scaleIdx + 1]) || 1.0;
                }

                let textureAlias = '';
                const texIdx = idxOf('TEX');
                if (texIdx !== -1 && texIdx + 1 < tokens.length) {
                    textureAlias = tokens[texIdx + 1];
                }

                let name = '';
                let monsterWeapon = 'NONE';
                if (typeCmd === 'PLAYER') {
                    const thacoIdx = idxOf('THACO');
                    if (thacoIdx !== -1 && thacoIdx + 2 < tokens.length) {
                        name = tokens[thacoIdx + 2];
                    }
                    if (thacoIdx !== -1 && thacoIdx + 4 < tokens.length) {
                        monsterWeapon = tokens[thacoIdx + 4];
                    }
                } else if (typeCmd === 'YOURGUN') {
                    const dIdx = tokens.lastIndexOf('D');
                    if (dIdx !== -1 && dIdx + 2 < tokens.length) {
                        name = tokens[dIdx + 2];
                    }
                }

                if (!name && modelId !== -1) {
                    name = `model_${modelId}`;
                }

                let cleanPath = filePath.replace(/\\/g, '/');
                if (cleanPath.startsWith('../')) {
                    cleanPath = cleanPath.substring(3);
                }

                const modelDef = {
                    id: modelId,
                    name: name,
                    format: format,
                    filepath: cleanPath,
                    rawFilepath: filePath,
                    scale: scale,
                    textureAlias: textureAlias,
                    monsterWeapon: monsterWeapon,
                    type: typeCmd
                };

                if (name) {
                    this.modelsByName.set(name.toLowerCase(), modelDef);
                }
                if (modelId !== -1) {
                    this.modelsById.set(modelId, modelDef);
                }
            }
        } catch (e) {
            console.error('Failed to load or parse model list:', e);
        }
        return {
            byName: this.modelsByName,
            byId: this.modelsById
        };
    }

    getModelByName(name) {
        if (!name) return null;
        return this.modelsByName.get(name.toLowerCase().trim()) || null;
    }

    getModelById(id) {
        return this.modelsById.get(id) || null;
    }
}

export class LevelMapParser {
    constructor() {
        this.instances = [];
        this.startPos = { x: 0, y: 100, z: 0, rot: 0 };
        this.lights = [];
        this.monsters = [];
    }

    async parse(url) {
        const resp = await fetch(url);
        if (!resp.ok) throw new Error(`HTTP error ${resp.status} loading ${url}`);
        const text = await resp.text();
        return this.parseText(text);
    }

    parseText(text) {
        const lines = text.split(/\r?\n/);

        this.instances = [];
        this.lights = [];
        let currentObj = null;

        for (let line of lines) {
            line = line.trim();
            if (!line || line.startsWith('//')) continue;

            const tokens = line.split(/\s+/);
            const cmd = tokens[0];

            if (cmd === 'OBJECT') {
                const name = tokens[1] || '';
                currentObj = {
                    name: name,
                    x: 0, y: 0, z: 0,
                    rot: 0,
                    shadow: 1,
                    modelName: '',
                    textureAlias: '',
                    monsterId: 0,
                    ability: 0
                };
                this.instances.push(currentObj);

                if (name.toLowerCase() === 'startpos') {
                    currentObj.isStartPos = true;
                } else if (name.toLowerCase().includes('light')) {
                    currentObj.isLight = true;
                }
            } else if (currentObj && cmd === 'CO_ORDINATES') {
                currentObj.x = parseFloat(tokens[1]) || 0;
                currentObj.y = (parseFloat(tokens[2]) || 0) + 28.0;
                currentObj.z = parseFloat(tokens[3]) || 0;

                if (currentObj.isStartPos) {
                    this.startPos.x = currentObj.x;
                    this.startPos.y = currentObj.y + 100.0;
                    this.startPos.z = currentObj.z;
                }
            } else if (currentObj && cmd === 'ROT_ANGLE') {
                currentObj.rot = parseFloat(tokens[1]) || 0;
                if (currentObj.isStartPos) {
                    this.startPos.rot = currentObj.rot;
                }
                if (tokens.length >= 3) {
                    currentObj.modelName = tokens[2] || '';
                }
                if (tokens.length >= 4) {
                    currentObj.textureAlias = tokens[3] || '';
                }
                if (tokens.length >= 5) {
                    currentObj.monsterId = parseInt(tokens[4], 10) || 0;
                }
                if (tokens.length >= 6) {
                    currentObj.ability = parseInt(tokens[5], 10) || 0;
                }
            } else if (currentObj && cmd === 'LIGHT_SOURCE') {
                // LIGHT_SOURCE <flicker|Spotlight> POS x y z DIR x y z COLOUR r g b
                const num = (i) => parseFloat(tokens[i]) || 0;
                const posIdx = tokens.indexOf('POS');
                const dirIdx = tokens.indexOf('DIR');
                const colIdx = tokens.indexOf('COLOUR');
                const hasPos = posIdx >= 0;
                const light = {
                    type: (tokens[1] || 'flicker').toLowerCase() === 'spotlight' ? 'spotlight' : 'flicker',
                    x: hasPos ? num(posIdx + 1) : currentObj.x,
                    y: hasPos ? num(posIdx + 2) + 28.0 : currentObj.y,
                    z: hasPos ? num(posIdx + 3) : currentObj.z,
                    dx: dirIdx >= 0 ? num(dirIdx + 1) : 0,
                    dy: dirIdx >= 0 ? num(dirIdx + 2) : -1,
                    dz: dirIdx >= 0 ? num(dirIdx + 3) : 0,
                    r: colIdx >= 0 ? num(colIdx + 1) : 1,
                    g: colIdx >= 0 ? num(colIdx + 2) : 1,
                    b: colIdx >= 0 ? num(colIdx + 3) : 1
                };
                currentObj.light = light;
                this.lights.push(light);
            } else if (currentObj && cmd === 'SHADOW') {
                currentObj.shadow = parseInt(tokens[1], 10) || 0;
            }
        }

        return {
            instances: this.instances,
            startPos: this.startPos
        };
    }
}
