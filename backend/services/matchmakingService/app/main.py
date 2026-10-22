import logging
from contextlib import asynccontextmanager
from datetime import datetime, timezone
from uuid import UUID, uuid4

import httpx
from fastapi import FastAPI, HTTPException, Query, Request
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field

from . import auth_client, config, pgdb, redis_client

logger = logging.getLogger("matchmaking")

ERROR_CODES = {
    403: "FORBIDDEN",
    404: "NOT_FOUND",
    409: "CONFLICT",
    422: "UNPROCESSABLE",
    503: "SERVICE_UNAVAILABLE",
    500: "INTERNAL_ERROR",
}


@asynccontextmanager
async def lifespan(app: FastAPI):
    logging.basicConfig(level=logging.INFO)
    await pgdb.init()
    logger.info("matchmaking started on :%d", config.APP_PORT)
    yield
    await pgdb.close()


class CreateInviteRequest(BaseModel):
    fromId: int
    toId: int
    ttlSeconds: int = Field(default=30, ge=5, le=120)


class ActorRequest(BaseModel):
    id: int


app = FastAPI(title="Durak Matchmaking Service", version="1.1.0", lifespan=lifespan)


@app.exception_handler(HTTPException)
async def http_error(request: Request, exc: HTTPException):
    return JSONResponse(
        {"code": ERROR_CODES.get(exc.status_code, "ERROR"), "message": str(exc.detail)},
        status_code=exc.status_code,
    )


@app.exception_handler(RequestValidationError)
async def validation_error(request: Request, exc: RequestValidationError):
    first = exc.errors()[0].get("msg", "invalid request") if exc.errors() else "invalid request"
    return JSONResponse({"code": "UNPROCESSABLE", "message": first}, status_code=422)


@app.get("/health")
async def health():
    return {"status": "ok"}


def utcnow() -> datetime:
    return datetime.now(timezone.utc)


def _is_timed_out(invite) -> bool:
    exp = invite["expires_at"]
    return isinstance(exp, datetime) and exp <= utcnow()


def _invite_dict(row) -> dict:
    status = row["status"]
    if status == "pending" and _is_timed_out(row):
        status = "expired"
    return {
        "id": str(row["id"]),
        "fromId": row["from_id"],
        "fromName": row["from_name"],
        "toId": row["to_id"],
        "toName": row["to_name"],
        "status": status,
        "createdAt": row["created_at"].isoformat(),
        "expiresAt": row["expires_at"].isoformat(),
    }


def _match_dict(match, players) -> dict:
    result = {
        "id": str(match["id"]),
        "gameId": str(match["game_id"]),
        "players": [
            {
                "id": p["player_id"],
                "name": p["name"],
                "seat": p["seat"],
                "isFirstAttacker": p["is_first_attacker"],
            }
            for p in players
        ],
        "status": match["status"],
        "createdAt": match["created_at"].isoformat(),
    }
    if match["finished_at"]:
        result["finishedAt"] = match["finished_at"].isoformat()
    return result


async def _get_pending_invite(conn, invite_id: UUID):
    cur = await conn.execute("SELECT * FROM invites WHERE id = %s FOR UPDATE", (invite_id,))
    invite = await cur.fetchone()
    if not invite:
        raise HTTPException(404, "Invite not found")
    if invite["status"] != "pending" or _is_timed_out(invite):
        raise HTTPException(409, "Invite is no longer pending")
    return invite


async def _create_game(from_id: int, from_name: str, to_id: int, to_name: str, match_id: UUID, first_attacker_id: int) -> dict:
    body = {
        "players": [
            {"id": from_id, "name": from_name},
            {"id": to_id, "name": to_name},
        ],
        "matchId": str(match_id),
        "firstAttackerId": first_attacker_id,
    }
    try:
        async with httpx.AsyncClient(timeout=5) as client:
            resp = await client.post(f"{config.GAME_URL}/games", json=body)
    except httpx.HTTPError as e:
        logger.warning("game-service unreachable: %s", e)
        raise HTTPException(503, "game-service is unreachable")
    if resp.status_code != 201:
        logger.warning("game-service error %s: %s", resp.status_code, resp.text)
        raise HTTPException(503, f"game-service error: {resp.text}")
    return resp.json()


