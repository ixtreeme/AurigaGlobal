quest dragonlair_zone begin
	state start begin
		function clear(arg)
			clear_server_timer("dragonlair_prepare", arg)
			clear_server_timer("dragonlair_end", arg)
			clear_server_timer("dragonlair_complete", arg)
			if d.find(arg) then
				d.setf(arg, "was_completed", 1)
				d.clear_regen(arg)
				d.kill_all(arg)
				d.exit_all_lobby(arg, 1)
			end
		end




		when dragonlair_complete.server_timer begin
			dragonlair_zone.clear(get_server_timer_arg())
		end

		when 2493.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 2080000 and idx < 2090000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 2 then
					d.setf(idx, "was_completed", 1)
					d.complete(2493, 1800, 208, "dragonlair_zone")
					if party.is_party() then
						notice_all(1257, party.get_leader_name())
					else
						notice_all(1256, pc.get_name())
					end

					d.notice(idx, 1255, "", true)
					--d.notice(idx, 1261, "", true)
					d.kill_all(idx)
					d.clear_regen(idx)
					d.spawn_mob(idx, 30121, 181, 173)
					local bonus = 10 + game.get_event_flag("dungeon_bonus")
					if math.random(1, 100) <= bonus then
						d.spawn_mob(idx, 1093, 181, 173)
					end
					--server_timer("dragonlair_complete", 30, idx)
				end
			end
		end

		when 8031.kill or 8032.kill or 8033.kill or 8034.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 2080000 and idx < 2090000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 1 then
					local c = d.getf(idx, "count") - 1
					if c == 0 then
						d.setf(idx, "floor", 2)
						local success = d.set_vid_invincible(d.getf(idx, "boss"), false)
						if not success then
							d.notice(idx, 1014, "", true)
							dragonlair_zone.clear(idx)
						else
							d.notice(idx, 1276, "", true)
						end
					else
						d.setf(idx, "count", c)
						if 1 == 3 then
							d.notice(idx, 1278, "", true)
						else
							d.notice(idx, 1277, string.format("%d", c), true)
						end
					end
				end
			end
		end

		when dragonlair_end.server_timer begin
			local arg = get_server_timer_arg()
			d.notice(arg, 1040, "", true)
			d.notice(arg, 1041, "", true)
			dragonlair_zone.clear(arg)
		end

		when dragonlair_prepare.server_timer begin
			local arg = get_server_timer_arg()
			if d.find(arg) then
				local boss = d.spawn_mob(arg, 2493, 181, 173)
				local success = d.set_vid_invincible(boss, true)
				if not success then
					d.notice(arg, 1014, "", true)
					dragonlair_zone.clear(arg)
				else
					d.setf(arg, "boss", boss)
					d.regen_file(arg, "data/dungeon/dragonlair/regen.json")
					d.setf(arg, "count", 4)
					server_timer("dragonlair_end", 1799, arg)
					d.notice(arg, 1128, string.format("%d", 30), true)
					d.notice(arg, 1279, "", true)
				end
			else
				dragonlair_zone.clear(arg)
			end
		end

		when logout begin
			local idx = pc.get_map_index()
			if idx >= 2080000 and idx < 2090000 then
				pc.setf("dragonlair_zone", "disconnect", get_global_time() + 300)
			end
		end

		when login begin
			local idx = pc.get_map_index()
			if idx == 208 then
				pc.warp(535400, 1428400)
			elseif idx >= 2080000 and idx < 2090000 then
				pc.set_warp_location(219, 5354, 14284)
				pc.setf("dragonlair_zone", "idx", idx)
				pc.setf("dragonlair_zone", "ch", pc.get_channel_id())
				if d.getf(idx, "floor") == 0 then
					d.setf(idx, "floor", 1)
					server_timer("dragonlair_prepare", 1, idx)
					d.setf(idx, "was_completed", 0)
				end
			end
		end

		when 30121.chat."Beran-Setaou Dungeon" begin
			local lang = pc.get_language()

			say_title(string.format("%s:", mob_name(30121)))
			say("")
			say(string.format(gameforge[lang].common.dungeon_1, pc.get_name()))
			local mapIdx = pc.get_map_index()
			-- if mapIdx != 219 then
				-- return
			-- end

			say(gameforge[lang].common.dungeon_2)
			local agree = select(gameforge[lang].common.yes, gameforge[lang].common.no)
			say_title(string.format("%s:", mob_name(30121)))
			say("")
			if agree == 2 then
				say(gameforge[lang].common.dungeon_3)
				return
			end

			local goahead = 1
			local rejoinTIME = pc.getf("dragonlair_zone", "disconnect") - get_global_time()
			if rejoinTIME > 0 then
				local rejoinIDX = pc.getf("dragonlair_zone", "idx")
				if rejoinIDX > 0 then
					local rejoinCH = pc.getf("dragonlair_zone", "ch")
					if rejoinCH != 0 and rejoinCH != pc.get_channel_id() then
						say(string.format(gameforge[lang].common.dungeon_26, rejoinCH))
						return
					end

					if rejoinCH != 0 and d.find(rejoinIDX) then
						if d.getf(rejoinIDX, "was_completed") == 0 then
							say(gameforge[lang].common.dungeon_4)
							local agree2 = select(gameforge[lang].common.yes, gameforge[lang].common.no)
							if agree2 == 2 then
								say_title(string.format("%s:", mob_name(30121)))
								say("")
								say(gameforge[lang].common.dungeon_3)
								return
							end

							local f = d.getf(rejoinIDX, "floor")
							if f != 0 then
								goahead = 0
								pc.warp(843500, 1066800, rejoinIDX)
							end
						end
					end
				end
			end

			if goahead == 1 then
				pc.setf("dragonlair_zone", "disconnect", 0)
				pc.setf("dragonlair_zone", "idx", 0)
				pc.setf("dragonlair_zone", "ch", 0)

				say(gameforge[lang].common.dungeon_5)
				say_reward(string.format(gameforge[lang].common.dungeon_6, 60))
				say_reward(string.format(gameforge[lang].common.dungeon_7, 100))
				say_reward(string.format("- %s: %d", item_name(30179), 1))
				if party.is_party() then
					say_reward(gameforge[lang].common.dungeon_8)
				end
				say(gameforge[lang].common.dungeon_9)
				local n = select(gameforge[lang].common.yes, gameforge[lang].common.no)
				say_title(string.format("%s:", mob_name(30121)))
				say("")
				if n == 2 then
					say(gameforge[lang].common.dungeon_3)
					return
				end

				local flag = string.format("ww_208_%d", pc.get_channel_id())
				local ww_flag = game.get_event_flag(flag) - get_global_time()
				if ww_flag > 0 then
					say(gameforge[lang].common.dungeon_28)
					say(string.format(gameforge[lang].common.dungeon_29, ww_flag))
					return
				end

				myname = pc.get_name()
				result, cooldown, name = d.check_entrance(60, 100, 30179, 0, 1, "dragonlair_zone.cooldown")
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
						say(string.format(gameforge[lang].common.dungeon_18, 1, item_name(30179)))
					else
						say(string.format(gameforge[lang].common.dungeon_17, name, 1, item_name(30179)))
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
					d.remove_item(30179, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1800, "dragonlair_zone")
					game.set_event_flag(string.format("ww_208_%d", pc.get_channel_id()), get_global_time() + 5)
					d.join_cords(208, 8435, 10668)
				end
			end
		end
	end
end
