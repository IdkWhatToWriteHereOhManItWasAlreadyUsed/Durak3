-- liquibase formatted sql

-- changeset divmone:10
ALTER TABLE users DROP COLUMN IF EXISTS google_id;