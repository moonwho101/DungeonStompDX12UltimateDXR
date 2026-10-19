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
                for (let v = 0; v < 3; v++) {
                    const x = parseFloat(tokens[i++]);
                    const y = parseFloat(tokens[i++]);
                    const z = parseFloat(tokens[i++]);
                    verts.push({ x, y, z, u: 0, v: 0 });
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
                for (let v = 0; v < 4; v++) {
                    const x = parseFloat(tokens[i++]);
                    const y = parseFloat(tokens[i++]);
                    const z = parseFloat(tokens[i++]);
                    qv.push({ x, y, z, u: 0, v: 0 });
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
                    verts.push({ x, y, z, u, v: vt });
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
                    qv.push({ x, y, z, u, v: vt });
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
                    shadow: 1
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
