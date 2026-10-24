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
        } catch (e) { }
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

        auth: {
            google: (code, redirectUri) => post('/api/auth/google', {
                code,
                redirect_uri: redirectUri,
            }, { auth: false }),

            me: () => get('/api/auth/me'),

            logout: () => del('/api/auth/logout'),

            user: (id) => get('/api/auth/users/' + encodeURIComponent(id), null, { auth: false }),
        },

        lobby: {
            openPresence: (status = 'in_lobby') => post('/api/lobby/presence', {
                status,
                clientVersion: CONFIG.clientVersion,
            }),

            closePresence: () => del('/api/lobby/presence').catch(() => null),

            heartbeat: (sessionId) => post(
                '/api/lobby/presence/' + encodeURIComponent(sessionId) + '/heartbeat',
                undefined
            ),

            players: (params = {}) => get('/api/lobby/players', {
                excludeSelf: params.excludeSelf !== false,
                limit: params.limit || 100,
                offset: params.offset || 0,
            }),
        },

        invites: {
            create: (toId, ttlSeconds = CONFIG.inviteTtlSeconds) => post('/api/invites', {
                toId,
                ttlSeconds,
            }),

            list: (params = {}) => get('/api/invites', {
                direction: params.direction || 'incoming',
                status: params.status || 'pending',
            }),

            get: (inviteId) => get('/api/invites/' + encodeURIComponent(inviteId)),

            accept: (inviteId) => post('/api/invites/' + encodeURIComponent(inviteId) + '/accept'),

            decline: (inviteId) => post('/api/invites/' + encodeURIComponent(inviteId) + '/decline'),

            cancel: (inviteId) => post('/api/invites/' + encodeURIComponent(inviteId) + '/cancel'),
        },

        matches: {
            list: (status = 'all') => get('/api/matches', { status }),

            get: (matchId) => get('/api/matches/' + encodeURIComponent(matchId)),
        },

        games: {
            get: (gameId) => get('/api/games/' + encodeURIComponent(gameId)),

            state: (gameId) => get('/api/games/' + encodeURIComponent(gameId) + '/state'),

            move: (gameId, move) => post(
                '/api/games/' + encodeURIComponent(gameId) + '/moves',
                {
                    ...move,
                    clientMoveId: move.clientMoveId || makeMoveId(),
                }
            ),

            history: (gameId) => get('/api/games/' + encodeURIComponent(gameId) + '/moves/history'),

            resign: (gameId) => post('/api/games/' + encodeURIComponent(gameId) + '/resign'),

            result: (gameId) => get('/api/games/' + encodeURIComponent(gameId) + '/result'),
        },
    };
})();

function makeMoveId() {
    if (window.crypto && typeof window.crypto.randomUUID === 'function') {
        return window.crypto.randomUUID();
    }
    return 'move-' + Date.now() + '-' + Math.random().toString(16).slice(2);
}
