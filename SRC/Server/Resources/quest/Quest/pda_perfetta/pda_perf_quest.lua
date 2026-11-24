quest pda_perf_quest begin
	state start begin
		function GetSkillList()
			local groups = special.active_skill_list[pc.get_job()+1]
			local skill_list = groups and groups[pc.get_skill_group()] or {}
			local skill_ids = {}
			local name_list = {}
			for _, skill_id in ipairs(skill_list) do
				local skill_level = pc.get_skill_level(skill_id)
				local name = locale.GM_SKILL_NAME_DICT[skill_id]
				if name and skill_level >= 30 and skill_level < 40 then
					table.insert(skill_ids, skill_id)
					table.insert(name_list, name)
				end
			end
			return skill_ids, name_list
		end

		when "catalog.10.perfect_soul_stone_211da47ac32f".typed_use begin
			local lang = pc.get_language()
			local messages = gameforge[lang] or gameforge.en
			if not pc.can_warp() then
				syschat(messages.reset_scroll._35)
				return
			end

			local title = game.typed_item_use_name("Perfect Soul Stone")
			say_title(string.format("%s:", title))
			say("")
			if pc.get_skill_group() == 0 then
				say(messages.pda_perf_quest._1)
				return
			end

			local skill_ids, name_list = pda_perf_quest.GetSkillList()
			say("G1-G10 (30-39) -> P (40)")
			if #skill_ids == 0 then
				say_reward(messages.pda_perf_quest._5)
				return
			end

			if select(messages.common.wait) ~= 1 then
				return
			end
			say_title(string.format("%s:", title))
			say("")
			say(lang == "hu" and "Melyik legyen?" or messages.pda_perf_quest._6)
			table.insert(name_list, lang == "hu" and "Most nem" or messages.pda_perf_quest._7)
			local s = select_table(name_list)
			if not skill_ids[s] then
				return
			end

			local skill_name = name_list[s]
			local skill_id = skill_ids[s]
			say_title(string.format("%s:", title))
			say("")
			say(string.format(messages.pda_perf_quest._8, skill_name))
			say(messages.pda_perf_quest._9)
			local a = select(messages.common.yes, messages.common.no)
			if a ~= 1 then
				return
			end

			say_title(string.format("%s:", title))
			say("")
			if pc.typed_perfect_soul_stone(skill_id) ~= 1 then
				say("A tárgy most nem használható. / This item cannot be used now.")
				return
			end
			say(string.format(messages.pda_perf_quest._10, skill_name))
		end
	end
end
