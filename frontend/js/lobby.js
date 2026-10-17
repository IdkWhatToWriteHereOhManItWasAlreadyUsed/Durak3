const Lobby = (() => {
    let players = [];

    const BOT_NAMES = [
        'Рахмат', 'Тимур', 'Наиль', 'Данис', 'Ильдар', 'Азат',
        'Марат', 'Рустем', 'Булат', 'Айрат', 'Салават', 'Ирек'
    ];

    const el = {};

    function init() {
        el.name = document.getElementById('playerName');
        el.playersBody = document.querySelector('#playersGrid tbody');
        el.replaysBody = document.querySelector('#replaysGrid tbody');
        el.replayProgress = document.getElementById('replayProgress');

        el.name.value = randomName();

        document.getElementById('refreshList').addEventListener('click', refreshPlayers);
        document.getElementById('refreshReplays').addEventListener('click', refreshReplays);
        document.getElementById('demoInvite').addEventListener('click', incomingInvite);

        document.querySelectorAll('.menu-tab').forEach(tab => {
            tab.addEventListener('click', () => {
                document.querySelectorAll('.menu-tab').forEach(t => t.classList.remove('active'));
                document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));
                tab.classList.add('active');
                document.getElementById('tab-' + tab.dataset.tab).classList.add('active');
            });
        });

        refreshPlayers();
        refreshReplays();
    }

    function randomName() {
        const syllables = ['al', 'an', 'ar', 'as', 'ea', 'en', 'er', 'ha', 'he',
            'in', 'is', 'ka', 'le', 'me', 'na', 'ne', 'or', 'ra', 're', 'se',
            'ta', 'te', 'ti', 'to', 'va', 've'];
        const vowels = ['a', 'e', 'i', 'o', 'u', 'y'];
        const count = 2 + Math.floor(Math.random() * 3);
        let name = '';
        for (let i = 0; i < count; i++) {
            if (i > 0 && Math.random() > 0.7) name += vowels[Math.floor(Math.random() * vowels.length)];
            name += syllables[Math.floor(Math.random() * syllables.length)];
        }
        return name.charAt(0).toUpperCase() + name.slice(1);
    }

    function shortId() {
        let id = '';
        const hex = '0123456789abcdef';
        for (let i = 0; i < 8; i++) id += hex[Math.floor(Math.random() * 16)];
        return id;
    }

    function refreshPlayers() {
        players = [];
        const used = new Set();
        const count = 2 + Math.floor(Math.random() * 4);
        while (players.length < count) {
            const name = BOT_NAMES[Math.floor(Math.random() * BOT_NAMES.length)];
            if (used.has(name)) continue;
            used.add(name);
            players.push({ name, id: shortId() });
        }
        renderPlayers();
    }

    function renderPlayers() {
        el.playersBody.innerHTML = '';
        for (const p of players) {
            const tr = document.createElement('tr');
            tr.innerHTML = `<td>${escapeHtml(p.name)}</td><td>${p.id}</td>`;
            const td = document.createElement('td');
            const btn = document.createElement('button');
            btn.className = 'invite-btn';
            btn.textContent = 'Пригласить';
            btn.addEventListener('click', () => outgoingInvite(p));
            td.appendChild(btn);
            tr.appendChild(td);
            el.playersBody.appendChild(tr);
        }
    }

    function outgoingInvite(player) {
        Sound.play('invite');
        App.showModal({
            title: 'Ожидание ответа',
            text: `Игрок ${player.name} думает...`,
            showButtons: false,
            progress: true,
        });

        let left = 100;
        const timer = setInterval(() => {
            left -= 1;
            App.setModalProgress(left);
            if (left <= 0) {
                clearInterval(timer);
                Sound.stop();
                App.hideModal();
                App.showModal({
                    title: 'Нет ответа',
                    text: 'Игрок не ответил на приглашение.',
                    showYes: false,
                    closeText: 'Ок',
                });
            }
        }, 100);

        const accept = Math.random() > 0.2;
        const delay = 1500 + Math.random() * 1500;
        setTimeout(() => {
            clearInterval(timer);
            if (!App.isModalOpen()) return;
            App.hideModal();
            if (accept) {
                Sound.play('accept');
                App.startGame({
                    playerName: el.name.value || 'Игрок',
                    opponentName: player.name,
                    startFirst: true,
                });
            } else {
                Sound.stop();
                App.showModal({
                    title: 'Отказ',
                    text: `Игрок ${player.name} отклонил приглашение.`,
                    showYes: false,
                    closeText: 'Ок',
                });
            }
        }, delay);
    }

    function incomingInvite() {
        const player = players[Math.floor(Math.random() * players.length)] || { name: 'Соперник' };
        Sound.play('invite');

        let left = 30;
        App.showModal({
            title: 'Приглашение в игру',
            text: `${player.name} приглашает вас в игру. Принять?`,
            yesText: 'Да',
            noText: 'Нет',
            progress: true,
            onYes: () => {
                stop();
                Sound.play('accept');
                App.startGame({
                    playerName: el.name.value || 'Игрок',
                    opponentName: player.name,
                    startFirst: false,
                });
            },
            onNo: () => { stop(); Sound.stop(); },
        });

        const timer = setInterval(() => {
            left -= 1;
            App.setModalProgress((left / 30) * 100);
            if (left <= 0) { stop(); Sound.stop(); App.hideModal(); }
        }, 1000);

        function stop() { clearInterval(timer); }
    }

    function refreshReplays() {
        let step = 0;
        el.replayProgress.style.width = '0%';
        const timer = setInterval(() => {
            step += 4;
            el.replayProgress.style.width = Math.min(step, 100) + '%';
            if (step >= 100) {
                clearInterval(timer);
                renderReplays();
            }
        }, 40);
    }

    function renderReplays() {
        let all = [];
        try { all = JSON.parse(localStorage.getItem('durak_replays') || '[]'); } catch (e) { all = []; }
        el.replaysBody.innerHTML = '';
        if (all.length === 0) {
            const tr = document.createElement('tr');
            tr.innerHTML = '<td colspan="3" class="empty">Повторов пока нет — сыграйте партию.</td>';
            el.replaysBody.appendChild(tr);
            return;
        }
        all.forEach(rec => {
            const tr = document.createElement('tr');
            tr.innerHTML = `<td>${escapeHtml(rec.p0)}</td><td>${escapeHtml(rec.p1)}</td>`;
            const td = document.createElement('td');
            const btn = document.createElement('button');
            btn.className = 'watch-btn';
            btn.textContent = 'Смотреть';
            btn.addEventListener('click', () => {
                Sound.play('accept');
                App.openReplay(rec);
            });
            td.appendChild(btn);
            tr.appendChild(td);
            el.replaysBody.appendChild(tr);
        });
    }

    function escapeHtml(s) {
        return String(s).replace(/[&<>"']/g, c => (
            { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]
        ));
    }

    return { init };
})();
