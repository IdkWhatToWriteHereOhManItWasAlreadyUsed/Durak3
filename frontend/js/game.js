/**
 * Игровой экран.
 *
 * Партия полностью авторитетна на сервере (game-service):
 *   GET  /api/games/{gameId}/state
 *   POST /api/games/{gameId}/moves  { type: CARD|MOVE|GRAB|WIN, card?, stack? }
 *   POST /api/games/{gameId}/resign
 *   GET  /api/games/{gameId}/result
 *
 * Клиент только готовит ход, проверяет его для UX и отправляет на сервер.
 * Обновления приходят по WebSocket (`game.state`, `game.finished`), а
 * polling остаётся страховкой при разрыве соединения.
 */
const Game = (() => {
    let renderer = null;
    let canvas = null;

    let state = null;
    let session = null;
    let gameId = null;
    let myId = null;
    let myName = 'Вы';
    let opponentName = 'Соперник';

    let page = 0;
    let selected = -1;
    let cursor = null;
    let dragging = false;
    let busy = false;
    let finished = false;
    let active = false;

    let turnTimer = null;
    let turnSeconds = 0;
    let turnKey = '';
    let pollTimer = null;
    let pollInFlight = false;
    let winSent = false;
    let gen = 0;

    const el = {};

    function init() {
        canvas = document.getElementById('gameCanvas');
        renderer = new GameRenderer(canvas);

        el.info = document.getElementById('lblInfo');
        el.time = document.getElementById('moveTime');
        el.pageLabel = document.getElementById('pageLabel');
        el.grab = document.getElementById('btnGrab');
        el.move = document.getElementById('btnMove');
        el.resign = document.getElementById('btnResign');

        canvas.addEventListener('pointerdown', onPointerDown);
        canvas.addEventListener('pointermove', onPointerMove);
        window.addEventListener('pointerup', onPointerUp);
        canvas.addEventListener('pointerleave', () => {
            if (dragging) { dragging = false; refresh(); }
        });

        el.grab.addEventListener('click', onGrab);
        el.move.addEventListener('click', onMoveTransfer);
        if (el.resign) el.resign.addEventListener('click', onResign);
        document.getElementById('btnDecPage').addEventListener('click', () => switchPage(-1));
        document.getElementById('btnIncPage').addEventListener('click', () => switchPage(1));
        document.getElementById('btnLeave').addEventListener('click', onLeave);

        Realtime.on('game.state', onRealtimeState);
        Realtime.on('game.finished', onRealtimeFinished);
        Realtime.on('connected', () => {
            if (active && !finished) loadState(false);
        });
    }

    /* ----------------------------- lifecycle ------------------------------ */

    function start(opts = {}) {
        if (!opts.gameId) {
            App.toast('Не передан gameId', 'error');
            App.goMenu();
            return;
        }

        const myGen = ++gen;
        gameId = opts.gameId;
        myId = opts.myId != null ? opts.myId : Auth.id();
        myName = (Auth.user() && Auth.user().username) || 'Вы';
        opponentName = opts.opponentName || 'Соперник';

        page = 0;
        selected = -1;
        cursor = null;
        dragging = false;
        busy = false;
        finished = false;
        active = true;
        winSent = false;
        turnKey = '';
        state = null;
        session = null;

        stopTurnTimer();
        stopPolling();
        setStatus('Загрузка партии…');
        if (el.time) el.time.textContent = '';
        if (el.grab) el.grab.disabled = true;
        if (el.move) el.move.disabled = true;
        if (el.resign) el.resign.disabled = true;
        if (el.pageLabel) el.pageLabel.textContent = '1 / 1';

        CardAssets.whenReady(async () => {
            if (myGen !== gen || !active) return;
            await loadState(true);
            startPolling();
        });
    }

    function stop() {
        gen++;
        active = false;
        finished = true;
        busy = true;
        stopTurnTimer();
        stopPolling();
        if (el.time) el.time.textContent = '';
    }

    function isActive() {
        return active;
    }

    function currentGameId() {
        return gameId;
    }

    /* ------------------------------ state --------------------------------- */

    async function loadState(showErrors) {
        if (!active || pollInFlight || !gameId) return;
        pollInFlight = true;
        try {
            const data = await Api.games.state(gameId);
            if (active) applyState(data);
        } catch (e) {
            if (!active) return;
            if (e.status === 404 || e.status === 403) {
                App.toast(e.status === 403 ? 'Нет доступа к партии' : 'Партия не найдена', 'error');
                App.goMenu();
            } else if (showErrors && e.status !== 401) {
                setStatus('Не удалось получить состояние: ' + e.message);
            }
        } finally {
            pollInFlight = false;
        }
    }

    function applyState(next) {
        if (!active || !next) return;
        if (next.gameId && gameId && next.gameId !== gameId) return;

        state = next;
        resolveNames(state);
        session = new ServerSession(state, myId);

        if (state.status === 'finished') {
            refresh();
            finishGame(null);
            return;
        }

        if (state.players && state.players.length) {
            const opponent = state.players.find(p => p.id !== myId);
            if (opponent && opponent.name) opponentName = opponent.name;
        }

        const maxPage = Math.max(0, Math.ceil(session.PlayerCards[0].length / 9) - 1);
        page = Math.min(page, maxPage);

        refresh();
        updateStatus();
        syncTurnTimer();
        maybeConfirmWin();
    }

    function resolveNames(data) {
        const players = Array.isArray(data.players) ? data.players : [];
        const me = players.find(p => p.id === myId);
        if (me && me.name) myName = me.name;
        const opponent = players.find(p => p.id !== myId);
        if (opponent && opponent.name) opponentName = opponent.name;
    }

    function updateStatus() {
        if (!state) return;

        if (state.status === 'waiting') {
            setStatus('Ожидание соперника…');
            return;
        }
        if (!myTurn()) {
            setStatus('Ход соперника' + (opponentName ? ' (' + opponentName + ')' : '') + '…');
            return;
        }
        if (session.isAttackerTurn()) {
            setStatus(session.allStacksEmpty()
                ? 'Ваш ход: положите карту'
                : 'Ваш ход: подкиньте карту или передайте ход');
        } else if (session.allCardsBeaten()) {
            setStatus('Отбились! Нажмите «Отбой / передать ход»');
        } else {
            setStatus('Отбивайтесь или забирайте карты');
        }
    }

    function maybeConfirmWin() {
        if (winSent || !state || state.status !== 'active') return;

        const handEmpty = !state.yourHand || state.yourHand.length === 0;
        const tableEmpty = !state.table || state.table.every(stack => !stack || stack.length === 0);
        if (!handEmpty || !tableEmpty || (state.deckSize || 0) > 0) return;

        winSent = true;
        Api.games.move(gameId, { type: 'WIN' })
            .then(data => { if (active) applyState(data); })
            .catch(() => { /* сервер сам завершит партию */ });
    }

    /* ------------------------------ render -------------------------------- */

    function refresh() {
        if (!session) return;

        const maxPage = Math.max(0, Math.ceil(session.PlayerCards[0].length / 9) - 1);
        if (page > maxPage) page = maxPage;

        renderer.draw(session, { selectedCard: selected, cursor, page });

        if (el.pageLabel) el.pageLabel.textContent = `${page + 1} / ${maxPage + 1}`;
        const prev = document.getElementById('btnDecPage');
        const next = document.getElementById('btnIncPage');
        if (prev) prev.disabled = page <= 0;
        if (next) next.disabled = page >= maxPage;

        const canAct = myTurn() && !busy && !finished;
        if (el.grab) el.grab.disabled = !canAct;
        if (el.move) el.move.disabled = !canAct;
        if (el.resign) el.resign.disabled = !active || finished;
    }

    function myTurn() {
        return !!state && active && !finished &&
            state.status === 'active' &&
            state.currentPlayerId === myId;
    }

    function setStatus(text) {
        if (el.info) el.info.textContent = text;
    }

    /* ------------------------------ pointer ------------------------------- */

    function toField(event) {
        const rect = canvas.getBoundingClientRect();
        return {
            x: (event.clientX - rect.left) * (canvas.width / rect.width),
            y: (event.clientY - rect.top) * (canvas.height / rect.height),
        };
    }

    function stackX(index) {
        const { W, CARD_W, DIST } = FIELD;
        switch (index) {
            case 0: return W / 2 - CARD_W * 2 - DIST;
            case 1: return W / 2 - CARD_W;
            case 2: return W / 2 + DIST;
            default: return W / 2 + CARD_W + DIST * 2;
        }
    }

    function getSelectedCard(x, y) {
        if (!session) return -1;
        const { H, CARD_W, CARD_H, DIST } = FIELD;
        if (y > H - DIST || y < H - DIST - CARD_H) return -1;
        for (let i = 0; i < 9; i++) {
            const left = DIST * (i + 1) + CARD_W * i;
            const right = DIST * (i + 1) + CARD_W * (i + 1);
            if (x > left && x < right && session.PlayerCards[0].length > i) {
                return i + 9 * page;
            }
        }
        return -1;
    }

    function getSelectedStack(x, y) {
        const { GAME_Y, CARD_H, CARD_W } = FIELD;
        if (y < GAME_Y || y > GAME_Y + CARD_H) return -1;
        for (let i = 0; i < 4; i++) {
            const sx = stackX(i);
            if (x > sx && x < sx + CARD_W) return i;
        }
        return -1;
    }

    function onPointerDown(event) {
        if (busy || finished || !session) return;
        if (!myTurn()) {
            setStatus('Сейчас не Ваш ход!');
            return;
        }
        const point = toField(event);
        selected = getSelectedCard(point.x, point.y);
        if (selected !== -1) {
            cursor = point;
            dragging = true;
            canvas.setPointerCapture && canvas.setPointerCapture(event.pointerId);
            refresh();
        } else {
            setStatus('Карта не выбрана');
        }
    }

    function onPointerMove(event) {
        if (!dragging || busy || finished || !myTurn()) return;
        cursor = toField(event);
        refresh();
    }

    function onPointerUp(event) {
        if (!dragging || busy || finished || !session) {
            dragging = false;
            return;
        }
        dragging = false;

        if (!myTurn()) {
            setStatus('Сейчас не Ваш ход!');
            resetSelection();
            return;
        }

        const point = toField(event);
        const stackNum = getSelectedStack(point.x, point.y);

        if (selected === -1) {
            resetSelection();
            return;
        }
        if (stackNum === -1) {
            setStatus('Положите карту в нормальное место!');
            resetSelection();
            return;
        }

        const card = session.PlayerCards[0][selected];
        if (!card) {
            resetSelection();
            return;
        }

        if (session.isAttackerTurn()) {
            if (session.canPush(card, session.GameStack[stackNum])) {
                resetSelection();
                sendMove({ type: 'CARD', card, stack: stackNum },
                    'Вы положили ' + cardName(card));
            } else {
                setStatus('Эту карту сюда положить нельзя!');
                resetSelection();
            }
        } else {
            const stack = session.GameStack[stackNum];
            const top = stack[stack.length - 1];
            if (stack.length % 2 === 1 && session.canBeat(card, top)) {
                resetSelection();
                sendMove({ type: 'CARD', card, stack: stackNum },
                    'Вы отбились ' + cardName(card));
            } else if (stack.length % 2 === 0) {
                setStatus('Здесь нет карты, которую можно отбить!');
                resetSelection();
            } else {
                setStatus('Вы не можете побить эту карту!');
                resetSelection();
            }
        }
    }

    function resetSelection() {
        selected = -1;
        cursor = null;
        refresh();
    }

    /* ------------------------------- moves -------------------------------- */

    async function sendMove(move, message) {
        if (busy || finished || !active) return;

        busy = true;
        setStatus(message);
        refresh();

        try {
            const data = await Api.games.move(gameId, move);
            if (active) applyState(data);
        } catch (e) {
            if (!active) return;
            if (e.status === 409) {
                setStatus(e.message || 'Ход отклонён сервером');
            } else if (e.status !== 401) {
                setStatus('Ошибка хода: ' + e.message);
            }
            await loadState(false);
        } finally {
            busy = false;
            if (active && !finished) refresh();
        }
    }

    function onMoveTransfer() {
        if (busy || finished || !session) return;
        if (!myTurn()) {
            setStatus('Сейчас не ваш ход!');
            return;
        }

        if (session.isAttackerTurn()) {
            if (session.allStacksEmpty()) {
                setStatus('Вы не положили ни одной карты');
                return;
            }
            sendMove({ type: 'MOVE' }, 'Вы передали ход сопернику!');
        } else {
            if (!session.allCardsBeaten()) {
                setStatus('Не все карты отбиты!');
                return;
            }
            sendMove({ type: 'MOVE' }, 'Вы отбились');
        }
    }

    function onGrab() {
        if (busy || finished || !session) return;
        if (!myTurn() || session.isAttackerTurn()) {
            setStatus('Сейчас не время таким заниматься!');
            return;
        }
        sendMove({ type: 'GRAB' }, 'Вы забрали карты');
    }

    async function onResign() {
        if (!active || finished) return;

        App.showModal({
            owner: 'game',
            title: 'Сдаться?',
            text: 'Партия будет завершена, а победа достанется сопернику.',
            yesText: 'Сдаться',
            noText: 'Продолжить',
            onYes: async () => {
                try {
                    await Api.games.resign(gameId);
                    await finishGame(null);
                } catch (e) {
                    if (e.status !== 401) App.toast('Не удалось сдаться: ' + e.message, 'error');
                }
            },
        });
    }

    function onLeave() {
        if (!active) {
            App.goMenu();
            return;
        }
        if (finished) {
            App.goMenu();
            return;
        }

        App.showModal({
            owner: 'game',
            title: 'Выйти в меню?',
            text: 'Партия останется активной — к ней можно будет вернуться.',
            yesText: 'Выйти',
            noText: 'Остаться',
            onYes: () => App.goMenu(),
        });
    }

    /* ------------------------------- timer -------------------------------- */

    function syncTurnTimer() {
        const key = state
            ? `${state.status}:${state.currentPlayerId}:${state.movesCount}`
            : '';

        if (key !== turnKey) {
            turnKey = key;
            if (myTurn()) startTurnTimer();
            else stopTurnTimer();
            return;
        }
        if (myTurn()) {
            if (!turnTimer) startTurnTimer();
        } else {
            stopTurnTimer();
        }
    }

    function startTurnTimer() {
        stopTurnTimer();
        turnSeconds = CONFIG.turnLimitSeconds;
        updateTimerLabel();
        turnTimer = setInterval(() => {
            turnSeconds--;
            updateTimerLabel();
            if (turnSeconds <= 0) {
                stopTurnTimer();
                onTurnTimeout();
            }
        }, 1000);
    }

    function stopTurnTimer() {
        clearInterval(turnTimer);
        turnTimer = null;
        if (el.time) el.time.textContent = '';
    }

    function updateTimerLabel() {
        if (!el.time) return;
        el.time.textContent = myTurn() && !finished ? '⏱ ' + turnSeconds + ' c' : '';
    }

    function onTurnTimeout() {
        if (finished || busy || !myTurn()) return;

        setStatus('Время на ход истекло!');
        if (session.isAttackerTurn()) {
            if (session.allStacksEmpty()) {
                const card = session.PlayerCards[0]
                    .filter(Boolean)
                    .sort((a, b) => a.power - b.power)[0];
                if (card) {
                    sendMove({ type: 'CARD', card, stack: 0 }, 'Время вышло: авто-ход');
                    return;
                }
            }
            sendMove({ type: 'MOVE' }, 'Время вышло: передача хода');
        } else {
            sendMove({ type: 'GRAB' }, 'Время вышло: забираем карты');
        }
    }

    /* ------------------------------ polling ------------------------------- */

    function startPolling() {
        stopPolling();
        pollTimer = setInterval(() => {
            if (!active || finished || document.hidden) return;
            loadState(false);
        }, CONFIG.gamePollMs);
    }

    function stopPolling() {
        clearInterval(pollTimer);
        pollTimer = null;
    }

    /* --------------------------- realtime events -------------------------- */

    function onRealtimeState(payload) {
        if (!active || finished) return;
        const data = payload && (payload.state || payload);
        if (!data) return;
        if (data.gameId && gameId && data.gameId !== gameId) return;

        if (data.yourHand !== undefined || data.table !== undefined || data.currentPlayerId !== undefined) {
            applyState(data);
        } else {
            loadState(false);
        }
    }

    function onRealtimeFinished(payload) {
        if (!active || finished) return;
        const result = payload && (payload.result || payload);
        if (result && result.gameId && gameId && result.gameId !== gameId) return;
        finishGame(result || null);
    }

    /* ----------------------------- game over ------------------------------ */

    async function finishGame(result) {
        if (finished || !active) return;

        finished = true;
        busy = true;
        stopTurnTimer();
        stopPolling();
        const myGen = gen;

        let res = result;
        if (!res || res.winnerId === undefined) {
            try {
                res = await Api.games.result(gameId);
            } catch (e) {
                res = res || null;
            }
        }
        if (myGen !== gen || !active) return;

        const won = res && res.winnerId != null
            ? res.winnerId === myId
            : !!(session && session.PlayerCards[0].length === 0);
        const draw = !won && res && res.winnerId == null && res.reason === 'draw';

        if (draw) {
            drawResultText('Ничья');
            await sleep(1200);
        } else if (won) {
            await animateVictory();
        } else {
            await animateDefeat();
        }

        if (myGen !== gen || !active) return;
        endScreen(draw ? null : won, res);
    }

    function endScreen(won, result) {
        const replayId = result && result.replayId;
        const summary = {
            id: replayId,
            player1: myName,
            player2: opponentName,
        };

        let title;
        let text;
        if (won === null) {
            title = 'Ничья';
            text = 'Партия завершена вничью.';
        } else if (won) {
            title = 'Победа!';
            text = 'Вы выиграли партию.' + (replayId ? ' Повтор сохранён.' : '');
        } else {
            title = 'Поражение';
            text = 'Вы проиграли. Дурак — ' + (myName || 'вы') + '.';
        }

        if (replayId) {
            App.showModal({
                owner: 'game',
                title,
                text,
                yesText: 'Смотреть повтор',
                noText: 'В меню',
                onYes: () => App.openReplay(replayId, summary),
                onNo: () => App.goMenu(),
                onClose: () => App.goMenu(),
            });
        } else {
            App.showModal({
                owner: 'game',
                title,
                text,
                showYes: false,
                closeText: 'В меню',
                onClose: () => App.goMenu(),
            });
        }
    }

    function drawResultText(text) {
        const { ctx } = renderer;
        const { W, H } = FIELD;
        ctx.fillStyle = '#006400';
        ctx.fillRect(0, 0, W, H);
        ctx.font = 'bold 72px "Times New Roman", serif';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.lineJoin = 'round';
        ctx.lineWidth = 8;
        ctx.strokeStyle = '#ffffff';
        ctx.strokeText(text, W / 2, H / 2);
        ctx.fillStyle = '#000000';
        ctx.fillText(text, W / 2, H / 2);
    }

    async function animateVictory() {
        Sound.play('text');
        await sleep(700);
        if (!active) return;
        drawResultText('You');
        await sleep(255);
        Sound.play('text');
        await sleep(500);
        if (!active) return;
        drawResultText('You Win');
        await sleep(1777);
        if (!active) return;
        await confetti(7);
    }

    async function animateDefeat() {
        const text = 'You Lose';
        drawResultText('');
        for (let i = 1; i <= text.length; i++) {
            if (!active) return;
            if (text[i - 1] !== ' ') Sound.play('text');
            await sleep(200);
            if (!active) return;
            drawResultText(text.substring(0, i));
        }
        await sleep(1800);
    }

    function confetti(seconds) {
        return new Promise(resolve => {
            const { ctx } = renderer;
            const { W, H } = FIELD;
            const background = document.createElement('canvas');
            background.width = W;
            background.height = H;
            background.getContext('2d').drawImage(canvas, 0, 0);

            const colors = ['#ff0000', '#00ff00', '#0000ff', '#ffff00', '#ff00ff',
                '#00ffff', '#ffa500', '#8a2be2', '#00008b', '#ffffff'];
            const pieces = [];
            const end = performance.now() + seconds * 1000;
            try { Sound.play('win'); } catch (e) { /* ignore */ }

            function frame(now) {
                for (let k = 0; k < 2; k++) {
                    const ix = Math.random() * W;
                    pieces.push({
                        x: ix,
                        ix,
                        y: -8,
                        size: 15 + Math.random() * 5,
                        color: colors[Math.floor(Math.random() * colors.length)],
                        speed: 5 + Math.random() * 5,
                        amp: 5 + Math.random() * 15,
                        freq: Math.random() * 0.1,
                        t: 0,
                        comp: 1,
                    });
                }
                for (const piece of pieces) {
                    piece.t += 1.5;
                    piece.y += piece.speed;
                    piece.x = piece.ix + piece.amp * Math.sin(piece.t * piece.freq);
                    if (Math.floor(Math.random() * 6) === 2) piece.comp = Math.random() * 0.8 + 0.2;
                }

                ctx.drawImage(background, 0, 0);
                for (let i = pieces.length - 1; i >= 0; i--) {
                    const piece = pieces[i];
                    if (piece.y > H) { pieces.splice(i, 1); continue; }
                    const hz = piece.size / 2;
                    const vt = (piece.size / 2) * piece.comp;
                    ctx.fillStyle = piece.color;
                    ctx.beginPath();
                    ctx.moveTo(piece.x, piece.y - vt);
                    ctx.lineTo(piece.x + hz, piece.y);
                    ctx.lineTo(piece.x, piece.y + vt);
                    ctx.lineTo(piece.x - hz, piece.y);
                    ctx.closePath();
                    ctx.fill();
                }
                if (now < end && active) requestAnimationFrame(frame);
                else resolve();
            }
            requestAnimationFrame(frame);
        });
    }

    function switchPage(direction) {
        if (!session) return;
        const maxPage = Math.max(0, Math.ceil(session.PlayerCards[0].length / 9) - 1);
        const next = page + direction;
        if (next < 0 || next > maxPage) return;
        page = next;
        selected = -1;
        cursor = null;
        refresh();
    }

    function sleep(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }

    return { init, start, stop, isActive, currentGameId };
})();