@app.post("/invites", status_code=201)
async def create_invite(body: CreateInviteRequest):
    if body.fromId == body.toId:
        raise HTTPException(422, "fromId and toId must differ")

    from_name, to_name = await auth_client.user_name(body.fromId), await auth_client.user_name(body.toId)

    async with pgdb.pool.connection() as conn:
        async with conn.transaction():
            cur = await conn.execute(
                """SELECT 1 FROM invites
                   WHERE status = 'pending' AND expires_at > now()
                     AND ((from_id = %s AND to_id = %s) OR (from_id = %s AND to_id = %s))""",
                (body.fromId, body.toId, body.toId, body.fromId),
            )
            if await cur.fetchone():
                raise HTTPException(409, "Invite already active")
            cur = await conn.execute(
                """INSERT INTO invites(from_id, from_name, to_id, to_name, expires_at)
                   VALUES (%s, %s, %s, %s, now() + make_interval(secs => %s)) RETURNING *""",
                (body.fromId, from_name, body.toId, to_name, body.ttlSeconds),
            )
            invite = await cur.fetchone()

    invite_dict = _invite_dict(invite)
    redis_client.publish("invite.created", invite_dict)
    return invite_dict


@app.get("/invites")
async def list_invites(
    id: int = Query(...),
    direction: str = Query("all", pattern="^(incoming|outgoing|all)$"),
    status: str = Query("pending"),
    limit: int = Query(100, ge=1, le=500),
    offset: int = Query(0, ge=0),
):
    conditions, params = [], []
    if direction == "incoming":
        conditions.append("to_id = %s")
        params.append(id)
    elif direction == "outgoing":
        conditions.append("from_id = %s")
        params.append(id)
    else:
        conditions.append("(from_id = %s OR to_id = %s)")
        params += [id, id]

    if status == "expired":
        conditions.append("(status = 'expired' OR (status = 'pending' AND expires_at <= now()))")
    elif status != "all":
        cond, p = "status = %s", [status]
        if status == "pending":
            cond += " AND expires_at > now()"
        conditions.append(cond)
        params += p

    where = " AND ".join(conditions)
    async with pgdb.pool.connection() as conn:
        cur = await conn.execute(f"SELECT count(*) AS total FROM invites WHERE {where}", tuple(params))
        total = (await cur.fetchone())["total"]
        cur = await conn.execute(
            f"SELECT * FROM invites WHERE {where} ORDER BY created_at DESC LIMIT %s OFFSET %s",
            tuple(params) + (limit, offset))
        rows = await cur.fetchall()

    return {"total": total, "items": [_invite_dict(r) for r in rows]}


@app.get("/invites/{invite_id}")
async def get_invite(invite_id: str):
    try:
        iid = UUID(invite_id)
    except ValueError:
        raise HTTPException(404, "Invite not found")
    async with pgdb.pool.connection() as conn:
        cur = await conn.execute("SELECT * FROM invites WHERE id = %s", (iid,))
        invite = await cur.fetchone()
    if not invite:
        raise HTTPException(404, "Invite not found")
    return _invite_dict(invite)


@app.post("/invites/{invite_id}/accept")
async def accept_invite(invite_id: str, body: ActorRequest):
    try:
        iid = UUID(invite_id)
    except ValueError:
        raise HTTPException(404, "Invite not found")

    match_id = uuid4()
    async with pgdb.pool.connection() as conn:
        async with conn.transaction():
            invite = await _get_pending_invite(conn, iid)
            if invite["to_id"] != body.id:
                raise HTTPException(409, "Only the recipient can accept the invite")
            game = await _create_game(
                invite["from_id"], invite["from_name"],
                invite["to_id"], invite["to_name"],
                match_id, invite["from_id"])

            cur = await conn.execute("UPDATE invites SET status = 'accepted' WHERE id = %s", (iid,))
            cur = await conn.execute(
                "INSERT INTO matches(id, game_id) VALUES (%s, %s) RETURNING *",
                (match_id, game["id"]))
            match = await cur.fetchone()
            cur = await conn.execute(
                """INSERT INTO match_players(match_id, player_id, name, seat, is_first_attacker)
                   VALUES (%s, %s, %s, 0, TRUE), (%s, %s, %s, 1, FALSE)""",
                (match_id, invite["from_id"], invite["from_name"],
                 match_id, invite["to_id"], invite["to_name"]))
            cur = await conn.execute(
                "SELECT * FROM match_players WHERE match_id = %s ORDER BY seat", (match_id,))
            players = await cur.fetchall()

    accepted = _invite_dict(invite)
    accepted["status"] = "accepted"
    redis_client.publish("invite.accepted", accepted)
    redis_client.publish("game.started", {
        "matchId": str(match_id),
        "gameId": game["id"],
        "players": _match_dict(match, players)["players"],
    })
    return _match_dict(match, players)


