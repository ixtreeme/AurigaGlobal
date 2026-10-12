# .msa / motion rendszer audit (szerveroldali kivezetes)

Cel: a szerver ne tartson nyilvan .msa (animacio ido/sebesseg) fajlokat; a
kliens adja/olvassa az animacio adatokat, a szerver pedig vagy a kliens
packet idejebol, vagy egyszer (offline) DB-be sutott ertekekbol dolgozzon.

## 1. Jelenlegi szerveroldali .msa olvasok

- `GameServer/entity/motion.cpp` (`CMotionManager`):
  - PC: `data/pc/warrior/...run.msa`, `walk.msa` (8 race x 8 fegyver-mod x
    run/walk). `MotionDuration` + `accumulation.y`.
  - Mob: `data/monster/<folder>/motlist.txt` -> WALK/RUN/NORMAL_ATTACK/
    SPECIAL1..5 `.msa`; `MotionDuration` + `accumulation.y`.
  - Mob skill: `motioneventdata` -> `SPECIAL_ATTACKING` eventek
    (`startingtime` -> ms, `position.y` -> talalati tavolsag) ->
    `CMob::AddSkillSplash` -> `m_mobSkillInfo[].vecSplashAttack`.
- `GameServer/entity/ani.cpp` (ANI): PC combo/attack `.msa`
  `DirectInputTime` -> tamadasi sebesseg (race, weapon, combo, horse).
  **Phase 0-ban kivezetve (a1860469)**: `ENABLE_ANTICHEAT` nem definialt,
  `ani_attack_speed`-nek nem volt forduló hivója, megis ~896 fajlt olvasott
  bootkor; a 339 fajl (combo_*.msa / attack.msa a `data/pc*` alatt) a
  szerverrol torolve.
- `CMotionManager::GetNormalAttackDuration` + `m_map_normalAttackDuration`
  (POLYMORPH_BUG_FIX): felepult, de soha nem olvasta senki -> Phase 0-ban
  torolve (a `MOB_GetNormalAttackDuration` minden mobhoz ujra megnyitotta a
  `motlist.txt`-t es a `.msa`-kat).

## 2. Mi kell tenylegesen a .msa-bol (fogyasztok)

| Fogyaszto | Mit hasznal | Miert kell szerveroldal |
|---|---|---|
| `MovementSystem::GetMoveMotionSpeed` -> `MovementDuration` | PC (race, mod, walk/run) `duration` + `accum.y` -> speed | szerver szamolja a mozgas idejet/validal, es a kliens fele `dwDuration`-t kuld |
| `MovementSystem::GetMoveSpeed` -> `CombatSystem.cpp:7564` | ugyanaz | intercept celzas (target pozicio becslese) |
| `AISystem.cpp:469,504` | mob NORMAL_ATTACK / SPECIAL_x `MotionDuration` | mob tamadas/skill utan a kovetkezo AI allapot ideje |
| `LostCastleDungeon.cpp:276` (`CalcMeleeHitDelayMs`) | PC attack `MotionDuration` | klon (fake player) talalat kesleltetese |
| `SkillSystem.cpp:4233+` | mob skill splash (`startingtime`, `position.y`) | a mob skill sebzesenek idozitese/tavolsaga |
| ~~`GET_ATTACK_SPEED` / ANI~~ | PC combo `DirectInputTime` | csak `ENABLE_ANTICHEAT` alatt; most kikapcsolva, Phase 0 torolte |

Tehat ket dolog kell: **PC mozgas-sebesseg** es **mob tamadas/skill idozites
+ splash**. A mob AI/skill timing nem valtható ki kliens-packetből, mert a
mobokat a szerver vezérli.

## 3. Kiváltasi lehetosegek

### A. Offline "bake" DB-be (ajanlott, kliens valtoztatas nelkul)

Egyszeri eszkoz (Python vagy kis C++ tool) beolvassa a **kliens** `.msa`/
`motlist.txt` fajlokat, es SQL-t general:

- `mob_proto` uj oszlopok: `attack_duration` (int, ms),
  `skill1_duration`..`skill5_duration` (int, ms).
- Uj tabla `mob_skill_splash (vnum, skill_index, event_index, timing_ms,
  hit_distance)`.
- Uj tabla `pc_motion_speed (race, mode, walk_speed, run_speed)` (float) -
  a `data/pc*` run/walk .msa-bol.
- (Opcionalis, ha az anticheat visszakerul) `pc_attack_speed (race, weapon,
  combo, horse, speed)` az ANI `DirectInputTime`-bol.

Runtime: `CMotionManager` API-k maradnak (`GetMotionDuration`,
`GetAccumVector` helyett `GetMoveMotionSpeed`), de az adat a DB-bol jon
(proto betoltes részeként), nem fajlbol. A `.msa` + `motlist.txt` a
szerverrol torolhető (9109 fajl + motlist).

Eloirányok: PC sebesseg ~128 ertek; mob duration ~2 x mobszam; splash
eventek par ezer sor.

### B. Kliens-oldali timing a packetben (hosszu tavu, legtisztabb)

- `TPacketCGMove` / `TPacketCGAttack` bovitese a kliens szamitott
  animacio-idejevel (`dwDuration`/`dwMotionTime`). A kliens mar ismeri a
  sajat .msa-jat; a szerver a kapott idot hasznalja es range-checkeli
  (sebesseg pontok + max tavolsag/ido).
- Ekkor **PC** oldalon egyaltalan nem kell .msa-t tartani a szerveren
  (sem DB-bake).
- A **mob** timing tovabbra is szerver-kell (A. mob resze), mert a
  mobokat a szerver vezérli.
- Kliens build + protokoll valtozas kell.

## 4. Javasolt fazisok

- **Phase 0 (KESZ, a1860469)**: halott ANI kivezetes, dead duration kod
  torlese, 339 felesleges PC `.msa` torlese a szerver data-bol. (~900 fajl
  olvasas megszunt bootkor.)
- **Phase 1**: offline extractor + DB sema (A. pont) + migracios SQL-ek.
- **Phase 2**: GameServer/Database betoltes DB-bol; `CMotionManager`/AI/
  SkillSystem atkotese; biztonsagi fallback konstansok (pl. PC speed 300,
  mob attack 2s). A fajlparser meg marad, de csak fallbackkent.
- **Phase 3**: `motion.cpp` fajlparser + `data/monster/**/motlist.txt` es
  `.msa` torlese a szerverrol. Opcionalisan B. pont (kliens packet).

## 5. Kockazatok / megjegyzesek

- A bake a **kliens data verziójahoz** kotott: uj kliens animacio -> ujra
  kell futtatni az extractort (dokumentalni kell).
- A legkritikusabb adat a mob skill splash (`startingtime`, tavolsag):
  ez befolyasolja a sebzest, tehat az elso bake-nek ezt kell lefednie.
- Ha barmely ertek hianyzik, a szerver fallbackje legyen ertelmes
  (PC speed ~300, mob attack ~2s, splash nelkul azonnali talalat).
- A `data/pc/**` run/walk `.msa`-k addig kellenek, amig a Phase 2/3 nem
  keszul el (a CMotionManager meg olvassa oket).
