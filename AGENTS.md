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
  `20261103`.
- Unity: `D:/Auriga-Unity6/SRC/AurigaUnity-20261103`, Unity Version Control branch
  `/main/unity63-integration-20261103`. Use the project's pinned Unity 6.3 Editor
  (currently `6000.3.27f1`). Use `cm` for this workspace's version control.
- Continue server and Unity development on the branches listed above. The
  mission engine branches are the source history for the 20261103 transition.

The previous `D:/Auriga-Unity6/SRC/AurigaUnity` workspace and the older
`D:/Auriga-Unity/SRC/AurigaUnity` and
`D:/Auriga-Unity másolata/SRC/AurigaUnity` workspaces contain local pending
changes. Preserve those changes; use the active Unity 6.3 workspace for this
server/client integration work.
