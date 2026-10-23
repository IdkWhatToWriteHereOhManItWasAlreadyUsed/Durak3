-- liquibase formatted sql

-- changeset divmone:4
CREATE UNIQUE INDEX IF NOT EXISTS idx_users_email     ON users(email);
CREATE INDEX        IF NOT EXISTS idx_users_google_id ON users(google_id);
CREATE UNIQUE INDEX IF NOT EXISTS idx_sessions_token  ON sessions(token);
CREATE INDEX        IF NOT EXISTS idx_sessions_user_id ON sessions(user_id);
CREATE INDEX        IF NOT EXISTS idx_sessions_expires_at ON sessions(expires_at);
