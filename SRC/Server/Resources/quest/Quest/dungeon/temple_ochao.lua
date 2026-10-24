quest temple_ochao begin
	state start begin
		function clear(arg)
			clear_server_timer("temple_ochao_prepare", arg)
			clear_server_timer("temple_ochao_check", arg)
			clear_server_timer("temple_ochao_end", arg)
			clear_server_timer("temple_ochao_complete", arg)
			if d.find(arg) then
				d.setf(arg, "was_completed", 1)
				d.clear_regen(arg)
				d.kill_all(arg)
				d.exit_all_lobby(arg, 1)
			end
		end

		when temple_ochao_complete.server_timer begin
			temple_ochao.clear(get_server_timer_arg())
		end

		when 6393.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 2090000 and idx < 2100000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 4 then
					d.setf(idx, "was_completed", 1)
					d.complete(6393, 1800, 209, "temple_ochao")
					if party.is_party() then
						notice_all(934, party.get_leader_name())
					else
						notice_all(935, pc.get_name())
					end

					d.notice(idx, 1217, "", true)
					--d.notice(idx, 1218, "", true)
					d.kill_all(idx)
					d.clear_regen(idx)
					d.spawn_mob(idx, 20408, 399, 407)
					local bonus = 10 + game.get_event_flag("dungeon_bonus")
					if math.random(1, 100) <= bonus then
						d.spawn_mob(idx, 6192, 399, 407)
					end
					--server_timer("temple_ochao_complete", 30, idx)
				end
			end
		end

		when temple_ochao_end.server_timer begin
			local arg = get_server_timer_arg()
			d.notice(arg, 1040, "", true)
			d.notice(arg, 1041, "", true)
			temple_ochao.clear(arg)
		end

		when 8035.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 2090000 and idx < 2100000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 3 then
					local c = d.getf(idx, "count2") + 1
					if c == 4 then
						d.setf(idx, "floor", 4)
						d.spawn_mob(idx, 6393, 399, 407)
						d.notice(idx, 1214, "", true)
						d.notice(idx, 1215, "", true)
					else
						if c == 3 then
							d.notice(idx, 1212, "1", true)
						else
							d.notice(idx, 1213, string.format("%d", 4 - c), true)
						end
						d.setf(idx, "count2", c)
					end
				end
			end
		end

		when 6391.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 2090000 and idx < 2100000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 2 then
					d.setf(idx, "floor", 3)
					d.setf(idx, "count2", 0)
					d.spawn_mob(idx, 8035, 399, 404)
					d.spawn_mob(idx, 8035, 381, 386)
					d.spawn_mob(idx, 8035, 365, 403)
					d.spawn_mob(idx, 8035, 381, 420)
					d.notice(idx, 1216, "", true)
				end
			end
		end

		when 3301.kill or 3302.kill or 3303.kill or 3304.kill or 3305.kill or 3101.kill or 3102.kill or 3103.kill or 3104.kill or 3105.kill or 3801.kill or 3802.kill or 3803.kill or 3804.kill or 3805.kill begin
			local idx = pc.get_map_index()
			if idx >= 2090000 and idx < 2100000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 1 then
					local c = d.getf(idx, "count1") + 1
					if c >= 100 then
						d.setf(idx, "floor", 2)
						clear_server_timer("temple_ochao_check", idx)
						d.spawn_mob(idx, 6391, 399, 407)
						d.notice(idx, 1221, "", true)
						d.notice(idx, 1222, "", true)
					else
						d.setf(idx, "count1", c)
					end
				end
			end
		end

		when temple_ochao_check.server_timer begin
			local arg = get_server_timer_arg()
			if d.find(arg) then
				if d.getf(arg, "floor") == 1 then
					d.notice(arg, 1223, string.format("%d", 100 - d.getf(arg, "count1")), true)
				end
			else
				temple_ochao.clear(arg)
			end
		end

		when temple_ochao_prepare.server_timer begin
			local arg = get_server_timer_arg()
			if d.find(arg) then
				d.setf(arg, "count1", 0)
				d.regen_file(arg, "data/dungeon/temple_ochao/regen_1.json")
				server_timer("temple_ochao_end", 1799, arg)
				d.notice(arg, 1128, string.format("%d", 30), true)
				d.notice(arg, 1220, "", true)
				server_loop_timer("temple_ochao_check", 10, arg)
			else
				temple_ochao.clear(arg)
			end
		end

		when logout begin
			local idx = pc.get_map_index()
			if idx >= 2090000 and idx < 2100000 then
				pc.setf("temple_ochao", "disconnect", get_global_time() + 300)
			end
		end

		when login begin
			local idx = pc.get_map_index()
			if idx == 209 then
				pc.warp(536900, 1331400)
			elseif idx >= 2090000 and idx < 2100000 then
				pc.set_warp_location(219, 5354, 14284)
				pc.setf("temple_ochao", "idx", idx)
				pc.setf("temple_ochao", "ch", pc.get_channel_id())
				if d.getf(idx, "floor") == 0 then
					d.setf(idx, "floor", 1)
					server_timer("temple_ochao_prepare", 1, idx)
					d.setf(idx, "was_completed", 0)
				end
			end
		end

		when 20408.chat."A new territory?!?" begin
			local lang = pc.get_language()

			say_title(string.format("%s:", mob_name(20408)))
			say("")
			say(string.format(gameforge[lang].common.dungeon_1, pc.get_name()))
			local mapIdx = pc.get_map_index()
			-- if mapIdx != 219 then
				-- return
			-- end

			say(gameforge[lang].common.dungeon_2)
			local agree = select(gameforge[lang].common.yes, gameforge[lang].common.no)
			say_title(string.format("%s:", mob_name(20408)))
			say("")
			if agree == 2 then
				say(gameforge[lang].common.dungeon_3)
				return
			end

			local goahead = 1
			local rejoinTIME = pc.getf("temple_ochao", "disconnect") - get_global_time()
			if rejoinTIME > 0 then
				local rejoinIDX = pc.getf("temple_ochao", "idx")
				if rejoinIDX > 0 then
					local rejoinCH = pc.getf("temple_ochao", "ch")
					if rejoinCH != 0 and rejoinCH != pc.get_channel_id() then
						say(string.format(gameforge[lang].common.dungeon_26, rejoinCH))
						return
					end

					if rejoinCH != 0 and d.find(rejoinIDX) then
						if d.getf(rejoinIDX, "was_completed") == 0 then
							say(gameforge[lang].common.dungeon_4)
							local agree2 = select(gameforge[lang].common.yes, gameforge[lang].common.no)
							if agree2 == 2 then
								say_title(string.format("%s:", mob_name(20408)))
								say("")
								say(gameforge[lang].common.dungeon_3)
								return
							end

							local f = d.getf(rejoinIDX, "floor")
							if f != 0 then
								goahead = 0
								pc.warp(853700, 1416400, rejoinIDX)
							end
						end
					end
				end
			end

			if goahead == 1 then
				pc.setf("temple_ochao", "disconnect", 0)
				pc.setf("temple_ochao", "idx", 0)
				pc.setf("temple_ochao", "ch", 0)

				say(gameforge[lang].common.dungeon_5)
				say_reward(string.format(gameforge[lang].common.dungeon_6, 92))
				say_reward(string.format(gameforge[lang].common.dungeon_7, 112))
				say_reward(string.format("- %s: %d", item_name(76025), 1))
				if party.is_party() then
					say_reward(gameforge[lang].common.dungeon_8)
				end
				say(gameforge[lang].common.dungeon_9)
				local n = select(gameforge[lang].common.yes, gameforge[lang].common.no)
				say_title(string.format("%s:", mob_name(20408)))
				say("")
				if n == 2 then
					say(gameforge[lang].common.dungeon_3)
					return
				end

				local flag = string.format("ww_210_%d", pc.get_channel_id())
				local ww_flag = game.get_event_flag(flag) - get_global_time()
				if ww_flag > 0 then
					say(gameforge[lang].common.dungeon_28)
					say(string.format(gameforge[lang].common.dungeon_29, ww_flag))
					return
				end

				myname = pc.get_name()
				result, cooldown, name = d.check_entrance(92, 112, 76025, 0, 1, "temple_ochao.cooldown")
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
						say(string.format(gameforge[lang].common.dungeon_18, 1, item_name(76025)))
					else
						say(string.format(gameforge[lang].common.dungeon_17, name, 1, item_name(76025)))
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
					d.remove_item(76025, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1800, "temple_ochao")
					game.set_event_flag(string.format("ww_209_%d", pc.get_channel_id()), get_global_time() + 5)
					d.join_cords(209, 8836, 14498)
				end
			end
		end
	end
end
