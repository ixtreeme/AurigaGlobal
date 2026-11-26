# Native-only stole and sash appearance

Stole and absorption-sash presenters accept authored asset paths only. Empty
native state explicitly removes the model/effect; no saved numeric actor part
can resurrect it on mounting, armor replacement, late loading or selection.
The numerical Apply/ResolveVnum/ApplyPreview APIs and local raw-part handlers
are removed. Native actor packets no longer carry accessory/rune/effect parts;
their authored native appearance fields remain authoritative.

Real converted skeleton/model tests cover independent stole and absorption
sash on owner/observer, effect-only profiles, mount offset, local hiding,
empty-state removal and attachment refresh. 14 native stole checks, 13 actor
checks and 403 costume mesh checks passed in Unity 6000.3.27f1; Editor/player
compilation and 46 native tests passed.

Connected isolated fixture game54 equipped and unequipped a real native stole
instance and verified the authored profile in both clients. Its raw accessory
parts remained zero. Trade, exact drop/pickup, recovery clocks and reconnect
also passed. Other pending work and content were preserved.
