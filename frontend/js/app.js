
const App = (() => {
    const screens = {};
    let modalOpen = false;
    let modalHandlers = { onYes: null, onNo: null, onClose: null };

    const modalEls = {};

    function init() {
        ['menu', 'game', 'replay'].forEach(name => {
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

        modalEls.yes.addEventListener('click', () => { hideModal(); if (modalHandlers.onYes) modalHandlers.onYes(); });
        modalEls.no.addEventListener('click', () => { hideModal(); if (modalHandlers.onNo) modalHandlers.onNo(); });
        modalEls.close.addEventListener('click', () => { hideModal(); if (modalHandlers.onClose) modalHandlers.onClose(); });

        Lobby.init();
        Game.init();
        Replay.init();

        showScreen('menu');
    }

    function showScreen(name) {
        Object.values(screens).forEach(s => s.classList.remove('active'));
        if (screens[name]) screens[name].classList.add('active');
    }

    function startGame(opts) {
        showScreen('game');
        Game.start(opts);
    }

    function openReplay(rec) {
        showScreen('replay');
        Replay.open(rec);
    }

    function showModal(opts) {
        modalHandlers = { onYes: opts.onYes || null, onNo: opts.onNo || null, onClose: opts.onClose || null };
        modalEls.title.textContent = opts.title || '';
        modalEls.text.textContent = opts.text || '';

        if (opts.progress) {
            modalEls.progressWrap.classList.remove('hidden');
            modalEls.progress.style.width = '100%';
        } else {
            modalEls.progressWrap.classList.add('hidden');
        }

        if (opts.showButtons === false) {
            modalEls.buttons.classList.add('hidden');
            modalEls.closeRow.classList.add('hidden');
        } else if (opts.showYes === false) {
            modalEls.buttons.classList.add('hidden');
            modalEls.closeRow.classList.remove('hidden');
            modalEls.close.textContent = opts.closeText || 'Ок';
        } else {
            modalEls.buttons.classList.remove('hidden');
            modalEls.closeRow.classList.add('hidden');
            modalEls.yes.textContent = opts.yesText || 'Да';
            modalEls.no.textContent = opts.noText || 'Нет';
        }

        modalEls.root.classList.remove('hidden');
        modalOpen = true;
    }

    function hideModal() {
        modalEls.root.classList.add('hidden');
        modalOpen = false;
        modalHandlers = { onYes: null, onNo: null, onClose: null };
    }

    function setModalProgress(percent) {
        modalEls.progress.style.width = Math.max(0, Math.min(100, percent)) + '%';
    }

    function isModalOpen() { return modalOpen; }

    return { init, showScreen, startGame, openReplay, showModal, hideModal, setModalProgress, isModalOpen };
})();

document.addEventListener('DOMContentLoaded', App.init);
