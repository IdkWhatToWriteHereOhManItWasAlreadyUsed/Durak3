const App = (() => {
    const screens = {};
    const modalEls = {};

    let modalOpen = false;
    let modalOwner = null;
    let modalHandlers = { onYes: null, onNo: null, onClose: null };
    let loadingDepth = 0;

    function init() {
        ['login', 'menu', 'game', 'replay'].forEach(name => {
            screens[name] = document.getElementById('screen-' + name);
        });

        modalEls.root = document.getElementById('modal');
        modalEls.title = document.getElementById('modalTitle');
        modalEls.text = document.getElementById('modalText');
        modalEls.progressWrap = document.getElementById('modalProgressWrap');
        modalEls.progress = document.getElementById('modalProgress');
        modalEls.buttons = document.getElementById('modalButtons');
        modalEls.closeRow = document.getElementById('modalCloseRow');
        modalEls.yes = document.getElementById('modalYes');
        modalEls.no = document.getElementById('modalNo');
        modalEls.close = document.getElementById('modalClose');

        modalEls.yes.addEventListener('click', () => {
            const handler = modalHandlers.onYes;
            hideModal();
            if (handler) handler();
        });
        modalEls.no.addEventListener('click', () => {
            const handler = modalHandlers.onNo;
            hideModal();
            if (handler) handler();
        });
        modalEls.close.addEventListener('click', () => {
            const handler = modalHandlers.onClose;
            hideModal();
            if (handler) handler();
        });

        Lobby.init();
        Game.init();
        Replay.init();
        Auth.init();
    }

    function showScreen(name) {
        Object.values(screens).forEach(s => {
            if (s) s.classList.remove('active');
        });
        if (screens[name]) screens[name].classList.add('active');
    }

    async function goMenu() {
        if (typeof Game !== 'undefined') Game.stop();
        await Lobby.leave();
        showScreen('menu');
        Lobby.enter();
    }

    async function startGame(opts) {
        if (typeof Game !== 'undefined') Game.stop();
        Lobby.leave();
        showScreen('game');
        Game.start(opts);
    }

    async function openReplay(replayId, summary) {
        if (typeof Game !== 'undefined') Game.stop();
        Lobby.leave();
        showScreen('replay');
        Replay.open(replayId, summary);
    }

    function showModal(opts = {}) {
        modalOwner = opts.owner || null;
        modalHandlers = {
            onYes: opts.onYes || null,
            onNo: opts.onNo || null,
            onClose: opts.onClose || null,
        };

        modalEls.title.textContent = opts.title || '';
        modalEls.text.textContent = opts.text || '';

        if (opts.progress) {
            modalEls.progressWrap.classList.remove('hidden');
            modalEls.progress.style.width = '100%';
        } else {
            modalEls.progressWrap.classList.add('hidden');
        }

        const showYes = opts.showYes !== false;
        const showNo = opts.showNo !== false;

        if (opts.showButtons === false) {
            modalEls.buttons.classList.add('hidden');
            modalEls.closeRow.classList.add('hidden');
        } else if (!showYes && !showNo) {
            modalEls.buttons.classList.add('hidden');
            modalEls.closeRow.classList.remove('hidden');
            modalEls.close.textContent = opts.closeText || 'Ок';
        } else {
            modalEls.buttons.classList.remove('hidden');
            modalEls.closeRow.classList.add('hidden');
            modalEls.yes.classList.toggle('hidden', !showYes);
            modalEls.no.classList.toggle('hidden', !showNo);
            modalEls.yes.textContent = opts.yesText || 'Да';
            modalEls.no.textContent = opts.noText || 'Нет';
        }

        modalEls.root.classList.remove('hidden');
        modalOpen = true;
    }

    /**
     * @param {string=} owner Закрывает модалку только если она принадлежит owner.
     * @returns {boolean} true, если модалка действительно закрыта.
     */
    function hideModal(owner) {
        if (owner && modalOwner !== owner) return false;
        modalEls.root.classList.add('hidden');
        modalOpen = false;
        modalOwner = null;
        modalHandlers = { onYes: null, onNo: null, onClose: null };
        return true;
    }

    function setModalProgress(percent) {
        modalEls.progress.style.width = Math.max(0, Math.min(100, percent)) + '%';
    }

    function isModalOpen() {
        return modalOpen;
    }

    function toast(message, kind = 'info') {
        const wrap = document.getElementById('toastWrap');
        if (!wrap) return;

        const item = document.createElement('div');
        item.className = 'toast toast-' + kind;
        item.textContent = message;
        item.addEventListener('click', () => item.remove());
        wrap.appendChild(item);

        setTimeout(() => {
            item.classList.add('toast-out');
            setTimeout(() => item.remove(), 250);
        }, kind === 'error' ? 5000 : 3500);
    }

    function showLoading(text) {
        loadingDepth++;
        const overlay = document.getElementById('loadingOverlay');
        const label = document.getElementById('loadingText');
        if (label) label.textContent = text || 'Загрузка…';
        if (overlay) overlay.classList.remove('hidden');
    }

    function hideLoading() {
        loadingDepth = Math.max(0, loadingDepth - 1);
        if (loadingDepth > 0) return;
        const overlay = document.getElementById('loadingOverlay');
        if (overlay) overlay.classList.add('hidden');
    }

    return {
        init,
        showScreen,
        goMenu,
        startGame,
        openReplay,
        showModal,
        hideModal,
        setModalProgress,
        isModalOpen,
        toast,
        showLoading,
        hideLoading,
    };
})();

document.addEventListener('DOMContentLoaded', App.init);
