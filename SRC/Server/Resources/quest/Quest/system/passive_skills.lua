quest passive_skills begin
	state start begin
		when "catalog.10.stone_of_soul_boost_e79b666e1446".typed_use begin
			local item_title = game.typed_item_use_name("Stone of Soul Boost")
			if not pc.can_warp() then
				syschat(gameforge[pc.get_language()].reset_scroll._35)
				return
			end

			say_title(string.format("%s:", item_title))
			say("")
			local lang = pc.get_language()
			if pc.get_skill_group() == 0 then
				say(gameforge[lang].passive_skills._1)
				return
			end

			local waitTime = pc.getqf("next_time_pda1") - get_time()
			if waitTime > 0 then
				if not pc.is_skill_book_no_delay() then
					waitHours = math.floor(waitTime / 3600)
					waitMinutes = math.floor(waitTime - math.floor(waitHours * 3600))
					if waitMinutes < 0 then
						waitMinutes = 0
					end

					say(string.format(gameforge[lang].passive_skills._2, waitHours, waitMinutes / 60))
					return
				end
			end

			GRAND_MASTER_SKILL_LEVEL = 30
			PERFECT_MASTER_SKILL_LEVEL = 40
			local check_learned = 0
			local new_skill_list = {236, 237, 238, 239, 240, 241, 242, 243}
			for i = 1, table.getn(new_skill_list) do
				skill_vnum = new_skill_list[i]
				if pc.get_skill_level(skill_vnum) >= GRAND_MASTER_SKILL_LEVEL and pc.get_skill_level(skill_vnum) < PERFECT_MASTER_SKILL_LEVEL then
					check_learned = 1
				end
			end

			if check_learned == 0 then
				say(gameforge[lang].passive_skills._3)
				return
			end

			new_skill_list_name = {
								[236] = gameforge[lang].passive_skills._4,
								[237] = gameforge[lang].passive_skills._5,
								[238] = gameforge[lang].passive_skills._6,
								[239] = gameforge[lang].passive_skills._7,
								[240] = gameforge[lang].passive_skills._8,
								[241] = gameforge[lang].passive_skills._9,
								[242] = gameforge[lang].passive_skills._10,
								[243] = gameforge[lang].passive_skills._11,
			}

			local skill_vnum_list = {}
			local skill_name_list = {}
			for i = 1, table.getn(new_skill_list) do
				skill_vnum = new_skill_list[i]
				if pc.get_skill_level(skill_vnum) >= GRAND_MASTER_SKILL_LEVEL and pc.get_skill_level(skill_vnum) < PERFECT_MASTER_SKILL_LEVEL then
					table.insert(skill_vnum_list, skill_vnum)
					table.insert(skill_name_list, new_skill_list_name[skill_vnum])
				end
			end

			if table.getn(skill_vnum_list) == 0 then
				say(gameforge[lang].passive_skills._3)
				return
			end

			say(gameforge[lang].passive_skills._12)
			local s = select(gameforge[lang].common.yes, gameforge[lang].common.no)
			if s == 2 then
				return
			end

			say_title(string.format("%s:", item_title))
			say("")
			say(gameforge[lang].passive_skills._13)
			table.insert(skill_name_list, gameforge[lang].common.cancel)
			local i = select_table(skill_name_list)
			if i == table.getn(skill_name_list)then
				return
			end

			local name = skill_name_list[i]
			local vnum = skill_vnum_list[i]
			local level = pc.get_skill_level(vnum)
			local cur_alignment = pc.get_real_alignment()
			local need_alignment = 1000 + 500 * (level - 30)

			say_title(string.format("%s:", item_title))
			say("")
			if cur_alignment <- 19000+need_alignment then
				say_reward(gameforge[lang].passive_skills._14)
				return
			end

			say(string.format(gameforge[lang].passive_skills._15, name))
			local confirm = select(gameforge[lang].common.yes, gameforge[lang].common.no)
			if confirm == 1 then
				local saved = pc.typed_item_redeem({}, 0)
				if not saved then
					say("A targy most nem hasznalhato. / This item cannot be used now.")
					return
				end
				if pc.is_skill_book_no_delay() then
					pc.remove_skill_book_no_delay()
				end

				pc.setqf("next_time_pda1", get_time() + 60 * 60 * 4)

				say_title(string.format("%s:", item_title))
				say("")
				if pc.learn_grand_master_skill(vnum) then
					if pc.get_skill_level(vnum) >= 40 then
						say(string.format(gameforge[lang].passive_skills._17, name))
					else
						say(string.format(gameforge[lang].passive_skills._18, name, level-30+1+1))
					end

					say(gameforge[lang].passive_skills._19)
				else
					pc.change_alignment(-number(need_alignment / 3, need_alignment / 2))
					say(gameforge[lang].passive_skills._20)
				end

			end

			return
		end

		when "catalog.10.stone_of_souls_counterattack_19aabe994d0c".typed_use begin
			local item_title = game.typed_item_use_name("Stone of Souls (Counterattack)")
			if not pc.can_warp() then
				syschat(gameforge[pc.get_language()].reset_scroll._35)
				return
			end

			say_title(string.format("%s:", item_title))
			say("")
			local lang = pc.get_language()
			if pc.get_skill_group() == 0 then
				say(gameforge[lang].passive_skills._1)
				return
			end

			local waitTime = pc.getqf("next_time_pda2") - get_time()
			if waitTime > 0 then
				if not pc.is_skill_book_no_delay() then
					waitHours = math.floor(waitTime / 3600)
					waitMinutes = math.floor(waitTime - math.floor(waitHours * 3600))
					if waitMinutes < 0 then
						waitMinutes = 0
					end

					say(string.format(gameforge[lang].passive_skills._2, waitHours, waitMinutes / 60))
					return
				end
			end

			GRAND_MASTER_SKILL_LEVEL = 30
			PERFECT_MASTER_SKILL_LEVEL = 40
			local check_learned = 0
			local new_skill_list = {221, 222, 223, 224, 225, 226, 227, 228}
			for i = 1, table.getn(new_skill_list) do
				skill_vnum = new_skill_list[i]
				if pc.get_skill_level(skill_vnum) >= GRAND_MASTER_SKILL_LEVEL and pc.get_skill_level(skill_vnum) < PERFECT_MASTER_SKILL_LEVEL then
					check_learned = 1
				end
			end

			if check_learned == 0 then
				say(gameforge[lang].passive_skills._3)
				return
			end

			new_skill_list_name = {
								[221] = gameforge[lang].passive_skills._21,
								[222] = gameforge[lang].passive_skills._22,
								[223] = gameforge[lang].passive_skills._23,
								[224] = gameforge[lang].passive_skills._24,
								[225] = gameforge[lang].passive_skills._25,
								[226] = gameforge[lang].passive_skills._26,
								[227] = gameforge[lang].passive_skills._27,
								[228] = gameforge[lang].passive_skills._28,
			}

			local skill_vnum_list = {}
			local skill_name_list = {}
			for i = 1, table.getn(new_skill_list) do
				skill_vnum = new_skill_list[i]
				if pc.get_skill_level(skill_vnum) >= GRAND_MASTER_SKILL_LEVEL and pc.get_skill_level(skill_vnum) < PERFECT_MASTER_SKILL_LEVEL then
					table.insert(skill_vnum_list, skill_vnum)
					table.insert(skill_name_list, new_skill_list_name[skill_vnum])
				end
			end

			if table.getn(skill_vnum_list) == 0 then
				say(gameforge[lang].passive_skills._3)
				return
			end

			say(gameforge[lang].passive_skills._12)
			local s = select(gameforge[lang].common.yes, gameforge[lang].common.no)
			if s == 2 then
				return
			end

			say_title(string.format("%s:", item_title))
			say("")
			say(gameforge[lang].passive_skills._13)
			table.insert(skill_name_list, gameforge[lang].common.cancel)
			local i = select_table(skill_name_list)
			if i == table.getn(skill_name_list)then
				return
			end

			local name = skill_name_list[i]
			local vnum = skill_vnum_list[i]
			local level = pc.get_skill_level(vnum)
			local cur_alignment = pc.get_real_alignment()
			local need_alignment = 1000 + 500 * (level - 30)

			say_title(string.format("%s:", item_title))
			say("")
			if cur_alignment <- 19000+need_alignment then
				say_reward(gameforge[lang].passive_skills._14)
				return
			end

			say(string.format(gameforge[lang].passive_skills._15, name))
			local confirm = select(gameforge[lang].common.yes, gameforge[lang].common.no)
			if confirm == 1 then
				local saved = pc.typed_item_redeem({}, 0)
				if not saved then
					say("A targy most nem hasznalhato. / This item cannot be used now.")
					return
				end
				if pc.is_skill_book_no_delay() then
					pc.remove_skill_book_no_delay()
				end

				pc.setqf("next_time_pda2", get_time() + 60 * 60 * 6)

				say_title(string.format("%s:", item_title))
				say("")
				if pc.learn_grand_master_skill(vnum) then
					if pc.get_skill_level(vnum) >= 40 then
						say(string.format(gameforge[lang].passive_skills._17, name))
					else
						say(string.format(gameforge[lang].passive_skills._18, name, level-30+1+1))
					end

					say(gameforge[lang].passive_skills._19)
				else
					pc.change_alignment(-number(need_alignment / 3, need_alignment / 2))
					say(gameforge[lang].passive_skills._20)
				end

			end

			return
		end
	end
end
