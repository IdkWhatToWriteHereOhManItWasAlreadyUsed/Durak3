SELECT 'CREATE DATABASE matchmaking'
WHERE NOT EXISTS (SELECT FROM pg_database WHERE datname = 'matchmaking')\gexec
