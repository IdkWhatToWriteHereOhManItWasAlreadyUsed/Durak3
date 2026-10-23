-- liquibase formatted sql

-- changeset divmone:11
CREATE TABLE purchase_methods(
    id INT PRIMARY KEY,
    user_id INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    type VARCHAR(50) NOT NULL,
        details JSONB NOT NULL,
        created_at TIMESTAMP NOT NULL DEFAULT now()
)

-- changeset divmone:12
CREATE INDEX idx_purchase_methods_user_id ON purchase_methods(user_id);

