quest monkey_tower_razor93 begin
	state start begin
		function clear(arg)
			clear_server_timer("monkey_tower_razor93_prepare", arg)
			clear_server_timer("monkey_tower_razor93_checker", arg)
			clear_server_timer("monkey_tower_razor93_warp", arg)
			clear_server_timer("monkey_tower_razor93_end", arg)
			clear_server_timer("monkey_tower_razor93_complete", arg)
			if d.find(arg) then
				d.setf(arg, "was_completed", 1)
				d.clear_regen(arg)
				d.kill_all(arg)
				d.exit_all_lobby(arg, 1)
			end
		end

		function check_floor(idx)
			if idx < 3670000 or idx >= 3680000 or not d.find(idx) then return end
			if d.getf(idx, "was_completed") != 0 then return end
			local f = d.getf(idx, "floor")
			if f == 1 and d.getf(idx, "gate_unlocked") == 0 then
				if d.count_monster(idx, 5121) == 0 and d.count_monster(idx, 5122) == 0 then
					if d.set_vid_invincible(d.getf(idx, "floor1_vid1"), false) then
						d.setf(idx, "gate_unlocked", 1)
						d.notice(idx, 2156, "", true)
					end
				end
			elseif f == 13 and d.getf(idx, "boss_unlocked") == 0 then
				if d.count_monster(idx, 5125) == 0 and d.count_monster(idx, 5126) == 0 then
					if d.set_vid_invincible(d.getf(idx, "boss_vid_monkey"), false) then
						d.setf(idx, "boss_unlocked", 1)
					end
				end
			end
		end

		when monkey_tower_razor93_checker.server_timer begin
			monkey_tower_razor93.check_floor(get_server_timer_arg())
		end

		when monkey_tower_razor93_complete.server_timer begin
			monkey_tower_razor93.clear(get_server_timer_arg())
		end

		when monkey_tower_razor93_end.server_timer begin
			local arg = get_server_timer_arg()
			d.notice(arg, 1040, "", true)--Az ido lejárt, sajnos elbuktál. Teleportálnak téged
			d.notice(arg, 1041, "", true)--ki a runról.
			monkey_tower_razor93.clear(arg)
		end

		when 8061.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 3670000 and idx < 3680000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 2 then
					if d.getf(idx, "unique_vid") == npc.get_vid() then
						d.setf(idx, "floor", 5)
						d.setf(idx, "count1", 0)
						d.setf(idx, "count2", 0)
						d.clear_regen(idx)
						d.kill_all(idx)
						local monkey_vid = d.spawn_mob(idx, 20460, 626, 181)
						if monkey_vid == 0 or not d.set_vid_invincible(monkey_vid, true) then
							monkey_tower_razor93.clear(idx)
							return
						end
						d.setf(idx, "boss_vid", monkey_vid)
						d.setf(idx, "statue_hits", 0)

						-- d.spawn_mob(idx, 6118, 638, 150)
						-- d.spawn_mob(idx, 6118, 600, 150)
						-- d.spawn_mob(idx, 6118, 590, 180)
						-- d.spawn_mob(idx, 6118, 612, 204)
						d.set_regen_file(idx, "data/dungeon/monkey_razor/floor_4a.json")
						d.notice(idx, 1015, "", true)--A megfelelo metin elpusztult, hamarosan a következo emeletre teleportálnak.
						server_timer("monkey_tower_razor93_warp", 2, idx)
					else
						d.notice(idx, 1016, "", true)--Egy hamis metin eltunik...
					end
				end
			end
		end


		when monkey_tower_razor93_warp.server_timer begin
			local arg = get_server_timer_arg()
			if d.find(arg) then
				clear_server_timer("monkey_tower_razor93_warp", arg)
				local f = d.getf(arg, "floor")
				if f == 5 then
					d.jump_all(arg, 6539, 437)
					d.notice(arg, 2158, "", true)
					d.notice(arg, 2159, "", true)
					d.notice(arg, 2160, "", true)
				elseif f == 13 then
					d.jump_all(arg, 6491, 667)
					d.notice(arg, 2161, "", true)
				end
			else
				monkey_tower_razor93.clear(arg)
			end
		end

		when monkey_tower_razor93_prepare.server_timer begin
			local arg = get_server_timer_arg()
			if not d.find(arg) or d.getf(arg, "was_completed") != 0 then return end
			local gate = d.spawn_mob(arg, 30111, 245, 196, 270)
			if gate == 0 or not d.set_vid_invincible(gate, true) then
				monkey_tower_razor93.clear(arg)
				return
			end
			d.setf(arg, "floor1_vid1", gate)
			d.setf(arg, "gate_unlocked", 0)
			d.regen_file(arg, "data/dungeon/monkey_razor/floor_1.json")
			server_timer("monkey_tower_razor93_end", 2999, arg)
			server_loop_timer("monkey_tower_razor93_checker", 2, arg)
			d.notice(arg, 1128, "50", true)
			d.notice(arg, 2155, "", true)
		end

		when 5121.kill or 5122.kill with pc.in_dungeon() begin
			monkey_tower_razor93.check_floor(pc.get_map_index())
		end

		when 30111.kill with pc.in_dungeon() begin-- az ajtót lecsaptad
			local idx = pc.get_map_index()
			if idx >= 3670000 and idx < 3680000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") != 1 or d.getf(idx, "gate_unlocked") != 1 then return end
				if npc.get_vid() != d.getf(idx, "floor1_vid1") then return end
				d.setf(idx, "floor", 2)



				d.notice(idx, 2157, "", true)--Pusztítsd el a metinkövet.

				d.spawn_mob(idx, 8059, 392, 190)

			end
		end

		when 8059.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 3670000 and idx < 3680000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") != 2 or d.getf(idx, "metins_spawned") != 0 then return end
				d.setf(idx, "metins_spawned", 1)
				local stonepos = {
					[1] = {["x"] = 370, ["y"] = 181},
					[2] = {["x"] = 390, ["y"] = 159},
					[3] = {["x"] = 420, ["y"] = 170},
					[4] = {["x"] = 427, ["y"] = 196},
					[5] = {["x"] = 403, ["y"] = 203},
					[6] = {["x"] = 392, ["y"] = 190},
					[7] = {["x"] = 405, ["y"] = 183},
					[8] = {["x"] = 416, ["y"] = 187}
				}

				local random_num = number(1, table.getn(stonepos))
				for index, position in ipairs(stonepos) do
					local vid = d.spawn_mob(idx, 8061, position["x"], position["y"])
					if random_num == index then
						d.setf(idx, "unique_vid", vid)

					end
				end

				d.notice(idx, 1013, "", true) -- Pusztítsd el a megfelelo metint, hogy tovább menj.
			end
		end  -- <<--- ez a fontos lezáró a when blokkhoz

		when 5125.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx < 3670000 or idx >= 3680000 or d.getf(idx, "was_completed") != 0 then return end
			if d.getf(idx, "floor") == 5 and number(1, 100) > 85 then
				game.drop_item(50090, 1)
			elseif d.getf(idx, "floor") == 13 then
				monkey_tower_razor93.check_floor(idx)
			end
		end

		when 20460.take with pc.in_dungeon() and item.get_vnum() == 50090 begin
			local idx = pc.get_map_index()
			if idx < 3670000 or idx >= 3680000 or d.getf(idx, "was_completed") != 0 then return end
			if d.getf(idx, "floor") != 5 then return end
			local statue = d.getf(idx, "boss_vid")
			if statue == 0 or npc.get_vid() != statue then return end
			local hits = d.getf(idx, "statue_hits")
			if hits >= 5 then return end
			-- Shared by every party member. The fifth spear finishes the stage;
			-- do not reduce HP to zero while the statue is still invincible.
			if hits < 4 and not npc.npc_decrease_hp_by_percent_razor93(statue, 20) then return end
			hits = hits + 1
			d.setf(idx, "statue_hits", hits)
			item.remove()
			if hits == 5 then
				d.setf(idx, "floor", 13)
				d.clear_regen(idx)
				d.kill_all(idx)
				local boss = d.spawn_mob(idx, 2832, 625, 457)
				if boss == 0 or not d.set_vid_invincible(boss, true) then
					monkey_tower_razor93.clear(idx)
					return
				end
				d.setf(idx, "boss_vid_monkey", boss)
				d.setf(idx, "boss_unlocked", 0)
				d.regen_file(idx, "data/dungeon/monkey_razor/boss.json")
				pc.remove_item(50090, pc.count_item(50090))
				d.notice(idx, 2162, "", true)
				d.notice(idx, 2163, "", true)
				server_timer("monkey_tower_razor93_warp", 10, idx)
			end
		end

		when 5126.kill with pc.in_dungeon() begin
			monkey_tower_razor93.check_floor(pc.get_map_index())
		end

		when 2832.kill with pc.in_dungeon() begin--BOSS KILL LOGIKA
			local idx = pc.get_map_index()
			if idx >= 3670000 and idx < 3680000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 13 then
					d.setf(idx, "was_completed", 1)
					d.clear_regen(idx)
					d.complete(2832, 1, 367, "monkey_tower_razor93")
					if party.is_party() then
						notice_all(2164, party.get_leader_name())--A %s És csapata felszabadította a Bagoly kazamatát!
					else
						notice_all(2165, pc.get_name())--%s felszabadította a Bagoly kazamatát!
					end
					d.kill_all(idx)
					d.notice(idx, 1068, "", true)--Gratulálunk, a Runt befejezted!
					d.spawn_mob(idx, 20009, 619, 444)--Nemere
					--d.notice(idx, 1261, "", true)--30 másodperc múlva teleportálnak ki.
					--server_timer("monkey_tower_razor93_complete", 30, idx)
					local bonus = 10 + game.get_event_flag("dungeon_bonus")
					if math.random(1, 100) <= bonus then
						d.spawn_mob(idx, 693, 619, 444)--Ork
						syschat("Dungeon bonus spawm: Ork ")
					end
				end
			end
		end



		when logout begin-- KIDOBÁS/KIJELENTKEZÉS
			local idx = pc.get_map_index()
			if idx >= 3670000 and idx < 3680000 then
				pc.setf("monkey_tower_razor93", "disconnect", get_global_time() + 300)--rejoin time
			end
		end

		when login begin
			local idx = pc.get_map_index()
			if idx == 367 then--HA NEM JÖTT LÉTRE A DUNGEON , PUSZTULSZ VISSZA INDULÓHELYRE
				pc.warp(535400, 128400)
			elseif idx >= 3670000 and idx < 3680000 then
				pc.set_warp_location(363, 4691, 7185)
				pc.setf("monkey_tower_razor93", "idx", idx)
				pc.setf("monkey_tower_razor93", "ch", pc.get_channel_id())
				if d.getf(idx, "floor") == 0 then
					d.setf(idx, "floor", 1)
					server_timer("monkey_tower_razor93_prepare", 1, idx)
					d.setf(idx, "was_completed", 0)
				end
			end
		end

		when 20009.chat."Monkey Dungeon" begin
			local lang = pc.get_language()
			addimage(25, 10, "owl_dungeon03.tga")
			addimage(238, 75, "owlghost.tga")
			say_title(string.format("%s:", mob_name(20009)))
			say("")
			say(string.format(gameforge[lang].common.dungeon_1, pc.get_name()))
			-- local mapIdx = pc.get_map_index()
			-- if mapIdx != 219 then
				-- return
			-- end

			say(gameforge[lang].common.dungeon_2)
			local agree = select(gameforge[lang].common.yes, gameforge[lang].common.no)
			say_title(string.format("%s:", mob_name(20009)))
			say("")
			if agree == 2 then
				say(gameforge[lang].common.dungeon_3)
				return
			end

			local goahead = 1
			local rejoinTIME = pc.getf("monkey_tower_razor93", "disconnect") - get_global_time()
			if rejoinTIME > 0 then
				local rejoinIDX = pc.getf("monkey_tower_razor93", "idx")
				if rejoinIDX > 0 then
					local rejoinCH = pc.getf("monkey_tower_razor93", "ch")
					if rejoinCH != 0 and rejoinCH != pc.get_channel_id() then
						say(string.format(gameforge[lang].common.dungeon_26, rejoinCH))
						return
					end

					if rejoinCH != 0 and d.find(rejoinIDX) then
						if d.getf(rejoinIDX, "was_completed") == 0 then
							say(gameforge[lang].common.dungeon_4)
							local agree2 = select(gameforge[lang].common.yes, gameforge[lang].common.no)
							if agree2 == 2 then
								say_title(string.format("%s:", mob_name(20009)))
								say("")
								say(gameforge[lang].common.dungeon_3)
								return
							end

							local f = d.getf(rejoinIDX, "floor")
							if f == 1 then
								goahead = 0
								pc.warp(611800, 44700, rejoinIDX)
							elseif f == 2 then
								goahead = 0
								pc.warp(629100, 43900, rejoinIDX)
							elseif f == 5 then
								goahead = 0
								pc.warp(653900, 43700, rejoinIDX)
							elseif f == 13 then
								goahead = 0
								pc.warp(649100, 66700, rejoinIDX)
							end
						end
					end
				end
			end

			if goahead == 1 then
				pc.setf("monkey_tower_razor93", "disconnect", 0)
				pc.setf("monkey_tower_razor93", "idx", 0)
				pc.setf("monkey_tower_razor93", "ch", 0)

				say(gameforge[lang].common.dungeon_5)
				say_reward(string.format(gameforge[lang].common.dungeon_6, 30))
				say_reward(string.format(gameforge[lang].common.dungeon_7, 50))
				--say_reward(string.format("- %s: %d", item_name(0), 1))
				if party.is_party() then
					say_reward(gameforge[lang].common.dungeon_8)
				end
				say(gameforge[lang].common.dungeon_9)
				local n = select(gameforge[lang].common.yes, gameforge[lang].common.no)
				say_title(string.format("%s:", mob_name(20009)))
				say("")
				if n == 2 then
					say(gameforge[lang].common.dungeon_3)
					return
				end

				local flag = string.format("ww_567_%d", pc.get_channel_id())
				local ww_flag = game.get_event_flag(flag) - get_global_time()
				if ww_flag > 0 then
					say(gameforge[lang].common.dungeon_28)
					say(string.format(gameforge[lang].common.dungeon_29, ww_flag))
					return
				end

				myname = pc.get_name()
				result, cooldown, name = d.check_entrance(30, 50, 0, 0, 1, "monkey_tower_razor93.cooldown")
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
					d.remove_item(0, 0, 0, 0, 50090, 255, 30708, 255, 30303, 255, 30707, 255, 0, 0, 0, 0, 0, "monkey_tower_razor93")
					game.set_event_flag(string.format("ww_567_%d", pc.get_channel_id()), get_global_time() + 5)
					d.join_cords(367, 6118, 447)
				end
			end
		end
	end
end
