import logging
from contextlib import asynccontextmanager

import httpx

from . import config, redis_client

logger = logging.getLogger("auth_client")


async def user_name(player_id: int) -> str:
    cached = redis_client.cache_get(config.NAME_CACHE_KEY.format(player_id))
    if cached:
        return cached

    name = None
    try:
        async with httpx.AsyncClient(timeout=2) as client:
            resp = await client.get(f"{config.AUTH_URL}/users/{player_id}")
        if resp.status_code == 200:
            name = resp.json().get("username")
    except httpx.HTTPError as e:
        logger.warning("auth lookup for %s failed: %s", player_id, e)

    if not name:
        return f"Player {player_id}"

    redis_client.cache_set(config.NAME_CACHE_KEY.format(player_id), name, config.NAME_CACHE_TTL)
    return name
