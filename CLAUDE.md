# Working on Phone OS with Claude

Read [CONTRIBUTING.md](CONTRIBUTING.md) for the rules of the code and the checks. This file adds the way of working between the owner and you: what is decided is written down, so that it does not live only in a conversation.

## Start of every session

Read [docs/decisions.md](docs/decisions.md) and [docs/open-points.md](docs/open-points.md) first. Before proposing something, check whether a decision already covers it, and say so instead of re-deciding it. If you want to go against a decision, do not just do it: say so, and add a new entry that supersedes the old one once the owner agrees.

## Decision log

Keep one running log of decisions in docs/decisions.md. It exists so that a decision made in a conversation does not live only in that conversation, and so that the owner and you can both read and update it. The entry format is at the top of that file.

### What counts as a decision

Write an entry whenever one of these happens:
- the owner chooses between options, or refuses one (a limit, a feature, a licence, a name)
- you pick an approach where a reasonable alternative exists and the reason is not obvious
- a measurement on the device changes what the code does
- a decision is reversed

Do not log routine edits, refactors with no choice in them, or anything the code and its tests already say.

### Rules

1. Write the entry in the same commit as the change it explains, or before it. A change that embodies a decision and has no entry is incomplete.
2. Say who decided. The owner's decisions are the owner's: security limits, what the product refuses to do, licences. Never record an agent's proposal as the owner's decision. If the owner has not confirmed it, set the status to open and ask.
3. Keep the three words apart in "Verified": host-tested, compiled, verified on the device.
4. Open work stays in docs/open-points.md and nowhere else. A decision that is waiting for the owner is listed there and points to its D-number; when it is decided, the open point is removed in the same commit and the entry in decisions.md becomes decided.
5. The changelog names what changed for the people who use the device and refers to the D-number; the reasons stay in decisions.md.
6. A lesson measured on the device still goes into docs/hardware/hardware-lessons.md; if it changed what the code does, add a decision that links to it.
7. Add entries at the end of docs/decisions.md. Never rewrite or delete an old one; to change a decision, add a new entry that says it supersedes the old number and set the old status to "superseded by D-<n>". The status line and the "Verified" line are the only lines of an old entry that change. A link path that changes because a file moved may be updated in an old entry; nothing else.

## Bounded problems

Work on one bounded problem at a time. Before starting, write what "done" means for it in a few lines: what must work, what is tested, what is verified on the device and what is not. Keep to that boundary and do not change other areas on the way; if something outside it needs a change, say so and log it as its own decision.
