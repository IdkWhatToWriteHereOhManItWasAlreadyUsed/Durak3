const Game = (() => {
    let session = null;
    let renderer = null;
    let canvas = null;
    let names = { human: 'Вы', bot: 'Соперник' };
    let page = 0;
    let selected = -1;
    let cursor = null;
    let dragging = false;
    let busy = false;
    let finished = false;
    let botPlan = [];
    let turnTimer = null;
    let turnSeconds = 0;
    let record = null;
    let gameGen = 0;

    const BOT_DELAY = 750;
    const TURN_LIMIT = 30;

    const el = {};

    function init() {
        canvas = document.getElementById('gameCanvas');
        renderer = new GameRenderer(canvas);
        el.info = document.getElementById('lblInfo');
        el.time = document.getElementById('moveTime');
        el.pageLabel = document.getElementById('pageLabel');

        canvas.addEventListener('pointerdown', onPointerDown);
        canvas.addEventListener('pointermove', onPointerMove);
        window.addEventListener('pointerup', onPointerUp);
        canvas.addEventListener('pointerleave', () => { if (dragging) { dragging = false; refresh(); } });

        document.getElementById('btnGrab').addEventListener('click', onGrab);
        document.getElementById('btnMove').addEventListener('click', onMoveTransfer);
        document.getElementById('btnDecPage').addEventListener('click', () => switchPage(-1));
        document.getElementById('btnIncPage').addEventListener('click', () => switchPage(1));
        document.getElementById('btnLeave').addEventListener('click', () => {
            stopTimers();
            App.showScreen('menu');
        });
    }

    function start(opts) {
        CardAssets.whenReady(() => {
            gameGen++;
            session = new GameSession();
            names.human = opts.playerName || 'Вы';
            names.bot = opts.opponentName || 'Соперник';
            page = 0;
            selected = -1;
            cursor = null;
            dragging = false;
            busy = false;
            finished = false;
            botPlan = [];

            if (opts.startFirst === false) {
                session.CurrPlayerMove = 1;
                session.CurrPlayerAttacker = 1;
            } else {
                session.CurrPlayerMove = 0;
                session.CurrPlayerAttacker = 0;
            }

            record = {
                p0: names.human,
                p1: names.bot,
                states: [snapshot(session)],
                labels: ['Начало партии'],
                date: Date.now(),
            };

            setStatus(session.CurrPlayerMove === 0 ? 'Ваш ход' : 'Ход соперника');
            refresh();
            tick();
        });
    }

    function snapshot(s) {
        return JSON.parse(JSON.stringify({
            PlayerCards: s.PlayerCards,
            Deck: s.Deck,
            GameStack: s.GameStack,
            Trump: s.Trump,
            CurrPlayerMove: s.CurrPlayerMove,
            CurrPlayerAttacker: s.CurrPlayerAttacker,
        }));
    }

    function pushState(label) {
        record.states.push(snapshot(session));
        record.labels.push(label);
    }

    function setStatus(text) { el.info.textContent = text; }

    function refresh() {
        if (!session) return;
        const maxPage = Math.max(0, Math.ceil(session.PlayerCards[0].length / 9) - 1);
        if (page > maxPage) page = maxPage;
        renderer.draw(session, { selectedCard: selected, cursor, page });
        el.pageLabel.textContent = `${page + 1} / ${maxPage + 1}`;
        document.getElementById('btnDecPage').disabled = page <= 0;
        document.getElementById('btnIncPage').disabled = page >= maxPage;
    }

    function toField(e) {
        const rect = canvas.getBoundingClientRect();
        return {
            x: (e.clientX - rect.left) * (canvas.width / rect.width),
            y: (e.clientY - rect.top) * (canvas.height / rect.height),
        };
    }

    function stackX(i) {
        const { W, CARD_W, DIST } = FIELD;
        switch (i) {
            case 0: return W / 2 - CARD_W * 2 - DIST;
            case 1: return W / 2 - CARD_W;
            case 2: return W / 2 + DIST;
            default: return W / 2 + CARD_W + DIST * 2;
        }
    }

    function getSelectedCard(x, y) {
        const { H, CARD_W, CARD_H, DIST } = FIELD;
        if (y > H - DIST || y < H - DIST - CARD_H) return -1;
        for (let i = 0; i < 9; i++) {
            const left = DIST * (i + 1) + CARD_W * i;
            const right = DIST * (i + 1) + CARD_W * (i + 1);
            if (x > left && x < right && session.PlayerCards[0].length > i)
                return i + 9 * page;
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

    function onPointerDown(e) {
        if (busy || finished || !session) return;
        if (session.CurrPlayerMove !== 0) { setStatus('Сейчас не Ваш ход!'); return; }
        const p = toField(e);
        selected = getSelectedCard(p.x, p.y);
        if (selected !== -1) {
            cursor = p;
            dragging = true;
            canvas.setPointerCapture?.(e.pointerId);
            refresh();
        } else {
            setStatus('Карта не выбрана');
        }
    }

    function onPointerMove(e) {
        if (!dragging || busy || finished) return;
        if (session.CurrPlayerMove !== 0) return;
        cursor = toField(e);
        refresh();
    }

    function onPointerUp(e) {
        if (!dragging || busy || finished || !session) { dragging = false; return; }
        dragging = false;
        if (session.CurrPlayerMove !== 0) {
            setStatus('Сейчас не Ваш ход!');
            selected = -1; cursor = null; refresh();
            return;
        }
        const p = toField(e);
        const stackNum = getSelectedStack(p.x, p.y);

        if (selected === -1) { selected = -1; cursor = null; refresh(); return; }

        if (stackNum === -1) {
            setStatus('Положите карту в нормальное место!');
            selected = -1; cursor = null; refresh();
            return;
        }

        const card = session.PlayerCards[0][selected];

        if (session.isAttackerTurn()) {
            if (session.canPush(card, session.GameStack[stackNum])) {
                doHumanMove(card, stackNum, 'Вы положили ' + cardName(card));
            } else {
                setStatus('Эту карту сюда положить нельзя!');
                selected = -1; cursor = null; refresh();
            }
        } else {
            const stack = session.GameStack[stackNum];
            if (stack.length % 2 === 1 && session.canBeat(card, stack[stack.length - 1])) {
                doHumanMove(card, stackNum, 'Вы отбились ' + cardName(card));
            } else if (stack.length % 2 === 0) {
                setStatus('Здесь нет карты, которую можно отбить!');
                selected = -1; cursor = null; refresh();
            } else {
                setStatus('Вы не можете побить эту карту!');
                selected = -1; cursor = null; refresh();
            }
        }
    }

    function doHumanMove(card, stackNum, label) {
        session.doMove(card, stackNum);
        pushState(label);
        selected = -1;
        cursor = null;
        checkCardsPage();
        refresh();
        afterAction();
    }

    function onMoveTransfer() {
        if (busy || finished || !session) return;
        if (session.CurrPlayerMove !== 0) { setStatus('Сейчас не ваш ход!'); return; }

        if (session.isAttackerTurn()) {
            if (session.allStacksEmpty()) { setStatus('Вы не положили ни одной карты'); return; }
            setStatus('Вы передали ход сопернику!');
            session.transferMove(MoveType.mtTransfer);
            pushState('Вы передали ход');
        } else {
            if (!session.allCardsBeaten()) { setStatus('Не все карты отбиты!'); return; }
            setStatus('Атакуйте!');
            session.giveCardsAfterDefense();
            session.transferMove(MoveType.mtTransfer);
            pushState('Вы отбились');
        }
        selected = -1; cursor = null;
        checkCardsPage();
        refresh();
        afterAction();
    }

    function onGrab() {
        if (busy || finished || !session) return;
        if (session.isAttackerTurn() || session.CurrPlayerMove !== 0) {
            setStatus('Сейчас не время таким заниматься!');
            return;
        }
        session.grab();
        session.giveCardsAfterDefense();
        session.transferMove(MoveType.mtGrab);
        pushState('Вы забрали карты');
        selected = -1; cursor = null;
        checkCardsPage();
        refresh();
        afterAction();
    }

    function switchPage(dir) {
        const maxPage = Math.max(0, Math.ceil(session.PlayerCards[0].length / 9) - 1);
        const next = page + dir;
        if (next < 0 || next > maxPage) return;
        page = next;
        selected = -1; cursor = null;
        refresh();
    }

    function checkCardsPage() {
        page = Math.max(0, Math.ceil(session.PlayerCards[0].length / 9) - 1);
    }

    function afterAction() { tick(); }

    function checkGameOver() {
        if (session.Deck.length > 0) return false;
        if (!session.allStacksEmpty()) return false;
        return session.PlayerCards[0].length === 0 || session.PlayerCards[1].length === 0;
    }

    function tick() {
        if (finished) return;
        if (checkGameOver()) { finishGame(); return; }

        if (session.CurrPlayerMove === 1) {
            stopTurnTimer();
            busy = true;
            setStatus('Ход соперника...');
            scheduleBot();
        } else {
            busy = false;
            if (session.isAttackerTurn()) setStatus('Ваш ход: положите карту или передайте ход');
            else if (session.allCardsBeaten()) setStatus('Отбились! Нажмите «Отбой / передать ход»');
            else setStatus('Отбивайтесь или забирайте карты');
            startTurnTimer();
        }
    }

    function scheduleBot() {
        const gen = gameGen;
        setTimeout(() => { if (gen === gameGen && !finished) botStep(); }, BOT_DELAY);
    }

    function botStep() {
        if (finished) return;
        if (checkGameOver()) { finishGame(); return; }

        if (botPlan.length > 0) {
            const step = botPlan.shift();
            if (session.PlayerCards[1].indexOf(step.card) !== -1) {
                session.doMove(step.card, step.stackNum);
                pushState('Соперник отбился ' + cardName(step.card));
            }
            refresh();
            if (botPlan.length > 0) { scheduleBot(); return; }
            botPass();
            return;
        }

        const action = DurakBot.decide(session, 1);
        if (!action) { tick(); return; }

        switch (action.kind) {
            case 'move':
                if (session.PlayerCards[1].indexOf(action.card) !== -1) {
                    session.doMove(action.card, action.stackNum);
                    pushState('Соперник положил ' + cardName(action.card));
                }
                refresh();
                tick();
                break;
            case 'plan':
                botPlan = action.steps.slice();
                botStep();
                break;
            case 'grab':
                session.grab();
                session.giveCardsAfterDefense();
                session.transferMove(MoveType.mtGrab);
                pushState('Соперник забрал карты');
                refresh();
                tick();
                break;
            case 'pass':
                botPass();
                break;
            default:
                tick();
        }
    }

    function botPass() {
        if (session.isAttackerTurn()) {
            session.transferMove(MoveType.mtTransfer);
            pushState('Соперник передал ход');
        } else {
            session.giveCardsAfterDefense();
            session.transferMove(MoveType.mtTransfer);
            pushState('Соперник отбился');
        }
        refresh();
        tick();
    }

    function startTurnTimer() {
        stopTurnTimer();
        turnSeconds = TURN_LIMIT;
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
        if (turnTimer) clearInterval(turnTimer);
        turnTimer = null;
    }

    function stopTimers() { stopTurnTimer(); finished = true; gameGen++; }

    function updateTimerLabel() {
        el.time.textContent = (session && session.CurrPlayerMove === 0 && !finished)
            ? '⏱ ' + turnSeconds + ' c' : '';
    }

    function onTurnTimeout() {
        if (finished || busy) return;
        if (session.CurrPlayerMove !== 0) return;
        setStatus('Время на ход истекло!');
        if (session.isAttackerTurn()) {
            if (session.allStacksEmpty()) {
                const hand = session.PlayerCards[0];
                const card = [...hand].sort((a, b) => a.power - b.power)[0];
                if (card) {
                    session.doMove(card, 0);
                    pushState('Время вышло: авто-ход');
                    refresh();
                    afterAction();
                    return;
                }
            }
            onMoveTransfer();
        } else {
            onGrab();
        }
    }

    function finishGame() {
        if (finished) return;
        finished = true;
        busy = true;
        stopTurnTimer();
        el.time.textContent = '';

        const humanWon = session.PlayerCards[0].length === 0;
        record.result = humanWon ? 'win' : 'lose';
        saveReplay(record);

        if (humanWon) animateVictory();
        else animateDefeat();
    }

    function saveReplay(rec) {
        try {
            const all = JSON.parse(localStorage.getItem('durak_replays') || '[]');
            all.unshift(rec);
            localStorage.setItem('durak_replays', JSON.stringify(all.slice(0, 30)));
        } catch (e) { }
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
        drawResultText('You');
        await sleep(255);
        Sound.play('text');
        await sleep(500);
        drawResultText('You Win');
        await sleep(1777);
        await confetti(7);
        endScreen(true);
    }

    async function animateDefeat() {
        const text = 'You Lose';
        drawResultText('');
        for (let i = 1; i <= text.length; i++) {
            if (text[i - 1] !== ' ') Sound.play('text');
            await sleep(200);
            drawResultText(text.substring(0, i));
        }
        await sleep(1800);
        endScreen(false);
    }

    function confetti(seconds) {
        return new Promise(resolve => {
            const { ctx } = renderer;
            const { W, H } = FIELD;
            const bg = document.createElement('canvas');
            bg.width = W; bg.height = H;
            bg.getContext('2d').drawImage(canvas, 0, 0);

            const colors = ['#ff0000', '#00ff00', '#0000ff', '#ffff00', '#ff00ff',
                '#00ffff', '#ffa500', '#8a2be2', '#00008b', '#ffffff'];
            const pieces = [];
            const end = performance.now() + seconds * 1000;
            try { Sound.play('win'); } catch (e) { }

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
                for (const p of pieces) {
                    p.t += 1.5;
                    p.y += p.speed;
                    p.x = p.ix + p.amp * Math.sin(p.t * p.freq);
                    if (Math.floor(Math.random() * 6) === 2) p.comp = Math.random() * 0.8 + 0.2;
                }

                ctx.drawImage(bg, 0, 0);
                for (let i = pieces.length - 1; i >= 0; i--) {
                    const p = pieces[i];
                    if (p.y > H) { pieces.splice(i, 1); continue; }
                    const hz = p.size / 2;
                    const vt = (p.size / 2) * p.comp;
                    ctx.fillStyle = p.color;
                    ctx.beginPath();
                    ctx.moveTo(p.x, p.y - vt);
                    ctx.lineTo(p.x + hz, p.y);
                    ctx.lineTo(p.x, p.y + vt);
                    ctx.lineTo(p.x - hz, p.y);
                    ctx.closePath();
                    ctx.fill();
                }
                if (now < end) requestAnimationFrame(frame);
                else resolve();
            }
            requestAnimationFrame(frame);
        });
    }

    function endScreen(won) {
        App.showModal({
            title: won ? 'Победа!' : 'Поражение',
            text: won ? 'Вы выиграли партию. Повтор сохранён.' : 'Вы проиграли. Повтор сохранён.',
            showYes: false,
            closeText: 'В меню',
            onClose: () => App.showScreen('menu'),
        });
    }

    function sleep(ms) { return new Promise(r => setTimeout(r, ms)); }

    return { init, start, stopTimers };
})();
