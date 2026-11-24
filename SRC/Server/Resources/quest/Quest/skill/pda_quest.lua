quest pda_quest begin
	state start begin
		function GetSkillList(min_level)
			local skill_list = special.active_skill_list[pc.get_job()+1][pc.get_skill_group()]
			local vnum_list = {}
			local name_list = {}
			for i = 1,table.getn(skill_list) do
				local skill_vnum = skill_list[i]
				local skill_level = pc.get_skill_level(skill_vnum)
				if skill_level >= min_level and skill_level < 40 then
					table.insert(vnum_list, skill_list[i])
					table.insert(name_list, locale.GM_SKILL_NAME_DICT[skill_vnum])
				end
			end

			return vnum_list, name_list
		end

		when "catalog.10.soul_stone_fc0c0404af91".typed_use begin
			local item_title = game.typed_item_use_name("Soul Stone")
			if not pc.can_warp() then
				syschat(gameforge[pc.get_language()].reset_scroll._35)
				return
			end

			say_title(string.format("%s:", item_title))
			say("")
			local lang = pc.get_language()
			if pc.get_skill_group() == 0 then
				say(gameforge[lang].pda_quest._1)
				return
			end

			local cooldown = pc.getf("pda_quest", "cooldown") - get_global_time()
			if cooldown > 0 then
				if pc.is_skill_book_no_delay() then
					pc.remove_skill_book_no_delay()
				else
					local h = math.floor(cooldown / 3600)
					local m = math.floor((cooldown - (3600 * h)) / 60)
					local hS = gameforge[lang].common.hours
					if h == 1 then
						hS = gameforge[lang].common.hour
					end
					local mS = gameforge[lang].common.minutes
					if m == 1 then
						mS = gameforge[lang].common.minute
					end

					say(string.format(gameforge[lang].common.need_to_wait, h, hS, m, mS))
					return
				end
			end

			local vnum_list, name_list = pda_quest.GetSkillList(30)
			if table.getn(vnum_list) == 0 then
				say(gameforge[lang].pda_quest._2)
				return
			end

			say(gameforge[lang].pda_quest._3)
			table.insert(name_list, gameforge[lang].common.cancel)
			local s = select_table(name_list)
			if table.getn(name_list) == s then
				return
			end

			local skill_name = name_list[s]
			local skill_vnum = vnum_list[s]
			local skill_level = pc.get_skill_level(skill_vnum)
			local cur_alignment = pc.get_real_alignment()
			local need_alignment = 0
			say_title(string.format("%s:", item_title))
			say("")
			-- if cur_alignment < (need_alignment - 19000) then
				-- say(gameforge[lang].pda_quest._4)
				-- return
			-- end

			say(gameforge[lang].pda_quest._5)
			say_reward(string.format(gameforge[lang].pda_quest._6, skill_name))
			say_reward(string.format(gameforge[lang].pda_quest._7, skill_level-30+1))
			-- if cur_alignment < 0 then
				-- say_reward(string.format(gameforge[lang].pda_quest._8, need_alignment, need_alignment*2))
				-- say_reward(gameforge[lang].pda_quest._9)
				-- say_reward(gameforge[lang].pda_quest._10)
				-- need_alignment=need_alignment*2
			-- else
				-- say_reward(string.format(gameforge[lang].pda_quest._11, need_alignment))
			-- end

			-- if cur_alignment >= 0 and cur_alignment < need_alignment then
				-- say_reward(gameforge[lang].pda_quest._12)
				-- say_reward(gameforge[lang].pda_quest._13)
			-- end

			say(gameforge[lang].pda_quest._14)
			local s = select(gameforge[lang].pda_quest._15, gameforge[lang].common.cancel)
			if s == 2 then
				return
			end

			say_title(string.format("%s:", item_title))
			say("")
			local saved = pc.typed_item_redeem({}, 0)
			if not saved then
				say("A targy most nem hasznalhato. / This item cannot be used now.")
				return
			end
			local res = pc.learn_grand_master_skill(skill_vnum)
			if res then
				--pc.change_alignment(-need_alignment)
				say(gameforge[lang].pda_quest._17)
				say("")
				say_reward(string.format(gameforge[lang].pda_quest._18, skill_name))
				if pc.get_skill_level(skill_vnum) == 40 then
					say_reward(gameforge[lang].pda_quest._19)
				else
					say_reward(string.format(gameforge[lang].pda_quest._20, skill_level-30+2))
				end

				say_reward(string.format(gameforge[lang].pda_quest._21, need_alignment))
			else
				local karma = 0
				--pc.change_alignment(-karma)
				say(gameforge[lang].pda_quest._16)
				say_reward(string.format(gameforge[lang].pda_quest._21, karma))
			end

			pc.setf("pda_quest", "cooldown", get_global_time() + 21600)
		end
	end
end
