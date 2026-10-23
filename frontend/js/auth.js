/**
 * Аутентификация через auth-service / api-gateway.
 *
 * Единственный способ входа — Google OAuth2:
 *   POST /api/auth/google { code, redirect_uri } -> { id, email, username, accessToken }
 * Полученный accessToken — opaque UUID (НЕ JWT), хранится в localStorage
 * и отправляется как `Authorization: Bearer <token>`.
 *
 * googleClientId берётся из переменной окружения GOOGLE_CLIENT_ID
 * (см. js/oauth.js) либо из window.DURAK_CONFIG / js/config.js.
 */
const Auth = (() => {
    const USER_KEY = 'durak.user';
    const OAUTH_STATE_KEY = 'durak.oauth.state';
    const OAUTH_REDIRECT_KEY = 'durak.oauth.redirect';

    const el = {};
    let user = null;

    function init() {
        el.note = document.getElementById('loginNote');
        el.google = document.getElementById('btnGoogleLogin');
        el.userName = document.getElementById('userName');
        el.userId = document.getElementById('userId');
        el.logout = document.getElementById('btnLogout');
        el.conn = document.getElementById('connStatus');

        if (el.google) el.google.addEventListener('click', loginWithGoogle);
        if (el.logout) el.logout.addEventListener('click', logout);

        if (el.google && !CONFIG.googleClientId) {
            el.google.disabled = true;
            note('Google-вход выключен: не задан GOOGLE_CLIENT_ID (см. .env.example).');
        }

        Api.onUnauthorized(handleExpired);
        Realtime.on('session-expired', handleExpired);
        Realtime.onStatus(renderConnection);

        bootstrap();
    }

    async function bootstrap() {
        const params = new URLSearchParams(location.search);
        let loginError = '';

        if (params.get('error')) {
            loginError = 'Google вернул ошибку: ' + params.get('error');
            cleanUrl();
        }

        const code = params.get('code');
        if (!loginError && code) {
            const expectedState = sessionStorage.getItem(OAUTH_STATE_KEY);
            const actualState = params.get('state');
            const redirectUri = sessionStorage.getItem(OAUTH_REDIRECT_KEY) || CONFIG.oauthRedirectUri;

            if (expectedState && actualState && expectedState !== actualState) {
                loginError = 'Неверный state OAuth-ответа. Попробуйте войти ещё раз.';
                cleanUrl();
            } else {
                App.showLoading('Вход через Google…');
                try {
                    const response = await Api.auth.google(code, redirectUri);
                    applySession(response);
                } catch (e) {
                    loginError = e.message || 'Не удалось войти через Google';
                } finally {
                    App.hideLoading();
                    cleanUrl();
                    sessionStorage.removeItem(OAUTH_STATE_KEY);
                    sessionStorage.removeItem(OAUTH_REDIRECT_KEY);
                }
            }
        }

        if (!user && Api.getToken()) {
            try {
                const me = await Api.auth.me();
                user = { ...readStoredUser(), ...me };
                saveUser();
            } catch (e) {
                Api.setToken('');
                clearUser();
            }
        }

        if (user) onAuthenticated(false);
        else showLogin(loginError);
    }

    function loginWithGoogle() {
        if (!CONFIG.googleClientId) {
            note('Google-вход выключен: не задан GOOGLE_CLIENT_ID (см. .env.example).');
            return;
        }
        if (!CONFIG.oauthRedirectUri) {
            note('Откройте фронтенд по http(s), чтобы Google мог вернуть code.');
            return;
        }

        const state = randomState();
        sessionStorage.setItem(OAUTH_STATE_KEY, state);
        sessionStorage.setItem(OAUTH_REDIRECT_KEY, CONFIG.oauthRedirectUri);

        const url = new URL('https://accounts.google.com/o/oauth2/v2/auth');
        url.searchParams.set('client_id', CONFIG.googleClientId);
        url.searchParams.set('redirect_uri', CONFIG.oauthRedirectUri);
        url.searchParams.set('response_type', 'code');
        url.searchParams.set('scope', 'openid email profile');
        url.searchParams.set('state', state);
        url.searchParams.set('prompt', 'select_account');
        location.href = url.toString();
    }

    function applySession(response) {
        if (!response || !response.accessToken) {
            throw new Error('auth-service не вернул accessToken');
        }
        Api.setToken(response.accessToken);
        user = {
            id: response.id,
            email: response.email,
            username: response.username || response.email || ('Игрок #' + response.id),
        };
        saveUser();
    }

    function onAuthenticated(showToast) {
        updateUserChip();
        Realtime.connect();
        App.goMenu();
        if (showToast) App.toast('Вы вошли как ' + (user.username || ('#' + user.id)));
    }

    async function logout() {
        await Lobby.leave();
        try { await Api.auth.logout(); } catch (e) { /* токен мог уже истечь */ }
        Api.setToken('');
        clearUser();
        Realtime.disconnect();
        updateUserChip();
        App.showScreen('login');
        showLogin('Вы вышли из аккаунта.');
    }

    function handleExpired() {
        if (!user && !Api.getToken()) return;
        Api.setToken('');
        clearUser();
        Realtime.disconnect();
        if (typeof Lobby !== 'undefined') Lobby.leave();
        updateUserChip();
        App.showScreen('login');
        showLogin('Сессия истекла. Войдите снова.');
    }

    function showLogin(message) {
        App.showScreen('login');
        note(message);
        if (el.conn) el.conn.textContent = 'Не подключено';
    }

    function note(text) {
        if (el.note) el.note.textContent = text || '';
    }

    function updateUserChip() {
        if (el.userName) el.userName.textContent = user ? (user.username || ('Игрок #' + user.id)) : '';
        if (el.userId) el.userId.textContent = user ? '#' + user.id : '';
    }

    function renderConnection(status) {
        if (!el.conn) return;
        const labels = {
            connecting: 'Подключение…',
            reconnecting: 'Переподключение…',
            open: 'Онлайн',
            closed: 'Офлайн',
        };
        el.conn.textContent = labels[status] || status;
        el.conn.dataset.status = status;
    }

    function saveUser() {
        try { localStorage.setItem(USER_KEY, JSON.stringify(user)); } catch (e) { /* ignore */ }
    }

    function readStoredUser() {
        try { return JSON.parse(localStorage.getItem(USER_KEY) || 'null') || {}; } catch (e) { return {}; }
    }

    function clearUser() {
        user = null;
        try { localStorage.removeItem(USER_KEY); } catch (e) { /* ignore */ }
    }

    function cleanUrl() {
        try {
            history.replaceState(null, '', location.pathname + location.hash);
        } catch (e) { /* ignore */ }
    }

    function randomState() {
        if (window.crypto && typeof window.crypto.randomUUID === 'function') return window.crypto.randomUUID();
        return Math.random().toString(36).slice(2) + Date.now().toString(36);
    }

    return {
        init,
        logout,
        user: () => user,
        id: () => user ? user.id : null,
        token: () => Api.getToken(),
        showLogin,
    };
})();
