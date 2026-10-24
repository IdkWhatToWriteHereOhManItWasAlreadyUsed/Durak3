const CONFIG = Object.assign({
    apiBase: '',
    wsUrl: '',
    googleClientId: '',
    oauthRedirectUri: '',
    clientVersion: '2.0.0',

    heartbeatMs: 15000,
    playersPollMs: 5000,
    invitesPollMs: 2000,
    outgoingInvitePollMs: 1500,
    gamePollMs: 3000,
    inviteTtlSeconds: 10,
    turnLimitSeconds: 120,
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
