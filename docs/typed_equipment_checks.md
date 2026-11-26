# Typed equipment checks

Weapon-dependent aura maintenance reads the authoritative typed equipment
projection. An unavailable projection is still loading; it does not strip
Aura of the Sword or Enchanted Blade. Once loaded, removing the combat weapon
removes the aura and its attack bonus through the existing affect lifecycle.

Expanded skills use the native equipped weapon subtype and instance identity.
Class weapon restrictions are unchanged. Delayed volleys, meteors and pulses
retain the full 64-bit instance ID, so changing to another weapon cannot
accidentally preserve a cast through a truncated ID. Bow detection uses the
same native facts; cosmetic weapon slots cannot satisfy it.

Normal horse riding preserves the four ceremonial body-armor restrictions
through their active typed catalog definitions. The policy checks only the
body slot and armor category. Equipment hydration must finish before a new
riding action. Typed mount seals keep their separate companion action path.

The overhead belt-bag occupancy counter and its Unity state are removed.
The account mount collection count, rank colors and rank icons remain.
GC249 keeps its 13-byte framing: the server writes zero to the final reserved
int32 and Unity ignores that word, including nonzero values from older senders.
The ordinary typed belt equipment slot is unaffected.

No catalog, player inventory or quest data migration is part of this change.
The deferred content and Mission Studio work are outside its scope.

## Validation

The scoped server build and all 14 selected native suites pass, including
real affect ticks, unloaded equipment, aura removal, horse policy admission,
all class weapon combinations and identities above 32 bits. Unity 6000.3.27f1
compiles the paired client and passes TextTail Original Parity validation.
Local network gameplay evidence is recorded with the publication journal;
batch-mode checks do not replace a rendered visual inspection.

The isolated local TLS gameplay run passed with the pinned Unity Editor and
real server: typed sword equip, aura cast, survival across multiple affect
ticks, aura removal on unequip, ceremonial armor blocking a summoned normal
horse, riding after removing it, dismount and persisted placement on reconnect.
The test uses its own account/database and the client inventory space finder;
no live character or catalog rows were changed.
