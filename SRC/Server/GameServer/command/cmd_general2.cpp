#include "stdafx.h"
#include "../dungeon/DungeonUi.hpp"
#include "../combat/DeathRecap.hpp"
#include "../social/SocialActions.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include <Core/Logging.hpp>
#include "../ecs/systems/AffectSystem.hpp"
#include "../ecs/systems/QuestSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../item/ItemCatalogRuntime.hpp"
#include "../item/ItemStore.hpp"
#include "../item/TypedQuestItems.hpp"
#include "../network/desc.h"
#include <Headers/Header_GC.h>
#include <item/ItemWireCodec.hpp>
#include <item/TypedItemDetails.hpp>
#include <cmath>
#include "../ecs/AIHelpers.hpp"
#include <common/service.h>
#include <common/CommonDefines.h>
#include <common/length.h>
#include "char_interface.hpp"
#include "../ecs/CharacterAccessors.hpp"
#include "skill.h"
#include "config.h"
#include "sectree_manager.h"
#include "../world/AtlasActions.hpp"
#include "db.h"
#ifdef ENABLE_CHOOSE_DOCTRINE_GUI
#include "horsename_manager.h"
#endif

#define MAX_STATUS_ALTERNATIVE g_iStatusPointSetMaxValue

#ifdef ENABLE_BLOCK_MULTIFARM
ACMD(do_drop_block) {
	ecs::PlayerRuntime::BlockDrop(character);
}

ACMD(do_drop_unblock) {
	ecs::PlayerRuntime::UnblockDrop(character);
}
#endif

ACMD(do_remove_affect)
{
	if (!ecs::PlayerRuntime::IsValid(character))
		return;

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	uint32_t dwAffect = 0;
	str_to_number(dwAffect, arg1);

	switch (dwAffect)
	{
		case AFF_JEONGWIHON:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_JEONGWIHON))
					AffectSystem::RemoveAffect(character, SKILL_JEONGWI);
			}
			break;
		case AFF_GEOMGYEONG:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_GEOMGYEONG))
					AffectSystem::RemoveAffect(character, SKILL_GEOMKYUNG);
			}
			break;
		case AFF_CHEONGEUN:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_CHEONGEUN))
					AffectSystem::RemoveAffect(character, SKILL_CHUNKEON);
			}
			break;
		case AFF_GYEONGGONG:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_GYEONGGONG))
					AffectSystem::RemoveAffect(character, SKILL_GYEONGGONG);
			}
			break;
		case AFF_GWIGUM:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_GWIGUM))
					AffectSystem::RemoveAffect(character, SKILL_GWIGEOM);
			}
			break;
		case AFF_TERROR:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_TERROR))
					AffectSystem::RemoveAffect(character, SKILL_TERROR);
			}
			break;
		case AFF_JUMAGAP:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_JUMAGAP))
					AffectSystem::RemoveAffect(character, SKILL_JUMAGAP);
			}
			break;
		case AFF_MUYEONG:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_MUYEONG))
					AffectSystem::RemoveAffect(character, SKILL_MUYEONG);
			}
			break;
		case AFF_MANASHIELD:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_MANASHIELD))
					AffectSystem::RemoveAffect(character, SKILL_MANASHILED);
			}
			break;
		case AFF_HOSIN:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_HOSIN))
					AffectSystem::RemoveAffect(character, SKILL_HOSIN);
			}
			break;
		case AFF_BOHO:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_BOHO))
					AffectSystem::RemoveAffect(character, SKILL_REFLECT);
			}
			break;
		case AFF_GICHEON:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_GICHEON))
					AffectSystem::RemoveAffect(character, SKILL_GICHEON);
			}
			break;
		case AFF_KWAESOK:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_KWAESOK))
					AffectSystem::RemoveAffect(character, SKILL_KWAESOK);
			}
			break;
		case AFF_JEUNGRYEOK:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_JEUNGRYEOK))
					AffectSystem::RemoveAffect(character, SKILL_JEUNGRYEOK);
			}
			break;
		default:
			return;
	}
}

ACMD(do_stat2)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	if (!*arg1)
		return;

	if (AffectSystem::IsPolymorphed(character)) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 314, "");
#endif
		return;
	}


	int64_t limit = 10;


	if (ecs::PointSystem::Get(character, POINT_STAT) < limit) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 851,

		"%lld"

		, static_cast<long long>(limit));
