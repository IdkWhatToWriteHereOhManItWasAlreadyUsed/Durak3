class ServerSession {
    constructor(state, myId) {
        const role = playerRole(state, myId);
        const hand = Array.isArray(state.yourHand) ? state.yourHand.slice() : [];
        const hidden = Number.isFinite(state.hiddenOpponentCards)
            ? Math.max(0, state.hiddenOpponentCards)
            : 0;

        this.gameId = state.gameId || '';
        this.status = state.status || 'waiting';
        this.players = Array.isArray(state.players) ? state.players.slice() : [];
        this.currentPlayerId = state.currentPlayerId;
        this.attackerId = state.attackerId;
        this.defenderId = state.defenderId;
        this.movesCount = state.movesCount || 0;

        this.PlayerCards = [hand, new Array(hidden).fill(null)];
        this.Deck = buildDeckPile(state.deckSize, state.deckTrumpCard);
        this.GameStack = normalizeTable(state.table);
        this.Trump = Number.isInteger(state.trump) ? state.trump : 0;

        this.CurrPlayerMove = state.currentPlayerId === myId ? 0 : 1;
        this.CurrPlayerAttacker = state.attackerId === myId ? 0 : 1;

        this.deckSize = this.Deck.length;
        this.deckTrumpCard = state.deckTrumpCard || (this.Deck.length ? this.Deck[this.Deck.length - 1] : null);
        this.hiddenOpponentCards = hidden;

        this.myRole = role;
        this.myId = myId;
    }

    isAttackerTurn() {
        return this.CurrPlayerAttacker === this.CurrPlayerMove;
    }

    canBeat(attacker, target) {
        if (!attacker || !target) return false;
        if (attacker.suit === this.Trump) {
            if (target.suit === this.Trump) return attacker.power > target.power;
            return true;
        }
        if (attacker.suit === target.suit) return attacker.power > target.power;
        return false;
    }

    canPush(card, stack) {
        if (!stack) return false;
        if (!this.isAttackerTurn()) {
            return stack.length % 2 === 1 && this.canBeat(card, stack[stack.length - 1]);
        }
        if (stack.length !== 0) return false;

        const tableCards = this.GameStack.reduce((all, s) => all.concat(s), []);
        if (tableCards.length === 0) return true;
        return tableCards.some(c => c.power === card.power);
    }

    allCardsBeaten() {
        return this.GameStack.every(s => s.length % 2 === 0);
    }

    allStacksEmpty() {
        return this.GameStack.every(s => s.length === 0);
    }

    totalTableCards() {
        return this.GameStack.reduce((total, s) => total + s.length, 0);
    }

    handSizeForViewer() {
        return this.PlayerCards[0].length;
    }
}

function playerRole(state, myId) {
    if (!state || !Array.isArray(state.players)) return 'player';
    const me = state.players.find(p => p.id === myId);
    if (!me) return 'spectator';
    if (state.attackerId === myId) return 'attacker';
    if (state.defenderId === myId) return 'defender';
    return 'player';
}

function buildDeckPile(deckSize, trumpCard) {
    const size = Math.max(0, Number(deckSize) || 0);
    const deck = new Array(size).fill(null);
    if (size > 0) deck[size - 1] = trumpCard || null;
    return deck;
}

function normalizeTable(table) {
    const stacks = [];
    for (let i = 0; i < 4; i++) {
        const source = Array.isArray(table) ? table[i] : null;
        stacks.push(Array.isArray(source) ? source.slice(0, 2) : []);
    }
    return stacks;
}
