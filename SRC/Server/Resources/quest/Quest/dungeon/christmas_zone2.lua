quest christmas_zone2 begin
	state start begin
		function clear(arg)
			clear_server_timer("christmas_zone2_prepare", arg)
			clear_server_timer("christmas_zone2_end", arg)
			clear_server_timer("christmas_zone2_complete", arg)
			clear_server_timer("christmas_zone2_first_stone", arg)
			clear_server_timer("christmas_zone2_check", arg)
			clear_server_timer("christmas_zone2_warp", arg)
			if d.find(arg) then
				d.setf(arg, "was_completed", 1)
				d.clear_regen(arg)
				d.kill_all(arg)
				d.exit_all_lobby(arg, 1)
			end
		end

		when christmas_zone2_complete.server_timer begin
			christmas_zone2.clear(get_server_timer_arg())
		end

		when christmas_zone2_end.server_timer begin
			local arg = get_server_timer_arg()
			d.notice(arg, 1040, "", true)
			d.notice(arg, 1041, "", true)
			christmas_zone2.clear(arg)
		end

		when 4479.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 250000 and idx < 260000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 2 and d.getf(idx, "step") == 8 then
					d.setf(idx, "step", 9)
					d.setf(idx, "was_completed", 1)
					d.complete(4479, 0, 25, "christmas_zone2")
					if party.is_party() then
						notice_all(1357, party.get_leader_name())
					else
						notice_all(1358, pc.get_name())
					end

					d.notice(idx, 1136, "", true)
					d.notice(idx, 1137, "", true)
					d.kill_all(idx)
					d.clear_regen(idx)
					-- local bonus = 10 + game.get_event_flag("dungeon_bonus")
					-- if math.random(1, 100) <= bonus then
						-- d.spawn_mob(idx, 5002, 766, 950)
					-- end
					server_timer("christmas_zone2_complete", 30, idx)
				end
			end
		end

		when 9430.take with pc.in_dungeon() and item.get_vnum() == 30892 begin
			local idx = pc.get_map_index()
			if idx >= 250000 and idx < 260000 then
				if d.getf(idx, "floor") == 2 and d.getf(idx, "can") == 1 then
					local s = d.getf(idx, "step")
					if s >= 4 and s < 8 then
						d.setf(idx, "step", s + 1)
						d.setf(idx, "can", 0)

						item.remove()

						local vid = npc.get_vid()
						if vid == d.getf(idx, "treevid_1") then
							npc.purge()
							d.spawn_mob(idx, 9431, 844, 907, 90)
						elseif vid == d.getf(idx, "treevid_2") then
							npc.purge()
							d.spawn_mob(idx, 9431, 770, 907, 90)
						elseif vid == d.getf(idx, "treevid_3") then
							npc.purge()
							d.spawn_mob(idx, 9431, 770, 990, 270)
						elseif vid == d.getf(idx, "treevid_4") then
							npc.purge()
							d.spawn_mob(idx, 9431, 844, 990, 270)
						end

						if s == 4 or s == 5 or s == 6 then
							d.setf(idx, "dropped", 0)
							d.regen_file(idx, "data/dungeon/christmas_zone2/regen_2f_mobs.json")
							d.notice(idx, 1339, "", true)
							server_loop_timer("christmas_zone2_check", 2, idx)
						elseif s == 7 then
							d.spawn_mob(idx, 4479, 766, 950)
							d.notice(idx, 1356, "", true)
						end
					end
				end
			end
		end

		when 4478.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 250000 and idx < 260000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 2 and d.getf(idx, "step") == 3 then
					d.setf(idx, "step", 4)
					d.setf(idx, "can", 1)
					game.drop_item(30892, 1)
					d.notice(idx, 1355, "", true)
				end
			end
		end

		when 30891.use with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 250000 and idx < 260000 then
				if d.getf(idx, "floor") == 2 and d.getf(idx, "step") == 2 then
					pc.remove_item(30891, 1)
					d.setf(idx, "step", 3)

					local treepos = {
								[1] = {["x"] = 844, ["y"] = 907, ["dir"] = 90},
								[2] = {["x"] = 770, ["y"] = 907, ["dir"] = 90},
								[3] = {["x"] = 770, ["y"] = 990, ["dir"] = 270},
								[4] = {["x"] = 844, ["y"] = 990, ["dir"] = 270}
					}

					for index, position in ipairs(treepos) do
						local vid = d.spawn_mob(idx, 9430, position["x"], position["y"], position["dir"])
						d.setf(idx, string.format("treevid_%d", index), vid)
					end

					d.spawn_mob(idx, 4478, 813, 953)
					d.notice(idx, 1353, "", true)
					d.notice(idx, 1354, "", true)
				end
			end
		end

		when 9425.take with pc.in_dungeon() and item.get_vnum() == 30890 begin
			local idx = pc.get_map_index()
			if idx >= 250000 and idx < 260000 then
				local f = d.getf(idx, "floor")
				if f == 1 then
					local s = d.getf(idx, "step")
					if s == 5 then
						d.setf(idx, "step", 6)
						item.remove()
						game.drop_item(30890, 1)
						d.spawn_mob(idx, 9426, 664, 529, 270)
						npc.kill()
						d.clear_regen(idx)
						d.kill_all_monsters(idx)
						d.regen_file(idx, "data/dungeon/christmas_zone2/regen_1f_mobs.json")
						d.notice(idx, 1344, "", true)
						d.notice(idx, 1339, "", true)
						server_loop_timer("christmas_zone2_check", 2, idx)
					end
				end
			end
		end

		when 4477.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 250000 and idx < 260000 then
				if d.getf(idx, "was_completed") != 0 then return end
				local f = d.getf(idx, "floor")
				if f == 1 then
					local s = d.getf(idx, "step")
					if s == 4 then
						d.setf(idx, "step", 5)
						d.notice(idx, 1342, "", true)
						d.notice(idx, 1343, "", true)
					end
				end
			end
		end

		when 9424.take with pc.in_dungeon() and item.get_vnum() == 30889 begin
			local idx = pc.get_map_index()
			if idx >= 250000 and idx < 260000 then
				local f = d.getf(idx, "floor")
				if f == 1 then
					local vid = npc.get_vid()
					--if vid == d.getf(idx, "unique_vid") then
					local s = d.getf(idx, "step")
					if s == 3 then
						d.setf(idx, "step", 4)
						item.remove()
						--local bossvid = 0
						--if d.getf("door_1") == vid then
						--	bossvid = d.spawn_mob(idx, 4477, 846, 505)
						--else
						local bossvid = d.spawn_mob(idx, 4477, 503, 515)
						--end

						if d.getf(idx, "stronger") == 1 then
							npc.set_vid_attack_mul(bossvid, 2.2)
							npc.set_vid_damage_mul(bossvid, 2.2)
						else
							npc.set_vid_attack_mul(bossvid, 1.4)
							npc.set_vid_damage_mul(bossvid, 1.4)
						end

						npc.kill()
						d.notice(idx, 1341, "", true)
					end
					--end
				end
			end
		end

		when 9426.take with pc.in_dungeon() and item.get_vnum() == 30890 begin
			local idx = pc.get_map_index()
			if idx >= 250000 and idx < 260000 then
				local f = d.getf(idx, "floor")
				if f == 1 then
					local s = d.getf(idx, "step")
					if s == 7 then
						d.setf(idx, "step", 8)
						item.remove()
						d.spawn_mob(idx, 9427, 664, 529, 270)
						npc.kill()
						d.clear_regen(idx)
						d.kill_all_monsters(idx)
						d.setf(idx, "killed", 6)
						d.regen_file(idx, "data/dungeon/christmas_zone2/regen_1f_stones2.json")
						d.notice(idx, 1347, "", true)
					end
				end
			end
		end

		when christmas_zone2_warp.server_timer begin
			local arg = get_server_timer_arg()
			clear_server_timer("christmas_zone2_warp", arg)
			if d.find(arg) then
				local f = d.getf(arg, "floor")
				if f == 2 then
					d.jump_all(arg, 23515, 31415)
					d.notice(arg, 1349, "", true)
				end
			else
				christmas_zone2.clear(arg)
			end
		end

		when 8715.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 250000 and idx < 260000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "unique_vid") == npc.get_vid() then
					if d.getf(idx, "floor") == 2 and d.getf(idx, "step") == 1 then
						d.setf(idx, "step", 2)
						d.kill_all_monsters(idx)
						game.drop_item(30891, 1)
						d.notice(idx, 1351, "", true)
						d.notice(idx, 1352, "", true)
					end
				else
					d.notice(idx, 1350, "", true)
				end
			end
		end

		when 9427.take with pc.in_dungeon() and item.get_vnum() == 30890 begin
			local idx = pc.get_map_index()
			if idx >= 250000 and idx < 260000 then
				local f = d.getf(idx, "floor")
				if f == 1 then
					local s = d.getf(idx, "step")
					if s == 9 then
						d.setf(idx, "floor", 2)
						d.setf(idx, "step", 1)
						item.remove()
						npc.kill()
						d.clear_regen(idx)
						d.kill_all_monsters(idx)

						local stonepos = {
									[1] = {["x"] = 842, ["y"] = 921},
									[2] = {["x"] = 799, ["y"] = 921},
									[3] = {["x"] = 763, ["y"] = 921},
									[4] = {["x"] = 763, ["y"] = 980},
									[5] = {["x"] = 799, ["y"] = 980},
									[6] = {["x"] = 842, ["y"] = 980}
						}

						local random_num = number(1, table.getn(stonepos))
						for index, position in ipairs(stonepos) do
							local vid = d.spawn_mob(idx, 8715, position["x"], position["y"])
							if random_num == index then
								d.setf(idx, "unique_vid", vid)
							end
						end

						d.regen_file(idx, "data/dungeon/christmas_zone2/regen_2f_gifts.json")

						d.notice(idx, 1003, "", true)
						server_timer("christmas_zone2_warp", 2, idx)
					end
				end
			end
		end

		when 4071.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 250000 and idx < 260000 then
				if d.getf(idx, "was_completed") != 0 then return end
				local f = d.getf(idx, "floor")
				if f == 2 and d.getf(idx, "dropped") == 0 then
					d.setf(idx, "dropped", 1)
					game.drop_item(30892, 1)
				end
			end
		end

		when christmas_zone2_check.server_timer begin
			local arg = get_server_timer_arg()
			if d.find(arg) then
				local f = d.getf(arg, "floor")
				if f == 1 then
					local s = d.getf(arg, "step")
					if s == 2 then
						if d.count_monster(arg) <= 0 then
							clear_server_timer("christmas_zone2_check", arg)
							d.setf(arg, "step", 3)
							d.notice(arg, 1340, "", true)
						end
					elseif s == 6 then
						if d.count_monster(arg) <= 0 then
							clear_server_timer("christmas_zone2_check", arg)
							d.setf(arg, "step", 7)
							d.notice(arg, 1345, "", true)
							d.notice(arg, 1346, "", true)
						end
					end
				elseif f == 2 then
					local s = d.getf(arg, "step")
					if s == 4 or s == 5 or s == 6 or s == 7 then
						if d.count_monster(arg) <= 0 then
							clear_server_timer("christmas_zone2_check", arg)
							d.clear_regen(arg)
							d.kill_all_monsters(arg)
							d.setf(arg, "can", 1)
							d.notice(arg, 1355, "", true)
						end
					end
				end
			else
				christmas_zone2.clear(arg)
			end
		end

		when 8714.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 250000 and idx < 260000 then
				if d.getf(idx, "was_completed") != 0 then return end
				local f = d.getf(idx, "floor")
				if f == 1 then
					local s = d.getf(idx, "step")
					if s == 1 then
						local c = d.getf(idx, "killed") - 1
						d.setf(idx, "killed", c)
						if c == 0 then
							clear_server_timer("christmas_zone2_first_stone", idx)
							d.setf(idx, "step", 2)
							d.clear_regen(idx)
							d.kill_all_monsters(idx)
							d.regen_file(idx, "data/dungeon/christmas_zone2/regen_1f_mobs.json")
							game.drop_item(30890, 1)
							game.drop_item(30889, 1)
							d.notice(idx, 1337, "", true)
							d.notice(idx, 1338, "", true)
							d.notice(idx, 1339, "", true)
							server_loop_timer("christmas_zone2_check", 2, idx)
						end
					elseif s == 8 then
						local c = d.getf(idx, "killed") - 1
						d.setf(idx, "killed", c)
						if c == 0 then
							d.setf(idx, "step", 9)
							d.clear_regen(idx)
							game.drop_item(30890, 1)
							d.notice(idx, 1348, "", true)
						end
					end
				end
			end
		end

		when christmas_zone2_first_stone.server_timer begin
			local arg = get_server_timer_arg()
			clear_server_timer("christmas_zone2_first_stone", arg)
			if d.find(arg) then
				d.setf(arg, "stronger", 1)
			else
				christmas_zone2.clear(arg)
			end
		end

		when christmas_zone2_prepare.server_timer begin
			local arg = get_server_timer_arg()
			clear_server_timer("christmas_zone2_prepare", arg)
			if d.find(arg) then
				d.setf(arg, "killed", 3)
				d.setf(arg, "stronger", 0)
				d.regen_file(arg, "data/dungeon/christmas_zone2/regen_1f_stones.json")
				d.spawn_mob(arg, 9425, 664, 529, 270)

				--local stonepos = {
				--			[1] = {["x"] = 563, ["y"] = 488, ["dir"] = 270},
				--			[2] = {["x"] = 781, ["y"] = 474, ["dir"] = 90}
				--}
				--
				--local random_num = number(1, table.getn(stonepos))
				--for index, position in ipairs(stonepos) do
				--	local vid = d.spawn_mob(arg, 9424, position["x"], position["y"], position["dir"])
				--	d.setf(arg, string.format("door_%d", index), vid)
				--	if random_num == index then
				--		d.setf(arg, "unique_vid", vid)
				--	end
				--end

				d.spawn_mob(arg, 9424, 563, 488, 270)

				server_timer("christmas_zone2_end", 2699, arg)
				d.notice(arg, 1128, "45", true)
				d.notice(arg, 1335, "", true)
				d.notice(arg, 1336, "", true)
				server_timer("christmas_zone2_first_stone", 299, arg)
			else
				christmas_zone2.clear(arg)
			end
		end

		when logout begin
			local idx = pc.get_map_index()
			if idx >= 250000 and idx < 260000 then
				pc.setf("christmas_zone2", "disconnect", get_global_time() + 300)
			end
		end

		when login begin
			local idx = pc.get_map_index()
			if idx == 25 then
				pc.warp(536900, 1331400)
			elseif idx >= 250000 and idx < 260000 then
				pc.set_warp_location(219, 5350, 14278)
				pc.setf("christmas_zone2", "idx", idx)
				pc.setf("christmas_zone2", "ch", pc.get_channel_id())
				if d.getf(idx, "floor") == 0 then
					d.setf(idx, "floor", 1)
					d.setf(idx, "step", 1)
					server_timer("christmas_zone2_prepare", 1, idx)
					d.setf(idx, "was_completed", 0)
				end
			end
		end

		when 9423.chat."A new territory?!?" with pc.get_map_index() == 219 begin
			local lang = pc.get_language()

			say_title(string.format("%s:", mob_name(9423)))
			say("")
			say(string.format(gameforge[lang].common.dungeon_1, pc.get_name()))
			say(gameforge[lang].common.dungeon_2)
			local agree = select(gameforge[lang].common.yes, gameforge[lang].common.no)
			say_title(string.format("%s:", mob_name(9423)))
			say("")
			if agree == 2 then
				say(gameforge[lang].common.dungeon_3)
				return
			end

			local goahead = 1
			local rejoinTIME = pc.getf("christmas_zone2", "disconnect") - get_global_time()
			if rejoinTIME > 0 then
				local rejoinIDX = pc.getf("christmas_zone2", "idx")
				if rejoinIDX > 0 then
					local rejoinCH = pc.getf("christmas_zone2", "ch")
					if rejoinCH != 0 and rejoinCH != pc.get_channel_id() then
						say(string.format(gameforge[lang].common.dungeon_26, rejoinCH))
						return
					end

					if rejoinCH != 0 and d.find(rejoinIDX) then
						if d.getf(rejoinIDX, "was_completed") == 0 then
							say(gameforge[lang].common.dungeon_4)
							local agree2 = select(gameforge[lang].common.yes, gameforge[lang].common.no)
							if agree2 == 2 then
								say_title(string.format("%s:", mob_name(9423)))
								say("")
								say(gameforge[lang].common.dungeon_3)
								return
							end

							local f = d.getf(rejoinIDX, "floor")
							if f == 1 then
								goahead = 0
								pc.warp(2320600, 3077800, rejoinIDX)
							elseif f == 2 then
								goahead = 0
								pc.warp(2351500, 3141500, rejoinIDX)
							end
						end
					end
				end
			end

			if goahead == 1 then
				pc.setf("christmas_zone2", "disconnect", 0)
				pc.setf("christmas_zone2", "idx", 0)
				pc.setf("christmas_zone2", "ch", 0)

				say(gameforge[lang].common.dungeon_5)
				say_reward(string.format(gameforge[lang].common.dungeon_6, 30))
				say_reward(string.format(gameforge[lang].common.dungeon_7, 140))
				say_reward(string.format("- %s: %d", item_name(78208), 1))
				if party.is_party() then
					say_reward(gameforge[lang].common.dungeon_8)
				end
				say(gameforge[lang].common.dungeon_9)
				local n = select(gameforge[lang].common.yes, gameforge[lang].common.no)
				say_title(string.format("%s:", mob_name(9423)))
				say("")
				if n == 2 then
					say(gameforge[lang].common.dungeon_3)
					return
				end

				local flag = string.format("ww_25_%d", pc.get_channel_id())
				local ww_flag = game.get_event_flag(flag) - get_global_time()
				if ww_flag > 0 then
					say(gameforge[lang].common.dungeon_28)
					say(string.format(gameforge[lang].common.dungeon_29, ww_flag))
					return
				end

				myname = pc.get_name()
				result, cooldown, name = d.check_entrance(30, 140, 78208, 0, 1, "christmas_zone2.cooldown")
				if result == 0 then
					say("Szólj Razornak. Hibakód: 004 ")
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
						say(string.format(gameforge[lang].common.dungeon_18, 1, item_name(78208)))
					else
						say(string.format(gameforge[lang].common.dungeon_17, name, 1, item_name(78208)))
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
					d.remove_item(78208, 1, 0, 0, 30889, 255, 30890, 255, 30891, 255, 30892, 255, 0, 0, 0, 0, 0, "christmas_zone2")
					game.set_event_flag(string.format("ww_25_%d", pc.get_channel_id()), get_global_time() + 5)
					d.join_cords(25, 23206, 30778)
				end
			end
		end
	end
end
