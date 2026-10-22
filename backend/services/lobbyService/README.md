# lobbyService

Python/FastAPI сервис учёта присутствия игроков (контракт — `openapi.yaml`).
Использует SQLite, общий инстанс Valkey (через Sentinel) и auth-service
(публичный `GET /users/{id}` для имён, кэш в Valkey).

## Запуск

```bash
python -m venv .venv
.venv/bin/pip install -r requirements.txt
.venv/bin/uvicorn app.main:app --port 8082
```

Данные: `data/lobby.db` (создаётся автоматически).

## Конфигурация (env)

| Переменная | По умолчанию | Описание |
|---|---|---|
| `LOBBY_SERVICE_PORT` | 8082 | порт HTTP |
| `AUTH_URL` | http://localhost:8081 | auth-service (stub на вакансии либо боевой) |
| `LOBBY_DB_PATH` | `<сервис>/data/lobby.db` | путь к SQLite |
| `REDIS_SENTINEL_HOST/PORT` | localhost:26379 | Sentinel |
| `REDIS_MASTER_NAME` | mymaster | master-set |
| `REDIS_PASSWORD` | из gameService/secrets/secure_data.json | пароль к Valkey/Redis |
| `PRESENCE_TTL_SECONDS` | 60 | время жизни сессии присутствия между heartbeat |

## События (Pub/Sub)

Каналы: `presence.online`, `presence.offline` — JSON `{id, name, sessionId, status}`.
