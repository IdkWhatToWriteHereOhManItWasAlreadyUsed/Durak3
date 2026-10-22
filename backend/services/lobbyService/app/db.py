from datetime import datetime, timedelta, timezone
from typing import Any

import aiosqlite

from . import config

_SCHEMA = """
CREATE TABLE IF NOT EXISTS presence_sessions (
    session_id     TEXT PRIMARY KEY,
    player_id      INTEGER NOT NULL,
    name           TEXT NOT NULL,
    status         TEXT NOT NULL,
    client_version TEXT,
    created_at     TEXT NOT NULL,
    last_seen      TEXT NOT NULL,
    expires_at     TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_presence_sessions_player
    ON presence_sessions(player_id);
"""

_db: aiosqlite.Connection


def now() -> str:
    return datetime.now(timezone.utc).isoformat()


def after_ttl() -> str:
    return (datetime.now(timezone.utc) + timedelta(seconds=config.PRESENCE_TTL_SECONDS)).isoformat()


async def init(db_path: str) -> None:
    global _db
    from pathlib import Path
    Path(db_path).parent.mkdir(parents=True, exist_ok=True)
    _db = await aiosqlite.connect(db_path)
    _db.row_factory = aiosqlite.Row
    await _db.executescript(_SCHEMA)
    await _db.commit()


async def close() -> None:
    await _db.close()


async def open_session(session_id: str, player_id: int, name: str, status: str, client_version: str | None) -> dict:
    created = now()
    expires = after_ttl()
    await _db.execute(
        "INSERT INTO presence_sessions(session_id, player_id, name, status, client_version, created_at, last_seen, expires_at)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
        (session_id, player_id, name, status, client_version, created, created, expires),
    )
    await _db.commit()
    return {
        "sessionId": session_id,
        "id": player_id,
        "name": name,
        "status": status,
        "lastSeen": created,
        "expiresAt": expires,
    }


async def get_alive(session_id: str) -> aiosqlite.Row | None:
    async with _db.execute(
        "SELECT * FROM presence_sessions WHERE session_id = ? AND expires_at > ?",
        (session_id, now()),
    ) as cursor:
        return await cursor.fetchone()


async def refresh(session_id: str) -> aiosqlite.Row | None:
    if not await get_alive(session_id):
        return None
    last = now()
    await _db.execute(
        "UPDATE presence_sessions SET last_seen = ?, expires_at = ? WHERE session_id = ?",
        (last, after_ttl(), session_id),
    )
    await _db.commit()
    async with _db.execute("SELECT * FROM presence_sessions WHERE session_id = ?", (session_id,)) as cursor:
        return await cursor.fetchone()


async def close_session(session_id: str) -> aiosqlite.Row | None:
    row = await get_alive(session_id)
    if not row:
        return None
    await _db.execute("DELETE FROM presence_sessions WHERE session_id = ?", (session_id,))
    await _db.commit()
    return row


async def list_online(exclude_player: int | None, limit: int, offset: int) -> tuple[int, list[dict]]:
    params: list[Any] = []
    where = "expires_at > ?"
    params.append(now())
    if exclude_player is not None:
        where += " AND player_id != ?"
        params.append(exclude_player)

    sql = f"SELECT count(*) FROM presence_sessions WHERE {where}"
    async with _db.execute(sql, params) as cursor:
        (total,) = await cursor.fetchone()

    sql = f"SELECT * FROM presence_sessions WHERE {where} ORDER BY created_at LIMIT ? OFFSET ?"
    params += [limit, offset]
    async with _db.execute(sql, params) as cursor:
        rows = await cursor.fetchall()

    return total, [
        {
            "sessionId": r["session_id"],
            "id": r["player_id"],
            "name": r["name"],
            "status": r["status"],
            "lastSeen": r["last_seen"],
            "expiresAt": r["expires_at"],
        }
        for r in rows
    ]


async def player_last_session(player_id: int) -> aiosqlite.Row | None:
    async with _db.execute(
        "SELECT * FROM presence_sessions WHERE player_id = ? ORDER BY created_at DESC LIMIT 1",
        (player_id,),
    ) as cursor:
        return await cursor.fetchone()
