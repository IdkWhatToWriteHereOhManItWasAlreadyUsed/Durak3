/**
 * Конфигурация клиента.
 *
 * Переопределить значения можно до подключения этого файла:
 *
 *   <script>
 *       window.DURAK_CONFIG = {
 *           apiBase: 'http://localhost:8222',
 *           wsUrl: 'ws://localhost:8090/ws',
 *           googleClientId: '....apps.googleusercontent.com',
 *       };
 *   </script>
 *
 * По умолчанию фронтенд работает с тем же origin: server.js проксирует
 * /api/* в api-gateway, а /ws — в realtime-gateway (см. frontend/server.js).
 */
const CONFIG = Object.assign({
    // HTTP-гейтвей (BFF), см. backend/gateways/apiGateway/openapi.yaml
    apiBase: '',
    // WebSocket-гейтвей, см. backend/gateways/realtimeGateway/openapi.yaml
    wsUrl: '',
    // OAuth2 client id из Google Cloud Console.
    googleClientId: '',
    // URI, зарегистрированный в Google. Пусто = вычисляется из текущего
    // адреса страницы (normalizeConfig ниже), т.е. origin + pathname.
    oauthRedirectUri: '',
    clientVersion: '2.0.0',

    heartbeatMs: 15000,
    playersPollMs: 5000,
    invitesPollMs: 2000,
    outgoingInvitePollMs: 1500,
    gamePollMs: 3000,
    inviteTtlSeconds: 30,
    turnLimitSeconds: 30,
    requestTimeoutMs: 15000,
}, window.DURAK_CONFIG || {});

(function normalizeConfig() {
    const served = location.protocol === 'http:' || location.protocol === 'https:';

    if (!CONFIG.apiBase) {
        CONFIG.apiBase = served ? location.origin : 'http://localhost:8222';
    }
    CONFIG.apiBase = CONFIG.apiBase.replace(/\/+$/, '');

    if (!CONFIG.wsUrl) {
        CONFIG.wsUrl = served
            ? (location.protocol === 'https:' ? 'wss://' : 'ws://') + location.host + '/ws'
            : 'ws://localhost:8090/ws';
    }

    if (!CONFIG.oauthRedirectUri) {
        CONFIG.oauthRedirectUri = served
            ? location.origin + location.pathname.replace(/index\.html$/, '')
            : '';
    }
})();
