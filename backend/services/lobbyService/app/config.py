import json
import os
from pathlib import Path

SERVICE_ROOT = Path(__file__).resolve().parents[1]
SERVICES_ROOT = SERVICE_ROOT.parent

APP_PORT = int(os.getenv("LOBBY_SERVICE_PORT", "8082"))

AUTH_URL = os.getenv("AUTH_URL", "http://localhost:8081")

SQLITE_PATH = os.getenv("LOBBY_DB_PATH", str(SERVICE_ROOT / "data" / "lobby.db"))

REDIS_SENTINEL_HOST = os.getenv("REDIS_SENTINEL_HOST", "localhost")
REDIS_SENTINEL_PORT = int(os.getenv("REDIS_SENTINEL_PORT", "26379"))
REDIS_MASTER_NAME = os.getenv("REDIS_MASTER_NAME", "mymaster")
REDIS_DB = int(os.getenv("REDIS_DB", "0"))
REDIS_SOCKET_TIMEOUT = int(os.getenv("REDIS_SOCKET_TIMEOUT", "3"))


def _redis_password() -> str:
    env = os.getenv("REDIS_PASSWORD")
    if env:
        return env
    secure = SERVICES_ROOT / "gameService" / "secrets" / "secure_data.json"
    if secure.exists():
        cfg = json.loads(secure.read_text())["redis_settings"]["redis-client-1"]
        return cfg["password"]
    return ""


REDIS_PASSWORD = _redis_password()

PRESENCE_TTL_SECONDS = int(os.getenv("PRESENCE_TTL_SECONDS", "60"))

NAME_CACHE_KEY = "names:{}"
NAME_CACHE_TTL = 300
