const FIELD = {
    W: 800,
    H: 600,
    CARD_W: 71,
    CARD_H: 96,
    GAME_Y: 220,
    DIST: 16,
};

class GameRenderer {
    constructor(canvas) {
        this.canvas = canvas;
        this.ctx = canvas.getContext('2d');
    }

    draw(session, opts = {}) {
        const { ctx } = this;
        const { W, H, CARD_W, CARD_H } = FIELD;

        ctx.clearRect(0, 0, W, H);
        ctx.fillStyle = '#006400';
        ctx.fillRect(0, 0, W, H);

        this._drawStacks(session);
        this._drawOpponent(session);
        this._drawPlayer(session, opts);
        this._drawDeck(session);
        this._drawStatus(session, opts);
        this._drawSelected(session, opts);
    }

    _stackX(i) {
        const { W, CARD_W, DIST } = FIELD;
        switch (i) {
            case 0: return W / 2 - CARD_W * 2 - DIST;
            case 1: return W / 2 - CARD_W;
            case 2: return W / 2 + DIST;
            default: return W / 2 + CARD_W + DIST * 2;
        }
    }

    _drawStacks(session) {
        const { ctx } = this;
        const { CARD_W, CARD_H, GAME_Y } = FIELD;
        for (let i = 0; i < 4; i++) {
            const x = this._stackX(i);
            const stack = session.GameStack[i];
            if (stack.length > 0) {
                const card = stack[stack.length - 1];
                if (card) CardAssets.drawCard(ctx, card, x, GAME_Y, CARD_W, CARD_H);
            } else {
                ctx.strokeStyle = '#b8860b';
                ctx.lineWidth = 2;
                ctx.strokeRect(x, GAME_Y, CARD_W, CARD_H);
            }
        }
    }

    _drawDeck(session) {
        const { ctx } = this;
        const { W, CARD_W, CARD_H, GAME_Y, DIST } = FIELD;
        const size = Number.isFinite(session.deckSize) ? session.deckSize : session.Deck.length;
        if (size === 0) return;

        const x = W - CARD_W - DIST;
        const trumpCard = session.deckTrumpCard || session.Deck[size - 1];
        if (trumpCard && Number.isInteger(trumpCard.suit)) {
            CardAssets.drawCard(ctx, trumpCard, x, GAME_Y, CARD_W, CARD_H);
        }
        if (size > 1) {
            CardAssets.drawBack(ctx, x, GAME_Y - 30, CARD_W, CARD_H);
        }
    }

    _drawOpponent(session) {
        const { ctx } = this;
        const { W, DIST } = FIELD;
        const count = session.PlayerCards[1].length;
        const { CARD_W, CARD_H } = FIELD;
        for (let i = 0; i < count * DIST; i += DIST) {
            const x = W / 2 + (DIST / 2) * count - i;
            CardAssets.drawBack(ctx, x, -20, CARD_W, CARD_H);
        }
    }

    _drawPlayer(session, opts) {
        const { ctx } = this;
        const { H, CARD_W, CARD_H, DIST } = FIELD;
        const hand = session.PlayerCards[0];
        const page = opts.page || 0;
        const start = 9 * page;
        const end = Math.min(start + 9, hand.length);
        const y = H - CARD_H - DIST;
        for (let i = start; i < end; i++) {
            if (i === opts.selectedCard) continue;
            const x = DIST + (i % 9) * (DIST + CARD_W);
            CardAssets.drawCard(ctx, hand[i], x, y, CARD_W, CARD_H);
        }
    }

    _drawSelected(session, opts) {
        if (opts.selectedCard == null || opts.selectedCard === -1 || !opts.cursor) return;
        const hand = session.PlayerCards[0];
        const card = hand[opts.selectedCard];
        if (!card) return;
        const { CARD_W, CARD_H } = FIELD;
        CardAssets.drawCard(
            this.ctx, card,
            opts.cursor.x - CARD_W / 2,
            opts.cursor.y - CARD_H / 2,
            CARD_W, CARD_H
        );
    }

    _drawStatus(session) {
        const { ctx } = this;
        const { W } = FIELD;
        ctx.save();
        ctx.font = 'bold 14px "Segoe UI", sans-serif';
        ctx.textBaseline = 'top';

        const trump = Number.isInteger(session.Trump) ? session.Trump : 0;
        const suitSym = ['♦', '♠', '♥', '♣'][trump] || '—';
        const suitRed = trump === 0 || trump === 2;
        const deckSize = Number.isFinite(session.deckSize) ? session.deckSize : session.Deck.length;
        ctx.fillStyle = '#ffffff';
        ctx.fillText('Колода: ' + deckSize, 12, 12);
        ctx.fillText('Козырь: ', 12, 34);
        const tw = ctx.measureText('Козырь: ').width;
        ctx.fillStyle = suitRed ? '#ff6b6b' : '#ffffff';
        ctx.fillText(suitSym, 12 + tw, 34);

        if (typeof session.isAttackerTurn === 'function') {
            ctx.fillStyle = '#ffe082';
            const who = session.isAttackerTurn()
                ? (session.CurrPlayerMove === 0 ? 'Вы атакуете' : 'Соперник атакует')
                : (session.CurrPlayerMove === 0 ? 'Вы отбиваетесь' : 'Соперник отбивается');
            ctx.textAlign = 'right';
            ctx.fillText(who, W - 12, 12);
            ctx.textAlign = 'left';
        }

        if (session.PlayerCards[1].length > 0) {
            ctx.fillStyle = 'rgba(255,255,255,.85)';
            ctx.fillText('Карт у соперника: ' + session.PlayerCards[1].length, W / 2 - 70, 4);
        }
        ctx.restore();
    }
}