#endif
		return;
	}

	uint8_t idx = 0;
	if (!strcmp(arg1, "st"))
		idx = POINT_ST;
	else if (!strcmp(arg1, "dx"))
		idx = POINT_DX;
	else if (!strcmp(arg1, "ht"))
		idx = POINT_HT;
	else if (!strcmp(arg1, "iq"))
		idx = POINT_IQ;
	else
		return;

	if (ecs::PointSystem::GetReal(character, idx) >= MAX_STATUS_ALTERNATIVE)
		return;

	limit = ecs::PointSystem::GetReal(character, idx) + limit >= MAX_STATUS_ALTERNATIVE ? MAX_STATUS_ALTERNATIVE - ecs::PointSystem::GetReal(character, idx) : limit;
	ecs::PointSystem::SetReal(character, idx, ecs::PointSystem::GetReal(character, idx) + limit);
	ecs::PointSystem::Set(character, idx, ecs::PointSystem::Get(character, idx) + limit);
	ecs::PointSystem::Compute(character);
	ecs::PointSystem::Change(character, idx, 0);

	if (idx == POINT_IQ) {
		ecs::PointSystem::Change(character, POINT_MAX_HP, 0);
	}
	else if (idx == POINT_HT) {
		ecs::PointSystem::Change(character, POINT_MAX_SP, 0);
	}

	ecs::PointSystem::Change(character, POINT_STAT, -limit);
	ecs::PointSystem::Compute(character);
}

#ifdef ENABLE_BIOLOGIST_UI
namespace {
constexpr std::array<std::string_view, 16> BiologistSpecimens{
    "material.orkfog", "material.atokkonyv", "material.demon_emlektargy", "material.jegmarvany",
    "material.zelkova_ag", "material.tugy_tablaja", "material.voros_szellemfa_ag", "material.vezetoi_jegyzetek",
    "material.rosszindulatu_ekszer", "material.bolcsesseg_ekszer", "material.sotet_pafrany", "material.chaegirab_gyuruje",
    "material.metin_kristaly", "material.tisztitott_mag", "material.sarkanyfog", "material.voros_sarkanykarom",
};
constexpr std::string_view BiologistElixir = "catalog.6.researcher_s_elixir_27e7eeaa51fc";
constexpr std::string_view BiologistElixirPlus = "catalog.6.researcher_s_elixir_7f2fb2106e29";
constexpr std::string_view BiologistBonusChanger = "catalog.10.magic_of_the_biologist_ed2ba53b116d";

const auriga::item::ItemCatalogEntry* FindBiologistItem(const auriga::item::ItemCatalog& catalog, std::string_view code)
{
    const auto& entries = catalog.Entries();
    const auto entry = std::ranges::find_if(entries, [&](const auto& row) { return row.second.base.contentCode == code; });
    return entry != entries.end() && !entry->second.base.retired && catalog.RefinedBaseStats(entry->first, 0)
        ? &entry->second : nullptr;
}

void SendTypedBiologist(entt::entity character, int mission, bool open)
{
    namespace item = auriga::item;
    auto* desc = ecs::PlayerRuntime::GetDesc(character);
    const auto catalog = item::runtime::CurrentCatalog();
    if (!catalog || !desc || !desc->IsPhase(PHASE_GAME))
        return;

    const auto& row = biologistMissionInfo[mission];
    const int required = static_cast<int>(row[0]);
    const int delivered = std::clamp<std::int64_t>(ecs::QuestSystem::GetFlag(character, "biologist.delivered"), 0, required);
    const auto waitUntil = std::max<std::int64_t>(0, ecs::QuestSystem::GetFlag(character, "biologist.time"));

    std::vector<std::byte> bytes;
    const auto put = [&](std::uint64_t value, unsigned width) {
        for (unsigned i = 0; i < width; ++i)
            bytes.push_back(static_cast<std::byte>((value >> (8 * i)) & 0xff));
    };
    put(HEADER_GC_TYPED_BIOLOGIST, 1);
    put(0, 4);
    put(1, 1);
    put(open, 1);
    put(mission, 1);
    put(required, 2);
    put(delivered, 2);
    put(waitUntil, 8);
    put(static_cast<int>(row[10]), 1);
    for (int i = 0; i < 4; ++i)
    {
        put(static_cast<int>(row[2 + i * 2]), 2);
        put(static_cast<std::uint32_t>(std::lround(row[3 + i * 2] * 1000.0)), 4);
    }

    std::vector<item::ItemGrant> preview;
    for (const auto code : {BiologistSpecimens[mission], BiologistElixir, BiologistElixirPlus})
    {
        const auto* definition = FindBiologistItem(*catalog, code);
        if (!definition)
        {
            put(0, 6);
            continue;
        }
        const auto key = definition->base.key;
        const auto encoded = item::EncodeItemKey(key);
        bytes.insert(bytes.end(), encoded.begin(), encoded.end());
        preview.push_back({key, 0, 1});
    }
    const auto details = item::EncodeTypedItemDetails(*catalog, {}, false, preview);
    if (!details || bytes.size() + details->size() > (1u << 20))
        return;
    bytes.insert(bytes.end(), details->begin(), details->end());
    for (unsigned i = 0; i < 4; ++i)
        bytes[1 + i] = static_cast<std::byte>((bytes.size() >> (8 * i)) & 0xff);
    desc->LargePacket(bytes.data(), static_cast<int>(bytes.size()));
}
}

