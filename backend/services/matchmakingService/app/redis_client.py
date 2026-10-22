import json
import logging

import redis
from redis.sentinel import Sentinel

from . import config

logger = logging.getLogger("redis")

_sentinel = Sentinel(
    [(config.REDIS_SENTINEL_HOST, config.REDIS_SENTINEL_PORT)],
    sentinel_kwargs={"socket_timeout": config.REDIS_SOCKET_TIMEOUT},
    socket_timeout=config.REDIS_SOCKET_TIMEOUT,
    password=config.REDIS_PASSWORD or None,
    db=config.REDIS_DB,
    decode_responses=True,
)

_master: redis.Redis | None = None


def master() -> redis.Redis:
    global _master
    if _master is None:
        _master = _sentinel.master_for(config.REDIS_MASTER_NAME)
    return _master


def cache_get(key: str) -> str | None:
    try:
        return master().get(key)
    except redis.RedisError as e:
        logger.warning("redis get failed: %s", e)
        return None


def cache_set(key: str, value: str, ttl: int) -> None:
    try:
        master().set(key, value, ex=ttl)
    except redis.RedisError as e:
        logger.warning("redis set failed: %s", e)


def publish(channel: str, payload: dict) -> None:
    try:
        master().publish(channel, json.dumps(payload, ensure_ascii=False))
    except redis.RedisError as e:
        logger.warning("redis publish to %s failed: %s", channel, e)
