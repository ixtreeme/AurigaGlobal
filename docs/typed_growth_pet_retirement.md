# Growth pet runtime retirement

Growth pets use PersistentItem::growthPet, the typed item transaction worker and
PetSystem::SummonTyped. New_PetSystem and its raw item/socket/SQL/affect state
are removed. Combat keeps AwardTypedGrowthPetMobExperience; the obsolete
second XP path and item-ID recall affect have no runtime consumer.

The existing GC character insert + additional-info pair carries the typed
follower's level. A level change republishes that pair through ViewReencode. TypedPetLevel contains only the visible level. It is populated before
the first insert and refreshed from typed persistent progression after level-up.
Pet names, traits, skills, lifetime, recall intent, skin, feeding and evolution
remain owned by the typed catalog and persistence.

original_pet_study_contract.json is offline migration evidence: it freezes the
historical skill table, known-book success/cooldown and source hashes. Catalog
migration does not depend on rebuilding or retaining an executable old pet
motor. It does not introduce or activate catalog rows.

Retired hatch packet 146 remains framed solely for rejection by the legacy
item boundary. Active hatching/feeding/evolution/study use the typed pet
protocol; packet layouts and Unity runtime codecs remain compatible.
