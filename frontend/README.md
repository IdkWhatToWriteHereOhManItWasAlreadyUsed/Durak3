# Дурак — веб-фронтенд (онлайн-клиент)

Браузерный клиент игры «Дурак» на чистом HTML/CSS/JS (без фреймворков и
сборки). Ассеты: спрайт карт `cards.png`, рубашка `back.png` и звуки
`accept/invite/text/win.wav`.

Клиент не играет с локальным ботом. Он работает с микросервисами по
контрактам из `backend/`:

| Контракт | Что использует клиент |
|---|---|
| `backend/gateways/apiGateway/openapi.yaml` | HTTP API (`/api/auth`, `/api/lobby`, `/api/invites`, `/api/matches`, `/api/games`) |
| `backend/gateways/realtimeGateway/openapi.yaml` | WebSocket `/ws?token=...`, события `invite.*`, `game.*`, `lobby.*` |
| `backend/services/authService/docs/api.md` | Google OAuth2, opaque accessToken и `avatar_url` |
| `backend/services/lobbyService/openapi.yaml` | присутствие и heartbeat |
| `backend/services/matchmakingService/openapi.yaml` | приглашения и матчи |
| `backend/services/gameService/openapi.yaml` | состояние партии и ходы |

## Запуск

```bash
cd frontend
node server.js          # http://localhost:5500
node server.js 8080     # другой порт
```

`server.js` не только раздаёт статику, но и проксирует запросы, чтобы фронтенд
и гейтвеи были на одном origin (без CORS):

* `/api/*` → `http://localhost:8222` (`api-gateway`);
* `/ws`    → `http://localhost:8090/ws` (`realtime-gateway`).

Адреса можно переопределить:

```bash
API_TARGET=http://localhost:8080 WS_TARGET=http://localhost:8081 node server.js
```

### Конфигурация

Все настройки лежат в `js/config.js`. Их можно переопределить глобальным
объектом до подключения скрипта (например, отдельным `config.local.js`):

```html
<script>
    window.DURAK_CONFIG = {
        apiBase: 'http://localhost:8080',
        wsUrl: 'ws://localhost:8081/ws',
        googleClientId: 'xxxx.apps.googleusercontent.com',
        oauthRedirectUri: 'http://localhost:5500/',
    };
</script>
```

### Вход через Google

Вход в игру — только через Google. Client id не прописывается в коде руками,
а приходит из окружения:

* `node server.js` читает переменную окружения `GOOGLE_CLIENT_ID` и отдаёт её
  фронтенду как `js/oauth.js` (значение, как и весь `.env`, задаётся в корне);
* в docker-compose компонент `nginx` генерирует `js/oauth.js` при старте
  контейнера из шаблона `js/oauth.js.template` (envsubst).

Осталось один раз настроить Google:

1. Создайте OAuth 2.0 Client ID (Web application) в Google Cloud Console.
2. Добавьте туда адреса фронтенда в **Authorized redirect URIs**:
   `http://localhost:5500/` (node server.js) и `http://localhost:8080/` (nginx).
3. Пропишите client id в `.env` рядом с docker-compose (`GOOGLE_CLIENT_ID=...`,
   см. `.env.example`).
4. Кнопка «Войти через Google» станет активной — клиент отправит authorization
   code на `POST /api/auth/google` и сохранит выданный opaque `accessToken`.

Ответ `POST /api/auth/google` содержит профиль пользователя:

```json
{
  "id": 1,
  "email": "user@gmail.com",
  "username": "Дмитрий Лазута",
  "avatar_url": "https://lh3.googleusercontent.com/a/...",
  "accessToken": "b224a7c6-..."
}
```

Аватарка Google (`avatar_url`) сохраняется вместе с профилем и показывается
в шапке главного меню рядом с именем. Проверка сессии при перезагрузке
страницы (`GET /api/auth/me`) также обновляет `avatar_url`.

Token хранится в `localStorage` и передаётся как
`Authorization: Bearer <accessToken>` (WebSocket — как `?token=...`).

Пока `GOOGLE_CLIENT_ID` не задан, кнопка остаётся заблокированной, а на экране
входа выводится подсказка.

## Что реализовано

**Вход (`Auth`):**

* Google OAuth2 (`code` → `POST /api/auth/google`);
* отображение профиля: имя, id и аватарка Google;
* проверка сессии через `GET /api/auth/me` при перезагрузке;
* выход через `DELETE /api/auth/logout`;
* обработка `401` и `SESSION_EXPIRED` (возврат на экран входа).

**Лобби (`Lobby`):**

* `POST /api/lobby/presence` при входе в меню и `DELETE` при выходе;
* heartbeat `POST /api/lobby/presence/{sessionId}/heartbeat`;
* список онлайн-игроков `GET /api/lobby/players` + обновление по WS
  `lobby.players` / `presence.*`;
* приглашение игрока `POST /api/invites`, ожидание ответа с таймером,
  отмена `POST /api/invites/{id}/cancel`;
* входящие приглашения `GET /api/invites?direction=incoming&status=pending`
  и WS `invite.created`; принятие/отклонение через
  `POST /api/invites/{id}/accept|decline`;
* активные матчи `GET /api/matches?status=active` и кнопка «Продолжить партию».

**Партия (`Game`):**

* загрузка `GET /api/games/{gameId}/state`;
* ходы `POST /api/games/{gameId}/moves`:
  `CARD` (карта + номер стопки), `MOVE`, `GRAB`, `WIN`;
* сервер — источник истины: нелегальный ход возвращает `409`, клиент
  перечитывает состояние;
* обновления по WS `game.state` / `game.finished` + polling как fallback;
* сдача `POST /api/games/{gameId}/resign`;
* автоматическая сдача: `resign` отправляется при истечении таймера хода
  (30 секунд), а также при закрытии вкладки/браузера (keepalive-запрос);
* результат `GET /api/games/{gameId}/result`, победа/поражение и конфетти.

**Звук (`Sound`):**

* Web Audio API: файлы декодируются один раз и кешируются в буферах;
* никаких автопроигрываний при загрузке — AudioContext активируется
  только после действия пользователя.

## Структура

```
frontend/
├── index.html          экраны входа, меню и игры
├── server.js           статика + прокси /api и /ws
├── css/style.css       оформление
├── js/config.js        адреса API/WS и параметры OAuth
├── js/oauth.js         googleClientId из окружения (генерируется, см. шаблон)
├── js/api.js           HTTP-клиент api-gateway, ApiError, токен
├── js/realtime.js      WebSocket-клиент realtime-gateway
├── js/auth.js          Google OAuth / профиль с аватаркой / выход
├── js/lobby.js         присутствие, игроки, приглашения, матчи
├── js/game.js          игровой экран и серверные ходы
├── js/state.js         адаптер GameState → модель отрисовки
├── js/render.js        отрисовка поля
├── js/cards.js         спрайт карт и названия карт
├── js/audio.js         звуки (Web Audio API)
├── js/app.js           навигация, модалки, toast, загрузка
└── assets/             cards.png, back.png, *.wav
```

## Ограничения

* Для Google-входа бэкенд должен уметь обменивать code на профиль Google
  (auth-service), а api-gateway — проксировать `/api/auth/google`.
* Присутствие и матчи живут на сервере; если api-gateway не реализован,
  интерфейс покажет ошибки сети, а не локальную заглушку.
