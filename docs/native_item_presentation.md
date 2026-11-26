# Native item presentation

Chat and whisper use the server-verified tr instance reference and canonical ti item payload. The client no longer resolves numeric item-name placeholders or old item payloads against the prototype catalogue. Historical link labels remain readable without interactive old item lookup. Native inventory and linked item tooltips continue through the typed metadata service.

Ground pickups emit GC_CHAT_NEW 102 only after the authoritative write completes. Its payload is count# followed by a canonical ti link, including category, local key, refinement, quantity and localization key. Both generated ground rewards and transferred player-owned ground items use it. Money continues through the wallet notification. Logout/relogin fences prevent a completion notification reaching a replacement session.

The loot log stores canonical keys and resolves icons from typed definitions and explicit asset paths. Display names are never reverse-mapped to prototype numbers.

Validation: GameServer build; typed_ground and typed_item_grants native tests; pinned Unity 6000.3.27f1 Editor compilation, typed chat-link validator and native receipt routing validator (Hungarian/English, quantities, refinement, colliding names and noninteractive old history).
