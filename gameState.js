// Dependency-free shared world state. Both the collision code and the game
// logic need these, so they live in a leaf module to avoid import cycles.

export const MonsterState = Object.freeze({
    IDLE: 'IDLE',
    ALERT: 'ALERT',
    CHASE: 'CHASE',
    ATTACK: 'ATTACK',
    HURT: 'HURT',
    DEATH: 'DEATH'
});

export const monsters = [];
export const doors = [];
