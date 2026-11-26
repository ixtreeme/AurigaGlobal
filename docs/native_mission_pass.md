# Native Mission Pass rewards

Mission Pass schema 2 identifies rewards with category/local/refinement/count.
It has no prototype item field. The disabled example retains all 36 objectives
and 80 reward slots, mapped through the already reviewed typed content aliases;
objective artwork is an authored asset path. No reward content is substituted.

Migration 059 creates independent native progress, season, reward, receipt and
claim tables and native procedures. Claim reservation and item_native_award
enqueue commit together. Account/season/level/track form a stable receipt
identity. Duplicate requests and reconnect cannot enqueue another reward.
Existing typed award delivery owns Mall-space handling and item creation;
withdrawal uses the normal quoted/confirmed typed shared-storage transaction.

The config publisher defaults to preview. --apply requires an explicitly
reviewed configuration and private connection JSON. A hash pins objectives,
rewards and the season contract; both progress commits and claims check it.
After progress exists, changed contracts require a new season ID. The enabled
flag remains an operational switch. Coordinate config/SQL updates while the
game service is stopped. This work does not turn on a production season or
import unknown historical progress; the local server had no old Pass tables.

Snapshot transport version 2 includes complete native item metadata. Unity
refuses old/missing/unknown item contracts. Cards, generic reward tooltip,
native 3D preview requests and the shared native Mall button use typed state.
The old artwork/proto catalog argument and click_mall command are removed.

Validation: native rules and seven config tests passed; Unity 6000.3.27f1
Editor and player compilation; actual native transport/hover/claim-button/
storage UI validator passed. The native SQL checks covered locked and premium
rewards, changed contracts/hashes, wrong recipients, atomic queue creation,
duplicate retry and absence of the retired item_award table.

Connected disposable world game65 ran the full 80-reward season with old
item/proto/award tables unavailable. The actual Unity codecs received native
metadata, rejected premium access, claimed a free reward once, retried it,
opened the native Mall, withdrew the exact quantity with confirmation, and
verified both quantity and claimed state after reconnect. The precise coverage
is recorded in unit2-live-proof.json in the external work directory. The
inherited generic wrapper label is not credited as unrelated test coverage.

Local production remains disabled. No unrelated drafts, deferred catalog
entries, inventories or pending Mission Studio changes are part of this unit.
