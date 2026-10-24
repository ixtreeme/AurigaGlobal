quest meleylair_zone begin
	state start begin
		function clear(arg)
			clear_server_timer("meleylair_prepare", arg)
			clear_server_timer("meleylair_checker", arg)
			clear_server_timer("meleylair_checker_loop", arg)
			clear_server_timer("meleylair_stone_spawn1", arg)
			clear_server_timer("meleylair_stone_spawn2", arg)
			clear_server_timer("meleylair_stone_limit", arg)
			clear_server_timer("meleylair_kill_boss", arg)
			clear_server_timer("meleylair_end", arg)
			clear_server_timer("meleylair_complete", arg)
			if d.find(arg) then
				d.setf(arg, "was_completed", 1)
				d.clear_regen(arg)
				d.kill_all(arg)
				d.exit_all_lobby(arg, 1)
			end
		end

		when meleylair_stone_limit.server_timer begin
			local arg = get_server_timer_arg()
			if d.find(arg) then
				d.notice(arg, 946, "", true)
				d.notice(arg, 947, "", true)
			end

			meleylair_zone.clear(arg)
		end

		when meleylair_complete.server_timer begin
			meleylair_zone.clear(get_server_timer_arg())
		end

		when 6118.take with pc.in_dungeon() and item.get_vnum() == 30341 begin
			local idx = pc.get_map_index()
			if idx >= 2120000 and idx < 2130000 then
				if d.getf(idx, "floor") == 19 then
					local vid = npc.get_vid()
					if d.getf(idx, string.format("a_%d", vid)) != 1 then
						d.setf(idx, string.format("a_%d", vid), 1)
						item.remove()
						d.set_meley_last_statue(idx, vid)
						d.setf(idx, "killed_count", d.getf(idx, "killed_count") + 1)
					end
				end
			end
		end

		-- when 6115.kill or 6110.kill with pc.in_dungeon() begin
			-- local idx = pc.get_map_index()
			-- if idx >= 2120000 and idx < 2130000 then
				-- if number(1, 100) > 94 then -- 6%
					-- game.drop_item(30341, 1)
				-- end
			-- end
		-- end

		when 6116.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 2120000 and idx < 2130000 then
				if d.getf(idx, "was_completed") != 0 then return end
				local c = d.getf(idx, "killed_count") + 1
				if c > 5 then
					local statue_vid = d.getf(idx, "statue_vid1")
					local success = d.set_vid_invincible(statue_vid, false, true)
					if not success then
						meleylair_zone.clear(idx)
					else
						statue_vid = d.getf(idx, "statue_vid2")
						success = d.set_vid_invincible(statue_vid, false, true)
						if not success then
							meleylair_zone.clear(idx)
						else
							statue_vid = d.getf(idx, "statue_vid3")
							success = d.set_vid_invincible(statue_vid, false, true)
							if not success then
								meleylair_zone.clear(idx)
							else
								statue_vid = d.getf(idx, "statue_vid4")
								success = d.set_vid_invincible(statue_vid, false, true)
								if not success then
									meleylair_zone.clear(idx)
								else
									d.setf(idx, "killed_count", 0)
									server_timer("meleylair_stone_spawn2", 30, idx)
									d.notice(idx, 1298, "", true)
								end
							end
						end
					end
				else
					d.setf(idx, "killed_count", c)
				end
			end
		end

		when 20422.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 2120000 and idx < 2130000 then
				if d.getf(idx, "was_completed") != 0 then return end
				-- if (party.is_party()) then
					-- for _, pid in ipairs({party.get_member_pids()}) do
						-- q.begin_other_pc_block(pid);
								-- --
							-- --	pc.give_item2(50101, 1)
						-- pc.give_item2(61122, 1)--zodi töredék
						-- pc.give_item2(86060, 10)--zodi büvölö
						-- pc.give_item2(30271, 10)--metinkö szilánk 10
						-- pc.give_item2(71084, 20)--1-5 fori 20db
								-- --
						-- q.end_other_pc_block();
					-- end -- for
				-- else
					-- pc.give_item2(61122, 1)--zodi töredék
					-- pc.give_item2(86060, 5)--zodi büvölö
					-- pc.give_item2(30271, 5)--metinkö szilánk 5
					-- pc.give_item2(71084, 10)--1-5 fori 20db
				-- end
				local f = d.getf(idx, "floor")
				if f >= 13 then
					local c = d.getf(idx, "killed_count") + 1
					if c > 5 then
						local statue_vid = d.getf(idx, "statue_vid1")
						local success = d.set_vid_invincible(statue_vid, false, true)
						if not success then
							meleylair_zone.clear(idx)
						else
							statue_vid = d.getf(idx, "statue_vid2")
							success = d.set_vid_invincible(statue_vid, false, true)
							if not success then
								meleylair_zone.clear(idx)
							else
								statue_vid = d.getf(idx, "statue_vid3")
								success = d.set_vid_invincible(statue_vid, false, true)
								if not success then
									meleylair_zone.clear(idx)
								else
									statue_vid = d.getf(idx, "statue_vid4")
									success = d.set_vid_invincible(statue_vid, false, true)
									if not success then
										meleylair_zone.clear(idx)
									else
										d.setf(idx, "killed_count", 0)
										server_timer("meleylair_stone_spawn2", 30, idx)
										d.notice(idx, 1298, "", true)
									end
								end
							end
						end
					else
						d.setf(idx, "killed_count", c)
					end
				else
					local c = d.getf(idx, "killed_count") + 1
					if c > 3 then
						local statue_vid = d.getf(idx, "statue_vid1")
						local success = d.set_vid_invincible(statue_vid, false, true)
						if not success then
							meleylair_zone.clear(idx)
						else
							statue_vid = d.getf(idx, "statue_vid2")
							success = d.set_vid_invincible(statue_vid, false, true)
							if not success then
								meleylair_zone.clear(idx)
							else
								statue_vid = d.getf(idx, "statue_vid3")
								success = d.set_vid_invincible(statue_vid, false, true)
								if not success then
									meleylair_zone.clear(idx)
								else
									statue_vid = d.getf(idx, "statue_vid4")
									success = d.set_vid_invincible(statue_vid, false, true)
									if not success then
										meleylair_zone.clear(idx)
									else
										d.setf(idx, "killed_count", 0)
										server_timer("meleylair_stone_spawn1", 30, idx)
										d.notice(idx, 1298, "", true)
									end
								end
							end
						end
					else
						d.setf(idx, "killed_count", c)
					end
				end
			end
		end

		when meleylair_stone_spawn1.server_timer begin
			local arg = get_server_timer_arg()
			clear_server_timer("meleylair_stone_spawn1", arg)
			if d.find(arg) then
				local statue_vid = d.getf(arg, "statue_vid1")
				local success = d.set_vid_invincible(statue_vid, true)
				if not success then
					meleylair_zone.clear(arg)
				else
					statue_vid = d.getf(arg, "statue_vid2")
					success = d.set_vid_invincible(statue_vid, true)
					if not success then
						meleylair_zone.clear(arg)
					else
						statue_vid = d.getf(arg, "statue_vid3")
						success = d.set_vid_invincible(statue_vid, true)
						if not success then
							meleylair_zone.clear(arg)
						else
							statue_vid = d.getf(arg, "statue_vid4")
							success = d.set_vid_invincible(statue_vid, true)
							if not success then
								meleylair_zone.clear(arg)
							else
								d.setf(arg, "killed_count", 0)
								local stonepos = {
											[1] = {["x"] = 140, ["y"] = 131},
											[2] = {["x"] = 130, ["y"] = 122},
											[3] = {["x"] = 121, ["y"] = 128},
											[4] = {["x"] = 128, ["y"] = 140}
								}

								for index, position in ipairs(stonepos) do
									d.spawn_mob(arg, 20422, position["x"], position["y"])
								end

								d.notice(arg, 1297, "", true)
							end
						end
					end
				end
			end
		end

		when meleylair_stone_spawn2.server_timer begin
			local arg = get_server_timer_arg()
			clear_server_timer("meleylair_stone_spawn2", arg)
			if d.find(arg) then
				local statue_vid = d.getf(arg, "statue_vid1")
				local success = d.set_vid_invincible(statue_vid, true)
				if not success then
					meleylair_zone.clear(arg)
				else
					statue_vid = d.getf(arg, "statue_vid2")
					success = d.set_vid_invincible(statue_vid, true)
					if not success then
						meleylair_zone.clear(arg)
					else
						statue_vid = d.getf(arg, "statue_vid3")
						success = d.set_vid_invincible(statue_vid, true)
						if not success then
							meleylair_zone.clear(arg)
						else
							statue_vid = d.getf(arg, "statue_vid4")
							success = d.set_vid_invincible(statue_vid, true)
							if not success then
								meleylair_zone.clear(arg)
							else
								d.setf(arg, "killed_count", 0)
								local stonepos = {
											[1] = {["x"] = 140, ["y"] = 131},
											[2] = {["x"] = 130, ["y"] = 122},
											[3] = {["x"] = 121, ["y"] = 128},
											[4] = {["x"] = 128, ["y"] = 140},
											[5] = {["x"] = 140, ["y"] = 131},
											[6] = {["x"] = 141, ["y"] = 126},
								}

								for index, position in ipairs(stonepos) do
									if index > 4 then
										d.spawn_mob(arg, 6116, position["x"], position["y"])
									else
										d.spawn_mob(arg, 20422, position["x"], position["y"])
									end
								end

								d.notice(arg, 1297, "", true)
								d.notice(arg, 1299, "", true)
							end
						end
					end
				end
			end
		end

		when meleylair_checker.server_timer begin
			local arg = get_server_timer_arg()
			clear_server_timer("meleylair_checker", arg)
			if d.find(arg) then
				local f = d.getf(arg, "floor")
				if f == 6 then
					local statue_vid = d.getf(arg, "statue_vid1")
					local success = d.set_vid_invincible(statue_vid, false)
					if not success then
						meleylair_zone.clear(arg)
					else
						statue_vid = d.getf(arg, "statue_vid2")
						success = d.set_vid_invincible(statue_vid, false)
						if not success then
							meleylair_zone.clear(arg)
						else
							statue_vid = d.getf(arg, "statue_vid3")
							success = d.set_vid_invincible(statue_vid, false)
							if not success then
								meleylair_zone.clear(arg)
							else
								statue_vid = d.getf(arg, "statue_vid4")
								success = d.set_vid_invincible(statue_vid, false)
								if not success then
									meleylair_zone.clear(arg)
								else
									d.setf(arg, "floor", 7)
									d.set_regen_file(arg, "data/dungeon/meley/regen2.json")
									server_timer("meleylair_stone_spawn1", 30, arg)
									d.notice(arg, 1296, "50", true)
								end
							end
						end
					end
				elseif f == 12 then
					local statue_vid = d.getf(arg, "statue_vid1")
					local success = d.set_vid_invincible(statue_vid, false)
					if not success then
						meleylair_zone.clear(arg)
					else
						statue_vid = d.getf(arg, "statue_vid2")
						success = d.set_vid_invincible(statue_vid, false)
						if not success then
							meleylair_zone.clear(arg)
						else
							statue_vid = d.getf(arg, "statue_vid3")
							success = d.set_vid_invincible(statue_vid, false)
							if not success then
								meleylair_zone.clear(arg)
							else
								statue_vid = d.getf(arg, "statue_vid4")
								success = d.set_vid_invincible(statue_vid, false)
								if not success then
									meleylair_zone.clear(arg)
								else
									clear_server_timer("meleylair_stone_spawn1", arg)
									d.setf(arg, "floor", 13)
									d.set_regen_file(arg, "data/dungeon/meley/regen3.json")
									server_timer("meleylair_stone_spawn2", 30, arg)
									d.notice(arg, 1296, "5", true)
								end
							end
						end
					end
				elseif f == 18 then
					d.setf(arg, "floor", 19)
					d.notice(arg, 1300, "", true)
					d.setf(arg, "killed_count", 0)
					server_timer("meleylair_stone_limit", 60, arg)
				end
			else
				meleylair_zone.clear(arg)
			end
		end

		when 20420.chat."Reward" with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			local lang = pc.get_language()
			say_title(string.format("%s:", mob_name(20420)))
			say("")
			if idx >= 2120000 and idx < 2130000 then
				if d.getf(idx, "was_completed") != 1 or d.getf(idx, "floor") != 20 then return end
				if d.getf(idx, "notice_world") != 1 then
					d.setf(idx, "notice_world", 1)
					d.complete(6118, 2400, 212, "meleylair_zone")
					if party.is_party() then
						notice_all(1301, party.get_leader_name())
					else
						notice_all(1302, pc.get_name())
					end

					local bonus = 0 + game.get_event_flag("dungeon_bonus")
					if math.random(1, 100) <= bonus then
						d.spawn_mob(idx, 5003, 135, 135)
					end
				end

				if pc.can_drop() then
					local reward = d.getf(idx, string.format(pc.get_player_id()))
					if reward == 1 then
						say(gameforge[lang].meleylair_zone._2)
					else
						say(gameforge[lang].meleylair_zone._3)
						say(gameforge[lang].meleylair_zone._4)
						say("")
						local reward_menu = select(item_name(50270), gameforge[lang].common.close)
						if reward_menu == 1 then
							say_title(string.format("%s:", mob_name(20420)))
							say("")
							say(string.format(gameforge[lang].meleylair_zone._5, item_name(50270)))
							say("")
							local agree = select(gameforge[lang].common.yes, gameforge[lang].common.no)
							if agree == 1 then
								d.setf(idx, string.format(pc.get_player_id()), 1)
								pc.give_item2(50270, 1)
								say_title(string.format("%s:", mob_name(20420)))
								say("")
								say(gameforge[lang].meleylair_zone._6)
							end
						end
					end
				else
					say(gameforge[lang].meleylair_zone._7)
				end
			else
				say(gameforge[lang].meleylair_zone._1)
			end
		end

		when meleylair_kill_boss.server_timer begin
			local arg = get_server_timer_arg()
			clear_server_timer("meleylair_kill_boss", arg)
			if d.find(arg) then
				d.kill_meley(arg)
				d.setf(arg, "was_completed", 1)
				d.notice(arg, 1207, "", true)
				d.notice(arg, 1208, "", true)
				--d.notice(arg, 1252, "", true)
				d.spawn_mob(arg, 20420, 130, 130, 180)
				--server_timer("meleylair_complete", 180, arg)
				d.spawn_mob(arg, 3910, 130, 130, 180)
			end
		end

		when meleylair_checker_loop.server_timer begin
			local arg = get_server_timer_arg()
			if d.find(arg) then
				local f = d.getf(arg, "floor")
				if f == 5 then
					d.attack_meley(arg)
					d.setf(arg, "floor", 6)
					server_timer("meleylair_checker", 3, arg)
				elseif f == 11 then
					d.attack_meley(arg)
					d.setf(arg, "floor", 12)
					server_timer("meleylair_checker", 3, arg)
				elseif f == 17 then
					clear_server_timer("meleylair_stone_spawn2", arg)
					d.attack_meley(arg)
					d.setf(arg, "floor", 18)
					d.kill_all_monsters(arg)
					d.clear_regen(arg)
					server_timer("meleylair_checker", 3, arg)
				elseif f == 19 then
					if d.getf(arg, "killed_count") == 4 then
						clear_server_timer("meleylair_stone_limit", arg)
						d.setf(arg, "floor", 20)
						server_timer("meleylair_kill_boss", 1, arg)
					end
				end
			else
				clear_server_timer("meleylair_checker_loop", arg)
				meleylair_zone.clear(arg)
			end
		end

		when meleylair_end.server_timer begin
			local arg = get_server_timer_arg()
			d.notice(arg, 1040, "", true)
			d.notice(arg, 1041, "", true)
			meleylair_zone.clear(arg)
		end

		when meleylair_prepare.server_timer begin
			local arg = get_server_timer_arg()
			if d.find(arg) then
				server_timer("meleylair_end", 3599, arg)
				local bossvid = d.spawn_mob(arg, 6193, 130, 77, 360)
				d.setf(arg, "boss", bossvid)
				local success = d.set_vid_invincible(bossvid, true)
				if not success then
					meleylair_zone.clear(arg)
				else
					local stonepos = {
								[1] = {["x"] = 123, ["y"] = 137, ["dir"] = 225},
								[2] = {["x"] = 123, ["y"] = 124, ["dir"] = 225},
								[3] = {["x"] = 136, ["y"] = 123, ["dir"] = 225},
								[4] = {["x"] = 137, ["y"] = 137, ["dir"] = 225}
					}

					local random_num = number(1, table.getn(stonepos))
					for index, position in ipairs(stonepos) do
						local vid = d.spawn_mob(arg, 6118, position["x"], position["y"], position["dir"])
						d.setf(arg, string.format("statue_vid%d", index), vid)
					end

					d.set_regen_file(arg, "data/dungeon/meley/regen1.json")
					server_loop_timer("meleylair_checker_loop", 2, arg)
					d.notice(arg, 599, "", true)
					d.notice(arg, 600, string.format("%d", 60), true)
					d.notice(arg, 1296, "75", true)
				end
			else
				meleylair_zone.clear(arg)
			end
		end

		when logout begin
			local idx = pc.get_map_index()
			if idx >= 2120000 and idx < 2130000 then
				pc.setf("meleylair_zone", "disconnect", get_global_time() + 300)
			end
		end

		when login begin
			local idx = pc.get_map_index()
			if idx == 212 then
				pc.warp(535400, 1428400)
			elseif idx >= 2120000 and idx < 2130000 then
				pc.set_warp_location(219, 5354, 14284)
				pc.setf("meleylair_zone", "idx", idx)
				pc.setf("meleylair_zone", "ch", pc.get_channel_id())
				if d.getf(idx, "floor") == 0 then
					if not party.is_party() then
						d.setf(idx, "floor", 1)
						server_timer("meleylair_prepare", 1, idx)
						d.setf(idx, "was_completed", 0)
						d.setf(idx, "guild_id", guild.get_id())
						game.drop_item(30341, 1)
						game.drop_item(30341, 1)
						game.drop_item(30341, 1)
						game.drop_item(30341, 1)
					else
						if party.is_party() then
							d.setf(idx, "floor", 1)
							server_timer("meleylair_prepare", 1, idx)
							d.setf(idx, "was_completed", 0)
							game.drop_item(30341, 1)
							game.drop_item(30341, 1)
							game.drop_item(30341, 1)
							game.drop_item(30341, 1)
						end
					end
				end
			end
		end

		when 20419.chat."A new territory?!?" begin
			local lang = pc.get_language()

			say_title(string.format("%s:", mob_name(20419)))
			say("")
			say(string.format(gameforge[lang].common.dungeon_1, pc.get_name()))
			local mapIdx = pc.get_map_index()
			if mapIdx != 219 then
				return
			end

			say(gameforge[lang].common.dungeon_2)
			local agree = select(gameforge[lang].common.yes, gameforge[lang].common.no)
			say_title(string.format("%s:", mob_name(20419)))
			say("")
			if agree == 2 then
				say(gameforge[lang].common.dungeon_3)
				return
			end

			local goahead = 1
			local rejoinTIME = pc.getf("meleylair_zone", "disconnect") - get_global_time()
			if rejoinTIME > 0 then
				local rejoinIDX = pc.getf("meleylair_zone", "idx")
				if rejoinIDX > 0 then
					local rejoinCH = pc.getf("meleylair_zone", "ch")
					if rejoinCH != 0 and rejoinCH != pc.get_channel_id() then
						say(string.format(gameforge[lang].common.dungeon_26, rejoinCH))
						return
					end

					if rejoinCH != 0 and d.find(rejoinIDX) then
						if d.getf(rejoinIDX, "was_completed") == 0 then
							say(gameforge[lang].common.dungeon_4)
							local agree2 = select(gameforge[lang].common.yes, gameforge[lang].common.no)
							if agree2 == 2 then
								say_title(string.format("%s:", mob_name(20419)))
								say("")
								say(gameforge[lang].common.dungeon_3)
								return
							end

							local f = d.getf(rejoinIDX, "floor")
							if f != 0 then
								goahead = 0
								pc.warp(320000, 1529000, rejoinIDX)
							end
						end
					end
				end
			end

			if goahead == 1 then
				pc.setf("nemere_zone", "disconnect", 0)
				pc.setf("nemere_zone", "idx", 0)
				pc.setf("nemere_zone", "ch", 0)

				say(gameforge[lang].common.dungeon_5)
				say_reward(string.format(gameforge[lang].common.dungeon_6, 120))
				say_reward(string.format(gameforge[lang].common.dungeon_7, 140))
				say_reward(string.format("- %s: %d", item_name(71130), 1))
				if party.is_party() then
					say_reward(gameforge[lang].common.dungeon_8)
				end
				say(gameforge[lang].common.dungeon_9)
				local n = select(gameforge[lang].common.yes, gameforge[lang].common.no)
				say_title(string.format("%s:", mob_name(20395)))
				say("")
				if n == 2 then
					say(gameforge[lang].common.dungeon_3)
					return
				end

				myname = pc.get_name()
				result, cooldown, name = d.check_entrance(120, 140, 71130, 0, 1, "meleylair_zone.cooldown")
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
					if myname == name then
						say(string.format(gameforge[lang].common.dungeon_18, 1, item_name(71130)))
					else
						say(string.format(gameforge[lang].common.dungeon_17, name, 1, item_name(71130)))
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
						d.remove_item(71130, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "")

						d.remove_item_meley(30341, 2400, "meleylair_zone")
						d.join_cords(212, 3200, 15290)


				end
			end
		end
	end
end
