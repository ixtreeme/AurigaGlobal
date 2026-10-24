quest dungeon_spider begin
	state start begin
		function clear(arg)
			clear_server_timer("spider_prepare", arg)
			clear_server_timer("spider_end", arg)
			clear_server_timer("spider_complete", arg)
			if d.find(arg) then
				d.setf(arg, "was_completed", 1)
				d.clear_regen(arg)
				d.kill_all(arg)
				d.exit_all_lobby(arg, 1)
			end
		end

		when spider_complete.server_timer begin
			dungeon_spider.clear(get_server_timer_arg())
		end

		when 2092.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 2170000 and idx < 2180000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 4 then
					d.setf(idx, "was_completed", 1)
					d.complete(2092, 900, 217, "dungeon_spider")
					if party.is_party() then
						notice_all(936, party.get_leader_name())
					else
						notice_all(937, pc.get_name())
					end

					d.notice(idx, 1136, "", true)
					--d.notice(idx, 1137, "", true)
					d.kill_all(idx)
					d.clear_regen(idx)
					d.spawn_mob(idx, 30130, 367, 588)
					local bonus = 10 + game.get_event_flag("dungeon_bonus")
					if math.random(1, 100) <= bonus then
						d.spawn_mob(idx, 4011, 367, 588)
					end
					--server_timer("spider_complete", 30, idx)
				end
			end
		end

		when 2095.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 2170000 and idx < 2180000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 3 then
					local c = d.getf(idx, "count") - 1
					if c == 0 then
						d.setf(idx, "floor", 4)
						local success = d.set_vid_invincible(d.getf(idx, "boss"), false)
						if not success then
							d.notice(idx, 1135, "", true)
							dungeon_spider.clear(idx)
						else
							d.clear_regen(idx)
							d.notice(idx, 1134, "", true)
						end
					else
						d.setf(idx, "count", c)
					end
				end
			end
		end

		when spider_end.server_timer begin
			local arg = get_server_timer_arg()
			d.notice(arg, 1040, "", true)
			d.notice(arg, 1041, "", true)
			dungeon_spider.clear(arg)
		end

		when spider_prepare.server_timer begin
			local arg = get_server_timer_arg()
			if d.find(arg) then
				d.setf(arg, "count", 9)
				d.spawn_mob(arg, 2095, 400, 566)
				d.spawn_mob(arg, 2095, 400, 594)
				d.spawn_mob(arg, 2095, 362, 600)
				d.spawn_mob(arg, 2095, 337, 599)
				d.spawn_mob(arg, 2095, 335, 581)
				d.spawn_mob(arg, 2095, 344, 562)
				d.spawn_mob(arg, 2095, 364, 588)
				d.spawn_mob(arg, 2095, 379, 562)
				d.spawn_mob(arg, 2095, 368, 525)
				local boss = d.spawn_mob(arg, 2092, 367, 588)
				local success = d.set_vid_invincible(boss, true)
				if not success then
					d.notice(arg, 1133, "", true)
					dungeon_spider.clear(arg)
				else
					d.notice(arg, 1128, string.format("%d", 30), true)
					server_timer("spider_end", 1799, arg)
					d.setf(arg, "boss", boss)
					d.notice(arg, 1131, "", true)
					d.notice(arg, 1132, "", true)
				end
			else
				dungeon_spider.clear(arg)
			end
		end

		when logout begin
			local idx = pc.get_map_index()
			if idx >= 2170000 and idx < 2180000 then
				pc.setf("dungeon_spider", "disconnect", get_global_time() + 300)
			end
		end

		when login begin
			local idx = pc.get_map_index()
			--if idx == 217 then
			--	pc.warp(535400, 1428400)
			if idx >= 2170000 and idx < 2180000 then
				pc.set_warp_location(219, 5354, 14284)
				pc.setf("dungeon_spider", "idx", idx)
				pc.setf("dungeon_spider", "ch", pc.get_channel_id())
				if d.getf(idx, "floor") == 0 then
					d.setf(idx, "floor", 3)
					server_timer("spider_prepare", 1, idx)
					d.setf(idx, "was_completed", 0)
				end
			end
		end

		when 30130.chat."A new territory?!?" begin
			local lang = pc.get_language()

			say_title(string.format("%s:", mob_name(30130)))
			say("")
			say(string.format(gameforge[lang].common.dungeon_1, pc.get_name()))
			local mapIdx = pc.get_map_index()
			-- if mapIdx != 219 then
				-- return
			-- end

			say(gameforge[lang].common.dungeon_2)
			local agree = select(gameforge[lang].common.yes, gameforge[lang].common.no)
			say_title(string.format("%s:", mob_name(30130)))
			say("")
			if agree == 2 then
				say(gameforge[lang].common.dungeon_3)
				return
			end

			local goahead = 1
			local rejoinTIME = pc.getf("dungeon_spider", "disconnect") - get_global_time()
			if rejoinTIME > 0 then
				local rejoinIDX = pc.getf("dungeon_spider", "idx")
				if rejoinIDX > 0 then
					local rejoinCH = pc.getf("dungeon_spider", "ch")
					if rejoinCH != 0 and rejoinCH != pc.get_channel_id() then
						say(string.format(gameforge[lang].common.dungeon_26, rejoinCH))
						return
					end

					if rejoinCH != 0 and d.find(rejoinIDX) then
						if d.getf(rejoinIDX, "was_completed") == 0 then
							say(gameforge[lang].common.dungeon_4)
							local agree2 = select(gameforge[lang].common.yes, gameforge[lang].common.no)
							if agree2 == 2 then
								say_title(string.format("%s:", mob_name(30130)))
								say("")
								say(gameforge[lang].common.dungeon_3)
								return
							end

							local f = d.getf(rejoinIDX, "floor")
							if f != 0 then
								goahead = 0
								pc.warp(87900, 614700, rejoinIDX)
							end
						end
					end
				end
			end

			if goahead == 1 then
				pc.setf("dungeon_spider", "disconnect", 0)
				pc.setf("dungeon_spider", "idx", 0)
				pc.setf("dungeon_spider", "ch", 0)

				say(gameforge[lang].common.dungeon_5)
				say_reward(string.format(gameforge[lang].common.dungeon_6, 120))
				say_reward(string.format(gameforge[lang].common.dungeon_7, 140))
				say_reward(string.format("- %s: %d", item_name(30325), 1))
				if party.is_party() then
					say_reward(gameforge[lang].common.dungeon_8)
				end
				say(gameforge[lang].common.dungeon_9)
				local n = select(gameforge[lang].common.yes, gameforge[lang].common.no)
				say_title(string.format("%s:", mob_name(30130)))
				say("")
				if n == 2 then
					say(gameforge[lang].common.dungeon_3)
					return
				end

				local flag = string.format("ww_217_%d", pc.get_channel_id())
				local ww_flag = game.get_event_flag(flag) - get_global_time()
				if ww_flag > 0 then
					say(gameforge[lang].common.dungeon_28)
					say(string.format(gameforge[lang].common.dungeon_29, ww_flag))
					return
				end

				myname = pc.get_name()
				result, cooldown, name = d.check_entrance(120, 140, 30325, 0, 1, "dungeon_spider.cooldown")
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
						say(string.format(gameforge[lang].common.dungeon_18, 1, item_name(30325)))
					else
						say(string.format(gameforge[lang].common.dungeon_17, name, 1, item_name(30325)))
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
					d.remove_item(30325, 1, 0, 0, 30327, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 900, "dungeon_spider")
					game.set_event_flag(string.format("ww_217_%d", pc.get_channel_id()), get_global_time() + 5)
					d.join_cords(217, 879, 6147)
				end
			end
		end
	end
end
