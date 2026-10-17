const Sound = (() => {
    const files = {
        accept: 'assets/accept.wav',
        invite: 'assets/invite.wav',
        text: 'assets/text.wav',
        win: 'assets/win.wav',
    };

    const cache = {};
    let current = null;

    function get(name) {
        if (!files[name]) return null;
        if (!cache[name]) {
            const a = new Audio(files[name]);
            a.preload = 'auto';
            cache[name] = a;
        }
        return cache[name];
    }

    function stop() {
        if (!current) return;
        try { current.pause(); current.currentTime = 0; } catch (e) { }
        current = null;
    }

    function play(name, volume = 1) {
        const base = get(name);
        if (!base) return null;
        stop();
        const a = base.cloneNode();
        a.muted = false;
        a.volume = Math.max(0, Math.min(1, volume));
        current = a;
        a.addEventListener('ended', () => { if (current === a) current = null; });
        a.play().catch(() => { });
        return a;
    }

    function unlock() {
        window.removeEventListener('pointerdown', unlock);
        window.removeEventListener('keydown', unlock);
        for (const name of Object.keys(files)) {
            const a = get(name);
            if (!a) continue;
            a.muted = true;
            const p = a.play();
            const done = () => {
                a.pause();
                try { a.currentTime = 0; } catch (e) { }
                a.muted = false;
            };
            if (p && p.then) p.then(done).catch(() => { a.muted = false; });
            else done();
        }
    }
    window.addEventListener('pointerdown', unlock);
    window.addEventListener('keydown', unlock);

    return { play, stop, unlock };
})();
