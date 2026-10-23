-- liquibase formatted sql

-- changeset divmone:5
CREATE TABLE user_oauth_providers (
    id          BIGSERIAL PRIMARY KEY,
    user_id     BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    provider    VARCHAR(50)  NOT NULL,   -- 'google', 'apple', ...
    provider_id VARCHAR(255) NOT NULL,
    created_at  TIMESTAMP NOT NULL DEFAULT now(),
    UNIQUE(provider, provider_id)
);

-- changeset divmone:6
CREATE INDEX idx_oauth_providers_user_id ON user_oauth_providers(user_id);

-- changeset divmone:7
INSERT INTO user_oauth_providers (user_id, provider, provider_id)
SELECT id, 'google', google_id FROM users WHERE google_id IS NOT NULL AND google_id <> '';
