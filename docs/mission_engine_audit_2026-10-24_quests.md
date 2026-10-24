# Régi questek migrációs mátrixa — 2026-10-24

69 aktív forrás; mind megegyezik a távoli commitolt snapshot tartalmával.
Ez lexikális előosztályozás, nem kész konverzió vagy ekvivalenciabizonyíték.
A = közös gráfra jelölt; B = gráf + Lua action jelölt; C = hiányzó natív domainképesség / külön átvezetés.
A részletes API-, handler-, flag- és hashadatok a szomszédos `_quests.json` fájlban vannak.

| Forrás | Quest | Osztály | Állapot / handler | Előzetes indok |
|---|---|---|---:|---|
| `Quest/dungeon/catacombe.lua` | `catacombe` | C | 1 / 9 | domain-specific native services; native global-event/drop/progression authority; instance/global timers |
| `Quest/dungeon/christmas_zone1.lua` | `christmas_zone1` | C | 1 / 20 | domain-specific native services; native global-event/drop/progression authority; instance/global timers |
| `Quest/dungeon/christmas_zone2.lua` | `christmas_zone2` | C | 1 / 21 | domain-specific native services; native global-event/drop/progression authority; instance/global timers |
| `Quest/dungeon/deviltower_zone.lua` | `deviltower_zone` | C | 1 / 21 | domain-specific native services; native global-event/drop/progression authority; instance/global timers |
| `Quest/dungeon/dragonlair_zone.lua` | `dragonlair_zone` | C | 1 / 8 | domain-specific native services; native global-event/drop/progression authority; instance/global timers |
| `Quest/dungeon/dungeon_spider.lua` | `dungeon_spider` | C | 1 / 8 | domain-specific native services; native global-event/drop/progression authority; instance/global timers |
| `Quest/dungeon/enchanted_forest.lua` | `enchanted_forest` | C | 1 / 8 | domain-specific native services; native global-event/drop/progression authority; instance/global timers |
| `Quest/dungeon/meleylair_zone.lua` | `meleylair_zone` | C | 1 / 16 | domain-specific native services; instance/global timers |
| `Quest/dungeon/monkey_tower_razor93.lua` | `monkey_tower_razor93` | C | 1 / 16 | domain-specific native services; native global-event/drop/progression authority; instance/global timers |
| `Quest/dungeon/nemere_zone.lua` | `nemere_zone` | C | 1 / 7 | domain-specific native services; native global-event/drop/progression authority; instance/global timers |
| `Quest/dungeon/owl_zone_razor93.lua` | `owl_zone_razor93` | C | 1 / 21 | domain-specific native services; native global-event/drop/progression authority; instance/global timers |
| `Quest/dungeon/razador_zone.lua` | `razador_zone` | C | 1 / 7 | domain-specific native services; native global-event/drop/progression authority; instance/global timers |
| `Quest/dungeon/slime_dungeon.lua` | `slime_dungeon` | C | 1 / 7 | domain-specific native services; native global-event/drop/progression authority; instance/global timers |
| `Quest/dungeon/temple_ochao.lua` | `temple_ochao` | C | 1 / 11 | domain-specific native services; native global-event/drop/progression authority; instance/global timers |
| `Quest/dungeon/underwater_zone.lua` | `underwater_zone` | C | 1 / 21 | domain-specific native services; native global-event/drop/progression authority; instance/global timers |
| `Quest/events/auto_events.lua` | `auto_events` | C | 1 / 3 | native global-event/drop/progression authority; instance/global timers |
| `Quest/events/auto_events_notice.lua` | `auto_events_notice` | C | 1 / 2 | native global-event/drop/progression authority; instance/global timers |
| `Quest/events/drop2.quest` | `drop2` | C | 1 / 1 | native global-event/drop/progression authority |
| `Quest/events/drop3.quest` | `drop3` | C | 1 / 1 | native global-event/drop/progression authority |
| `Quest/events/drop4.quest` | `drop4` | C | 1 / 1 | native global-event/drop/progression authority |
| `Quest/events/dupa_run_pont.quest` | `dupla_run_pont` | C | 1 / 1 | native global-event/drop/progression authority |
| `Quest/events/event_map_control.lua` | `event_map_control` | C | 1 / 4 | native global-event/drop/progression authority |
| `Quest/events/events_mgr.lua` | `events_mgr` | C | 1 / 6 | domain-specific native services; native global-event/drop/progression authority |
| `Quest/events/gm_quiz.lua` | `gm_quiz` | C | 1 / 2 | native global-event/drop/progression authority; instance/global timers |
| `Quest/events/halloween.lua` | `metin_event` | C | 1 / 4 | native global-event/drop/progression authority; instance/global timers |
| `Quest/events/kari_event.lua` | `kari_event` | A | 1 / 1 | candidate for standard event/condition/dialogue/action graph; requires equivalence fixture |
| `Quest/events/miki_event.lua` | `random_50010_take` | A | 1 / 2 | candidate for standard event/condition/dialogue/action graph; requires equivalence fixture |
| `Quest/events/tanaka.lua` | `tanaka` | C | 1 / 1 | native global-event/drop/progression authority |
| `Quest/events/valentin_event.lua` | `valentin_event` | C | 1 / 7 | domain-specific native services |
| `Quest/gilda/crea_gilda.lua` | `crea_gilda` | C | 1 / 3 | native global-event/drop/progression authority |
| `Quest/gilda/create_ore.lua` | `heavenly_refine` | C | 1 / 1 | domain-specific native services |
| `Quest/gilda/guild_building.quest` | `guild_building` | C | 1 / 1 | domain-specific native services |
| `Quest/gilda/guild_building_alter_of_power.lua` | `alter_of_power` | C | 1 / 4 | domain-specific native services |
| `Quest/gilda/guild_building_npc.quest` | `guild_building_npc` | A | 1 / 3 | candidate for standard event/condition/dialogue/action graph; requires equivalence fixture |
| `Quest/gilda/guild_rank.lua` | `guild_rank` | C | 1 / 3 | domain-specific native services; privileged legacy API |
| `Quest/gilda/guild_rank_manager.lua` | `guild_rank_manager` | C | 1 / 3 | instance/global timers; privileged legacy API |
| `Quest/gilda/guild_war_join.lua` | `guild_war_join` | C | 1 / 2 | domain-specific native services |
| `Quest/gilda/guild_war_observer.lua` | `guild_war_observer` | C | 1 / 1 | domain-specific native services |
| `Quest/item/change_yang.lua` | `change_yang` | A | 1 / 4 | candidate for standard event/condition/dialogue/action graph; requires equivalence fixture |
| `Quest/item/cooldown_reset.lua` | `cooldown_reset` | B | 1 / 1 | bespoke procedural logic; graph extraction needs manual review |
| `Quest/item/lvl_back.lua` | `leveldown_66_to_65` | C | 1 / 1 | native global-event/drop/progression authority |
| `Quest/item/skill_140.lua` | `m1_to_g1` | C | 1 / 1 | native progression/crafting/character mutation |
| `Quest/item/teleportgyuru.quest` | `teleportgyuru` | A | 1 / 1 | candidate for standard event/condition/dialogue/action graph; requires equivalence fixture |
| `Quest/login/baule_start.lua` | `baule_start` | A | 1 / 6 | candidate for standard event/condition/dialogue/action graph; requires equivalence fixture |
| `Quest/login/begin_weapons.lua` | `beginer_weapons` | A | 1 / 1 | candidate for standard event/condition/dialogue/action graph; requires equivalence fixture |
| `Quest/login/levelup.lua` | `levelup` | A | 1 / 1 | candidate for standard event/condition/dialogue/action graph; requires equivalence fixture |
| `Quest/login/mley_notice.lua` | `meley_worldboss_timer` | C | 1 / 6 | domain-specific native services; native global-event/drop/progression authority |
| `Quest/login/show_server_time.lua` | `show_server_time` | A | 1 / 2 | candidate for standard event/condition/dialogue/action graph; requires equivalence fixture |
| `Quest/login/wold_boss_notice.lua` | `desert_worldboss_timer` | C | 1 / 6 | domain-specific native services; native global-event/drop/progression authority |
| `Quest/npc/darkforest.lua` | `darkforest` | A | 1 / 1 | candidate for standard event/condition/dialogue/action graph; requires equivalence fixture |
| `Quest/npc/fisher.lua` | `fisher` | A | 1 / 1 | candidate for standard event/condition/dialogue/action graph; requires equivalence fixture |
| `Quest/npc/marriage_manage.lua` | `marriage_manage` | C | 1 / 4 | domain-specific native services |
| `Quest/npc/sd5.lua` | `sd5` | A | 1 / 1 | candidate for standard event/condition/dialogue/action graph; requires equivalence fixture |
| `Quest/pda_perfetta/pda_perf_quest.lua` | `pda_perf_quest` | C | 1 / 1 | native progression/crafting/character mutation |
| `Quest/skill/pda_quest.lua` | `pda_quest` | C | 1 / 1 | native global-event/drop/progression authority |
| `Quest/skill/reset_scroll.lua` | `reset_scroll` | C | 1 / 7 | domain-specific native services; native progression/crafting/character mutation |
| `Quest/system/baule_pet.lua` | `baule_pet` | A | 1 / 1 | candidate for standard event/condition/dialogue/action graph; requires equivalence fixture |
| `Quest/system/costume_system.lua` | `costume_system` | A | 1 / 4 | candidate for standard event/condition/dialogue/action graph; requires equivalence fixture |
| `Quest/system/craft_new.lua` | `craft_new` | C | 1 / 1 | privileged legacy API |
| `Quest/system/doctrine.lua` | `doctrine` | A | 1 / 1 | candidate for standard event/condition/dialogue/action graph; requires equivalence fixture |
| `Quest/system/dungeon_mgr.lua` | `dungeon_mgr` | C | 1 / 2 | domain-specific native services |
| `Quest/system/meley_kill_announce.lua` | `meley_kill_announce` | A | 2 / 2 | candidate for standard event/condition/dialogue/action graph; requires equivalence fixture |
| `Quest/system/mining.lua` | `mining` | C | 1 / 2 | native global-event/drop/progression authority |
| `Quest/system/passive_skills.lua` | `passive_skills` | C | 1 / 2 | native global-event/drop/progression authority |
| `Quest/system/pet_system.lua` | `pet_system` | C | 1 / 1 | domain-specific native services |
| `Quest/system/runesys.lua` | `rune` | B | 4 / 6 | bespoke procedural logic; graph extraction needs manual review |
| `Quest/system/sash.lua` | `sash` | C | 1 / 1 | native progression/crafting/character mutation |
| `Quest/system/sash2.lua` | `sash2` | A | 1 / 0 | candidate for standard event/condition/dialogue/action graph; requires equivalence fixture |
| `Quest/zodiaco/zodiaco_in_arrivo.lua` | `zodiaco_in_arrivo` | A | 1 / 1 | candidate for standard event/condition/dialogue/action graph; requires equivalence fixture |


A 128, aktív questnévhez nem kapcsolható régi flag-névtér nem törölhető automatikusan.
Ezek tulajdonosi feloldása és a teljes content-regresszió a runtime nyugdíjazásának előfeltétele.
