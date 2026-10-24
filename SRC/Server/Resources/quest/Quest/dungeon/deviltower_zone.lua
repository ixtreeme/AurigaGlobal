quest deviltower_zone begin
	state start begin
		function clear(arg)
			clear_server_timer("deviltower_prepare", arg)
			clear_server_timer("deviltower_checker", arg)
			clear_server_timer("deviltower_warp", arg)
			clear_server_timer("deviltower_end", arg)
			clear_server_timer("deviltower_complete", arg)
			if d.find(arg) then
				d.setf(arg, "was_completed", 1)
				d.clear_regen(arg)
				d.kill_all(arg)
				d.exit_all_lobby(arg, 1)
			end
		end

		when deviltower_complete.server_timer begin
			deviltower_zone.clear(get_server_timer_arg())
		end

		when deviltower_end.server_timer begin
			local arg = get_server_timer_arg()
			d.notice(arg, 1040, "", true)
			d.notice(arg, 1041, "", true)
			deviltower_zone.clear(arg)
		end

		when 1093.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 660000 and idx < 670000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 13 then
					d.setf(idx, "was_completed", 1)
					d.complete(1093, 1, 66, "deviltower_zone")
					if party.is_party() then
						notice_all(1124, party.get_leader_name())
					else
						notice_all(1064, pc.get_name())
					end

					d.notice(idx, 1068, "", true)
					--d.notice(idx, 1261, "", true)
					--server_timer("deviltower_complete", 30, idx)
					d.spawn_mob(idx, 20348, 632, 166)--portos
					--d.notice(idx, 1261, "", true)--30 másodperc múlva teleportálnak ki.
					--server_timer("monkey_tower_razor93_complete", 30, idx)
					local bonus = 10 + game.get_event_flag("dungeon_bonus")
					if math.random(1, 100) <= bonus then
						d.spawn_mob(idx, 2493, 632, 166)--beran
						syschat("Dungeon bonus spawm: Ork ")
					end
				end
			end
		end

		when 20366.take with pc.in_dungeon() and item.get_vnum() == 30304 begin
			local idx = pc.get_map_index()
			if idx >= 660000 and idx < 670000 then
				if d.getf(idx, "floor") == 12 then
					npc.purge()
					d.setf(idx, "floor", 13)
					d.clear_regen(idx)
					d.kill_all(idx)
					pc.remove_item(30304, pc.count_item(30304))
					d.spawn_mob(idx, 1093, 647, 157)
					d.notice(idx, 1032, "", true)
					d.notice(idx, 1033, "", true)
					server_timer("deviltower_warp", 2, idx)
				end
			end
		end

		when 1040.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 660000 and idx < 670000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 11 then
					if number(1, 100) > 95 then -- 5%
						d.setf(idx, "floor", 12)
						game.drop_item(30304, 1)
					end
				end
			end
		end

		when 30302.use with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 660000 and idx < 670000 then
				if d.getf(idx, "floor") == 10 then
					d.setf(idx, "floor", 11)
					pc.remove_item(30302, pc.count_item(30302))
					d.spawn_mob(idx, 20366, 640, 460, 60)
					d.set_regen_file(idx, "data/dungeon/deviltower/regen8.json")
					d.notice(idx, 1029, "", true)
					d.notice(idx, 1030, "", true)
					server_timer("deviltower_warp", 2, idx)
				end
			end
		end

		when 8019.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 660000 and idx < 670000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 9 then
					d.setf(idx, "floor", 10)
					game.drop_item(30302, 1)
				end
			end
		end

		when 8018.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 660000 and idx < 670000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 8 then
					local c = d.getf(idx, "count") + 1
					if c == 4 then
						d.setf(idx, "floor", 9)
						d.notice(idx, 1028, "", true)
						d.spawn_mob(idx, 8019, 617, 676)
					else
						d.setf(idx, "count", c)
					end
				end
			end
		end

		when 20074.chat."Hey!" or 20075.chat."Hey!" or 20076.chat."Hey!" with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 660000 and idx < 670000 then
				if d.getf(idx, "floor") == 7 then
					d.setf(idx, "floor", 8)
					local lang = pc.get_language()

					say_title(string.format("%s:", mob_name(npc.get_race())))
					say("")
					say(gameforge[lang].deviltower_zone._1)
					say(gameforge[lang].deviltower_zone._2)
					say(gameforge[lang].deviltower_zone._3)
					wait()
					setskin(NOWINDOW)
					d.setf(idx, "count", 0)
					d.spawn_mob(idx, 8018, 639, 658)
					d.spawn_mob(idx, 8018, 611, 637)
					d.spawn_mob(idx, 8018, 596, 674)
					d.spawn_mob(idx, 8018, 629, 670)
					d.notice(idx, 1022, "", true)
					server_timer("deviltower_warp", 2, idx)
					npc.purge()
				end
			end
		end

		when 1092.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 660000 and idx < 670000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 6 then
					d.setf(idx, "floor", 7)
					d.notice(idx, 1026, "", true)
					party.give_blacksmith()
					local reward = {20074, 20075, 20076}
					d.spawn_mob(idx, reward[number(1, 3)], 425, 216, 60)
				end
			end
		end

		when 20073.take with pc.in_dungeon() and item.get_vnum() == 50084 begin
			local idx = pc.get_map_index()
			if idx >= 660000 and idx < 670000 then
				if d.getf(idx, "floor") == 5 then
					npc.purge()
					item.remove()

					local c = d.getf(idx, "count2") + 1
					if c == 5 then
						d.setf(idx, "floor", 6)
						d.clear_regen(idx)
						d.kill_all(idx)
						d.spawn_mob(idx, 1092, 418, 208)

						pc.remove_item(50084, pc.count_item(50084))

						d.notice(idx, 1021, "", true)
						d.notice(idx, 1022, "", true)
						server_timer("deviltower_warp", 2, idx)
					else
						d.setf(idx, "count2", c)
						if c == 4 then
							d.notice(idx, 1023, "", true)
						else
							d.notice(idx, 1024, string.format("%d", 5 - c), true)
						end
					end
				end
			end
		end

		when 1032.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 660000 and idx < 670000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 5 then
					local c = d.getf(idx, "count1") + 1
					if c == 8 then
						d.setf(idx, "count1", 0)
						game.drop_item(50084, 1)
					else
						d.setf(idx, "count1", c)
					end
				end
			end
		end

		when 8017.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 660000 and idx < 670000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 4 then
					if d.getf(idx, "unique_vid") == npc.get_vid() then
						d.setf(idx, "floor", 5)
						d.setf(idx, "count1", 0)
						d.setf(idx, "count2", 0)
						d.clear_regen(idx)
						d.kill_all(idx)
						d.spawn_mob(idx, 20073, 421, 452)
						d.spawn_mob(idx, 20073, 380, 460)
						d.spawn_mob(idx, 20073, 428, 414)
						d.spawn_mob(idx, 20073, 398, 392)
						d.spawn_mob(idx, 20073, 359, 426)
						d.set_regen_file(idx, "data/dungeon/deviltower/regen5.json")
						d.notice(idx, 1015, "", true)
						server_timer("deviltower_warp", 2, idx)
					else
						d.notice(idx, 1016, "", true)
					end
				end
			end
		end

		when 8016.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 660000 and idx < 670000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 3 then
					d.setf(idx, "floor", 4)

					local stonepos = {
								[1] = {["x"] = 385, ["y"] = 678},
								[2] = {["x"] = 417, ["y"] = 664},
								[3] = {["x"] = 404, ["y"] = 636},
								[4] = {["x"] = 372, ["y"] = 648}
					}

					local random_num = number(1, table.getn(stonepos))
					for index, position in ipairs(stonepos) do
						local vid = d.spawn_mob(idx, 8017, position["x"], position["y"])
						if random_num == index then
							d.setf(idx, "unique_vid", vid)
						end
					end

					d.notice(idx, 1013, "", true)
				end
			end
		end

		when 1091.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 660000 and idx < 670000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 2 then
					d.kill_all(idx)
					d.setf(idx, "floor", 3)
					d.spawn_mob(idx, 8016, 371, 633)
					d.notice(idx, 1097, "", true)
					server_timer("deviltower_warp", 2, idx)
				end
			end
		end

		when deviltower_warp.server_timer begin
			local arg = get_server_timer_arg()
			if d.find(arg) then
				clear_server_timer("deviltower_warp", arg)
				local f = d.getf(arg, "floor")
				if f == 2 then
					d.jump_all(arg, 3782, 26803)
				elseif f == 3 then
					d.jump_all(arg, 4017, 27285)
				elseif f == 5 then
					d.jump_all(arg, 4017, 27057)
					d.notice(arg, 1017, "", true)
				elseif f == 6 then
					d.jump_all(arg, 4022, 26823)
					d.notice(arg, 1025, "", true)
				elseif f == 8 then
					d.jump_all(arg, 4238, 27294)
				elseif f == 11 then
					d.jump_all(arg, 4238, 27059)
					d.notice(arg, 1031, "", true)
				elseif f == 13 then
					d.jump_all(arg, 4238, 26811)
					d.notice(arg, 1034, "", true)
				end
			else
				deviltower_zone.clear(arg)
			end
		end

		when deviltower_checker.server_timer begin
			local arg = get_server_timer_arg()
			if d.find(arg) then
				local f = d.getf(arg, "floor")
				if f == 1 then
					if d.count_monster(arg) == 0 then
						clear_server_timer("deviltower_checker", arg)
						d.setf(arg, "floor", 2)
						d.spawn_mob(arg, 1091, 210, 210)
						d.notice(arg, 1097, "", true)
						server_timer("deviltower_warp", 2, arg)
					end
				end
			else
				deviltower_zone.clear(arg)
			end
		end

		when deviltower_prepare.server_timer begin
			local arg = get_server_timer_arg()
			if d.find(arg) then
				d.regen_file(arg, "data/dungeon/deviltower/regen2.json")
				server_timer("deviltower_end", 2999, arg)
				server_loop_timer("deviltower_checker", 2, arg)
				d.notice(arg, 1128, string.format("%d", 50), true)
				d.notice(arg, 930, "", true)
			else
				deviltower_zone.clear(arg)
			end
		end

		when logout begin
			local idx = pc.get_map_index()
			if idx >= 660000 and idx < 670000 then
				pc.setf("deviltower_zone", "disconnect", get_global_time() + 300)
			end
		end

		when login begin
			local idx = pc.get_map_index()
			if idx == 66 then
				pc.warp(469100, 718500)
			elseif idx >= 660000 and idx < 670000 then
				pc.set_warp_location(363, 4691, 7185)
				pc.setf("deviltower_zone", "idx", idx)
				pc.setf("deviltower_zone", "ch", pc.get_channel_id())
				if d.getf(idx, "floor") == 0 then
					d.setf(idx, "floor", 1)
					server_timer("deviltower_prepare", 1, idx)
					d.setf(idx, "was_completed", 0)
				end
			end
		end

		when 20348.chat."A new territory?!?" begin
			local lang = pc.get_language()

			say_title(string.format("%s:", mob_name(20348)))
			say("")
			say(string.format(gameforge[lang].common.dungeon_1, pc.get_name()))
			-- local mapIdx = pc.get_map_index()
			-- if mapIdx != 219 then
				-- return
			-- end

			say(gameforge[lang].common.dungeon_2)
			local agree = select(gameforge[lang].common.yes, gameforge[lang].common.no)
			say_title(string.format("%s:", mob_name(20348)))
			say("")
			if agree == 2 then
				say(gameforge[lang].common.dungeon_3)
				return
			end

			local goahead = 1
			local rejoinTIME = pc.getf("deviltower_zone", "disconnect") - get_global_time()
			if rejoinTIME > 0 then
				local rejoinIDX = pc.getf("deviltower_zone", "idx")
				if rejoinIDX > 0 then
					local rejoinCH = pc.getf("deviltower_zone", "ch")
					if rejoinCH != 0 and rejoinCH != pc.get_channel_id() then
						say(string.format(gameforge[lang].common.dungeon_26, rejoinCH))
						return
					end

					if rejoinCH != 0 and d.find(rejoinIDX) then
						if d.getf(rejoinIDX, "was_completed") == 0 then
							say(gameforge[lang].common.dungeon_4)
							local agree2 = select(gameforge[lang].common.yes, gameforge[lang].common.no)
							if agree2 == 2 then
								say_title(string.format("%s:", mob_name(20348)))
								say("")
								say(gameforge[lang].common.dungeon_3)
								return
							end

							local f = d.getf(rejoinIDX, "floor")
							if f == 1 then
								goahead = 0
								pc.warp(377400, 2704000, rejoinIDX)
							elseif f == 2 then
								goahead = 0
								pc.warp(378200, 2680300, rejoinIDX)
							elseif f == 3 or f == 4 then
								goahead = 0
								pc.warp(401700, 2728500, rejoinIDX)
							elseif f == 5 then
								goahead = 0
								pc.warp(401700, 2705700, rejoinIDX)
							elseif f == 6 or f == 7 then
								goahead = 0
								pc.warp(402200, 2682300, rejoinIDX)
							elseif f == 8 or f == 9 or f == 10 then
								goahead = 0
								pc.warp(423800, 2729400, rejoinIDX)
							elseif f == 11 or f == 12 then
								goahead = 0
								pc.warp(423800, 2705900, rejoinIDX)
							elseif f == 13 then
								goahead = 0
								pc.warp(423800, 2681100, rejoinIDX)
							end
						end
					end
				end
			end

			if goahead == 1 then
				pc.setf("deviltower_zone", "disconnect", 0)
				pc.setf("deviltower_zone", "idx", 0)
				pc.setf("deviltower_zone", "ch", 0)

				say(gameforge[lang].common.dungeon_5)
				say_reward(string.format(gameforge[lang].common.dungeon_6, 40))
				say_reward(string.format(gameforge[lang].common.dungeon_7, 140))
				--say_reward(string.format("- %s: %d", item_name(0), 1))
				if party.is_party() then
					say_reward(gameforge[lang].common.dungeon_8)
				end
				say(gameforge[lang].common.dungeon_9)
				local n = select(gameforge[lang].common.yes, gameforge[lang].common.no)
				say_title(string.format("%s:", mob_name(20348)))
				say("")
				if n == 2 then
					say(gameforge[lang].common.dungeon_3)
					return
				end

				local flag = string.format("ww_66_%d", pc.get_channel_id())
				local ww_flag = game.get_event_flag(flag) - get_global_time()
				if ww_flag > 0 then
					say(gameforge[lang].common.dungeon_28)
					say(string.format(gameforge[lang].common.dungeon_29, ww_flag))
					return
				end

				myname = pc.get_name()
				result, cooldown, name = d.check_entrance(40, 140, 0, 0, 1, "deviltower_zone.cooldown")
				if result == 0 then
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
					say(gameforge[lang].common.unknownerr)
					--if myname == name then
					--	say(string.format(gameforge[lang].common.dungeon_18, 1, item_name(0)))
					--else
					--	say(string.format(gameforge[lang].common.dungeon_17, name, 1, item_name(0)))
					--end
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
					d.remove_item(0, 0, 0, 0, 50084, 255, 30302, 255, 30303, 255, 30304, 255, 0, 0, 0, 0, 0, "deviltower_zone")
					game.set_event_flag(string.format("ww_66_%d", pc.get_channel_id()), get_global_time() + 5)
					d.join_cords(66, 3774, 27040)
				end
			end
		end
	end
end