ACMD(do_open_biologist) {
	if (!ecs::PlayerRuntime::IsValid(character))
		return;

	int stat = ecs::QuestSystem::GetFlag(character, "biologist.stat");
	if (stat < 0 || stat >= static_cast<int>(BiologistSpecimens.size())) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 867, "");
#endif
		return;
	}
	int min = static_cast<int>(biologistMissionInfo[stat][11]);
	if ((ecs::PointSystem::GetLevel(character)) < min) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 861, "%d", min);
#endif
		return;
	}
	SendTypedBiologist(character, stat, true);
}

ACMD(do_delivery_biologist) {
	if (!ecs::PlayerRuntime::IsValid(character))
		return;

	char arg1[256], arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || !*arg2)
		return;

	int stat = ecs::QuestSystem::GetFlag(character, "biologist.stat");
	if (stat < 0 || stat > 15) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 867, "");
#endif
		return;
	}
	int min = static_cast<int>(biologistMissionInfo[stat][11]);
	if ((ecs::PointSystem::GetLevel(character)) < min) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 861, "%d", min);
#endif
		return;
	}

	int count = ecs::QuestSystem::GetFlag(character, "biologist.delivered");
	if (count >= static_cast<int>(biologistMissionInfo[stat][0])) {
		return;
	}

	namespace item = auriga::item;
	const auto catalog = item::runtime::CurrentCatalog();
	const auto* inventory = g_registry.try_get<ecs::TypedItems>(character);
	if (!catalog || !inventory || g_registry.all_of<ecs::TypedItemBusy>(character))
		return;
	const auto owned = [&](std::string_view code) -> std::optional<item::ItemKey> {
		const auto& entries = catalog->Entries();
		const auto found = std::ranges::find_if(entries, [code](const auto& row) { return row.second.base.contentCode == code; });
		if (found == entries.end() || found->second.base.retired ||
			!item::CountGameplayExchangeItems(inventory->state.items, inventory->state.fence.owner, found->first, 0))
			return {};
		return found->first;
	};
	const auto specimen = owned(BiologistSpecimens[stat]);
	if (!specimen) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 866, "");
#endif
		return;
	}

	int iarg1 = 0;
	str_to_number(iarg1, arg1);
	int iarg2 = 0;
	str_to_number(iarg2, arg2);
	if (iarg1 == 1 && iarg2 == 1) {
		return;
	}

	bool elisir = iarg1 == 1 ? true : false;
	bool potion = iarg2 == 1 ? true : false;

	int time = ecs::QuestSystem::GetFlag(character, "biologist.time") ;
	if (time > 0 && time - get_global_time() > 0 && !elisir && !potion) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 871, "");
#endif
		return;
	}

	std::vector<item::MissionItemAmount> costs{{*specimen, 0, 1}};
	if (elisir || potion) {
		const auto aid = owned(potion ? BiologistElixirPlus : BiologistElixir);
		if (!aid) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, potion ? 863 : 862, "");
