# Native observer equipment appearance

Actor ADD/UPDATE no longer publish armor, weapon or helmet item identities.
Character traits such as hairstyle remain in their original transport fields.
The existing versioned native appearance snapshot supplies authored model paths
and explicit weapon/tool classification to both owner and observer.

Unity creates no equipment from raw actor parts, including before the first
native snapshot. Pooled actors clear the previous identity's equipment; model
generation guards reject late results. The selection cache now stores validated,
versioned native snapshots and does not read the old numeric equipment cache.
Imported tool/proto lookup is removed from the actor renderer.

Validation: Release build and 46 native tests passed; Unity 6000.3.27f1 Editor
and player compilation; 13 native actor/cache checks, 403 actual costume mesh
checks and 68 tooltip checks. Connected isolated TLS fixture game53 verified
native appearance in both observer directions and zero retired armor/weapon/head
parts, plus move/drop/pickup, trade, recovery clocks and reconnect.

This unit does not publish catalog changes or unrelated pending work.
