/**
 * Клиент realtime-gateway (WebSocket BFF).
 *
 * Контракт: backend/gateways/realtimeGateway/openapi.yaml (AsyncAPI).
 * Handshake: GET /ws?token=<opaque accessToken>.
 * Формат сообщений: { type: string, payload: object }.
 */
const Realtime = (() => {
    const handlers = new Map();
    const queue = [];
    const statusHandlers = new Set();

    let socket = null;
    let reconnectTimer = null;
    let pingTimer = null;
    let attempts = 0;
    let manualClose = false;
    let status = 'closed';

    function on(type, fn) {
        if (!handlers.has(type)) handlers.set(type, new Set());
        handlers.get(type).add(fn);
        return () => off(type, fn);
    }

    function off(type, fn) {
        const set = handlers.get(type);
        if (set) set.delete(fn);
    }

    function onStatus(fn) {
        statusHandlers.add(fn);
        fn(status);
        return () => statusHandlers.delete(fn);
    }

    function setStatus(next) {
        status = next;
        statusHandlers.forEach(fn => {
            try { fn(next); } catch (e) { console.error(e); }
        });
    }

    function emit(type, payload) {
        const set = handlers.get(type);
        if (!set) return;
        set.forEach(fn => {
            try { fn(payload); } catch (e) { console.error(e); }
        });
    }

    function connect() {
        const token = Api.getToken();
        if (!token) return;
        if (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)) return;

        manualClose = false;
        clearTimeout(reconnectTimer);
        reconnectTimer = null;
        setStatus(attempts > 0 ? 'reconnecting' : 'connecting');

        const url = new URL(CONFIG.wsUrl);
        url.searchParams.set('token', token);

        try {
            socket = new WebSocket(url.toString());
        } catch (e) {
            scheduleReconnect();
            return;
        }

        socket.addEventListener('open', () => {
            attempts = 0;
            setStatus('open');
            startPing();
            while (queue.length) socket.send(queue.shift());
            emit('connected', null);
        });

        socket.addEventListener('message', event => {
            let message;
            try { message = JSON.parse(event.data); } catch (e) { return; }
            if (!message || !message.type) return;

            if (message.type === 'pong') {
                emit('pong', message.payload);
                return;
            }
            if (message.type === 'error') {
                const payload = message.payload || {};
                if (payload.code === 'SESSION_EXPIRED') emit('session-expired', payload);
                emit('error', payload);
                return;
            }
            emit(message.type, message.payload);
            emit('*', message);
        });

        socket.addEventListener('close', event => {
            stopPing();
            socket = null;
            if (event && event.code === 4401) {
                manualClose = true;
                setStatus('closed');
                emit('session-expired', { code: 'SESSION_EXPIRED' });
                return;
            }
            if (manualClose) {
                setStatus('closed');
                return;
            }
            setStatus('closed');
            scheduleReconnect();
        });

        socket.addEventListener('error', () => {
            // close придёт следом и запустит переподключение.
        });
    }

    function scheduleReconnect() {
        if (manualClose || reconnectTimer) return;
        if (!Api.getToken()) return;
        const delay = Math.min(10000, 1000 * Math.pow(1.6, attempts++));
        reconnectTimer = setTimeout(() => {
            reconnectTimer = null;
            connect();
        }, delay);
    }

    function startPing() {
        stopPing();
        pingTimer = setInterval(() => send('ping', {}), 25000);
    }

    function stopPing() {
        if (pingTimer) clearInterval(pingTimer);
        pingTimer = null;
    }

    function send(type, payload = {}) {
        const raw = JSON.stringify({ type, payload });
        if (socket && socket.readyState === WebSocket.OPEN) {
            socket.send(raw);
            return true;
        }
        queue.push(raw);
        connect();
        return false;
    }

    function disconnect() {
        manualClose = true;
        clearTimeout(reconnectTimer);
        reconnectTimer = null;
        stopPing();
        queue.length = 0;
        if (socket) {
            try { socket.close(1000, 'logout'); } catch (e) { /* ignore */ }
        }
        socket = null;
        attempts = 0;
        setStatus('closed');
    }

    function isConnected() {
        return !!socket && socket.readyState === WebSocket.OPEN;
    }

    return { connect, disconnect, send, on, off, onStatus, isConnected, status: () => status };
})();
