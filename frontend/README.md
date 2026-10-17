# Дурак — веб-фронтенд (онлайн-клиент)

Браузерный клиент игры «Дурак» на чистом HTML/CSS/JS (без фреймворков и
сборки). Ассеты — из оригинального `DurakDotnet/DurakOnCSharp`: спрайт карт
`Cards.png`, рубашка `Back.png` и звуки `accept/invite/text/win.wav`.

В отличие от первой версии, клиент **не играет с локальным ботом**. Он работает
с микросервисами по контрактам из `backend/`:

| Контракт | Что использует клиент |
|---|---|
| `backend/gateways/apiGateway/openapi.yaml` | HTTP API (`/api/auth`, `/api/lobby`, `/api/invites`, `/api/matches`, `/api/games`, `/api/replays`) |
| `backend/gateways/realtimeGateway/openapi.yaml` | WebSocket `/ws?token=...`, события `invite.*`, `game.*`, `lobby.*` |
| `backend/services/authService/docs/api.md` | Google OAuth2 и opaque accessToken |
| `backend/services/lobbyService/openapi.yaml` | присутствие и heartbeat |
| `backend/services/matchmakingService/openapi.yaml` | приглашения и матчи |
| `backend/services/gameService/openapi.yaml` | состояние партии и ходы |
| `backend/services/replayService/openapi.yaml` | список повторов и кадры |

## Запуск

```bash
cd frontend
node server.js          # http://localhost:5500
node server.js 8080     # другой порт
```

`server.js` не только раздаёт статику, но и проксирует запросы, чтобы фронтенд
и гейтвеи были на одном origin (без CORS):

* `/api/*` → `http://localhost:8080` (`api-gateway`);
* `/ws`    → `http://localhost:8081/ws` (`realtime-gateway`).

Адреса можно переопределить:

```bash
API_TARGET=http://localhost:8080 WS_TARGET=http://localhost:8081 node server.js
```

Просто открыть `index.html` файлом тоже можно — тогда клиент по умолчанию
пойдёт на `http://localhost:8080` и `ws://localhost:8081/ws`, но API-гейтвей
должен разрешать CORS. Для обычной разработки используйте `node server.js`.

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

1. Создайте OAuth 2.0 Client ID (Web application) в Google Cloud Console.
2. Добавьте `http://localhost:5500/` в **Authorized redirect URIs**.
3. Пропишите `googleClientId` в `js/config.js` (или `window.DURAK_CONFIG`).
4. Нажмите «Войти через Google» — клиент отправит authorization code на
   `POST /api/auth/google` и сохранит выданный opaque `accessToken`.

Пока `googleClientId` не задан, доступен ручной вход: вставьте `accessToken`,
полученный от auth-service (`POST /auth/google`), в поле на экране входа.
Токен хранится в `localStorage` и передаётся как
`Authorization: Bearer <accessToken>`.

## Что реализовано

**Вход (`Auth`):**

* Google OAuth2 (`code` → `POST /api/auth/google`);
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
* активные матчи `GET /api/matches?status=active` и кнопка «Продолжить партию»;
* список повторов `GET /api/replays`.

**Партия (`Game`):**

* загрузка `GET /api/games/{gameId}/state`;
* ходы `POST /api/games/{gameId}/moves`:
  `CARD` (карта + номер стопки), `MOVE`, `GRAB`, `WIN`;
* сервер — источник истины: нелегальный ход возвращает `409`, клиент
  перечитывает состояние;
* обновления по WS `game.state` / `game.finished` + polling как fallback;
* сдача `POST /api/games/{gameId}/resign`;
* результат `GET /api/games/{gameId}/result`, победа/поражение и конфетти;
* таймер хода (30 секунд) с авто-ходом при истечении.

**Повторы (`Replay`):**

* `GET /api/replays/{replayId}/frames` — пошаговые состояния;
* если сервер вернул только метаданные, кадры собираются из
  `GET /api/replays/{replayId}` (`initialState` + `moves`);
* кнопки «Назад / Вперёд», подписи ходов.

## Структура

```
frontend/
├── index.html          экраны входа, меню, игры и повторов
├── server.js           статика + прокси /api и /ws
├── css/style.css       оформление
├── js/config.js        адреса API/WS и параметры OAuth
├── js/api.js           HTTP-клиент api-gateway, ApiError, токен
├── js/realtime.js      WebSocket-клиент realtime-gateway
├── js/auth.js          Google OAuth / accessToken / выход
├── js/lobby.js         присутствие, игроки, приглашения, матчи, повторы
├── js/game.js          игровой экран и серверные ходы
├── js/replay.js        просмотр кадров повтора
├── js/state.js         адаптер GameState → модель отрисовки
├── js/render.js        отрисовка поля
├── js/cards.js         спрайт карт и названия карт
├── js/engine.js        правила локальной сессии (только fallback повторов)
├── js/bot.js           старый локальный бот (в онлайн-клиенте не подключён)
├── js/audio.js         звуки
├── js/app.js           навигация, модалки, toast, загрузка
└── assets/             cards.png, back.png, *.wav
```

## Ограничения

* Для Google-входа бэкенд должен уметь обменивать code на профиль Google
  (auth-service), а api-gateway — проксировать `/api/auth/google`.
* Присутствие и матчи живут на сервере; если api-gateway не реализован,
  интерфейс покажет ошибки сети, а не локальную заглушку.
* Локальные повторы в `localStorage` больше не пишутся — источник повторов
  replay-service.
