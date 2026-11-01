# Server and Unity development

For gameplay work, implement the relevant server and Unity changes together in
one work unit. Keep packet layouts, codecs, network handlers and the client
action in sync so the result can be tested locally. A domain/SQL foundation is
a checkpoint, not a completed playable feature.

Validate both sides before reporting a gameplay feature complete: appropriate
native tests, Unity compilation and validators, and the relevant local game
flow. State clearly which checks were actually run and what remains untested.

## Active workspaces

- Server: `D:/AurigaGlobal/LiveWork/AurigaGlobal`, Git branch
  `codex/mission-engine`.
- Unity: `D:/Auriga-Unity6/SRC/AurigaUnity`, Unity Version Control branch
  `/main/codex-mission-engine`. Use the project's pinned Unity 6.3 Editor
  (currently `6000.3.27f1`). Use `cm` for this workspace's version control.
- Continue Unity development on the mission engine branch. The temporary
  `/main/unity-6.3` branch has been consolidated into it.

The older `D:/Auriga-Unity/SRC/AurigaUnity` and
`D:/Auriga-Unity másolata/SRC/AurigaUnity` workspaces contain local pending
changes. Preserve those changes; use the active Unity 6.3 workspace for this
server/client integration work.
