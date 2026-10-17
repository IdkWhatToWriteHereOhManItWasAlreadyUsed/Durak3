/**
 * Просмотр повторов.
 *
 * Контракт replay-service / api-gateway:
 *   GET /api/replays/{replayId}/frames  -> { replayId, total, frames: ReplayFrame[] }
 *   GET /api/replays/{replayId}         -> полный Replay (fallback)
 *
 * Если сервер вернул только метаданные без state, кадры собираются на клиенте
 * из initialState + moves (совместимо с форматом DurakSharpServer).
 */
const Replay = (() => {
    let renderer = null;
    let canvas = null;

    let replayId = null;
    let summary = null;
    let frames = [];
    let index = 0;
    let gen = 0;

    const el = {};

    function init() {
        canvas = document.getElementById('replayCanvas');
        renderer = new GameRenderer(canvas);
        el.msg = document.getElementById('replayMsg');
        el.players = document.getElementById('replayPlayers');

        document.getElementById('btnPrevMove').addEventListener('click', () => step(-1));
        document.getElementById('btnNextMove').addEventListener('click', () => step(1));
        document.getElementById('btnReplayBack').addEventListener('click', () => App.goMenu());
    }

    async function open(idOrSummary, maybeSummary) {
        let meta = maybeSummary || null;
        if (idOrSummary && typeof idOrSummary === 'object') {
            meta = idOrSummary;
            replayId = idOrSummary.id;
        } else {
            replayId = idOrSummary;
        }

        if (!replayId) {
            App.toast('Не передан replayId', 'error');
            App.goMenu();
            return;
        }

        const myGen = ++gen;
        summary = meta;
        frames = [];
        index = 0;
        renderFrameInfo();
        setLoadingState();

        try {
            const data = await Api.replays.frames(replayId);
            if (myGen !== gen) return;

            frames = normalizeFrames(data && data.frames);
            if (!frames.length) {
                const replay = await Api.replays.get(replayId);
                if (myGen !== gen) return;
                summary = summary || {
                    id: replay.id,
                    player1: replay.player1,
                    player2: replay.player2,
                };
                frames = buildFrames(replay);
            }

            if (!frames.length) {
                throw new Error('Повтор не содержит кадров');
            }

            CardAssets.whenReady(() => {
                if (myGen !== gen) return;
                index = 0;
                draw();
            });
        } catch (e) {
            if (myGen !== gen) return;
            App.toast('Не удалось открыть повтор: ' + e.message, 'error');
            App.goMenu();
        }
    }

    function setLoadingState() {
        if (el.msg) el.msg.textContent = 'Загрузка повтора…';
        if (el.players) el.players.textContent = summary
            ? `${summary.player1 || '—'}  vs  ${summary.player2 || '—'}`
            : '';
        const prev = document.getElementById('btnPrevMove');
        const next = document.getElementById('btnNextMove');
        if (prev) prev.disabled = true;
        if (next) next.disabled = true;
    }

    function normalizeFrames(list) {
        if (!Array.isArray(list)) return [];
        return list
            .map((frame, i) => ({
                index: Number.isFinite(frame.index) ? frame.index : i,
                label: frame.label || '',
                state: normalizeState(frame.state),
            }))
            .filter(frame => frame.state)
            .sort((a, b) => a.index - b.index);
    }

    function normalizeState(state) {
        if (!state) return null;
        return {
            player1Cards: Array.isArray(state.player1Cards) ? state.player1Cards : [],
            player2Cards: Array.isArray(state.player2Cards) ? state.player2Cards : [],
            deck: Array.isArray(state.deck) ? state.deck : [],
            table: normalizeTable(state.table),
            trump: Number.isInteger(state.trump) ? state.trump : 0,
            currentPlayerIndex: clampIndex(state.currentPlayerIndex),
            attackerIndex: clampIndex(state.attackerIndex),
        };
    }

    function buildFrames(replay) {
        if (!replay || typeof GameSession === 'undefined') return [];

        const initial = replay.initialState || {};
        const players = Array.isArray(initial.players) ? initial.players : [];
        if (players.length < 2) return [];

        const session = Object.create(GameSession.prototype);
        session.PlayerCards = players.map(player => (player.cards || []).slice());
        session.Deck = (initial.deck || []).slice();
        session.GameStack = [[], [], [], []];
        session.Trump = Number.isInteger(initial.trump) ? initial.trump : 0;
        session.Moves = [];

        const firstIndex = players.findIndex(player => player.id === initial.firstAttackerId);
        session.CurrPlayerMove = firstIndex >= 0 ? firstIndex : 0;
        session.CurrPlayerAttacker = session.CurrPlayerMove;

        const result = [snapshotFrame('Начало партии', session)];
        const moves = Array.isArray(replay.moves) ? replay.moves.slice() : [];
        moves.sort((a, b) => (a.sequence || 0) - (b.sequence || 0));

        for (const move of moves) {
            const playerIndex = Math.max(0, players.findIndex(player => player.id === move.playerId));
            const playerName = (players[playerIndex] && players[playerIndex].name) || ('Игрок #' + move.playerId);

            if (move.type === 'CARD' && move.card) {
                const hand = session.PlayerCards[playerIndex] || session.PlayerCards[0];
                const cardIndex = hand.findIndex(card =>
                    card.suit === move.card.suit && card.power === move.card.power);
                if (cardIndex !== -1) hand.splice(cardIndex, 1);

                let stackIndex = Number.isInteger(move.stack) ? move.stack : -1;
                if (stackIndex < 0 || stackIndex > 3) {
                    stackIndex = session.GameStack.findIndex(stack => stack.length === 0);
                }
                if (stackIndex < 0) stackIndex = 0;
                session.GameStack[stackIndex].push(move.card);

                result.push(snapshotFrame(
                    `${playerName} кладёт ${cardName(move.card)}`,
                    session
                ));
            } else if (move.type === 'MOVE') {
                if (session.CurrPlayerMove === session.CurrPlayerAttacker) {
                    session.transferMove(MoveType.mtTransfer);
                    result.push(snapshotFrame(`${playerName} передаёт ход`, session));
                } else {
                    session.giveCardsAfterDefense();
                    session.transferMove(MoveType.mtTransfer);
                    result.push(snapshotFrame(`${playerName} отбился`, session));
                }
            } else if (move.type === 'GRAB') {
                session.grab();
                session.giveCardsAfterDefense();
                session.transferMove(MoveType.mtGrab);
                result.push(snapshotFrame(`${playerName} забирает карты`, session));
            } else if (move.type === 'WIN') {
                result.push(snapshotFrame(`${playerName} побеждает`, session));
                break;
            }
        }

        return result;
    }

    function snapshotFrame(label, session) {
        return {
            label,
            state: {
                player1Cards: session.PlayerCards[0].slice(),
                player2Cards: session.PlayerCards[1].slice(),
                deck: session.Deck.slice(),
                table: session.GameStack.map(stack => stack.slice()),
                trump: session.Trump,
                currentPlayerIndex: session.CurrPlayerMove,
                attackerIndex: session.CurrPlayerAttacker,
            },
        };
    }

    function step(direction) {
        const next = index + direction;
        if (next < 0 || next >= frames.length) return;
        index = next;
        draw();
    }

    function draw() {
        const frame = frames[index];
        if (!frame) return;

        renderer.draw(frameToSession(frame.state), { replay: true });

        if (el.msg) el.msg.textContent = frame.label || `Кадр ${index + 1}`;
        if (el.players) {
            const p1 = (summary && summary.player1) || 'Игрок 1';
            const p2 = (summary && summary.player2) || 'Игрок 2';
            el.players.textContent = `${p1}  vs  ${p2}   ·   ${index + 1} / ${frames.length}`;
        }

        const prev = document.getElementById('btnPrevMove');
        const next = document.getElementById('btnNextMove');
        if (prev) prev.disabled = index <= 0;
        if (next) next.disabled = index >= frames.length - 1;
    }

    function frameToSession(state) {
        const deck = (state.deck || []).slice();
        return {
            PlayerCards: [state.player1Cards || [], state.player2Cards || []],
            Deck: deck,
            GameStack: normalizeTable(state.table),
            Trump: Number.isInteger(state.trump) ? state.trump : 0,
            CurrPlayerMove: clampIndex(state.currentPlayerIndex),
            CurrPlayerAttacker: clampIndex(state.attackerIndex),
            deckSize: deck.length,
            deckTrumpCard: deck.length ? deck[deck.length - 1] : null,
        };
    }

    function clampIndex(value) {
        return value === 1 ? 1 : 0;
    }

    function renderFrameInfo() {
        if (el.msg) el.msg.textContent = '';
        if (el.players) {
            el.players.textContent = summary
                ? `${summary.player1 || '—'}  vs  ${summary.player2 || '—'}`
                : '';
        }
    }

    return { init, open };
})();
