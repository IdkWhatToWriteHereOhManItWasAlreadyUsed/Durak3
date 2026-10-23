/**
 * OAuth-конфиг для config.js.
 *
 * Файл перезаписывается автоматически:
 *   - node server.js — на лету из переменных окружения (GOOGLE_CLIENT_ID из .env);
 *   - docker (nginx) — при старте контейнера из js/oauth.js.template (envsubst).
 *
 * Если сервисы запускаются напрямую (открытие index.html файлом), здесь можно
 * прописать client id вручную либо через window.DURAK_CONFIG до этого скрипта.
 */
window.DURAK_CONFIG = Object.assign({}, window.DURAK_CONFIG || {}, {
    // OAuth2 client id приложения из Google Cloud Console
    googleClientId: '',
});
