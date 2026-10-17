#!/usr/bin/env node
'use strict';

const http = require('http');
const fs = require('fs');
const path = require('path');

const ROOT = __dirname;
const DEFAULT_PORT = 5500;

const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.js': 'application/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.svg': 'image/svg+xml',
  '.ico': 'image/x-icon',
  '.wav': 'audio/wav',
  '.mp3': 'audio/mpeg',
  '.txt': 'text/plain; charset=utf-8',
  '.map': 'application/json; charset=utf-8',
};

const NO_CACHE = {
  'Cache-Control': 'no-store, no-cache, must-revalidate',
  Pragma: 'no-cache',
  Expires: '0',
};

function parsePort() {
  const arg = process.argv.slice(2).find((a) => !a.startsWith('-'));
  const value = arg !== undefined ? arg : process.env.PORT;
  if (value === undefined) return DEFAULT_PORT;
  const port = Number(value);
  if (!Number.isInteger(port) || port < 1 || port > 65535) {
    console.error(`Некорректный порт: ${value}`);
    process.exit(1);
  }
  return port;
}

function send(res, status, type, body, method) {
  res.writeHead(status, { 'Content-Type': type, 'Content-Length': Buffer.byteLength(body), ...NO_CACHE });
  res.end(method === 'HEAD' ? undefined : body);
}

function handler(req, res) {
  const method = req.method === 'HEAD' ? 'HEAD' : 'GET';
  let pathname;
  try {
    pathname = decodeURIComponent(new URL(req.url, 'http://localhost').pathname);
  } catch {
    pathname = '/';
  }

  const resolved = path.resolve(ROOT, '.' + pathname);
  if (resolved !== ROOT && !resolved.startsWith(ROOT + path.sep)) {
    log(req, 403);
    return send(res, 403, 'text/plain; charset=utf-8', '403 Forbidden', method);
  }

  fs.stat(resolved, (err, stat) => {
    let file = resolved;
    if (!err && stat.isDirectory()) file = path.join(resolved, 'index.html');

    fs.readFile(file, (readErr, data) => {
      if (readErr) {
        log(req, 404);
        return send(res, 404, 'text/plain; charset=utf-8', '404 Not Found', method);
      }
      const type = MIME[path.extname(file).toLowerCase()] || 'application/octet-stream';
      res.writeHead(200, { 'Content-Type': type, 'Content-Length': data.length, ...NO_CACHE });
      res.end(method === 'HEAD' ? undefined : data);
      log(req, 200);
    });
  });
}

function log(req, status) {
  process.stdout.write(`${req.socket.remoteAddress} - "${req.method} ${req.url}" ${status}\n`);
}

function main() {
  if (!fs.existsSync(path.join(ROOT, 'index.html'))) {
    console.error(`Не найден index.html в ${ROOT}`);
    process.exit(1);
  }

  const port = parsePort();
  const server = http.createServer(handler);

  server.on('error', (err) => {
    console.error(err.code === 'EADDRINUSE' ? `Порт ${port} уже занят` : err.message);
    process.exit(1);
  });

  server.listen(port, () => {
    console.log('Дурак — веб-фронтенд');
    console.log(`  Раздаётся папка: ${ROOT}`);
    console.log(`  Открой в браузере: http://localhost:${port}/`);
    console.log('  Остановить: Ctrl+C');
  });

  process.on('SIGINT', () => {
    console.log('\nОстановлено.');
    server.close(() => process.exit(0));
  });
}

main();
