from uuid import uuid4

from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse, PlainTextResponse
from pydantic import BaseModel

from . import db

app = FastAPI(title="Durak Auth Service (stub)", version="0.1.0")

SESSION_TTL = "30 days"


class GoogleLoginBody(BaseModel):
    code: str | None = None
    redirect_uri: str | None = None


def _bearer(request: Request) -> str:
    auth = request.headers.get("Authorization", "")
    return auth[7:] if auth.startswith("Bearer ") else ""


def _user_from_token(token: str):
    try:
        return db.execute(
            """SELECT u.id, u.email, u.username, u.avatar_url
               FROM sessions s JOIN users u ON u.id = s.user_id
               WHERE s.token = %s::uuid AND s.expires_at > now()""",
            (token,), "one")
    except Exception:
        return None


def _session_response(user):
    token = db.execute(
        "INSERT INTO sessions(user_id) VALUES(%s) RETURNING token::text",
        (user["id"],), "one")["token"]
    return {"id": user["id"], "email": user["email"], "username": user["username"],
            "avatar_url": user["avatar_url"], "accessToken": token}


@app.post("/auth/google")
def google_login(body: GoogleLoginBody):
    if not body.code:
        return JSONResponse({"message": "google_token is required"}, status_code=400)
    if not body.redirect_uri:
        return JSONResponse({"message": "redirect_uri is required"}, status_code=400)

    google_id = f"stub:{body.code}"
    user = db.execute(
        "SELECT id, email, username, avatar_url FROM users WHERE google_id = %s",
        (google_id,), "one")
    if not user:
        user = db.execute(
            """INSERT INTO users(email, username, google_id, avatar_url)
               VALUES (%s, %s, %s, %s)
               RETURNING id, email, username, avatar_url""",
            (f"{body.code}@durak.test", body.code, google_id, ""), "one")
    return _session_response(user)


@app.get("/auth/me")
def me(request: Request):
    user = _user_from_token(_bearer(request))
    if not user:
        return JSONResponse({"message": "Invalid or expired token"}, status_code=401)
    return {"id": user["id"], "email": user["email"], "username": user["username"], "avatar_url": user["avatar_url"]}


@app.delete("/auth/logout")
@app.post("/auth/logout")
def logout(request: Request):
    token = _bearer(request)
    try:
        db.execute("DELETE FROM sessions WHERE token = %s::uuid", (token,))
    except Exception:
        pass
    return {"message": "Logged out"}


def _public_user(id_param: str):
    return _get_user_by_id(int(id_param))


def _get_user_by_id(user_id: int):
    row = db.execute(
        "SELECT id, email, username, avatar_url FROM users WHERE id = %s",
        (user_id,), "one")
    if not row:
        return None
    return dict(row)


@app.get("/auth/users/{id}")
@app.get("/users/{id}")
def user_by_id(id: str):
    user = _public_user(id)
    if not user:
        return PlainTextResponse("not found", status_code=404)
    return user
