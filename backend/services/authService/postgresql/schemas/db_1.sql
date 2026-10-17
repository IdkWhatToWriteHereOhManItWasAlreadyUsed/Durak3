CREATE TABLE IF NOT EXISTS users (
    id BIGSERIAL PRIMARY KEY,
    count INTEGER DEFAULT 1,
    email VARCHAR(255) NOT NULL,
    username VARCHAR(100) NOT NULL,
    created_at TIMESTAMP WITHOUT TIME ZONE NOT NULL DEFAULT now(),
    updated_at TIMESTAMP WITHOUT TIME ZONE DEFAULT now(),
    google_id VARCHAR(255),
    avatar_url VARCHAR(255)
);

CREATE TABLE IF NOT EXISTS sessions (
    token UUID NOT NULL DEFAULT gen_random_uuid(),
    user_id INTEGER,
    created_at TIMESTAMP WITHOUT TIME ZONE DEFAULT now(),
    expires_at TIMESTAMP WITHOUT TIME ZONE DEFAULT (now() + INTERVAL '30 days')
);