#endif
			return;
		}
		costs.push_back({*aid, 0, 1});
	}

	const int chance = potion ? 100 : static_cast<int>(biologistMissionInfo[stat][12]);
	const int required = static_cast<int>(biologistMissionInfo[stat][0]);
	const int cooldown = static_cast<int>(biologistMissionInfo[stat][1]);
	const auto exchange = RequestTypedGameplayItemExchange(character, std::move(costs), {}, 0,
		[character, count, required, cooldown, success = number(1, 100) <= chance](bool written) {
			if (!written) {
				ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "A biológus leadását nem sikerült menteni.");
				return;
			}
			const int delivered = count + (success ? 1 : 0);
			const bool completed = delivered >= required;
			const int remaining = completed ? 0 : cooldown;
			const int until = get_global_time() + remaining;
			if (success)
				ecs::QuestSystem::SetFlag(character, "biologist.delivered", delivered);
			ecs::QuestSystem::SetFlag(character, "biologist.time", until);
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, success ? 865 : 864, "");
#endif
			ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "biologist_delivered %d#%d#%d", completed ? 1 : 0, delivered, remaining);
			ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "biologist_time %d", until);
		});
	if (!exchange)
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "A leadás jelenleg nem elérhető. Próbáld újra.");
}

ACMD(do_reward_biologist) {
	if (!ecs::PlayerRuntime::IsValid(character))
		return;

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	int stat = ecs::QuestSystem::GetFlag(character, "biologist.stat");
	if (stat < 0 || stat >= static_cast<int>(BiologistSpecimens.size())) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 867, "");
#endif
		return;
	}
	int count = ecs::QuestSystem::GetFlag(character, "biologist.delivered");
	if (static_cast<int>(biologistMissionInfo[stat][0])-count > 0) {
		return;
	}

	int iarg1 = 0;
	str_to_number(iarg1, arg1);
	if (static_cast<int>(biologistMissionInfo[stat][10]) == 0 && iarg1 != 999) {
		return;
	} else if (static_cast<int>(biologistMissionInfo[stat][10]) == 1) {
		if (iarg1 < 0 || iarg1 > 3) {
			return;
		}
	}

	int newstat = stat + 1;
	ecs::QuestSystem::SetFlag(character, "biologist.stat", newstat);
	ecs::QuestSystem::SetFlag(character, "biologist.time", 0);
	ecs::QuestSystem::SetFlag(character, "biologist.delivered", 0);
	if (newstat == 16) {
		if (static_cast<int>(biologistMissionInfo[stat][10]) == 0) {
			int j = 0;
			for (int i = 0; i < 4; i++) {
				j += 2;
				uint8_t bApplyOn = biologistMissionInfo[stat][j];
				double lApplyValue = biologistMissionInfo[stat][j + 1];
				if (bApplyOn == APPLY_NONE || lApplyValue == 0) {
					continue;
				}
				else {
					bApplyOn = aApplyInfo[bApplyOn].bPointType;
					AffectSystem::AddAffect(character, static_cast<int>(biologistMissionInfo[stat][13]), bApplyOn, lApplyValue, 0, 315360000, 0, false);
				}
			}
		} else {
			iarg1 += 1;
			int j = 2 * iarg1;
			uint8_t bApplyOn = biologistMissionInfo[stat][j];
			double lApplyValue = biologistMissionInfo[stat][j + 1];
			if (bApplyOn != APPLY_NONE || lApplyValue != 0) {
				bApplyOn = aApplyInfo[bApplyOn].bPointType;
				AffectSystem::AddAffect(character, static_cast<int>(biologistMissionInfo[stat][13]), bApplyOn, lApplyValue, 0, 315360000, 0, false);
			}
		}
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 870, "");
#endif
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 868, "");
#endif
		ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "biologist_close");
		return;
	}
	else if (newstat > 15) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 868, "");
#endif
		ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "biologist_close");
		return;
	} else {
		int min = static_cast<int>(biologistMissionInfo[newstat][11]);
		if ((ecs::PointSystem::GetLevel(character)) < min) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 869, "%d", min);
#endif
			ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "biologist_close");
			return;
		} else {
			if (static_cast<int>(biologistMissionInfo[stat][10]) == 0) {
				int j = 0;
				for (int i = 0; i < 4; i++) {
					j += 2;
					uint8_t bApplyOn = biologistMissionInfo[stat][j];
					double lApplyValue = biologistMissionInfo[stat][j + 1];
					if (bApplyOn == APPLY_NONE || lApplyValue == 0) {
						continue;
					}
					else {
						bApplyOn = aApplyInfo[bApplyOn].bPointType;
						AffectSystem::AddAffect(character, static_cast<int>(biologistMissionInfo[stat][13]), bApplyOn, lApplyValue, 0, 315360000, 0, false);
					}
				}
			} else {
				iarg1 += 1;
				int j = 2 * iarg1;
				uint8_t bApplyOn = biologistMissionInfo[stat][j];
				double lApplyValue = biologistMissionInfo[stat][j + 1];
				if (bApplyOn != APPLY_NONE || lApplyValue != 0) {
					bApplyOn = aApplyInfo[bApplyOn].bPointType;
					AffectSystem::AddAffect(character, static_cast<int>(biologistMissionInfo[stat][13]), bApplyOn, lApplyValue, 0, 315360000, 0, false);
				}
			}

#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 870, "");
#endif
			SendTypedBiologist(character, newstat, false);
		}
	}
}

