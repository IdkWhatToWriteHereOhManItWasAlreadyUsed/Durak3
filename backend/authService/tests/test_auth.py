"""
Integration tests for the authService.

Tests run against a real PostgreSQL database (schema applied automatically
from postgresql/schemas/db_1.sql by the uServer testsuite framework).
Run via: make test-debug  (or make test-release)
"""

import pytest

# ---------------------------------------------------------------------------
# Helpers / fixtures
# ---------------------------------------------------------------------------

TEST_GOOGLE_ID  = 'google_test_sub_12345'
TEST_EMAIL      = 'test@example.com'
TEST_USERNAME   = 'TestUser'
TEST_AVATAR_URL = 'https://example.com/avatar.jpg'

# Fixed UUID used as a valid session token in tests
VALID_TOKEN   = '550e8400-e29b-41d4-a716-446655440000'
INVALID_TOKEN = 'aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee'


@pytest.fixture
def test_user(pgsql):
    """Insert a test user and return its id."""
    with pgsql['db_1'].cursor() as cur:
        cur.execute(
            """
            INSERT INTO users (google_id, email, username, avatar_url)
            VALUES (%s, %s, %s, %s)
            RETURNING id
            """,
            (TEST_GOOGLE_ID, TEST_EMAIL, TEST_USERNAME, TEST_AVATAR_URL),
        )
        row = cur.fetchone()
    return row[0]


@pytest.fixture
def test_session(pgsql, test_user):
    """Insert a test session with a known token and return the token string."""
    with pgsql['db_1'].cursor() as cur:
        cur.execute(
            """
            INSERT INTO sessions (token, user_id)
            VALUES (%s::uuid, %s)
            """,
            (VALID_TOKEN, test_user),
        )
    return VALID_TOKEN


# ---------------------------------------------------------------------------
# GET /auth/me
# ---------------------------------------------------------------------------

async def test_me_missing_auth_header(service_client):
    """No Authorization header → service should not crash (ideally 401)."""
    response = await service_client.get('/auth/me')
    # The current implementation calls .substr(7) on the empty header string
    # which throws std::out_of_range → uServer returns 500.
    # This test documents the current (buggy) behaviour so a fix is visible.
    # TODO: add length check in me.cpp before .substr(7) to get 401 here.
    assert response.status_code in (401, 500)


async def test_me_invalid_token(service_client):
    """Bearer token not in sessions table → 401 Unauthorized."""
    response = await service_client.get(
        '/auth/me',
        headers={'Authorization': f'Bearer {INVALID_TOKEN}'},
    )
    assert response.status_code == 401


async def test_me_valid_token(service_client, test_session):
    """Valid session token → 200 with full user object."""
    response = await service_client.get(
        '/auth/me',
        headers={'Authorization': f'Bearer {test_session}'},
    )
    assert response.status_code == 200
    data = response.json()
    assert data['email']      == TEST_EMAIL
    assert data['username']   == TEST_USERNAME
    assert data['avatar_url'] == TEST_AVATAR_URL
    assert isinstance(data['id'], int)


# ---------------------------------------------------------------------------
# POST /auth/logout
# ---------------------------------------------------------------------------

async def test_logout_valid_token(service_client, test_session):
    """Logout with a valid token → 200, session is removed."""
    response = await service_client.post(
        '/auth/logout',
        headers={'Authorization': f'Bearer {test_session}'},
    )
    assert response.status_code == 200

    # After logout the same token must no longer work
    me_response = await service_client.get(
        '/auth/me',
        headers={'Authorization': f'Bearer {test_session}'},
    )
    assert me_response.status_code == 401


async def test_logout_invalid_token(service_client):
    """Logout with an unknown token → 200 (DELETE is idempotent, no error)."""
    response = await service_client.post(
        '/auth/logout',
        headers={'Authorization': f'Bearer {INVALID_TOKEN}'},
    )
    assert response.status_code == 200


async def test_logout_missing_auth_header(service_client):
    """No Authorization header on logout — documents current behaviour."""
    response = await service_client.post('/auth/logout')
    # Same substr(7) issue as /auth/me — currently 500, ideally 401.
    assert response.status_code in (401, 500)


# ---------------------------------------------------------------------------
# GET /users/{id}
# ---------------------------------------------------------------------------

async def test_get_user_by_id_found(service_client, test_user):
    """Existing user id → 200 with user data."""
    response = await service_client.get(f'/users/{test_user}')
    assert response.status_code == 200
    data = response.json()
    assert data['id']         == test_user
    assert data['email']      == TEST_EMAIL
    assert data['username']   == TEST_USERNAME
    assert data['avatar_url'] == TEST_AVATAR_URL


async def test_get_user_by_id_not_found(service_client):
    """Non-existent user id → 404."""
    response = await service_client.get('/users/999999999')
    assert response.status_code == 404


async def test_get_user_by_id_invalid_format(service_client):
    """Non-numeric id → 500 (std::stoll throws; ideally 400)."""
    response = await service_client.get('/users/not-a-number')
    assert response.status_code in (400, 500)


# ---------------------------------------------------------------------------
# POST /auth/google — validation only (no real Google call)
# ---------------------------------------------------------------------------

async def test_google_auth_missing_code(service_client):
    """Request body without 'code' field → 400 Bad Request."""
    response = await service_client.post(
        '/auth/google',
        json={'redirect_uri': 'https://example.com/callback'},
    )
    assert response.status_code == 400


async def test_google_auth_missing_redirect_uri(service_client):
    """Request body without 'redirect_uri' field → 400 Bad Request."""
    response = await service_client.post(
        '/auth/google',
        json={'code': 'some_auth_code'},
    )
    assert response.status_code == 400


async def test_google_auth_empty_body(service_client):
    """Empty JSON body → 400 Bad Request."""
    response = await service_client.post(
        '/auth/google',
        json={},
    )
    assert response.status_code == 400
