# ECS Migration Phase 0 Report

## Osszkep
- Kodbazis gyoker: `E:\AurigaGlobal\LiveWork\AurigaGlobal`
- Vizsgalt szervermodul: `SRC/Server/GameServer`
- `char.h` direkt include-fuggok: **133** fajl
- `char.h` tranzitiv include-fuggok: **134** fajl
- `CHARACTER` osztalybol kinyert mezodeklaraciok: **273**
- `CHARACTER` osztalybol kinyert metodusdeklaraciok: **856**
- `CHARACTER::` scope-pal talalt implementaciok: **627**
- Egyedi `CHARACTER` metodusnevek: **600**
- Lua quest binding fuggvenyek, amelyek `pc`/`npc`/`item` handle-t erintenek: **442**
- Quest handle osszesites: `pc=393`, `npc=27`, `item=45`

## Megjegyzesek
- A szerver oldalon **Python quest binding** nem talalhato; a quest interop teljes egeszeben Lua `ALUA(...)` bindingokon keresztul tortenik.
- A `CHARACTER` inventory automatikus kinyeresbol keszult. A deklaracioszam tartalmazza a makro-orzott agakat es az inline accessorokat is.
- A `CHARACTER` osztaly erosen aggregalt mezoket tartalmaz (`m_points`, `m_pointsInstant`, inventory/shop/party/guild/dungeon/affect/session allapotok), ezert a tiszta ECS bontas nem 1:1 mezo->komponens lesz, hanem logikai szeletekre bontott komponenskeszlet kell.

## Fajlmellekletek
- `phase0_char_include_direct.txt`
- `phase0_char_include_transitive.txt`
- `phase0_character_members.txt`
- `phase0_character_method_declarations.txt`
- `phase0_character_method_definitions.txt`
- `phase0_quest_interop.txt`
- `phase0_desc_character_interactions.txt`

## Session-rendszer jeloltek (DESC <-> CHARACTER)
- `DESC::BindCharacter()` / `DESC::GetCharacter()` / `DESC::Destroy()` -> session bind/unbind lifecycle
- `CHARACTER::Disconnect()` / `CHARACTER::IsPC()` / `CHARACTER::ChatPacket()` / `CHARACTER::PointsPacket()` / `CHARACTER::SyncPacket()` -> outbound session sync
- `input_main.cpp` es tarsai -> descriptorbol karakter-azonositas, packet translation
- quest/game/shop/party/arena/war_map -> `ch->GetDesc()->Packet(...)` mintazatok, kesobbi `NetworkSyncSystem` / `SessionSystem` celpontok

## Build allapot
- `cmake -S . -B build`: **sikeres**
- `cmake --build build --config RelWithDebInfo --parallel 8`: **sikertelen**, mar a baseline agon is
- `cmake --build build --config RelWithDebInfo --target GameServer --parallel 8`: **sikertelen**, baseline hiba `LostCastleDungeon.cpp` (`SetCharType`, `SetFakePlayer`, `IsFakePlayer` hianyzik a `CHARACTER` API-bol)
- Tovabbi baseline hiba a teljes solution buildben: `ClientManagerBoot.cpp` (`str_to_number` tulterheles hianya `uint64_t`-ra), valamint `Mysql2Proto` linker hibak
