/**
 * HTTP-клиент api-gateway.
 *
 * Контракт: backend/gateways/apiGateway/openapi.yaml.
 * Все защищённые запросы отправляют `Authorization: Bearer <opaque accessToken>`;
 * токен хранится в localStorage и НЕ является JWT.
 */
class ApiError extends Error {
    constructor(message, status, code, data) {
        super(message);
        this.name = 'ApiError';
        this.status = status;
        this.code = code || (status ? 'HTTP_' + status : 'NETWORK_ERROR');
        this.data = data;
    }
}

const Api = (() => {
    const TOKEN_KEY = 'durak.accessToken';
    const unauthorizedHandlers = new Set();

    function getToken() {
        try { return localStorage.getItem(TOKEN_KEY) || ''; } catch (e) { return ''; }
    }

    function setToken(token) {
        try {
            if (token) localStorage.setItem(TOKEN_KEY, token);
            else localStorage.removeItem(TOKEN_KEY);
        } catch (e) { /* приватный режим */ }
    }

    function onUnauthorized(fn) {
        unauthorizedHandlers.add(fn);
        return () => unauthorizedHandlers.delete(fn);
    }

    function emitUnauthorized() {
        unauthorizedHandlers.forEach(fn => {
            try { fn(); } catch (e) { console.error(e); }
        });
    }

    function buildUrl(path, query) {
        const url = new URL(path, CONFIG.apiBase + '/');
        if (query) {
            Object.entries(query).forEach(([key, value]) => {
                if (value !== undefined && value !== null && value !== '') {
                    url.searchParams.set(key, String(value));
                }
            });
        }
        return url.toString();
    }

    async function request(method, path, options = {}) {
        const { body, query, auth = true } = options;
        const headers = { Accept: 'application/json' };
        const token = getToken();

        if (auth && token) headers.Authorization = 'Bearer ' + token;
        if (body !== undefined) headers['Content-Type'] = 'application/json';

        const controller = new AbortController();
        const timeout = setTimeout(() => controller.abort(), CONFIG.requestTimeoutMs);

        let response;
        try {
            response = await fetch(buildUrl(path, query), {
                method,
                headers,
                body: body === undefined ? undefined : JSON.stringify(body),
                credentials: 'omit',
                signal: controller.signal,
            });
        } catch (e) {
            clearTimeout(timeout);
            const reason = e && e.name === 'AbortError' ? 'Превышено время ожидания' : 'Сервер недоступен';
            throw new ApiError(reason, 0, 'NETWORK_ERROR', null);
        }
        clearTimeout(timeout);

        let data = null;
        const text = await response.text();
        if (text) {
            try { data = JSON.parse(text); } catch (e) { data = text; }
        }

        if (!response.ok) {
            const message = (data && (data.message || data.error)) || ('HTTP ' + response.status);
            const code = (data && data.code) || ('HTTP_' + response.status);
            const error = new ApiError(message, response.status, code, data);
            if (response.status === 401 && auth) emitUnauthorized();
            throw error;
        }

        return data;
    }

    const get = (path, query, options) => request('GET', path, { ...options, query });
    const post = (path, body, options) => request('POST', path, { ...options, body });
    const del = (path, options) => request('DELETE', path, options || {});

    return {
        getToken,
        setToken,
        onUnauthorized,
        request,

        // ------------------------------- Auth ---------------------------------
        auth: {
            /** POST /api/auth/google */
            google: (code, redirectUri) => post('/api/auth/google', {
                code,
                redirect_uri: redirectUri,
            }, { auth: false }),

            /** GET /api/auth/me -> { id, username } */
            me: () => get('/api/auth/me'),

            /** DELETE /api/auth/logout */
            logout: () => del('/api/auth/logout'),

            /** GET /api/auth/users/{id} */
            user: (id) => get('/api/auth/users/' + encodeURIComponent(id), null, { auth: false }),
        },

        // ------------------------------- Lobby --------------------------------
        lobby: {
            /** POST /api/lobby/presence -> Presence */
            openPresence: (status = 'in_lobby') => post('/api/lobby/presence', {
                status,
                clientVersion: CONFIG.clientVersion,
            }),

            /** DELETE /api/lobby/presence */
            closePresence: () => del('/api/lobby/presence').catch(() => null),

            /** POST /api/lobby/presence/{sessionId}/heartbeat -> Presence */
            heartbeat: (sessionId) => post(
                '/api/lobby/presence/' + encodeURIComponent(sessionId) + '/heartbeat',
                undefined
            ),

            /** GET /api/lobby/players -> { total, items: OnlinePlayer[] } */
            players: (params = {}) => get('/api/lobby/players', {
                excludeSelf: params.excludeSelf !== false,
                limit: params.limit || 100,
                offset: params.offset || 0,
            }),
        },

        // ---------------------------- Matchmaking -----------------------------
        invites: {
            /** POST /api/invites -> Invite */
            create: (toId, ttlSeconds = CONFIG.inviteTtlSeconds) => post('/api/invites', {
                toId,
                ttlSeconds,
            }),

            /** GET /api/invites -> { total, items: Invite[] } */
            list: (params = {}) => get('/api/invites', {
                direction: params.direction || 'incoming',
                status: params.status || 'pending',
            }),

            /** GET /api/invites/{inviteId} */
            get: (inviteId) => get('/api/invites/' + encodeURIComponent(inviteId)),

            /** POST /api/invites/{inviteId}/accept -> Match */
            accept: (inviteId) => post('/api/invites/' + encodeURIComponent(inviteId) + '/accept'),

            /** POST /api/invites/{inviteId}/decline -> Invite */
            decline: (inviteId) => post('/api/invites/' + encodeURIComponent(inviteId) + '/decline'),

            /** POST /api/invites/{inviteId}/cancel -> Invite */
            cancel: (inviteId) => post('/api/invites/' + encodeURIComponent(inviteId) + '/cancel'),
        },

        matches: {
            /** GET /api/matches -> { total, items: Match[] } */
            list: (status = 'all') => get('/api/matches', { status }),

            /** GET /api/matches/{matchId} */
            get: (matchId) => get('/api/matches/' + encodeURIComponent(matchId)),
        },

        // -------------------------------- Game --------------------------------
        games: {
            /** GET /api/games/{gameId} */
            get: (gameId) => get('/api/games/' + encodeURIComponent(gameId)),

            /** GET /api/games/{gameId}/state -> GameState */
            state: (gameId) => get('/api/games/' + encodeURIComponent(gameId) + '/state'),

            /**
             * POST /api/games/{gameId}/moves -> GameState
             * move: { type: 'CARD'|'MOVE'|'GRAB'|'WIN', card?, stack?, clientMoveId? }
             */
            move: (gameId, move) => post(
                '/api/games/' + encodeURIComponent(gameId) + '/moves',
                {
                    ...move,
                    clientMoveId: move.clientMoveId || makeMoveId(),
                }
            ),

            /** GET /api/games/{gameId}/moves/history -> { total, items } */
            history: (gameId) => get('/api/games/' + encodeURIComponent(gameId) + '/moves/history'),

            /** POST /api/games/{gameId}/resign -> Game */
            resign: (gameId) => post('/api/games/' + encodeURIComponent(gameId) + '/resign'),

            /** GET /api/games/{gameId}/result -> GameResult */
            result: (gameId) => get('/api/games/' + encodeURIComponent(gameId) + '/result'),
        },

        // ------------------------------- Replay -------------------------------
        replays: {
            /** GET /api/replays -> ReplayList */
            list: (params = {}) => get('/api/replays', {
                id: params.id,
                limit: params.limit || 20,
                offset: params.offset || 0,
            }),

            /** GET /api/replays/{replayId} -> Replay */
            get: (replayId) => get('/api/replays/' + encodeURIComponent(replayId)),

            /** GET /api/replays/{replayId}/frames -> { replayId, total, frames } */
            frames: (replayId, params = {}) => get(
                '/api/replays/' + encodeURIComponent(replayId) + '/frames',
                { from: params.from, to: params.to }
            ),
        },
    };
})();

function makeMoveId() {
    if (window.crypto && typeof window.crypto.randomUUID === 'function') {
        return window.crypto.randomUUID();
    }
    return 'move-' + Date.now() + '-' + Math.random().toString(16).slice(2);
}
