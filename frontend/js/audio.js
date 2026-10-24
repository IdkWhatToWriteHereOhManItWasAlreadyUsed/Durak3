const Sound = (() => {
    const files = {
        accept: 'assets/accept.wav',
        invite: 'assets/invite.wav',
        text: 'assets/text.wav',
        win: 'assets/win.wav',
    };

    let context = null;
    const buffers = new Map();
    const pending = new Map();
    let current = null;
    let playSeq = 0;

    function ensureContext() {
        const Ctx = window.AudioContext || window.webkitAudioContext;
        if (!Ctx) return null;
        if (!context) context = new Ctx();
        if (context.state === 'suspended' && context.resume) {
            context.resume().catch(() => { });
        }
        return context;
    }

    function decode(ctx, data) {
        return new Promise((resolve, reject) => {
            try { ctx.decodeAudioData(data, resolve, reject); }
            catch (e) { reject(e); }
        });
    }

    function load(name) {
        if (buffers.has(name)) return Promise.resolve(buffers.get(name));
        if (!pending.has(name)) {
            pending.set(name, fetch(files[name])
                .then(response => {
                    if (!response.ok) throw new Error(String(response.status));
                    return response.arrayBuffer();
                })
                .then(data => decode(ensureContext(), data))
                .then(buffer => {
                    buffers.set(name, buffer);
                    pending.delete(name);
                    return buffer;
                })
                .catch(() => {
                    pending.delete(name);
                    return null;
                }));
        }
        return pending.get(name);
    }

    function stop() {
        playSeq++;
        if (!current) return;
        try { current.source.stop(); } catch (e) { }
        current = null;
    }

    async function play(name, volume = 1) {
        if (!files[name]) return null;
        const ctx = ensureContext();
        if (!ctx) return null;

        const seq = ++playSeq;
        const buffer = await load(name);
        if (!buffer || seq !== playSeq) return null;

        stop();

        const source = ctx.createBufferSource();
        source.buffer = buffer;

        const gain = ctx.createGain();
        gain.gain.value = Math.max(0, Math.min(1, volume));

        source.connect(gain).connect(ctx.destination);
        source.onended = () => {
            if (current && current.source === source) current = null;
        };
        source.start();

        current = { source, gain };
        return source;
    }

    window.addEventListener('pointerdown', ensureContext);
    window.addEventListener('keydown', ensureContext);

    return { play, stop };
})();
