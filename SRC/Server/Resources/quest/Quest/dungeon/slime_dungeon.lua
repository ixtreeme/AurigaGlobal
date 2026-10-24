quest slime_dungeon begin
	state start begin
		function clear(arg)
			clear_server_timer("slime_prepare", arg)
			clear_server_timer("slime_end", arg)
			clear_server_timer("slime_complete", arg)
			if d.find(arg) then
				d.setf(arg, "was_completed", 1)
				d.clear_regen(arg)
				d.kill_all(arg)
				d.exit_all_lobby(arg, 1)
			end
		end

		when slime_complete.server_timer begin
			slime_dungeon.clear(get_server_timer_arg())
		end

		when 768.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 270000 and idx < 280000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 8 then
					d.setf(idx, "was_completed", 1)
					d.complete(768, 900, 27, "slime_dungeon")
					if party.is_party() then
						notice_all(928, party.get_leader_name())
					else
						notice_all(929, pc.get_name())
					end

					d.notice(idx, 1054, "", true)
					--d.notice(idx, 1055, "", true)
					d.kill_all(idx)
					d.clear_regen(idx)
					d.spawn_mob(idx, 9236, 285, 260)
					local bonus = 10 + game.get_event_flag("dungeon_bonus")
					if math.random(1, 100) <= bonus then
						d.spawn_mob(idx, 3491, 285, 260)--triton
					end

					--server_timer("slime_complete", 30, idx)
				end
			end
		end

		when slime_end.server_timer begin
			local arg = get_server_timer_arg()
			d.notice(arg, 1040, "", true)
			d.notice(arg, 1041, "", true)
			slime_dungeon.clear(arg)
		end

		when slime_prepare.server_timer begin
			local arg = get_server_timer_arg()
			if d.find(arg) then
				d.notice(arg, 1154, "", true)
				d.notice(arg, 1155, "", true)
				d.spawn_mob(arg, 768, 285, 260)
				d.notice(arg, 1128, string.format("%d", 30), true)
				server_timer("slime_end", 1799, arg)
			else
				slime_dungeon.clear(arg)
			end
		end

		when logout begin
			local idx = pc.get_map_index()
			if idx >= 270000 and idx < 280000 then
				pc.setf("slime_dungeon", "disconnect", get_global_time() + 300)
			end
		end

		when login begin
			local idx = pc.get_map_index()
			if idx == 27 then
				pc.warp(535400, 1428400)
			elseif idx >= 270000 and idx < 280000 then
				pc.set_warp_location(219, 5354, 14284)
				pc.setf("slime_dungeon", "idx", idx)
				pc.setf("slime_dungeon", "ch", pc.get_channel_id())
				if d.getf(idx, "floor") == 0 then
					d.setf(idx, "floor", 8)
					server_timer("slime_prepare", 1, idx)
					d.setf(idx, "was_completed", 0)
				end
			end
		end

		when 9236.chat."A new territory?!?" begin
			local lang = pc.get_language()

			say_title(string.format("%s:", mob_name(9236)))
			say("")
			say(string.format(gameforge[lang].common.dungeon_1, pc.get_name()))
			local mapIdx = pc.get_map_index()
			-- if mapIdx != 219 then
				-- return
			-- end

			say(gameforge[lang].common.dungeon_2)
			local agree = select(gameforge[lang].common.yes, gameforge[lang].common.no)
			say_title(string.format("%s:", mob_name(9236)))
			say("")
			if agree == 2 then
				say(gameforge[lang].common.dungeon_3)
				return
			end

			local goahead = 1
			local rejoinTIME = pc.getf("slime_dungeon", "disconnect") - get_global_time()
			if rejoinTIME > 0 then
				local rejoinIDX = pc.getf("slime_dungeon", "idx")
				if rejoinIDX > 0 then
					local rejoinCH = pc.getf("slime_dungeon", "ch")
					if rejoinCH != 0 and rejoinCH != pc.get_channel_id() then
						say(string.format(gameforge[lang].common.dungeon_26, rejoinCH))
						return
					end

					if rejoinCH != 0 and d.find(rejoinIDX) then
						if d.getf(rejoinIDX, "was_completed") == 0 then
							say(gameforge[lang].common.dungeon_4)
							local agree2 = select(gameforge[lang].common.yes, gameforge[lang].common.no)
							if agree2 == 2 then
								say_title(string.format("%s:", mob_name(9236)))
								say("")
								say(gameforge[lang].common.dungeon_3)
								return
							end

							local f = d.getf(rejoinIDX, "floor")
							if f != 0 then
								goahead = 0
								pc.warp(942000, 127700, rejoinIDX)
							end
						end
					end
				end
			end

			if goahead == 1 then
				pc.setf("slime_dungeon", "disconnect", 0)
				pc.setf("slime_dungeon", "idx", 0)
				pc.setf("slime_dungeon", "ch", 0)

				say(gameforge[lang].common.dungeon_5)
				say_reward(string.format(gameforge[lang].common.dungeon_6, 45))
				say_reward(string.format(gameforge[lang].common.dungeon_7, 65))
				say_reward(string.format("- %s: %d", item_name(30625), 1))
				if party.is_party() then
					say_reward(gameforge[lang].common.dungeon_8)
				end
				say(gameforge[lang].common.dungeon_9)
				local n = select(gameforge[lang].common.yes, gameforge[lang].common.no)
				say_title(string.format("%s:", mob_name(9236)))
				say("")
				if n == 2 then
					say(gameforge[lang].common.dungeon_3)
					return
				end

				local flag = string.format("ww_27_%d", pc.get_channel_id())
				local ww_flag = game.get_event_flag(flag) - get_global_time()
				if ww_flag > 0 then
					say(gameforge[lang].common.dungeon_28)
					say(string.format(gameforge[lang].common.dungeon_29, ww_flag))
					return
				end

				myname = pc.get_name()
				result, cooldown, name = d.check_entrance(45, 65, 30625, 0, 1, "slime_dungeon.cooldown")
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
						say(string.format(gameforge[lang].common.dungeon_18, 1, item_name(30625)))
					else
						say(string.format(gameforge[lang].common.dungeon_17, name, 1, item_name(30625)))
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
					d.remove_item(30625, 1, 0, 0, 30623, 255, 30624, 255, 0, 0, 0, 0, 0, 0, 0, 0, 900, "slime_dungeon")
					game.set_event_flag(string.format("ww_27_%d", pc.get_channel_id()), get_global_time() + 5)
					d.join_cords(27, 9420, 1277)
				end
			end
		end
	end
end
