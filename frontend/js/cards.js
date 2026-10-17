const CardAssets = (() => {
    const CARD_W = 71;
    const CARD_H = 96;
    const COLS = 9;
    const ROWS = 4;

    const cardsImage = new Image();
    cardsImage.src = 'assets/cards.png';
    const backImage = new Image();
    backImage.src = 'assets/back.png';

    let ready = false;
    const waiters = [];
    let loadCount = 0;
    function onload() {
        if (++loadCount === 2) {
            ready = true;
            waiters.splice(0).forEach(fn => fn());
        }
    }
    cardsImage.onload = onload;
    backImage.onload = onload;
    cardsImage.onerror = onload;
    backImage.onerror = onload;

    function whenReady(fn) {
        if (ready) fn();
        else waiters.push(fn);
    }

    function drawCard(ctx, card, x, y, w = CARD_W, h = CARD_H) {
        const col = card.power - 6;
        const row = card.suit;
        ctx.drawImage(
            cardsImage,
            col * CARD_W, row * CARD_H, CARD_W, CARD_H,
            x, y, w, h
        );
    }

    function drawBack(ctx, x, y, w = CARD_W, h = CARD_H) {
        ctx.drawImage(backImage, x, y, w, h);
    }

    return { CARD_W, CARD_H, COLS, ROWS, whenReady, drawCard, drawBack, isReady: () => ready };
})();

function makeCard(suit, power) {
    return { suit, power };
}

const SUIT_NAMES = ['бубен', 'пик', 'червей', 'крестей'];
const RANK_NAMES = {
    6: 'шестёркой', 7: 'семёркой', 8: 'восьмёркой', 9: 'девяткой',
    10: 'десяткой', 11: 'валетом', 12: 'дамой', 13: 'королём', 14: 'тузом'
};

function cardName(card) {
    return `${RANK_NAMES[card.power] || card.power} ${SUIT_NAMES[card.suit]}`;
}

function cardToString(card) {
    return `${card.suit},${card.power};`;
}
