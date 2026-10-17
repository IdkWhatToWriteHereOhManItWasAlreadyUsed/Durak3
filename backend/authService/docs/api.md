# Auth Service API

Base URL: `/auth`

---

## GET /auth/me

Возвращает идентификатор текущего пользователя по токену сессии.

### Заголовки

| Заголовок | Обязательный | Описание |
|---|---|---|
| `` | Да | `Bearer <token>` — токен из `/auth/google` |

### Ответы

#### 200 OK

```json
{
  "id": 42
}
```

| Поле | Тип | Описание |
|---|---|---|
| `id` | `integer` | Внутренний идентификатор пользователя |

#### 401 Unauthorized

Токен не передан, невалиден или сессия истекла.

```json
{
  "message": "Invalid or expired token"
}
```

### Пример

```bash
curl -X GET http://localhost/auth/me \
  -H "Authorization: Bearer 550e8400-e29b-41d4-a716-446655440000"
```

---

## POST /auth/google

Авторизация через Google OAuth2. Обменивает code на токен сессии.

### Тело запроса

```json
{
  "code": "4/0AX4XfWh...",
  "redirect_uri": "http://localhost:3000/oauth-callback"
}
```

| Поле | Тип | Обязательный | Описание |
|---|---|---|---|
| `code` | `string` | Да | Authorization code от Google |
| `redirect_uri` | `string` | Да | Должен совпадать с тем, что передавался в Google |

### Ответы

#### 200 OK

Если пользователь с таким `google_id` уже существует — возвращает его. Если нет — создаёт нового.

```json
{
  "id": 42,
  "email": "user@gmail.com",
  "username": "John Doe",
  "accessToken": "550e8400-e29b-41d4-a716-446655440000"
}
```

| Поле | Тип | Описание |
|---|---|---|
| `id` | `integer` | Внутренний идентификатор пользователя |
| `email` | `string` | Email из Google аккаунта |
| `username` | `string` | Имя из Google аккаунта |
| `accessToken` | `string` | UUID токен сессии, передавать в `Authorization: Bearer` |

#### 400 Bad Request

Не передан `code` или `redirect_uri`.

```json
{
  "message": "google_token is required"
}
```

#### 500 Internal Server Error

Ошибка при обмене кода с Google (например, невалидный code или проблемы с сетью).

```json
{
  "message": "Google token exchange failed: ..."
}
```

### Пример

```bash
curl -X POST http://localhost/auth/google \
  -H "Content-Type: application/json" \
  -d '{"code": "4/0AX4XfWh...", "redirect_uri": "http://localhost:3000/oauth-callback"}'
```

---

## DELETE /auth/logout

Завершает сессию — удаляет токен из БД.

### Заголовки

| Заголовок | Обязательный | Описание |
|---|---|---|
| `Authorization` | Да | `Bearer <token>` |

### Ответы

#### 200 OK

Сессия успешно удалена. Тело ответа пустое.

#### 401 Unauthorized

Токен не передан или не найден.

### Пример

```bash
curl -X DELETE http://localhost/auth/logout \
  -H "Authorization: Bearer 550e8400-e29b-41d4-a716-446655440000"
```

---

## GET /auth/users/{id}

Возвращает публичную информацию о пользователе по его внутреннему ID.

### Параметры пути

| Параметр | Тип | Описание |
|---|---|---|
| `id` | `integer` | Внутренний идентификатор пользователя |

### Ответы

#### 200 OK

```json
{
  "id": 42,
  "username": "John Doe",
  "email": "user@gmail.com"
}
```

#### 404 Not Found

```json
{
  "message": "User not found"
}
```

### Пример

```bash
curl http://localhost/auth/users/42
```

---

## Аутентификация

Все защищённые эндпоинты требуют заголовок:

```
Authorization: Bearer <token>
```

Токен — UUID, выдаётся при логине через `/auth/google`. Срок действия — **30 дней**. После истечения или вызова `/auth/logout` токен становится невалидным и нужно логиниться заново.