ACMD(do_change_biologist) {
	if (!ecs::PlayerRuntime::IsValid(character))
		return;

	char arg1[256], arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || !*arg2)
		return;


	int iarg1 = 0;
	str_to_number(iarg1, arg1);
	if (iarg1 < 0 || iarg1 > 15) {
		return;
	}

	if (static_cast<int>(biologistMissionInfo[iarg1][10]) != 1) {
		return;
	}

	int iarg2 = 0;
	str_to_number(iarg2, arg2);
	if (iarg2 < 0 || iarg2 > 3) {
		return;
	}

	int stat = ecs::QuestSystem::GetFlag(character, "biologist.stat");
	if (stat <= iarg1)
		return;

    const auto& row = biologistMissionInfo[iarg1];
    const int apply = static_cast<int>(row[2 + iarg2 * 2]);
    const double amount = row[3 + iarg2 * 2];
    const int affect = static_cast<int>(row[13]);
    if (apply == APPLY_NONE || amount == 0)
        return;
    const auto point = aApplyInfo[apply].bPointType;
    if (point == POINT_NONE || AffectSystem::FindAffect(character, affect, point))
        return;

    const auto catalog = auriga::item::runtime::CurrentCatalog();
    const auto* changer = catalog ? FindBiologistItem(*catalog, BiologistBonusChanger) : nullptr;
    if (!changer)
        return;
    const auto exchange = RequestTypedGameplayItemExchange(character, {{changer->base.key, 0, 1}}, {}, 0,
        [character, affect, point, amount](bool written) {
            if (!written || !ecs::PlayerRuntime::IsValid(character))
                return;
            AffectSystem::RemoveAffect(character, affect);
            AffectSystem::AddAffect(character, affect, point, amount, 0, 315360000, 0, false);
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 873, "");
#endif
            ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "biologistch_close");
        });
    if (!exchange)
        ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "A bónuszcseréhez szükséges tárgy vagy művelet nem érhető el.");
}
#endif

ACMD(do_gotoxy)
{
    RequestAtlasTeleport(character,argument);
}

ACMD(do_atlasinfo) { SendAtlasInfo(character); }
ACMD(do_social) { HandleSocialCommand(character,argument); }
ACMD(do_dungeon_ui) { DungeonUi::Handle(character,argument); }
ACMD(do_death_ui) { DeathRecap::Handle(character,argument); }

#ifdef ENABLE_SAVEPOINT_SYSTEM
ACMD(do_open_savepoint) {
	if (ecs::PlayerRuntime::IsObserverMode(character)) {
		return;
	}

	char query[512] = {0};
	snprintf(query, sizeof(query), "SELECT slot, name, map, x, y FROM player.savepoint WHERE id = %u", (ecs::PlayerRuntime::GetPlayerID(character)));
	std::unique_ptr<SQLMsg> res(DBManager::instance().DirectQuery(query));
	if (res->Get()->uiNumRows > 0) {
		std::vector<int> stat;
		for (int i = 0; i < 6; i++) {
			stat.push_back(0);
		}

		MYSQL_ROW data;
		while ((data = res->Get()->FetchRow())) {
			int c = 0;
			int id = atoi(data[c++]);
			std::string name = data[c++];
			int mapIdx = atoi(data[c++]), x = atoi(data[c++]), y = atoi(data[c++]);
			ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "append_savepoint %d %s %d %d %d", id, name.c_str(), mapIdx, x, y);
			stat[id] = 1;
		}

		for (int i = 0; i < 6; i++) {
			if (stat[i] == 0) {
				ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "append_savepoint %d %s %d %d %d", i, "-", 0, 0, 0);
			}
		}
	} else {
		for (int i = 0; i < 6; i++) {
			ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "append_savepoint %d %s %d %d %d", i, "-", 0, 0, 0);
		}
	}

	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "open_savepoint");
}

