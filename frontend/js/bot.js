const DurakBot = {
    decide(session, botIndex) {
        if (session.CurrPlayerMove !== botIndex) return null;
        if (session.isAttackerTurn()) return this._attacker(session, botIndex);
        return this._defender(session, botIndex);
    },

    _attacker(session, botIndex) {
        const hand = session.PlayerCards[session.CurrPlayerMove];
        const tableCount = session.totalTableCards();

        if (hand.length === 0) return tableCount > 0 ? { kind: 'pass' } : null;

        const defenderIndex = 1 - session.CurrPlayerAttacker;
        const defenderHand = session.PlayerCards[defenderIndex].length;
        const maxTable = Math.min(6, defenderHand);

        if (tableCount > 0 && tableCount >= maxTable) return { kind: 'pass' };

        const evenStacks = [];
        session.GameStack.forEach((s, i) => { if (s.length % 2 === 0) evenStacks.push(i); });

        const powers = new Set();
        session.GameStack.forEach(s => s.forEach(c => powers.add(c.power)));

        const sorted = [...hand].sort((a, b) => {
            const aT = a.suit === session.Trump ? 1 : 0;
            const bT = b.suit === session.Trump ? 1 : 0;
            if (aT !== bT) return aT - bT;
            return a.power - b.power;
        });

        for (const card of sorted) {
            if (tableCount > 0 && !powers.has(card.power)) continue;
            for (const si of evenStacks) {
                if (session.canPush(card, session.GameStack[si]))
                    return { kind: 'move', card, stackNum: si };
            }
        }
        if (tableCount > 0) return { kind: 'pass' };
        return null;
    },

    _defender(session, botIndex) {
        const odd = [];
        session.GameStack.forEach((s, i) => { if (s.length % 2 === 1) odd.push(i); });
        if (odd.length === 0) return { kind: 'pass' };

        const working = [...session.PlayerCards[session.CurrPlayerMove]];
        const steps = [];

        for (const si of odd) {
            const stack = session.GameStack[si];
            const top = stack[stack.length - 1];
            let bestIdx = -1;
            let bestCard = null;
            for (let i = 0; i < working.length; i++) {
                const c = working[i];
                if (!session.canBeat(c, top)) continue;
                if (!bestCard) { bestIdx = i; bestCard = c; continue; }
                const cT = c.suit === session.Trump ? 1 : 0;
                const bT = bestCard.suit === session.Trump ? 1 : 0;
                if (cT < bT || (cT === bT && c.power < bestCard.power)) { bestIdx = i; bestCard = c; }
            }
            if (bestIdx === -1) return { kind: 'grab' };
            steps.push({ card: bestCard, stackNum: si });
            working.splice(bestIdx, 1);
        }
        return { kind: 'plan', steps };
    }
};