@app.post("/invites/{invite_id}/decline")
async def decline_invite(invite_id: str, body: ActorRequest):
    try:
        iid = UUID(invite_id)
    except ValueError:
        raise HTTPException(404, "Invite not found")

    async with pgdb.pool.connection() as conn:
        async with conn.transaction():
            invite = await _get_pending_invite(conn, iid)
            if invite["to_id"] != body.id:
                raise HTTPException(409, "Only the recipient can decline the invite")
            cur = await conn.execute(
                "UPDATE invites SET status = 'declined' WHERE id = %s RETURNING *", (iid,))
            updated = await cur.fetchone()

    result = _invite_dict(updated)
    redis_client.publish("invite.declined", result)
    return result


@app.post("/invites/{invite_id}/cancel")
async def cancel_invite(invite_id: str, body: ActorRequest):
    try:
        iid = UUID(invite_id)
    except ValueError:
        raise HTTPException(404, "Invite not found")

    async with pgdb.pool.connection() as conn:
        async with conn.transaction():
            invite = await _get_pending_invite(conn, iid)
            if invite["from_id"] != body.id:
                raise HTTPException(403, "Only the sender can cancel the invite")
            cur = await conn.execute(
                "UPDATE invites SET status = 'cancelled' WHERE id = %s RETURNING *", (iid,))
            updated = await cur.fetchone()

    return _invite_dict(updated)


@app.get("/matches")
async def list_matches(
    id: int = Query(...),
    status: str = Query("all", pattern="^(active|finished|all)$"),
    limit: int = Query(100, ge=1, le=500),
    offset: int = Query(0, ge=0),
):
    conditions = ["mp.player_id = %s"]
    params = [id]
    if status != "all":
        conditions.append("m.status = %s")
        params.append(status)
    where = " AND ".join(conditions)

    async with pgdb.pool.connection() as conn:
        cur = await conn.execute(
            f"""SELECT count(DISTINCT m.id) AS total FROM matches m
                JOIN match_players mp ON mp.match_id = m.id WHERE {where}""",
            tuple(params))
        total = (await cur.fetchone())["total"]
        cur = await conn.execute(
            f"""SELECT DISTINCT m.* FROM matches m JOIN match_players mp ON mp.match_id = m.id
                WHERE {where} ORDER BY m.created_at DESC LIMIT %s OFFSET %s""",
            tuple(params) + (limit, offset))
        matches = await cur.fetchall()

        items = []
        for m in matches:
            cur = await conn.execute(
                "SELECT * FROM match_players WHERE match_id = %s ORDER BY seat", (m["id"],))
            players = await cur.fetchall()
            items.append(_match_dict(m, players))
    return {"total": total, "items": items}


@app.get("/matches/{match_id}")
async def get_match(match_id: str):
    try:
        mid = UUID(match_id)
    except ValueError:
        raise HTTPException(404, "Match not found")
    async with pgdb.pool.connection() as conn:
        cur = await conn.execute("SELECT * FROM matches WHERE id = %s", (mid,))
        match = await cur.fetchone()
        if not match:
            raise HTTPException(404, "Match not found")
        cur = await conn.execute(
            "SELECT * FROM match_players WHERE match_id = %s ORDER BY seat", (mid,))
        players = await cur.fetchall()
    return _match_dict(match, players)


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=config.APP_PORT)
