# matchmakingService

Python/FastAPI сервис приглашений и матчей (контракт — `openapi.yaml`).
При принятии приглашения вызывается game-service (`POST /games`) и создаётся матч.
Хранение — Postgres, события — Pub/Sub общего Valkey (через Sentinel).

## Запуск

```bash
python -m venv .venv
.venv/bin/pip install -r requirements.txt
.venv/bin/uvicorn app.main:app --port 8083
```

## Конфигурация (env)

| Переменная | По умолчанию | Описание |
|---|---|---|
| `MATCHMAKING_SERVICE_PORT` | 8083 | порт HTTP |
| `AUTH_URL` | http://localhost:8081 | auth-service (имена) |
| `GAME_URL` | http://localhost:8101 | game-service |
| `MATCHMAKING_DSN` | postgres://postgres:postgres@localhost:5432/matchmaking | Postgres |
| `REDIS_SENTINEL_HOST/PORT` | localhost:26379 | Sentinel |
| `REDIS_MASTER_NAME` | mymaster | master-set |
| `REDIS_PASSWORD` | из gameService/secrets/secure_data.json | пароль к Valkey/Redis |

Таблицы (`invites`, `matches`, `match_players`) создаются при старте автоматически.

## События (Pub/Sub)

Каналы: `invite.created`, `invite.accepted`, `invite.declined`, `game.started`.
Событие `game.started` содержит `{matchId, gameId, players}`.
