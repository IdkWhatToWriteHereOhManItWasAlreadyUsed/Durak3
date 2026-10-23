-- liquibase formatted sql

-- changeset divmone:3
ALTER TABLE users ADD COLUMN is_admin BOOLEAN NOT NULL DEFAULT false;
-- rollback ALTER TABLE users DROP COLUMN is_admin;