ACMD(do_empty_savepoint) {
	if (ecs::PlayerRuntime::IsObserverMode(character)) {
		return;
	}

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	if (!*arg1) {
		return;
	}

	if (!isdigit(*arg1)) {
		return;
	}

	int slot;
	str_to_number(slot, arg1);
	if (slot < 0 || slot > 5) {
		return;
	}

	char query[512] = {0};
	snprintf(query, sizeof(query), "DELETE FROM player.savepoint WHERE id = %u AND slot = %d", (ecs::PlayerRuntime::GetPlayerID(character)), slot);
	DBManager::instance().Query(query);
	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "update_savepoint %d %s %d %d %d", slot, "-", 0, 0, 0);
}

ACMD(do_go_savepoint) {
	if (ecs::PlayerRuntime::IsObserverMode(character)) {
		return;
	}

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	if (!*arg1) {
		return;
	}

	if (!isdigit(*arg1)) {
		return;
	}

	int slot;
	str_to_number(slot, arg1);
	if (slot < 0 || slot > 5) {
		return;
	}

	if (!ecs::PlayerRuntime::CanWarp(character)) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 528, "");
#endif
		return;
	}

	if (ecs::PlayerRuntime::GetMapIndex(character) > 10000) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 1288, "");
#endif
		return;
	}

	char query[512] = {0};
	snprintf(query, sizeof(query), "SELECT g_x, g_y, map FROM player.savepoint WHERE id = %u AND slot = %d", (ecs::PlayerRuntime::GetPlayerID(character)), slot);
	std::unique_ptr<SQLMsg> res(DBManager::instance().DirectQuery(query));
	if (res->Get()->uiNumRows > 0) {
		MYSQL_ROW data;
		while ((data = res->Get()->FetchRow())) {
			int c = 0;
			int x = atoi(data[c++]), y = atoi(data[c++]), mapIdx = atoi(data[c++]);

			if (mapIdx == ecs::PlayerRuntime::GetMapIndex(character)) {
				int x2 = x - ecs::PlayerRuntime::GetX(character);
				int y2 = y - ecs::PlayerRuntime::GetY(character);
				double nDist = 0;
				const double nDistant = 5000.0;
				nDist = sqrt(pow((float)x2, 2) + pow((float)y2, 2));
				if (nDistant > nDist) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 1287, "");
#endif
					return;
				}
			}

			ecs::MovementSystem::WarpSet(character, x, y);
		}
	}
}

ACMD(do_save_savepoint) {
	if (ecs::PlayerRuntime::IsObserverMode(character)) {
		return;
	}

	char arg1[256];
	char arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	if (!*arg1) {
		return;
	}

	if (!isdigit(*arg1)) {
		return;
	}

	int slot;
	str_to_number(slot, arg1);
	if (slot < 0 || slot > 5) {
		return;
	}

	int len = strlen(arg2);
	if (len < 0 || len > 8) {
		return;
	}

	int iPulse = thecore_pulse();
	if (iPulse - ecs::PlayerRuntime::GetSavePointTime(character) < PASSES_PER_SEC(10)) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 1286, "");
#endif
		return;
	}

	std::string name = arg2;
	if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890_") != std::string::npos || name.find_first_not_of(' ') == std::string::npos) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 1289, "");
#endif
		return;
	}

	char query[512] = {0};
	snprintf(query, sizeof(query), "SELECT * FROM player.savepoint WHERE id = %u AND slot = %d", (ecs::PlayerRuntime::GetPlayerID(character)), slot);
	std::unique_ptr<SQLMsg> res(DBManager::instance().DirectQuery(query));
	if (res->Get()->uiNumRows > 0) {
		LOG_ERROR("{} savepoint slot ({}) is not empty. Maybe a hacker?", ecs::PlayerRuntime::GetName(character).data(), slot);
	} else {
		int mapIdx = ecs::PlayerRuntime::GetMapIndex(character);
		if (mapIdx > 10000) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 531, "");
#endif
			return;
		}

		int arrayMapIndexBlocked[] = {81, 103, 105, 110, 113, 111};
		for (int i = 0; i < (int)_countof(arrayMapIndexBlocked); i++) {
			if (mapIdx == arrayMapIndexBlocked[i]) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 531, "");
