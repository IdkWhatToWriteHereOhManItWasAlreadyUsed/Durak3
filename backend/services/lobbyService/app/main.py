import logging
from contextlib import asynccontextmanager
from datetime import datetime, timedelta, timezone
from typing import Literal
from uuid import uuid4

from fastapi import FastAPI, HTTPException, Request
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse
from pydantic import BaseModel

from . import auth_client, config, db, redis_client

logger = logging.getLogger("lobby")

ERROR_CODES = {
    403: "FORBIDDEN",
    404: "NOT_FOUND",
    409: "CONFLICT",
    422: "UNPROCESSABLE",
    500: "INTERNAL_ERROR",
}


@asynccontextmanager
async def lifespan(app: FastAPI):
    logging.basicConfig(level=logging.INFO)
    await db.init(config.SQLITE_PATH)
    logger.info("lobby started on :%d", config.APP_PORT)
    yield
    await db.close()


class PresenceOpenRequest(BaseModel):
    id: int
    status: Literal["in_lobby", "in_game", "away"] = "in_lobby"
    clientVersion: str | None = None


app = FastAPI(title="Durak Lobby Service", version="1.1.0", lifespan=lifespan)


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


@app.post("/presence", status_code=201)
async def open_presence(body: PresenceOpenRequest):
    name = await auth_client.user_name(body.id)
    session_id = str(uuid4())
    presence = await db.open_session(session_id, body.id, name, body.status, body.clientVersion)
    redis_client.publish("presence.online", {
        "id": body.id,
        "name": name,
        "sessionId": session_id,
        "status": body.status,
    })
    return presence


@app.get("/presence")
async def list_online(
    excludeSelf: bool = True,
    id: int | None = None,
    limit: int = 50,
    offset: int = 0,
):
    total, items = await db.list_online(id if excludeSelf else None, limit, offset)
    return {"total": total, "items": items}


@app.delete("/presence/{sessionId}", status_code=204)
async def close_presence(sessionId: str):
    row = await db.close_session(sessionId)
    if not row:
        raise HTTPException(404, "Presence session not found")
    redis_client.publish("presence.offline", {
        "id": row["player_id"],
        "name": row["name"],
        "sessionId": sessionId,
    })


@app.post("/presence/{sessionId}/heartbeat")
async def heartbeat(sessionId: str):
    row = await db.refresh(sessionId)
    if not row:
        raise HTTPException(404, "Presence session not found")
    return {
        "sessionId": row["session_id"],
        "id": row["player_id"],
        "name": row["name"],
        "status": row["status"],
        "lastSeen": row["last_seen"],
        "expiresAt": row["expires_at"],
    }


@app.get("/presence/{player_id}/status")
async def get_status(player_id: int):
    row = await db.player_last_session(player_id)
    if not row:
        raise HTTPException(404, "Presence session not found")
    online = row["expires_at"] > db.now()
    return {
        "id": row["player_id"],
        "online": online,
        "status": row["status"],
        "lastSeen": row["last_seen"],
    }


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=APP_PORT)
