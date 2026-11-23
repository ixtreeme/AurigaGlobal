#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
namespace auriga::item {
struct DungeonEntryBinding {std::uint32_t map;std::string_view gate,ticketCode,alternateTicketCode{};};
// Authored dungeon identities and native catalog content codes.
inline constexpr std::array DungeonEntryBindings{
 DungeonEntryBinding{366,"dungeon.easter.entry","catalog.10.empty_basket_91293abed792"},
 DungeonEntryBinding{161,"dungeon.halloween.entry","catalog.10.ampoule_with_blood_e3d2daf02c11"},
 DungeonEntryBinding{380,"dungeon.lost_castle.entry","material.arpa"},
 DungeonEntryBinding{373,"dungeon.nightmare.entry","catalog.6.entry_permit_ecc50f34dded"},
 DungeonEntryBinding{355,"dungeon.orcs.entry","catalog.10.ork_dungeon_key_97b2ab2dff75"},
 DungeonEntryBinding{357,"dungeon.pyramid.entry","catalog.10.ankh_cross_03e592942268"},
 DungeonEntryBinding{218,"dungeon.rune.entry","catalog.10.ticket_of_the_runes_8546eaf0058e"},
 DungeonEntryBinding{369,"dungeon.triton.entry","catalog.10.triton_temple_key_b0da09c41c85"},
 DungeonEntryBinding{377,"dungeon.valentine.entry","catalog.10.chocolate_heart_45049195b359"},
 DungeonEntryBinding{179,"dungeon.viking.entry","catalog.10.ironclad_amulet_26fbf68fe199"},
 DungeonEntryBinding{216,"dungeon.catacombe.entry","catalog.10.shrunken_head_62c933bd7709","catalog.10.shrunken_head_856a35571910"},
 DungeonEntryBinding{208,"dungeon.dragonlair.entry","catalog.10.twisted_key_78eb5e549261"},
 DungeonEntryBinding{217,"dungeon.spider.entry","catalog.10.arachnids_key_0926c108398f"},
 DungeonEntryBinding{210,"dungeon.enchanted_forest.entry","catalog.10.stone_of_the_dryads_e460b18a7b72"},
 DungeonEntryBinding{352,"dungeon.nemere.entry","catalog.10.nemere_ticket_1cbf08edd8cf"},
 DungeonEntryBinding{351,"dungeon.razador.entry","catalog.10.razador_ticket_24917d334e09"},
 DungeonEntryBinding{27,"dungeon.slime.entry","catalog.10.piece_of_menhir_slime_93d26b69b81c"},
 DungeonEntryBinding{209,"dungeon.temple_ochao.entry","catalog.3.ochao_ticket_cf52bd498aaf"},
 DungeonEntryBinding{18,"dungeon.owl.entry","catalog.10.owl_pass_a71005b2ac06"},
 DungeonEntryBinding{25,"dungeon.christmas_extreme.entry","catalog.10.christmas_dungeon_extreme_d211aaddcf78"},
 DungeonEntryBinding{26,"dungeon.christmas_medium.entry","catalog.10.christmas_dungeon_medium_76e6f89b09a8"},
 DungeonEntryBinding{212,"dungeon.meley.entry","catalog.10.meley_erlaubnis_6bc641881c2a"}
};
inline const DungeonEntryBinding* DungeonEntryForMap(std::uint32_t map){for(const auto& b:DungeonEntryBindings)if(b.map==map)return &b;return nullptr;}
inline std::string DungeonEntryCountCommand(std::uint32_t map,std::uint64_t count){if(!map || count>INT32_MAX)return {};return "nx_dungeon_item 1 "+std::to_string(map)+" "+std::to_string(count);}
inline std::string DungeonEntryBusyCommand(bool busy){return busy?"nx_dungeon_busy 1 1":"nx_dungeon_busy 1 0";}
}