#endif
				return;
			}
		}

		LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(mapIdx);
		if (!map) {
			return;
		}

		int x = (ecs::PlayerRuntime::GetX(character) - map->m_setting.iBaseX) / 100;
		int y = (ecs::PlayerRuntime::GetY(character) - map->m_setting.iBaseY) / 100;
		PIXEL_POSITION pos { ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), ecs::PlayerRuntime::GetZ(character) };

		DBManager::instance().QueryPrepared(
			"INSERT INTO player.savepoint (id, slot, name, map, x, y, g_x, g_y) VALUES(?, ?, ?, ?, ?, ?, ?, ?)",
			{
				SqlParam::UInt(ecs::PlayerRuntime::GetPlayerID(character)),
				SqlParam::Int(slot),
				SqlParam::Text(name),
				SqlParam::Int(mapIdx),
				SqlParam::Int(x),
				SqlParam::Int(y),
				SqlParam::Int(pos.x),
				SqlParam::Int(pos.y)
			});
		ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "update_savepoint %d %s %d %d %d", slot, name.c_str(), mapIdx, x, y);
		ecs::PlayerRuntime::SetSavePointTime(character);
	}
}
#endif

#ifdef ENABLE_CHOOSE_DOCTRINE_GUI
ACMD(do_doctrine_choose) {
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1 || (ecs::PointSystem::GetLevel(character)) < 5 || SkillSystem::GetSkillGroup(character) != 0) {
		return;
	}

	int group;
	str_to_number(group, arg1);
	if (group >= 1 && group <= 2) {
		SkillSystem::SetSkillGroup(character, group);
		SkillSystem::ClearSkill(character);
		//ch->SetSkillLevel(122, 2);
		SkillSystem::SetSkillLevel(character, 131, 1);
		//ch->SetSkillLevel(137, 20);
		//ch->SetSkillLevel(138, 20);
		//ch->SetSkillLevel(139, 20);
		//ch->SetSkillLevel(140, 20);

		MountSystem::SetHorseLevel(character, 1);
		CAffect * pkAff = nullptr;
		if (!(pkAff = AffectSystem::FindAffect(character, AFFECT_HORSE_NAME))) {
			ecs::QuestSystem::SetFlag(character, "horse_name.valid_till", get_global_time() + 126144000);
			AffectSystem::AddAffect(character, AFFECT_HORSE_NAME, 0, 0, 0, 126144000, 0, true);
			std::string name = ecs::PlayerRuntime::GetName(character).data();
			name += " Horse";
			CHorseNameManager::instance().UpdateHorseName((ecs::PlayerRuntime::GetPlayerID(character)), name.c_str(), true);

			if (MountSystem::GetSummonedHorse(character) != entt::null) {
				MountSystem::SummonHorse(character, false, true);
				MountSystem::SummonHorse(character, true, true);
			}
		}

		int job = ecs::PlayerRuntime::GetJob(character);
		if (job == JOB_ASSASSIN || job == JOB_SHAMAN) {
			if (!(pkAff = AffectSystem::FindAffect(character, AFFECT_PVM_RACE, aApplyInfo[APPLY_ATTBONUS_MONSTER].bPointType))) {
				AffectSystem::AddAffect(character, AFFECT_PVM_RACE, aApplyInfo[APPLY_ATTBONUS_MONSTER].bPointType, 10, 0, 126144000, 0, false);
			}

#ifdef ENABLE_STRONG_METIN
			if (!(pkAff = AffectSystem::FindAffect(character, AFFECT_PVM_RACE, aApplyInfo[APPLY_ATTBONUS_METIN].bPointType))) {
				AffectSystem::AddAffect(character, AFFECT_PVM_RACE, aApplyInfo[APPLY_ATTBONUS_METIN].bPointType, 10, 0, 126144000, 0, false);
			}
#endif

#ifdef ENABLE_STRONG_BOSS
			if (!(pkAff = AffectSystem::FindAffect(character, AFFECT_PVM_RACE, aApplyInfo[APPLY_ATTBONUS_BOSS].bPointType))) {
				AffectSystem::AddAffect(character, AFFECT_PVM_RACE, aApplyInfo[APPLY_ATTBONUS_BOSS].bPointType, 10, 0, 126144000, 0, false);
			}
#endif
		}

		ecs::PointSystem::Compute(character);
		SkillSystem::SendSkillLevelPacket(character);
	}
}
#endif

