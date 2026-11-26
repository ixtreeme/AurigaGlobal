# Native-only client combat and equipment

Unity /main changeset 224 removes raw inventory-cell/vnum fallback decisions
from combat and equipment presentation. Weapon, ammunition and tool checks use
typed equipment/inventory facts. Missing native metadata leaves the actor
unarmed; it cannot infer equipment from an imported catalog.

Appearance uses authored model addresses and keeps actor hairstyle, mount
motion, costume hiding and observer presentation. The binder no longer
subscribes to raw item state or exposes the old wear-cell item identities.
Obsolete raw-equipment fixtures retire with their implementation.

Unity 6000.3.27f1 compiled successfully. Focused checks passed: equipment 711,
ammunition 1,340, typed hover/receiver boundaries 67, and costume appearance
403 (2,521 total). The costume fixture used the actual converted skeleton,
model definitions and assets, including hide/show and owner/observer cases.
The first fixture attempts exposed missing assets in the private test copy;
the successful run used the real dependency graph copied from the workspace.
The final combined retirement release has a separate local server/client
verification record. No catalog or inventory data changed in this unit.
