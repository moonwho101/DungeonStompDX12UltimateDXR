// Parser for textures.dat, Objects.dat, and level1.map for Dungeon Stomp Three.js

export class TextureManager {
    constructor() {
        this.aliases = new Map(); // alias_name -> relative path from Textures/
    }

    async loadTexturesDat(url) {
        try {
            const resp = await fetch(url);
            if (!resp.ok) return;
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

export class ObjectsDatParser {
    constructor() {
        this.objects = new Map(); // object_id -> { id, name, polys: [] }
    }

    async parse(url) {
        const resp = await fetch(url);
        if (!resp.ok) throw new Error(`HTTP error ${resp.status} loading ${url}`);
        const text = await resp.text();
        const tokens = text.trim().split(/\s+/);

        let i = 0;
        let currentObject = null;
        let currentTexture = 'wall1';

        while (i < tokens.length) {
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

            if (tok === 'TEXTURE') {
                i++;
                currentTexture = tokens[i++];
                continue;
            }

            if (tok === 'TYPE' || tok === 'SCALE' || tok === 'SHADOW' || tok === 'CONNECTION') {
                i++;
                i++; // Skip key and value or parameter
                continue;
            }

            if (tok === 'TRI') {
                i++;
                const verts = [];
                const triUvs = [
                    { u: 0, v: 0 },
                    { u: 0, v: 1 },
                    { u: 1, v: 0 }
                ];
                for (let v = 0; v < 3; v++) {
                    const x = parseFloat(tokens[i++]);
                    const y = parseFloat(tokens[i++]);
                    const z = parseFloat(tokens[i++]);
                    verts.push({ x, y, z, u: triUvs[v].u, v: triUvs[v].v });
                }
                currentObject.polys.push({
                    texture: currentTexture,
                    verts: verts
                });
                continue;
            }

            if (tok === 'QUAD') {
                i++;
                const qv = [];
                const quadUvs = [
                    { u: 0, v: 0 },
                    { u: 0, v: 1 },
                    { u: 1, v: 1 },
                    { u: 1, v: 0 }
                ];
                for (let v = 0; v < 4; v++) {
                    const x = parseFloat(tokens[i++]);
                    const y = parseFloat(tokens[i++]);
                    const z = parseFloat(tokens[i++]);
                    qv.push({ x, y, z, u: quadUvs[v].u, v: quadUvs[v].v });
                }
                const verts = [
                    qv[0], qv[1], qv[3],
                    qv[2], qv[3], qv[1]
                ];
                currentObject.polys.push({
                    texture: currentTexture,
                    verts: verts
                });
                continue;
            }

            if (tok === 'TRITEX') {
                i++;
                const verts = [];
                for (let v = 0; v < 3; v++) {
                    const x = parseFloat(tokens[i++]);
                    const y = parseFloat(tokens[i++]);
                    const z = parseFloat(tokens[i++]);
                    const u = parseFloat(tokens[i++]);
                    const vt = parseFloat(tokens[i++]);
                    verts.push({ x, y, z, u, v: 1.0 - vt });
                }
                currentObject.polys.push({
                    texture: currentTexture,
                    verts: verts
                });
                continue;
            }

            if (tok === 'QUADTEX') {
                i++;
                const qv = [];
                for (let v = 0; v < 4; v++) {
                    const x = parseFloat(tokens[i++]);
                    const y = parseFloat(tokens[i++]);
                    const z = parseFloat(tokens[i++]);
                    const u = parseFloat(tokens[i++]);
                    const vt = parseFloat(tokens[i++]);
                    qv.push({ x, y, z, u, v: 1.0 - vt });
                }
                const verts = [
                    qv[0], qv[1], qv[3],
                    qv[2], qv[3], qv[1]
                ];
                currentObject.polys.push({
                    texture: currentTexture,
                    verts: verts
                });
                continue;
            }

            i++;
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

            for (let line of lines) {
                line = line.trim();
                if (!line || line.startsWith('FILENAME') || line.startsWith('END_FILE') || line.startsWith('//')) {
                    continue;
                }

                const tokens = line.split(/\s+/);
                if (tokens.length < 5) continue;

                const typeCmd = tokens[0].toUpperCase();
                if (typeCmd !== 'PLAYER' && typeCmd !== 'YOURGUN') continue;

                let format = '';
                for (let j = 1; j < Math.min(5, tokens.length); j++) {
                    const tokUpper = tokens[j].toUpperCase();
                    if (tokUpper === '3DS' || tokUpper === 'MD2') {
                        format = tokUpper;
                        break;
                    }
                }

                let modelId = -1;
                let filePath = '';
                const modelIdIdx = tokens.findIndex(t => t.toUpperCase() === 'MODEL_ID');
                if (modelIdIdx !== -1 && modelIdIdx + 2 < tokens.length) {
                    modelId = parseInt(tokens[modelIdIdx + 1], 10);
                    filePath = tokens[modelIdIdx + 2];
                }

                let scale = 1.0;
                const scaleIdx = tokens.findIndex(t => t.toUpperCase() === 'SCALE');
                if (scaleIdx !== -1 && scaleIdx + 1 < tokens.length) {
                    scale = parseFloat(tokens[scaleIdx + 1]) || 1.0;
                }

                let textureAlias = '';
                const texIdx = tokens.findIndex(t => t.toUpperCase() === 'TEX');
                if (texIdx !== -1 && texIdx + 1 < tokens.length) {
                    textureAlias = tokens[texIdx + 1];
                }

                let name = '';
                let monsterWeapon = 'NONE';
                if (typeCmd === 'PLAYER') {
                    const thacoIdx = tokens.findIndex(t => t.toUpperCase() === 'THACO');
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
                    monsterWeapon: monsterWeapon
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
        const lines = text.split(/\r?\n/);

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
