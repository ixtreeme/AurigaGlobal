quest christmas_zone1 begin
	state start begin
		function clear(arg)
			clear_server_timer("christmas_zone1_prepare", arg)
			clear_server_timer("christmas_zone1_end", arg)
			clear_server_timer("christmas_zone1_complete", arg)
			clear_server_timer("christmas_zone1_wave", arg)
			clear_server_timer("christmas_zone1_warp", arg)
			clear_server_timer("christmas_zone1_loop", arg)
			if d.find(arg) then
				d.setf(arg, "was_completed", 1)
				d.clear_regen(arg)
				d.kill_all(arg)
				d.exit_all_lobby(arg, 1)
			end
		end

		when christmas_zone1_complete.server_timer begin
			christmas_zone1.clear(get_server_timer_arg())
		end

		when christmas_zone1_end.server_timer begin
			local arg = get_server_timer_arg()
			d.notice(arg, 1040, "", true)
			d.notice(arg, 1041, "", true)
			christmas_zone1.clear(arg)
		end

		when 4081.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 260000 and idx < 270000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 3 and d.getf(idx, "step") == 8 then
					d.setf(idx, "was_completed", 1)
					d.complete(4081, 0, 26, "christmas_zone1")
					if party.is_party() then
						notice_all(1333, party.get_leader_name())
					else
						notice_all(1334, pc.get_name())
					end

					d.notice(idx, 1136, "", true)
					d.notice(idx, 1137, "", true)
					d.kill_all(idx)
					d.clear_regen(idx)
					-- local bonus = 10 + game.get_event_flag("dungeon_bonus")
					-- if math.random(1, 100) <= bonus then
						-- d.spawn_mob(idx, 5001, 1050, 428)
					-- end
					server_timer("christmas_zone1_complete", 30, idx)
				end
			end
		end

		when 9298.take with pc.in_dungeon() and item.get_vnum() == 30777 begin
			local idx = pc.get_map_index()
			if idx >= 260000 and idx < 270000 then
				if d.getf(idx, "floor") == 3 and d.getf(idx, "step") == 7 then
					d.setf(idx, "step", 8)
					npc.kill()
					item.remove()
					d.clear_regen(idx)
					d.notice(idx, 1332, "", true)
					d.spawn_mob(idx, 4081, 1050, 428)
				end
			end
		end

		when 9293.chat."Hei!" with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			local lang = pc.get_language()
			if idx >= 260000 and idx < 270000 then
				local f = d.getf(idx, "floor")
				if f == 2 then
					if party.is_party() and not party.is_leader() then
						say(gameforge[lang].common.dungeon_24)
						return
					end

					local s = d.getf(idx, "step")
					if s == 1 then
						d.setf(idx, "step", 2)
						addimage(25, 10, "christmas2019_bg02.tga")
						addimage(225, 150, "santa.tga")
						say("[ENTER][ENTER]")
						say_title(string.format("%s:", mob_name(9293)))
						say("")
						say(gameforge[lang].christmas_zone1._1)
						wait()
						setskin(NOWINDOW)
						npc.kill()
						d.setf(idx, "step", 3)
						d.setf(idx, "wrong", 0)
					else
						say_title(string.format("%s:", mob_name(9293)))
						say("Szólj Razornak. Hibakód: 001 ")
						say(gameforge[lang].common.unknownerr)
					end
				elseif f == 3 then
					local s = d.getf(idx, "step")
					if s == 1 then
						d.setf(idx, "step", 2)
						addimage(25, 10, "christmas2019_bg04.tga")
						addimage(225, 150, "santa.tga")
						say("[ENTER][ENTER]")
						say_title(string.format("%s:", mob_name(9293)))
						say("")
						say(string.format(gameforge[lang].christmas_zone1._2, mob_name(9298), item_name(30777)))
						wait()
						addimage(25, 10, "christmas2019_bg04.tga")
						addimage(225, 150, "santa.tga")
						say("[ENTER][ENTER]")
						say_title(string.format("%s:", mob_name(9293)))
						say("")
						say(gameforge[lang].christmas_zone1._3)
						addimage(25, 130, "christmas2019_3floor_items.tga")
						say_reward(gameforge[lang].christmas_zone1._4)
						wait()
						setskin(NOWINDOW)
						d.setf(idx, "crafted", 0)
						d.regen_file(idx, "data/dungeon/christmas_zone1/regen_3floor_1.json")
						d.notice(idx, 1315, "", true)
						server_loop_timer("christmas_zone1_loop", 2, idx)
					elseif s == 6 then
						if pc.count_item(30774) > 0 and pc.count_item(30775) > 0 and pc.count_item(30776) > 0 then
							pc.remove_item(30774, 1)
							pc.remove_item(30775, 1)
							pc.remove_item(30776, 1)
							d.setf(idx, "step", 7)
							d.notice(idx, 1331, "", true)
							pc.give_item2(30777, 1)
							setskin(NOWINDOW)
							npc.purge()
						else
							addimage(25, 10, "christmas2019_bg04.tga")
							addimage(225, 150, "santa.tga")
							say("[ENTER][ENTER]")
							say_title(string.format("%s:", mob_name(9293)))
							say("")
							say(string.format(gameforge[lang].christmas_zone1._2, mob_name(9298), item_name(30777)))
							wait()
							addimage(25, 10, "christmas2019_bg04.tga")
							addimage(225, 150, "santa.tga")
							say("[ENTER][ENTER]")
							say_title(string.format("%s:", mob_name(9293)))
							say("")
							say(gameforge[lang].christmas_zone1._3)
							addimage(25, 130, "christmas2019_3floor_items.tga")
							say_reward(gameforge[lang].christmas_zone1._4)
						end
					else
						addimage(25, 10, "christmas2019_bg04.tga")
						addimage(225, 150, "santa.tga")
						say("[ENTER][ENTER]")
						say_title(string.format("%s:", mob_name(9293)))
						say("")
						say(string.format(gameforge[lang].christmas_zone1._2, mob_name(9298), item_name(30777)))
						wait()
						addimage(25, 10, "christmas2019_bg04.tga")
						addimage(225, 150, "santa.tga")
						say("[ENTER][ENTER]")
						say_title(string.format("%s:", mob_name(9293)))
						say("")
						say(gameforge[lang].christmas_zone1._3)
						addimage(25, 130, "christmas2019_3floor_items.tga")
						say_reward(gameforge[lang].christmas_zone1._4)
					end
				else
					say_title(string.format("%s:", mob_name(9293)))
					say("")
					say(gameforge[lang].christmas_zone1._1)
				end
			else
				say_title(string.format("%s:", mob_name(9293)))
				say("Szólj Razornak. Hibakód: 002 ")
				say(gameforge[lang].common.unknownerr)
			end
		end

		when 8456.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 260000 and idx < 270000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 3 then
					local s = d.getf(idx, "step")
					if s == 3 then
						d.setf(idx, "step", 4)
						d.spawn_mob(idx, 4079, 1050, 428)
						game.drop_item(30774, 1)
						d.notice(idx, 1326, "", true)
						d.notice(idx, 1327, "", true)
					elseif s == 5 then
						local c = d.getf(idx, "tokill") - 1
						d.setf(idx, "tokill", c)
						if c == 0 then
							d.setf(idx, "step", 6)
							game.drop_item(30776, 1)
							d.notice(idx, 1330, "", true)
						end
					end
				end
			end
		end

		when christmas_zone1_loop.server_timer begin
			local arg = get_server_timer_arg()
			if d.find(arg) then
				local f = d.getf(arg, "floor")
				if f == 2 then
					if d.count_monster(arg) <= 0 then
						clear_server_timer("christmas_zone1_loop", arg)
						d.clear_regen(arg)
						d.setf(arg, "step", 3)
						d.notice(arg, 1316, "", true)
					end
				elseif f == 3 then
					local s = d.getf(arg, "step")
					if s == 2 then
						if d.count_monster(arg) <= 0 then
							clear_server_timer("christmas_zone1_loop", arg)
							d.setf(arg, "step", 3)
							d.kill_all_monsters(arg)
							d.clear_regen(arg)
							local stonepos = {
												{1020, 448},
												{1019, 411},
												{1042, 391},
												{1070, 395},
												{1089, 439},
												{1051, 466},
												{1021, 448}
							}
							local rand_num = math.random(1, table.getn(stonepos))
							d.spawn_mob(arg, 8456, stonepos[rand_num][1], stonepos[rand_num][2])
							d.notice(arg, 1325, "", true)
						end
					end
				end
			else
				christmas_zone1.clear(arg)
			end
		end

		when 9296.take with pc.in_dungeon() and item.get_vnum() == 30773 begin
			local idx = pc.get_map_index()
			if idx >= 260000 and idx < 270000 then
				local f = d.getf(idx, "floor")
				if f == 2 then
					if d.getf(idx, "step") == 4 then
						item.remove()
						local g = d.getf(idx, "gift") - 1
						if g <= 0 then
							d.clear_regen(idx)
							d.setf(idx, "step", 3)
							npc.kill()
							d.notice(idx, 1316, "", true)
						else
							if g == 1 then
								d.notice(idx, 1322, "", true)
							else
								d.notice(idx, 1321, string.format("%d", g), true)
							end
							d.setf(idx, "gift", g)
						end
					end
				end
			end
		end

		when 4080.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 260000 and idx < 270000 then
				if d.getf(idx, "was_completed") != 0 then return end
				local f = d.getf(idx, "floor")
				if f == 2 then
					if d.getf(idx, "step") == 4 then
						game.drop_item(30773, 1)
						d.notice(idx, 1318, "", true)
					end
				end
			end
		end

		when 4079.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 260000 and idx < 270000 then
				if d.getf(idx, "was_completed") != 0 then return end
				local f = d.getf(idx, "floor")
				if f == 2 then
					if d.getf(idx, "step") == 5 then
						d.setf(idx, "step", 1)
						d.setf(idx, "floor", 3)
						d.kill_all(idx)
						d.clear_regen(idx)
						d.notice(idx, 1003, "", true)
						server_timer("christmas_zone1_warp", 10, idx)
					end
				elseif f == 3 then
					if d.getf(idx, "step") == 4 then
						d.setf(idx, "step", 5)
						d.setf(idx, "tokill", 7)
						d.kill_all_monsters(idx)
						d.clear_regen(idx)
						d.regen_file(idx, "data/dungeon/christmas_zone1/regen_3floor_2.json")
						game.drop_item(30775, 1)
						d.notice(idx, 1328, "", true)
						d.notice(idx, 1329, "", true)
					end
				end
			end
		end

		when 9295.click with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 260000 and idx < 270000 then
				local f = d.getf(idx, "floor")
				if f == 2 then
					-- Csak akkor működjön, ha a ládás rész aktív
					if d.getf(idx, "step") == 3 then
						local lang = pc.get_language()
						local chest_done = d.getf(idx, "chest_done")

						-- 🔹 Első kattintás: MINDIG SIKER
						if chest_done == 0 then
							d.setf(idx, "chest_done", 1)
							npc.kill()
							clear_server_timer("christmas_zone1_loop", idx) -- ha futna valami, leáll
							d.setf(idx, "step", 5)
							d.spawn_mob(idx, 4079, 194, 540)
							d.notice(idx, 1323, "", true)
							d.notice(idx, 1324, "", true)
						else
							-- 🔹 Ha már kiválasztották a ládát, a többi csak üzenetet ad
							say_title(string.format("%s:", mob_name(9295)))
							say("")
							say(gameforge[lang].common.dungeon_3) -- pl. "Már kiválasztottad a ládát."
						end
					end
				end
			end
		end


		when christmas_zone1_warp.server_timer begin
			local arg = get_server_timer_arg()
			clear_server_timer("christmas_zone1_warp", arg)
			if d.find(arg) then
				local f = d.getf(arg, "floor")
				if f == 2 then
					local chestpos = {
								[1] = {["x"] = 202, ["y"] = 631, ["dir"] = 270},
								[2] = {["x"] = 273, ["y"] = 575, ["dir"] = 315},
								[3] = {["x"] = 232, ["y"] = 467, ["dir"] = 270},
								[4] = {["x"] = 141, ["y"] = 484, ["dir"] = 315},
								[5] = {["x"] = 120, ["y"] = 563, ["dir"] = 45}
					}

					local random_num = number(1, table.getn(chestpos))
					for index, position in ipairs(chestpos) do
						local vid = d.spawn_mob(arg, 9295, position["x"], position["y"])
						if random_num == index then
							d.setf(arg, "unique_vid", vid)
						end
					end
					d.spawn_mob(arg, 9293, 195, 560, 180)
					--d.set_regen_file(arg, "data/dungeon/christmas_zone1/regen_snowflakes.json")
					d.jump_all(arg, 197, 23066)
					d.notice(arg, 1313, "", true)
				elseif f == 3 then
					d.spawn_mob(arg, 9293, 1100, 432, 45)
					d.spawn_mob(arg, 9298, 1002, 431, 90)
					d.set_regen_file(arg, "data/dungeon/christmas_zone1/regen_snowflakes_3floor.json")
					d.jump_all(arg, 1105, 22959)
					d.notice(arg, 1313, "", true)
				end
			else
				christmas_zone1.clear(arg)
			end
		end

		when 9294.take with pc.in_dungeon() and item.get_vnum() == 30772 begin
			local idx = pc.get_map_index()
			if idx >= 260000 and idx < 270000 then
				if d.getf(idx, "step") == 3 then
					d.setf(idx, "floor", 2)
					d.setf(idx, "step", 1)
					d.setf(idx, "unique_vid", 0)
					npc.kill()
					item.remove()
					d.clear_regen(idx)
					d.notice(idx, 1003, "", true)
					server_timer("christmas_zone1_warp", 2, idx)
				end
			end
		end

		when 4071.kill or 4072.kill or 4073.kill or 4074.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 260000 and idx < 270000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 1 then
					if d.getf(idx, "step") == 2 then
						if number(1, 100) > 98 then -- 2%
							d.setf(idx, "step", 3)
							game.drop_item(30772, 1)
							d.kill_all_monsters(idx)
							d.notice(idx, 1312, "", true)
						end
					end
				end
			end
		end

		when christmas_zone1_wave.server_timer begin
			local arg = get_server_timer_arg()
			clear_server_timer("christmas_zone1_wave", arg)
			if d.find(arg) then
				local f = d.getf(arg, "floor")
				if f == 1 then
					local s = d.getf(arg, "step")
					if s == 2 then
						d.set_regen_file(arg, "data/dungeon/christmas_zone1/regen_1floor.json")
					end
				end
			else
				christmas_zone1.clear(arg)
			end
		end

		when 8455.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 260000 and idx < 270000 then
				if d.getf(idx, "was_completed") != 0 then return end
				local f = d.getf(idx, "floor")
				if f == 1 then
					if d.getf(idx, "step") == 1 then
						if d.getf(idx, "unique_vid") == npc.get_vid() then
							d.setf(idx, "step", 2)
							d.clear_regen(idx)
							d.notice(idx, 1309, "", true)
							d.notice(idx, 1310, "", true)
							server_timer("christmas_zone1_wave", 2, idx)
						else
							d.notice(idx, 1311, "", true)
						end
					end
				elseif f == 2 then
					if d.getf(idx, "step") == 4 then
						local w = d.getf(idx, "wrong")
						if w == 3 then
							local c = d.getf(idx, "killed_all") - 1
							d.setf(idx, "killed_all", c)
							if c == 0 then
								d.setf(idx, "step", 3)
								d.notice(idx, 1316, "", true)
							end
						elseif w == 4 then
							if d.getf(idx, "unique_vid2") == npc.get_vid() then
								d.setf(idx, "step", 3)
								d.kill_all_monsters(idx)
								d.notice(idx, 1320, "", true)
								d.notice(idx, 1316, "", true)
							else
								d.notice(idx, 1311, "", true)
							end
						end
					end
				end
			end
		end

		when christmas_zone1_prepare.server_timer begin
			local arg = get_server_timer_arg()
			clear_server_timer("christmas_zone1_prepare", arg)
			if d.find(arg) then
				d.set_regen_file(arg, "data/dungeon/christmas_zone1/regen_snowflakes.json")
				d.spawn_mob(arg, 9294, 569, 343, 180)

				local stonepos = {
							[1] = {["x"] = 556, ["y"] = 208},
							[2] = {["x"] = 546, ["y"] = 269},
							[3] = {["x"] = 515, ["y"] = 332},
							[4] = {["x"] = 577, ["y"] = 339},
							[5] = {["x"] = 580, ["y"] = 304},
							[6] = {["x"] = 618, ["y"] = 293},
							[7] = {["x"] = 591, ["y"] = 258},
							[8] = {["x"] = 546, ["y"] = 302}
				}

				local random_num = number(1, table.getn(stonepos))
				for index, position in ipairs(stonepos) do
					local vid = d.spawn_mob(arg, 8455, position["x"], position["y"])
					if random_num == index then
						d.setf(arg, "unique_vid", vid)
					end
				end

				server_timer("christmas_zone1_end", 2699, arg)
				d.notice(arg, 1128, "45", true)
				d.notice(arg, 1308, "", true)
			else
				christmas_zone1.clear(arg)
			end
		end

		when logout begin
			local idx = pc.get_map_index()
			if idx >= 260000 and idx < 270000 then
				pc.setf("christmas_zone1", "disconnect", get_global_time() + 300)
			end
		end

		when login begin
			local idx = pc.get_map_index()
			if idx == 26 then
				pc.warp(536900, 1331400)
			elseif idx >= 260000 and idx < 270000 then
				pc.set_warp_location(219, 5350, 14278)
				pc.setf("christmas_zone1", "idx", idx)
				pc.setf("christmas_zone1", "ch", pc.get_channel_id())
				if d.getf(idx, "floor") == 0 then
					d.setf(idx, "floor", 1)
					d.setf(idx, "step", 1)
					server_timer("christmas_zone1_prepare", 1, idx)
					d.setf(idx, "was_completed", 0)
				end
			end
		end

		when 9293.chat."A new territory?!?" with pc.get_map_index() == 219 begin
			local lang = pc.get_language()

			say_title(string.format("%s:", mob_name(9293)))
			say("")
			say(string.format(gameforge[lang].common.dungeon_1, pc.get_name()))
			say(gameforge[lang].common.dungeon_2)
			local agree = select(gameforge[lang].common.yes, gameforge[lang].common.no)
			say_title(string.format("%s:", mob_name(9293)))
			say("")
			if agree == 2 then
				say(gameforge[lang].common.dungeon_3)
				return
			end

			local goahead = 1
			local rejoinTIME = pc.getf("christmas_zone1", "disconnect") - get_global_time()
			if rejoinTIME > 0 then
				local rejoinIDX = pc.getf("christmas_zone1", "idx")
				if rejoinIDX > 0 then
					local rejoinCH = pc.getf("christmas_zone1", "ch")
					if rejoinCH != 0 and rejoinCH != pc.get_channel_id() then
						say(string.format(gameforge[lang].common.dungeon_26, rejoinCH))
						return
					end

					if rejoinCH != 0 and d.find(rejoinIDX) then
						if d.getf(rejoinIDX, "was_completed") == 0 then
							say(gameforge[lang].common.dungeon_4)
							local agree2 = select(gameforge[lang].common.yes, gameforge[lang].common.no)
							if agree2 == 2 then
								say_title(string.format("%s:", mob_name(9293)))
								say("")
								say(gameforge[lang].common.dungeon_3)
								return
							end

							local f = d.getf(rejoinIDX, "floor")
							if f == 1 then
								goahead = 0
								pc.warp(54500, 2268000, rejoinIDX)
							elseif f == 2 then
								pc.warp(19400, 2306600, rejoinIDX)
							elseif f == 3 then
								pc.warp(110500, 2295900, rejoinIDX)
							end
						end
					end
				end
			end

			if goahead == 1 then
				pc.setf("christmas_zone1", "disconnect", 0)
				pc.setf("christmas_zone1", "idx", 0)
				pc.setf("christmas_zone1", "ch", 0)

				say(gameforge[lang].common.dungeon_5)
				say_reward(string.format(gameforge[lang].common.dungeon_6, 40))
				say_reward(string.format(gameforge[lang].common.dungeon_7, 140))
				say_reward(string.format("- %s: %d", item_name(78207), 1))
				if party.is_party() then
					say_reward(gameforge[lang].common.dungeon_8)
				end
				say(gameforge[lang].common.dungeon_9)
				local n = select(gameforge[lang].common.yes, gameforge[lang].common.no)
				say_title(string.format("%s:", mob_name(9293)))
				say("")
				if n == 2 then
					say(gameforge[lang].common.dungeon_3)
					return
				end

				local flag = string.format("ww_26_%d", pc.get_channel_id())
				local ww_flag = game.get_event_flag(flag) - get_global_time()
				if ww_flag > 0 then
					say(gameforge[lang].common.dungeon_28)
					say(string.format(gameforge[lang].common.dungeon_29, ww_flag))
					return
				end

				myname = pc.get_name()
				result, cooldown, name = d.check_entrance(40, 140, 78207, 0, 1, "christmas_zone1.cooldown")
				if result == 0 then
					say("Szólj Razornak. Hibakód: 003 ")
					say(gameforge[lang].common.unknownerr)
				elseif result == 2 then
					say(gameforge[lang].common.dungeon_10)
				elseif result == 3 then
					if myname == name then
						say(gameforge[lang].common.dungeon_20)
					else
						say(string.format(gameforge[lang].common.dungeon_19, name))
					end
				elseif result == 4 then
					if myname == name then
						say(gameforge[lang].common.dungeon_12)
					else
						say(string.format(gameforge[lang].common.dungeon_11, name))
					end
				elseif result == 5 then
					if myname == name then
						say(gameforge[lang].common.dungeon_14)
					else
						say(string.format(gameforge[lang].common.dungeon_13, name))
					end
				elseif result == 6 or result == 7 then
					if myname == name then
						say(string.format(gameforge[lang].common.dungeon_18, 1, item_name(78207)))
					else
						say(string.format(gameforge[lang].common.dungeon_17, name, 1, item_name(78207)))
					end
				elseif result == 8 then
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

					if myname == name then
						say(string.format(gameforge[lang].common.dungeon_16, h, hS, m, mS))
					else
						say(string.format(gameforge[lang].common.dungeon_15, name, h, hS, m, mS))
					end
				elseif result == 1 then
					say(gameforge[lang].common.dungeon_21)
					wait()
					d.remove_item(78207, 1, 0, 0, 30771, 255, 30772, 255, 30773, 255, 30774, 255, 30775, 255, 30776, 255, 0, "christmas_zone1")
					game.set_event_flag(string.format("ww_26_%d", pc.get_channel_id()), get_global_time() + 5)
					d.join_cords(26, 545, 22680)
				end
			end
		end
	end
end
