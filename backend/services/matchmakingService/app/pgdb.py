import psycopg
from psycopg import rows
from psycopg_pool import AsyncConnectionPool

from . import config

SCHEMA = """
CREATE TABLE IF NOT EXISTS invites (
    id         uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    from_id    bigint NOT NULL,
    from_name  TEXT NOT NULL DEFAULT '',
    to_id      bigint NOT NULL,
    to_name    TEXT NOT NULL DEFAULT '',
    status     TEXT NOT NULL DEFAULT 'pending',
    created_at timestamptz NOT NULL DEFAULT now(),
    expires_at timestamptz NOT NULL
);
CREATE TABLE IF NOT EXISTS matches (
    id          uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    game_id     uuid NOT NULL,
    status      TEXT NOT NULL DEFAULT 'active',
    created_at  timestamptz NOT NULL DEFAULT now(),
    finished_at timestamptz
);
CREATE TABLE IF NOT EXISTS match_players (
    match_id          uuid REFERENCES matches(id) ON DELETE CASCADE,
    player_id         bigint NOT NULL,
    name              TEXT NOT NULL DEFAULT '',
    seat              smallint NOT NULL,
    is_first_attacker boolean NOT NULL,
    PRIMARY KEY (match_id, seat)
);
CREATE INDEX IF NOT EXISTS idx_match_players_player ON match_players(player_id);
CREATE INDEX IF NOT EXISTS idx_invites_from_to ON invites(from_id, to_id, status);
CREATE INDEX IF NOT EXISTS idx_invites_to ON invites(to_id, status);
"""

pool: AsyncConnectionPool


async def init() -> None:
    global pool
    pool = AsyncConnectionPool(
        config.POSTGRES_DSN,
        kwargs={"row_factory": rows.dict_row, "autocommit": False},
    )
    async with pool.connection() as conn:
        await conn.execute(SCHEMA)


async def close() -> None:
    await pool.close()
