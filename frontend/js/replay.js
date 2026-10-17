const Replay = (() => {
    let renderer = null;
    let canvas = null;
    let rec = null;
    let index = 0;

    const el = {};

    function init() {
        canvas = document.getElementById('replayCanvas');
        renderer = new GameRenderer(canvas);
        el.msg = document.getElementById('replayMsg');
        el.players = document.getElementById('replayPlayers');
        document.getElementById('btnPrevMove').addEventListener('click', () => step(-1));
        document.getElementById('btnNextMove').addEventListener('click', () => step(1));
        document.getElementById('btnReplayBack').addEventListener('click', () => App.showScreen('menu'));
    }

    function open(data) {
        CardAssets.whenReady(() => {
            rec = data;
            index = 0;
            el.players.textContent = `${rec.p0}  vs  ${rec.p1}`;
            draw();
        });
    }

    function step(dir) {
        const next = index + dir;
        if (next < 0 || next >= rec.states.length) return;
        index = next;
        draw();
    }

    function stateToSession(state) {
        const s = Object.create(GameSession.prototype);
        s.PlayerCards = JSON.parse(JSON.stringify(state.PlayerCards));
        s.Deck = JSON.parse(JSON.stringify(state.Deck));
        s.GameStack = JSON.parse(JSON.stringify(state.GameStack));
        s.Trump = state.Trump;
        s.CurrPlayerMove = state.CurrPlayerMove;
        s.CurrPlayerAttacker = state.CurrPlayerAttacker;
        s.Moves = [];
        return s;
    }

    function draw() {
        const state = rec.states[index];
        const session = stateToSession(state);
        renderer.draw(session, { replay: true });
        el.msg.textContent = rec.labels[index] || '';
        document.getElementById('btnPrevMove').disabled = index <= 0;
        document.getElementById('btnNextMove').disabled = index >= rec.states.length - 1;
    }

    return { init, open };
})();
