quest reset_scroll begin
	state start begin
		when "catalog.10.tincture_of_kingdoms_83e4f6b6bb83".typed_use begin
			if not pc.can_warp() then
				syschat(gameforge[pc.get_language()].reset_scroll._35)
				return
			end

			say_title(string.format("%s:", item_name(71054)))
			say("")
			local lang = pc.get_language()
			ResetScroll.ShowRequisites(71054);
			if (not ResetScroll.CheckRequisites(71054)) then
				return
			end

			if (select(gameforge[lang].reset_scroll._1, gameforge[lang].reset_scroll._2) == 1) then
				say_title(string.format("%s:", item_name(71054)))
				say("")
				say(gameforge[lang].reset_scroll._7)
				local emp = select(
					gameforge[lang].reset_scroll._3,
					gameforge[lang].reset_scroll._4,
					gameforge[lang].reset_scroll._5,
					gameforge[lang].reset_scroll._6
				)

				if (emp == 4) then
					return
				end

				say_title(string.format("%s:", item_name(71054)))
				say("")
				local ret = pc.typed_change_empire(emp)
				if (ret == 1) then
					say_reward(string.format(gameforge[lang].reset_scroll._8, locale.empire_names[pc.get_empire()]))
					return
				elseif (ret == 2) then
					say_reward(gameforge[lang].reset_scroll._9)
					say_reward(gameforge[lang].reset_scroll._10)
					return
				elseif (ret == 3) then
					say_reward(gameforge[lang].reset_scroll._11)
					say_reward(gameforge[lang].reset_scroll._12)
					return
				elseif (ret ~= 999) then
					say(gameforge[lang].common.unknownerr)
					return
				end

				say(gameforge[lang].reset_scroll._13)
			end
		end

		when "catalog.10.status_reset_document_e79ad5457454".typed_use begin
			if not pc.can_warp() then
				syschat(gameforge[pc.get_language()].reset_scroll._35)
				return
			end

			say_title(string.format("%s:", item_name(71002)))
			say("")
			local lang = pc.get_language()
			say(gameforge[lang].reset_scroll._16)
			if (select(gameforge[lang].reset_scroll._1, gameforge[lang].reset_scroll._2) == 1) then
				if not pc.typed_item_redeem({}, 0) then
					say(gameforge[lang].common.unknownerr)
					return
				end
				pc.reset_point();
				say_title(string.format("%s:", item_name(71002)))
				say("")
				say(gameforge[lang].reset_scroll._17)
			end
		end

		when "catalog.10.modification_charm_679127259da2".typed_use begin
			if not pc.can_warp() then
				syschat(gameforge[pc.get_language()].reset_scroll._35)
				return
			end

			say_title(string.format("%s:", item_name(71048)))
			say("")
			local lang = pc.get_language()
			say(gameforge[lang].reset_scroll._48)
			if (select(gameforge[lang].reset_scroll._1, gameforge[lang].reset_scroll._2) == 1) then
				say_title(string.format("%s:", item_name(71048)))
				say("")
				if pc.is_polymorphed() then
					say(gameforge[lang].reset_scroll._18)
					return
				end

				local change = pc.typed_change_sex()
				if change == 3 then
					say(gameforge[lang].reset_scroll._49)
					return
				elseif change == 2 then
					say(gameforge[lang].reset_scroll._50)
					return
				elseif change == 0 then
					say(gameforge[lang].common.unknownerr)
					return
				end

				pc.polymorph(20032)
				say(gameforge[lang].reset_scroll._19)
			end
		end

		when 71099.use begin
			if not pc.can_warp() then
				syschat(gameforge[pc.get_language()].reset_scroll._35)
				return
			end

			say_title(string.format("%s:", item_name(item.get_vnum())))
			say("")
			ResetScroll.ShowRequisites(item.get_vnum());
			if (not ResetScroll.CheckRequisites(item.get_vnum())) then
				return;
			end

			local lang = pc.get_language()
			if (select(gameforge[lang].reset_scroll._1, gameforge[lang].reset_scroll._2) == 1) then
				say_title(string.format("%s:", item_name(item.get_vnum())))
				say("")
				say(gameforge[lang].reset_scroll._21)
				say_title(string.format("%s:[ENTER]", item_name(item.get_vnum())))
				local s = input();
				say_title(string.format("%s:", item_name(item.get_vnum())))
				say("")
				if (pc.get_name() == s) then
					say(gameforge[lang].reset_scroll._22)
					return
				elseif (s == nil) then
					say(gameforge[lang].reset_scroll._23)
					return
				end

				local ret = guild.change_master(s)
				if (ret == 0 or ret == 1 or ret == 4) then
					say_reward(string.format(gameforge[lang].reset_scroll._24, ret));
					return
				elseif (ret == 2) then
					say_reward(string.format(gameforge[lang].reset_scroll._25, s))
					say_reward(gameforge[lang].reset_scroll._26)
					return;
				end

				pc.remove_item(71099, 1)
				say(gameforge[lang].reset_scroll._27)
				big_notice_all(911, string.format("%s#%s#%s", pc.get_name(), guild.get_name(guild.get_id0()), s))
			end
		end

		function redistribute_stat(stat, label, display_item)
			local lang = pc.get_language()
			if not pc.can_warp() then
				syschat(gameforge[lang].reset_scroll._35)
				return
			end
			say_title(string.format("%s:", item_name(display_item)))
			say("")
			if select(gameforge[lang].reset_scroll._1, gameforge[lang].reset_scroll._2) ~= 1 then
				return
			end
			if not pc.can_warp() or not pc.typed_item_redeem({}, 0) then
				say_reward(gameforge[lang].reset_scroll._32)
				return
			end
			if pc.reset_status(stat) then
				say(string.format(gameforge[lang].reset_scroll._33, gameforge[lang].reset_scroll[label]))
			end
		end

		when "catalog.10.redistribution_vit_e630cba25bc9".typed_use begin
			reset_scroll.redistribute_stat(0, "_28", 71103)
		end

		when "catalog.10.redistribution_int_99427a85760d".typed_use begin
			reset_scroll.redistribute_stat(1, "_29", 71104)
		end

		when "catalog.10.redistribution_str_031297f88e89".typed_use begin
			reset_scroll.redistribute_stat(2, "_30", 71105)
		end

		when "catalog.10.redistribution_dex_55686f3043a2".typed_use begin
			reset_scroll.redistribute_stat(3, "_31", 71106)
		end

		when "catalog.10.scroll_of_lore_change_951ace981834".typed_use begin
			if not pc.can_warp() then
				syschat(gameforge[pc.get_language()].reset_scroll._35)
				return
			end

			say_title(string.format("%s:", item_name(71100)))
			say("")
			local lang = pc.get_language()
			ResetScroll.ShowRequisites(71100);
			if (not ResetScroll.CheckRequisites(71100)) then
				return
			end

			if (select(gameforge[lang].reset_scroll._1, gameforge[lang].reset_scroll._2) == 1) then
				say_title(string.format("%s:", item_name(71100)))
				say("")
				if not pc.can_warp() then
					say(gameforge[lang].reset_scroll._35)
					return
				end

				if not pc.typed_item_redeem({}, 0) then
					say(gameforge[lang].common.unknownerr)
					return
				end
				local addBonus = false
				local skillgroup = pc.get_skill_group()
				if pc.get_job() == 0 and skillgroup == 2 then
					addBonus = true
				elseif pc.get_job() == 1 then
					addBonus = true
				elseif pc.get_job() == 2 and skillgroup == 2 then
					addBonus = true
				elseif pc.get_job() == 3 then
					addBonus = true
				end

				if addBonus == true then
					affect.remove_collect(63, 10)
					affect.remove_collect(116, 10)
					affect.remove_collect(117, 10)
				end

				pc.clear_skill();
				pc.set_skill_group(0);

				set_quest_state("new_skill", "start")

				say(gameforge[lang].reset_scroll._36)
				if horse.is_summon() then
					if horse.is_riding() then
						horse.unride()
					end

					horse.unsummon()
				end

				pc.set_skill_level(131, 0)
				horse.set_level(0)
				select(gameforge[lang].common.wait)
				pc.warp(pc.get_x() * 100, pc.get_y() * 100)
			end
		end

		when "catalog.10.tincture_of_the_name_2cbd51b0aa94".typed_use begin
			if not pc.can_warp() then
				syschat(gameforge[pc.get_language()].reset_scroll._35)
				return
			end

			say_title(string.format("%s:", item_name(71055)))
			say("")
			ResetScroll.ShowRequisites(71055);
			if (not ResetScroll.CheckRequisites(71055)) then
				return;
			end

			local lang = pc.get_language()
			say(gameforge[lang].reset_scroll._39)
			local new_name = input()
			say_title(string.format("%s:", item_name(71055)))
			say("")
			local old_name = pc.get_name();
			if (new_name == old_name) then
				say_reward(gameforge[lang].reset_scroll._40)
				return
			end

			local ret = pc.typed_change_name(new_name);
			if (ret == 0) then
				say_reward(gameforge[lang].reset_scroll._41)
			elseif (ret == 1) then
				say_reward(gameforge[lang].reset_scroll._42)
			elseif (ret == 2) then
				say_reward(gameforge[lang].reset_scroll._43)
			elseif (ret == 3) then
				say_reward(gameforge[lang].reset_scroll._44)
			elseif (ret == 4) then
				say_reward(gameforge[lang].reset_scroll._45)
				say_reward(gameforge[lang].reset_scroll._46)
			else
				say_reward(gameforge[lang].reset_scroll._47)
			end
		end
	end
end
