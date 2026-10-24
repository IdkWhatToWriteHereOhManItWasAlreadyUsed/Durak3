const Lobby = (() => {
    const el = {};

    let active = false;
    let players = [];
    let playersLoading = false;

    let presenceSessionId = null;
    let presenceClosing = null;
    let heartbeatTimer = null;
    let playersTimer = null;
    let invitesTimer = null;

    let outgoing = null;
    let currentIncoming = null;
    let currentIncomingDeadline = 0;
    let currentIncomingTotal = 1;
    let incomingCountdown = null;
    let incomingInvites = new Map();
    let incomingLoading = false;
    let incomingBusy = false;
    let seenIncoming = new Set();
    let resumeMatch = null;

    const STATUS_LABELS = {
        in_lobby: 'В лобби',
        in_game: 'В игре',
        away: 'Отошёл',
    };

    function init() {
        el.playersBody = document.querySelector('#playersGrid tbody');
        el.resumeButton = document.getElementById('resumeGame');

        const refreshPlayersBtn = document.getElementById('refreshList');
        if (refreshPlayersBtn) refreshPlayersBtn.addEventListener('click', refreshPlayers);
        if (el.resumeButton) el.resumeButton.addEventListener('click', resumeActiveMatch);

        document.querySelectorAll('.menu-tab').forEach(tab => {
            tab.addEventListener('click', () => {
                document.querySelectorAll('.menu-tab').forEach(t => t.classList.remove('active'));
                document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));
                tab.classList.add('active');
                const panel = document.getElementById('tab-' + tab.dataset.tab);
                if (panel) panel.classList.add('active');
                if (tab.dataset.tab === 'find') refreshPlayers();
            });
        });

        Realtime.on('invite.created', onRealtimeInviteCreated);
        Realtime.on('invite.accepted', onRealtimeInviteAccepted);
        Realtime.on('invite.declined', onRealtimeInviteDeclined);
        Realtime.on('game.started', onRealtimeGameStarted);
        Realtime.on('lobby.players', onRealtimeLobbyPlayers);
        Realtime.on('presence.online', () => refreshPlayers());
        Realtime.on('presence.offline', () => refreshPlayers());
        Realtime.on('connected', () => {
            if (!active) return;
            refreshPlayers();
            refreshIncomingInvites();
        });
        Realtime.on('error', payload => {
            if (!payload || payload.code === 'SESSION_EXPIRED') return;
            App.toast(payload.message || payload.code || 'Ошибка realtime', 'error');
        });

        window.addEventListener('beforeunload', () => closePresence(true));
    }

    async function enter() {
        if (active) return;

        active = true;
        seenIncoming = new Set();
        incomingInvites = new Map();
        currentIncoming = null;

        if (presenceClosing) await presenceClosing;
        if (!active) return;

        await openPresence();
        if (!active) {
            closePresence(false);
            return;
        }

        startTimers();
        refreshPlayers();
        refreshIncomingInvites();
        loadResumeMatch();
    }

    async function leave() {
        if (!active && !presenceSessionId) return;

        active = false;
        stopTimers();
        cancelOutgoing(true);
        closeIncomingModal();
        incomingInvites.clear();
        seenIncoming.clear();

        const sessionId = presenceSessionId;
        presenceSessionId = null;
        if (sessionId) {
            presenceClosing = Api.lobby.closePresence().finally(() => { presenceClosing = null; });
            await presenceClosing;
        }
    }

    function startTimers() {
        stopTimers();
        heartbeatTimer = setInterval(heartbeat, CONFIG.heartbeatMs);
        playersTimer = setInterval(refreshPlayers, CONFIG.playersPollMs);
        invitesTimer = setInterval(refreshIncomingInvites, CONFIG.invitesPollMs);
    }

    function stopTimers() {
        clearInterval(heartbeatTimer);
        clearInterval(playersTimer);
        clearInterval(invitesTimer);
        heartbeatTimer = null;
        playersTimer = null;
        invitesTimer = null;
    }

    async function openPresence() {
        try {
            const presence = await Api.lobby.openPresence('in_lobby');
            presenceSessionId = presence.sessionId;
            return true;
        } catch (e) {
            if (e.status !== 401) App.toast('Присутствие не открыто: ' + e.message, 'error');
            return false;
        }
    }

    async function heartbeat() {
        if (!presenceSessionId) return;
        try {
            await Api.lobby.heartbeat(presenceSessionId);
        } catch (e) {
            if (e.status === 404) {
                presenceSessionId = null;
                if (active) await openPresence();
            }
        }
    }

    function closePresence(keepalive) {
        const sessionId = presenceSessionId;
        presenceSessionId = null;
        if (!sessionId) return;

        if (keepalive && Api.getToken()) {
            try {
                fetch(new URL('/api/lobby/presence', CONFIG.apiBase + '/'), {
                    method: 'DELETE',
                    headers: { Authorization: 'Bearer ' + Api.getToken() },
                    keepalive: true,
                }).catch(() => {});
            } catch (e) { }
        } else {
            Api.lobby.closePresence();
        }
    }

    async function refreshPlayers() {
        if (!active || playersLoading) return;
        playersLoading = true;
        try {
            const data = await Api.lobby.players({ excludeSelf: true, limit: 100, offset: 0 });
            players = Array.isArray(data.items) ? data.items : [];
            renderPlayers();
        } catch (e) {
            if (e.status !== 401) renderPlayers(e.message);
        } finally {
            playersLoading = false;
        }
    }

    function renderPlayers(errorText) {
        if (!el.playersBody) return;
        el.playersBody.innerHTML = '';

        if (errorText) {
            el.playersBody.innerHTML =
                `<tr><td colspan="4" class="empty">Не удалось получить список: ${escapeHtml(errorText)}</td></tr>`;
            return;
        }
        if (!players.length) {
            el.playersBody.innerHTML =
                '<tr><td colspan="4" class="empty">В лобби пока никого нет.</td></tr>';
            return;
        }

        for (const player of players) {
            const tr = document.createElement('tr');

            const nameTd = document.createElement('td');
            nameTd.textContent = player.name || ('Игрок #' + player.id);
            tr.appendChild(nameTd);

            const idTd = document.createElement('td');
            idTd.textContent = '#' + player.id;
            tr.appendChild(idTd);

            const statusTd = document.createElement('td');
            const badge = document.createElement('span');
            badge.className = 'status-badge status-' + (player.status || 'unknown');
            badge.textContent = STATUS_LABELS[player.status] || '—';
            statusTd.appendChild(badge);
            tr.appendChild(statusTd);

            const actionTd = document.createElement('td');
            const button = document.createElement('button');
            button.className = 'invite-btn';
            button.textContent = 'Пригласить';
            button.disabled = player.status === 'in_game' || !!outgoing;
            if (player.status === 'in_game') button.title = 'Игрок уже в игре';
            button.addEventListener('click', () => invitePlayer(player));
            actionTd.appendChild(button);
            tr.appendChild(actionTd);

            el.playersBody.appendChild(tr);
        }
    }

    async function invitePlayer(player) {
        if (!active) return;
        if (outgoing) {
            App.toast('Уже есть отправленное приглашение', 'error');
            return;
        }

        Sound.play('invite');
        try {
            const invite = await Api.invites.create(player.id, CONFIG.inviteTtlSeconds);
            outgoing = {
                invite,
                opponent: player,
                deadline: expiresAt(invite, CONFIG.inviteTtlSeconds),
                total: CONFIG.inviteTtlSeconds * 1000,
                progressTimer: null,
                pollTimer: null,
                resolving: false,
            };
            renderPlayers();
            showOutgoingModal();
            startOutgoingTracking();
        } catch (e) {
            Sound.stop();
            if (e.status === 409) App.toast('Игрок уже занят или приглашение уже отправлено', 'error');
            else if (e.status === 422) App.toast('Некорректное приглашение', 'error');
            else App.toast('Не удалось пригласить: ' + e.message, 'error');
        }
    }

    function showOutgoingModal() {
        const name = outgoing.opponent.name || ('Игрок #' + outgoing.opponent.id);
        App.showModal({
            owner: 'lobby',
            title: 'Ожидание ответа',
            text: `Игрок ${name} думает…`,
            progress: true,
            showNo: false,
            yesText: 'Отменить приглашение',
            onYes: cancelOutgoing,
        });
    }

    function startOutgoingTracking() {
        if (!outgoing) return;
        updateOutgoingProgress();
        outgoing.progressTimer = setInterval(updateOutgoingProgress, 200);
        outgoing.pollTimer = setInterval(pollOutgoingInvite, CONFIG.outgoingInvitePollMs);
    }

    function updateOutgoingProgress() {
        if (!outgoing) return;
        const left = Math.max(0, outgoing.deadline - Date.now());
        App.setModalProgress(Math.min(100, (left / outgoing.total) * 100));
    }

    async function pollOutgoingInvite() {
        if (!outgoing || outgoing.resolving) return;
        try {
            const invite = await Api.invites.get(outgoing.invite.id);
            if (!outgoing) return;

            if (invite.status === 'accepted') {
                await resolveAcceptedInvite(invite);
            } else if (['declined', 'expired', 'cancelled'].includes(invite.status)) {
                const name = outgoing.opponent.name || ('Игрок #' + outgoing.opponent.id);
                finishOutgoing();
                Sound.stop();
                App.toast(
                    invite.status === 'declined'
                        ? `Игрок ${name} отклонил приглашение`
                        : 'Приглашение истекло',
                    'error'
                );
                renderPlayers();
            }
        } catch (e) {
            if (e.status === 404) {
                finishOutgoing();
                App.toast('Приглашение больше не существует', 'error');
            }
        }
    }

    async function resolveAcceptedInvite(invite) {
        if (!outgoing || outgoing.resolving) return;
        outgoing.resolving = true;

        try {
            const opponentId = outgoing.opponent.id;
            const match = await findActiveMatch(opponentId);
            if (!match) {
                App.toast('Приглашение принято, но матч ещё не создан', 'error');
                finishOutgoing();
                loadResumeMatch();
                return;
            }

            const opponentName = outgoing.opponent.name;
            finishOutgoing();
            Sound.play('accept');
            startMatch(match, opponentName);
        } finally {
            if (outgoing) outgoing.resolving = false;
        }
    }

    async function findActiveMatch(opponentId) {
        const me = Auth.id();
        for (let attempt = 0; attempt < 4; attempt++) {
            try {
                const data = await Api.matches.list('active');
                const matches = (data.items || []).filter(match => {
                    const ids = (match.players || []).map(p => p.id);
                    return ids.includes(me) && ids.includes(opponentId);
                });
                if (matches.length) {
                    matches.sort((a, b) => new Date(b.createdAt || 0) - new Date(a.createdAt || 0));
                    return matches[0];
                }
            } catch (e) {
                if (e.status === 401) return null;
            }
            await sleep(500);
        }
        return null;
    }

    async function cancelOutgoing(silent) {
        if (!outgoing) return;
        const inviteId = outgoing.invite.id;
        finishOutgoing();
        Sound.stop();
        if (!silent) {
            try { await Api.invites.cancel(inviteId); } catch (e) { }
            App.toast('Приглашение отменено');
        } else {
            Api.invites.cancel(inviteId).catch(() => {});
        }
        renderPlayers();
    }

    function finishOutgoing() {
        if (!outgoing) return;
        clearInterval(outgoing.progressTimer);
        clearInterval(outgoing.pollTimer);
        outgoing = null;
        App.hideModal('lobby');
        renderPlayers();
        setTimeout(showNextIncoming, 250);
    }

    async function refreshIncomingInvites() {
        if (!active || incomingLoading) return;
        incomingLoading = true;
        try {
            const data = await Api.invites.list({ direction: 'incoming', status: 'pending' });
            const items = Array.isArray(data.items) ? data.items : [];
            const now = Date.now();
            const pendingIds = new Set();

            for (const invite of items) {
                if (!invite || !invite.id) continue;
                if (expiresAt(invite, CONFIG.inviteTtlSeconds) <= now) continue;
                pendingIds.add(invite.id);
                incomingInvites.set(invite.id, invite);

                if (!seenIncoming.has(invite.id)) {
                    seenIncoming.add(invite.id);
                    App.toast((invite.fromName || 'Игрок') + ' приглашает в игру', 'info');
                    Sound.play('invite');
                }
            }

            for (const id of Array.from(incomingInvites.keys())) {
                if (!pendingIds.has(id)) incomingInvites.delete(id);
            }
            for (const id of Array.from(seenIncoming)) {
                if (!pendingIds.has(id)) seenIncoming.delete(id);
            }
            if (currentIncoming && !pendingIds.has(currentIncoming.id)) {
                closeIncomingModal();
            }

            showNextIncoming();
        } catch (e) {
            if (e.status !== 401) { }
        } finally {
            incomingLoading = false;
        }
    }

    function registerIncoming(invite) {
        if (!invite || !invite.id || invite.status !== 'pending') return;
        if (invite.toId != null && invite.toId !== Auth.id()) return;
        if (expiresAt(invite, CONFIG.inviteTtlSeconds) <= Date.now()) return;

        incomingInvites.set(invite.id, invite);
        if (!seenIncoming.has(invite.id)) {
            seenIncoming.add(invite.id);
            Sound.play('invite');
            App.toast((invite.fromName || 'Игрок') + ' приглашает в игру', 'info');
        }
        showNextIncoming(invite);
    }

    function showNextIncoming(candidate) {
        if (!active || outgoing || incomingBusy || App.isModalOpen() || (typeof Game !== 'undefined' && Game.isActive())) return;

        let invite = candidate && candidate.id ? candidate : null;
        if (!invite) {
            const now = Date.now();
            for (const item of incomingInvites.values()) {
                if (expiresAt(item, CONFIG.inviteTtlSeconds) > now) { invite = item; break; }
            }
        }
        if (!invite) return;

        currentIncoming = invite;
        currentIncomingDeadline = expiresAt(invite, CONFIG.inviteTtlSeconds);
        currentIncomingTotal = Math.max(
            1000,
            currentIncomingDeadline - (invite.createdAt
                ? new Date(invite.createdAt).getTime()
                : currentIncomingDeadline - CONFIG.inviteTtlSeconds * 1000)
        );
        const fromName = invite.fromName || ('Игрок #' + invite.fromId);

        App.showModal({
            owner: 'lobby',
            title: 'Приглашение в игру',
            text: `${fromName} приглашает вас в игру. Принять?`,
            yesText: 'Да',
            noText: 'Нет',
            progress: true,
            onYes: acceptIncoming,
            onNo: declineIncoming,
            onClose: declineIncoming,
        });

        updateIncomingProgress();
        clearInterval(incomingCountdown);
        incomingCountdown = setInterval(() => {
            if (!currentIncoming) return;
            if (Date.now() >= currentIncomingDeadline) {
                const expired = currentIncoming;
                incomingInvites.delete(expired.id);
                seenIncoming.delete(expired.id);
                closeIncomingModal();
                refreshIncomingInvites();
                return;
            }
            updateIncomingProgress();
        }, 200);
    }

    function updateIncomingProgress() {
        const left = Math.max(0, currentIncomingDeadline - Date.now());
        App.setModalProgress((left / currentIncomingTotal) * 100);
    }

    async function acceptIncoming() {
        const invite = currentIncoming;
        if (!invite) return;
        incomingBusy = true;
        stopIncomingCountdown();

        try {
            const match = await Api.invites.accept(invite.id);
            incomingInvites.delete(invite.id);
            seenIncoming.delete(invite.id);
            currentIncoming = null;
            Sound.play('accept');

            const opponent = (match.players || []).find(p => p.id !== Auth.id());
            startMatch(match, opponent ? opponent.name : (invite.fromName || 'Соперник'));
        } catch (e) {
            incomingInvites.delete(invite.id);
            seenIncoming.delete(invite.id);
            currentIncoming = null;
            Sound.stop();
            App.toast(
                e.status === 409 ? 'Приглашение уже неактуально' : ('Не удалось принять: ' + e.message),
                'error'
            );
            refreshIncomingInvites();
        } finally {
            incomingBusy = false;
            setTimeout(showNextIncoming, 300);
        }
    }

    async function declineIncoming() {
        const invite = currentIncoming;
        if (!invite) return;
        incomingBusy = true;
        stopIncomingCountdown();
        currentIncoming = null;
        incomingInvites.delete(invite.id);
        seenIncoming.delete(invite.id);

        try { await Api.invites.decline(invite.id); } catch (e) { }
        Sound.stop();
        App.toast('Приглашение отклонено');
        incomingBusy = false;
        setTimeout(showNextIncoming, 300);
    }

    function closeIncomingModal() {
        stopIncomingCountdown();
        currentIncoming = null;
        App.hideModal('lobby');
    }

    function stopIncomingCountdown() {
        clearInterval(incomingCountdown);
        incomingCountdown = null;
    }

    function onRealtimeInviteCreated(payload) {
        if (!active) return;
        const invite = payload && (payload.invite || payload);
        registerIncoming(invite);
    }

    async function onRealtimeInviteAccepted(payload) {
        if (!active) return;
        const invite = payload && (payload.invite || null);
        const match = payload && (payload.match || (payload.gameId ? payload : null));

        if (outgoing && (!invite || invite.id === outgoing.invite.id)) {
            if (match && match.gameId) {
                const opponentName = outgoing.opponent.name;
                finishOutgoing();
                Sound.play('accept');
                startMatch(match, opponentName);
            } else {
                await resolveAcceptedInvite(invite || outgoing.invite);
            }
        }
    }

    function onRealtimeInviteDeclined(payload) {
        if (!outgoing) return;
        const invite = payload && (payload.invite || payload);
        if (invite && invite.id && invite.id !== outgoing.invite.id) return;

        const name = outgoing.opponent.name || ('Игрок #' + outgoing.opponent.id);
        finishOutgoing();
        Sound.stop();
        App.toast(`Игрок ${name} отклонил приглашение`, 'error');
    }

    async function onRealtimeGameStarted(payload) {
        if (!active || (typeof Game !== 'undefined' && Game.isActive())) return;
        if (!payload || !payload.gameId) return;

        const players = payload.players || [];
        const mine = players.length === 0 || players.some(p => p.id === Auth.id());
        if (!mine && !outgoing) return;
        if (!outgoing && !payload.matchId && players.length === 0) return;

        const opponent = players.find(p => p.id !== Auth.id());
        finishOutgoing();
        startMatch(payload, opponent ? opponent.name : 'Соперник');
    }

    function onRealtimeLobbyPlayers(payload) {
        if (!active) return;
        if (payload && Array.isArray(payload.items)) {
            players = payload.items;
            renderPlayers();
        } else {
            refreshPlayers();
        }
    }

    function startMatch(match, opponentName) {
        if (!match || !match.gameId) {
            App.toast('Матч создан без gameId', 'error');
            loadResumeMatch();
            return;
        }
        if (typeof Game !== 'undefined' && Game.isActive() && Game.currentGameId() === match.gameId) {
            return;
        }
        App.startGame({
            gameId: match.gameId,
            matchId: match.id,
            opponentName: opponentName || 'Соперник',
        });
    }

    async function loadResumeMatch() {
        if (!el.resumeButton) return;
        try {
            const data = await Api.matches.list('active');
            const me = Auth.id();
            const matches = (data.items || [])
                .filter(match => (match.players || []).some(p => p.id === me) && match.gameId)
                .sort((a, b) => new Date(b.createdAt || 0) - new Date(a.createdAt || 0));
            resumeMatch = matches[0] || null;

            if (resumeMatch) {
                const opponent = (resumeMatch.players || []).find(p => p.id !== me);
                el.resumeButton.textContent = 'Продолжить партию' +
                    (opponent ? ' с ' + (opponent.name || ('#' + opponent.id)) : '');
                el.resumeButton.classList.remove('hidden');
            } else {
                el.resumeButton.classList.add('hidden');
            }
        } catch (e) {
            resumeMatch = null;
            el.resumeButton.classList.add('hidden');
        }
    }

    function resumeActiveMatch() {
        if (!resumeMatch) return;
        const opponent = (resumeMatch.players || []).find(p => p.id !== Auth.id());
        App.startGame({
            gameId: resumeMatch.gameId,
            matchId: resumeMatch.id,
            opponentName: opponent ? opponent.name : 'Соперник',
        });
    }

    function expiresAt(invite, ttlSeconds) {
        if (invite && invite.expiresAt) {
            const value = new Date(invite.expiresAt).getTime();
            if (Number.isFinite(value)) return value;
        }
        if (invite && invite.createdAt) {
            const value = new Date(invite.createdAt).getTime();
            if (Number.isFinite(value)) return value + ttlSeconds * 1000;
        }
        return Date.now() + ttlSeconds * 1000;
    }

    function escapeHtml(value) {
        return String(value).replace(/[&<>"']/g, c => (
            { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]
        ));
    }

    function sleep(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }

    return { init, enter, leave, refreshPlayers };
})();
