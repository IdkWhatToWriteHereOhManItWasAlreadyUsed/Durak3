/**
 * Шаблон js/oauth.js: при старте контейнера nginx заменяет 684956786183-jso6ts1i38lqh5l8mep8tuspn99th3d2.apps.googleusercontent.com
 * значение из .env (envsubst) и записывает результат в js/oauth.js.
 */
window.DURAK_CONFIG = Object.assign({}, window.DURAK_CONFIG || {}, {
    // OAuth2 client id приложения из Google Cloud Console (.env: GOOGLE_CLIENT_ID)
    googleClientId: '684956786183-jso6ts1i38lqh5l8mep8tuspn99th3d2.apps.googleusercontent.com',
});
