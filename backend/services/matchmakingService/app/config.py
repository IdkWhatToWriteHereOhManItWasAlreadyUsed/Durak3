import os
from pathlib import Path

SERVICE_ROOT = Path(__file__).resolve().parents[1]
SERVICES_ROOT = SERVICE_ROOT.parent

APP_PORT = int(os.getenv("MATCHMAKING_SERVICE_PORT", "8083"))

AUTH_URL = os.getenv("AUTH_URL", "http://localhost:8081")
GAME_URL = os.getenv("GAME_URL", "http://localhost:8101")

POSTGRES_DSN = os.getenv("MATCHMAKING_DSN",
                         "host=localhost port=5432 dbname=matchmaking user=postgres password=postgres")

REDIS_SENTINEL_HOST = os.getenv("REDIS_SENTINEL_HOST", "localhost")
REDIS_SENTINEL_PORT = int(os.getenv("REDIS_SENTINEL_PORT", "26379"))
REDIS_MASTER_NAME = os.getenv("REDIS_MASTER_NAME", "mymaster")
REDIS_DB = int(os.getenv("REDIS_DB", "0"))
REDIS_SOCKET_TIMEOUT = int(os.getenv("REDIS_SOCKET_TIMEOUT", "3"))


REDIS_PASSWORD = os.getenv("REDIS_PASSWORD", "")

NAME_CACHE_KEY = "names:{}"
NAME_CACHE_TTL = 300
