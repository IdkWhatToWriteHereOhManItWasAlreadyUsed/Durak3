const MoveType = { mtGrab: 'mtGrab', mtTransfer: 'mtTransfer', mtWin: 'mtWin' };

function shuffle(arr) {
    for (let i = arr.length - 1; i > 0; i--) {
        const j = Math.floor(Math.random() * (i + 1));
        [arr[i], arr[j]] = [arr[j], arr[i]];
    }
    return arr;
}

class GameSession {
    constructor() {
        this.PlayerCards = [[], []];
        this.Deck = [];
        this.GameStack = [[], [], [], []];
        this.Trump = 0;
        this.CurrPlayerMove = 0;
        this.CurrPlayerAttacker = 0;
        this.Moves = [];

        this._initializeDeck();
        this._giveCards();
    }

    _initializeDeck() {
        const cards = [];
        for (let suit = 0; suit < 4; suit++)
            for (let power = 6; power <= 14; power++)
                cards.push(makeCard(suit, power));

        this.Deck = shuffle(cards);
        for (let i = 0; i < 4; i++) this.GameStack[i] = [];
    }

    _giveCards() {
        for (let i = 0; i < this.PlayerCards.length; i++) this.PlayerCards[i] = [];
        for (let i = 0; i < 6; i++) {
            for (const hand of this.PlayerCards) hand.push(this._draw());
        }
        if (this.Deck.length) this.Trump = this.Deck[this.Deck.length - 1].suit;
    }

    _draw() {
        return this.Deck.shift();
    }

    canBeat(attacker, target) {
        if (attacker.suit === this.Trump) {
            if (target.suit === this.Trump) return attacker.power > target.power;
            return true;
        }
        if (attacker.suit === target.suit) return attacker.power > target.power;
        return false;
    }

    canPush(card, stack) {
        if (this.CurrPlayerAttacker === this.CurrPlayerMove) {
            if (stack.length % 2 === 0) {
                for (const s of this.GameStack)
                    for (const c of s)
                        if (c.power === card.power) return true;
                for (const s of this.GameStack)
                    if (s.length !== 0) return false;
                return true;
            }
            return false;
        } else {
            if (stack.length % 2 === 1) {
                const top = stack[stack.length - 1];
                if (this.canBeat(card, top)) return true;
            }
            return false;
        }
    }

    grab() {
        for (const stack of this.GameStack)
            while (stack.length > 0)
                this.PlayerCards[this.CurrPlayerMove].push(stack.pop());
    }

    giveCardsAfterDefense() {
        for (let i = 0; i < 12; i++) {
            if (this.Deck.length > 0 && this.PlayerCards[this.CurrPlayerMove].length < 6)
                this.PlayerCards[this.CurrPlayerMove].push(this._draw());
            if (this.Deck.length > 0 && this.PlayerCards[this.CurrPlayerAttacker].length < 6)
                this.PlayerCards[this.CurrPlayerAttacker].push(this._draw());
        }
        for (const s of this.GameStack) s.length = 0;
    }

    allCardsBeaten() {
        return this.GameStack.every(s => s.length % 2 === 0);
    }

    allStacksEmpty() {
        return this.GameStack.every(s => s.length === 0);
    }

    doMove(card, gameStackNumber) {
        const idx = this.PlayerCards[this.CurrPlayerMove].indexOf(card);
        if (idx !== -1) this.PlayerCards[this.CurrPlayerMove].splice(idx, 1);
        this.GameStack[gameStackNumber].push(card);
    }

    getWinner() {
        for (let i = 0; i < this.PlayerCards.length; i++)
            if (this.PlayerCards[i].length === 0) return i;
        return -1;
    }

    transferMove(moveType) {
        if (moveType === MoveType.mtTransfer) {
            if (this.CurrPlayerMove === this.CurrPlayerAttacker) {
                this.CurrPlayerMove = (this.CurrPlayerMove + 1) % this.PlayerCards.length;
            } else {
                this.CurrPlayerAttacker = this.CurrPlayerMove;
            }
        } else {
            this.CurrPlayerMove = (this.CurrPlayerMove + 1) % this.PlayerCards.length;
            this.CurrPlayerAttacker = this.CurrPlayerMove;
        }
    }

    isGameFinished() {
        return this.PlayerCards.some(hand => hand.length === 0);
    }

    isAttackerTurn() {
        return this.CurrPlayerAttacker === this.CurrPlayerMove;
    }

    checkWinner() {
        if (this.Deck.length > 0) return -1;
        const w = this.getWinner();
        if (w === -1) return -1;
        return w;
    }

    totalTableCards() {
        return this.GameStack.reduce((n, s) => n + s.length, 0);
    }

    toString() {
        let s = '';
        this.PlayerCards.forEach((hand, i) => {
            s += 'PLAYER' + i + ':';
            for (const c of hand) s += cardToString(c);
        });
        s += 'DECK:';
        for (const c of this.Deck) s += cardToString(c);
        return s;
    }
}
