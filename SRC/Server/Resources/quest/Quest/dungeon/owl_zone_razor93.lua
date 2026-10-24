quest owl_zone_razor93 begin
	state start begin
		function clear(arg)
			clear_server_timer("owl_zone_razor93_prepare", arg)
			clear_server_timer("owl_zone_razor93_checker", arg)
			clear_server_timer("owl_zone_razor93_warp", arg)
			clear_server_timer("owl_zone_razor93_end", arg)
			clear_server_timer("owl_zone_razor93_complete", arg)
			if d.find(arg) then
				d.setf(arg, "was_completed", 1)
				d.clear_regen(arg)
				d.kill_all(arg)
				d.exit_all_lobby(arg, 1)
			end
		end

		when owl_zone_razor93_complete.server_timer begin
			owl_zone_razor93.clear(get_server_timer_arg())
		end

		when owl_zone_razor93_end.server_timer begin
			local arg = get_server_timer_arg()
			d.notice(arg, 1040, "", true)--Az ido lejárt, sajnos elbuktál. Teleportálnak téged
			d.notice(arg, 1041, "", true)--ki a runról.
			owl_zone_razor93.clear(arg)
		end

		when 719.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 180000 and idx < 190000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 13 then
					d.setf(idx, "was_completed", 1)
					d.complete(719, 900, 18, "owl_zone_razor93")
					if party.is_party() then
						notice_all(2151, party.get_leader_name())--A %s És csapata felszabadította a Bagoly kazamatát!
					else
						notice_all(2152, pc.get_name())--%s felszabadította a Bagoly kazamatát!
					end
					d.kill_all(idx)
					d.notice(idx, 1068, "", true)--Gratulálunk, a Runt befejezted!
					d.spawn_mob(idx, 9225, 587, 551)
					local bonus = 10 + game.get_event_flag("dungeon_bonus")
					if math.random(1, 100) <= bonus then
						d.spawn_mob(idx, 2598, 587, 551)
					end
					--d.notice(idx, 1261, "", true)--30 másodperc múlva teleportálnak ki.
					--server_timer("owl_zone_razor93_complete", 30, idx)
				end
			end
		end

		when 20366.take with pc.in_dungeon() and item.get_vnum() == 30707 begin
			local idx = pc.get_map_index()
			if idx >= 180000 and idx < 190000 then
				if d.getf(idx, "floor") == 12 then
					npc.purge()
					d.setf(idx, "floor", 13)
					d.clear_regen(idx)
					d.kill_all(idx)
					pc.remove_item(30707, pc.count_item(30707))
					d.spawn_mob(idx, 719, 587, 551)
					d.spawn_mob(idx, 714, 587, 551)
					d.spawn_mob(idx, 714, 583, 551)
					d.spawn_mob(idx, 714, 581, 551)
					d.spawn_mob(idx, 714, 575, 551)
					d.spawn_mob(idx, 714, 587, 551)
					d.spawn_mob(idx, 714, 589, 551)
					d.spawn_mob(idx, 714, 596, 551)
					d.spawn_mob(idx, 714, 600, 551)
					d.spawn_mob(idx, 715, 605, 551)
					d.spawn_mob(idx, 715, 587, 551)
					d.spawn_mob(idx, 715, 583, 551)
					d.spawn_mob(idx, 715, 581, 551)
					d.spawn_mob(idx, 715, 575, 551)
					d.spawn_mob(idx, 715, 587, 551)
					d.spawn_mob(idx, 715, 589, 551)
					d.spawn_mob(idx, 715, 596, 551)
					d.spawn_mob(idx, 715, 600, 551)
					d.spawn_mob(idx, 715, 605, 551)
					--d.notice(idx, 712, "", true)
					d.notice(idx, 1033, "", true)--Hamarosan feljutsz a kilencedik emeletre.
					server_timer("owl_zone_razor93_warp", 2, idx)
				end
			end
		end

		when 716.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 180000 and idx < 190000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 11 then
					d.setf(idx, "floor", 12)
					game.drop_item(30707, 1)

				end
			end
		end

		when 30708.use with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 180000 and idx < 190000 then
				if d.getf(idx, "floor") == 10 then
					d.setf(idx, "floor", 11)
					pc.remove_item(30708, pc.count_item(30708))
					d.spawn_mob(idx, 20366, 198, 588, 60)
					d.set_regen_file(idx, "data/dungeon/owl_dungeon/floor_2b.json")
					d.notice(idx, 1029, "", true)--Megtalálta a helyes kulcsot,
					d.notice(idx, 1030, "", true)--Rövidesen feljutsz a 8. emeletre.
					server_timer("owl_zone_razor93_warp", 2, idx)
				end
			end
		end

		when 8038.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 180000 and idx < 190000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 9 then
					d.setf(idx, "floor", 10)
					game.drop_item(30708, 1)
				end
			end
		end

		when 8425.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 180000 and idx < 190000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 8 then
					local c = d.getf(idx, "count") + 1
					if c == 4 then
						d.setf(idx, "floor", 9)
						d.notice(idx, 1028, "", true)--Keresd meg a kulcsot, hogy tovább mehess
						d.spawn_mob(idx, 8038, 571, 155)
					else
						d.setf(idx, "count", c)
					end
				end
			end
		end

		when 9228.chat."Next floor!" with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 180000 and idx < 190000 then
				if d.getf(idx, "floor") == 7 then
					d.setf(idx, "floor", 8)
					local lang = pc.get_language()
					--addimage(25, 10, "owl_dungeon03.tga")
					addimage(238, 75, "owlghost.tga")
					say_title(string.format("%s:", mob_name(npc.get_race())))
					say("")
					say(gameforge[lang].deviltower_zone._1)
					say(gameforge[lang].deviltower_zone._2)
					say(gameforge[lang].deviltower_zone._3)
					wait()
					setskin(NOWINDOW)
					d.setf(idx, "count", 0)
					d.spawn_mob(idx, 8425, 568, 193)
					d.spawn_mob(idx, 8425, 533, 156)
					d.spawn_mob(idx, 8425, 578, 124)
					d.spawn_mob(idx, 8425, 605, 155)
					d.notice(idx, 1022, "", true)--Hamarosan a következo emeletre teleportálnak.
					server_timer("owl_zone_razor93_warp", 2, idx)
					npc.purge()
				end
			end
		end

		when 718.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 180000 and idx < 190000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 6 then
					d.setf(idx, "floor", 7)
					d.notice(idx, 2150, "", true)--Kattints az NPC-re, hogy tovább mehess!
					party.give_blacksmith()
					local reward = {9228, 9228, 9228}
					d.spawn_mob(idx, reward[number(1, 3)], 215, 150, 60)
				end
			end
		end

		when 9226.take with pc.in_dungeon() and item.get_vnum() == 30709 begin
			local idx = pc.get_map_index()
			if idx >= 180000 and idx < 190000 then
				if d.getf(idx, "floor") == 5 then
					npc.purge()
					item.remove()

					local c = d.getf(idx, "count2") + 1
					if c == 5 then
						d.setf(idx, "floor", 6)
						d.clear_regen(idx)
						d.kill_all(idx)
						d.spawn_mob(idx, 718, 214, 152)

						pc.remove_item(30709, pc.count_item(30709))

						d.notice(idx, 1021, "", true)--Minden pecsétet felnyitottak.
						d.notice(idx, 1022, "", true)--Hamarosan a következo emeletre teleportálnak.
						server_timer("owl_zone_razor93_warp", 2, idx)
					else
						d.setf(idx, "count2", c)
						if c == 4 then
							d.notice(idx, 1023, "", true)--Még egy pecsét van hátra, amit fel kell nyitni.
						else
							d.notice(idx, 1024, string.format("%d", 5 - c), true)--Még %s pecsét van nyitva.
						end
					end
				end
			end
		end

		when 712.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 180000 and idx < 190000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 5 then
					local c = d.getf(idx, "count1") + 1
					if c == 8 then
						d.setf(idx, "count1", 0)
						game.drop_item(30709, 1)
					else
						d.setf(idx, "count1", c)
					end
				end
			end
		end

		when 8426.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 180000 and idx < 190000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 4 then
					if d.getf(idx, "unique_vid") == npc.get_vid() then
						d.setf(idx, "floor", 5)
						d.setf(idx, "count1", 0)
						d.setf(idx, "count2", 0)
						d.clear_regen(idx)
						d.kill_all(idx)
						d.spawn_mob(idx, 9226, 233, 530)
						d.spawn_mob(idx, 9226, 160, 530)
						d.spawn_mob(idx, 9226, 160, 633)
						d.spawn_mob(idx, 9226, 247, 626)
						d.spawn_mob(idx, 9226, 199, 585)
						d.set_regen_file(idx, "data/dungeon/owl_dungeon/floor_4a.json")
						d.notice(idx, 1015, "", true)--A megfelelo metin elpusztult, hamarosan a következo emeletre teleportálnak.
						server_timer("owl_zone_razor93_warp", 2, idx)
					else
						d.notice(idx, 1016, "", true)--Egy hamis metin eltunik...
					end
				end
			end
		end

		when 8026.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 180000 and idx < 190000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 3 then
					d.setf(idx, "floor", 4)

					local stonepos = {
								[1] = {["x"] = 566, ["y"] = 192},
								[2] = {["x"] = 539, ["y"] = 142},
								[3] = {["x"] = 592, ["y"] = 125},
								[4] = {["x"] = 603, ["y"] = 174}
					}

					local random_num = number(1, table.getn(stonepos))
					for index, position in ipairs(stonepos) do
						local vid = d.spawn_mob(idx, 8426, position["x"], position["y"])
						if random_num == index then
							d.setf(idx, "unique_vid", vid)
						end
					end

					d.notice(idx, 1013, "", true)--Pusztítsd el a megfelelo metint, hogy tovább menj.
				end
			end
		end

		when 8427.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 180000 and idx < 190000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 2 then
					d.kill_all(idx)
					d.setf(idx, "floor", 3)
					d.spawn_mob(idx, 8026, 570, 157)
					d.notice(idx, 1097, "", true)--Hamarosan a következo emeletre jutsz.
					server_timer("owl_zone_razor93_warp", 2, idx)
				end
			end
		end

		when owl_zone_razor93_warp.server_timer begin
			local arg = get_server_timer_arg()
			if d.find(arg) then
				clear_server_timer("owl_zone_razor93_warp", arg)
				local f = d.getf(arg, "floor")
				if f == 2 then
					d.jump_all(arg, 15943, 1187)
				elseif f == 3 then
					d.jump_all(arg, 16187, 1176)
				elseif f == 5 then
					d.jump_all(arg, 15816, 1685)
					d.notice(arg, 1017, "", true)--Nyissd ki az összes pecsétet, hogy tovább mehess.
				elseif f == 6 then
					d.jump_all(arg, 15943, 1187)
					d.notice(arg, 2153, "", true)--Öld meg a BOSS-t,hogy megjelenjen az NPC aki tovább enged.
				elseif f == 8 then
					d.jump_all(arg, 16187, 1176)
				elseif f == 11 then
					d.jump_all(arg, 15816, 1685)
					d.notice(arg, 1031, "", true)--Keresd meg a megfelelo kulcsot az emelethez.
				elseif f == 13 then
					d.jump_all(arg, 16251, 1602)
					d.notice(arg, 2154, "", true)--Öld meg a Fö BOSS-t, hogy befejezt a kazamatát.
				end
			else
				owl_zone_razor93.clear(arg)
			end
		end

		when owl_zone_razor93_checker.server_timer begin
			local arg = get_server_timer_arg()
			if d.find(arg) then
				local f = d.getf(arg, "floor")
				if f == 1 then
					if d.count_monster(arg) == 0 then
						clear_server_timer("owl_zone_razor93_checker", arg)
						d.setf(arg, "floor", 2)
						d.spawn_mob(arg, 8427, 219, 167)--2 emelet, kiss boss
						d.notice(arg, 2157, "", true)--Hamarosan a következo emeletre jutsz.
						server_timer("owl_zone_razor93_warp", 2, arg)
					end
				end
			else
				owl_zone_razor93.clear(arg)
			end
		end

		when owl_zone_razor93_prepare.server_timer begin
			local arg = get_server_timer_arg()
			if d.find(arg) then
				d.regen_file(arg, "data/dungeon/owl_dungeon/floor_1.json")
				server_timer("owl_zone_razor93_end", 2999, arg)
				server_loop_timer("owl_zone_razor93_checker", 2, arg)
				d.notice(arg, 1128, string.format("%d", 50), true)--Mostantól kezdve %s percetek lesz arra, hogy befejezzétek a várbörtönt.
				d.notice(arg, 930, "", true)--Öld meg az emelet összes szörnyét!
			else
				owl_zone_razor93.clear(arg)
			end
		end

		when logout begin
			local idx = pc.get_map_index()
			if idx >= 180000 and idx < 190000 then
				pc.setf("owl_zone_razor93", "disconnect", get_global_time() + 300)
			end
		end

		when login begin
			local idx = pc.get_map_index()
			if idx == 18 then
				pc.warp(469100, 718500)
			elseif idx >= 180000 and idx < 190000 then
				pc.set_warp_location(363, 4691, 7185)
				pc.setf("owl_zone_razor93", "idx", idx)
				pc.setf("owl_zone_razor93", "ch", pc.get_channel_id())
				if d.getf(idx, "floor") == 0 then
					d.setf(idx, "floor", 1)
					server_timer("owl_zone_razor93_prepare", 1, idx)
					d.setf(idx, "was_completed", 0)
				end
			end
		end

		when 9225.chat."Owl Dungeon" begin
			local lang = pc.get_language()
			addimage(25, 10, "owl_dungeon03.tga")
			addimage(238, 75, "owlghost.tga")
			say_title(string.format("%s:", mob_name(9225)))
			say("")
			say(string.format(gameforge[lang].common.dungeon_1, pc.get_name()))
			-- local mapIdx = pc.get_map_index()
			-- if mapIdx != 219 then
				-- return
			-- end

			say(gameforge[lang].common.dungeon_2)
			local agree = select(gameforge[lang].common.yes, gameforge[lang].common.no)
			say_title(string.format("%s:", mob_name(9225)))
			say("")
			if agree == 2 then
				say(gameforge[lang].common.dungeon_3)
				return
			end

			local goahead = 1
			local rejoinTIME = pc.getf("owl_zone_razor93", "disconnect") - get_global_time()
			if rejoinTIME > 0 then
				local rejoinIDX = pc.getf("owl_zone_razor93", "idx")
				if rejoinIDX > 0 then
					local rejoinCH = pc.getf("owl_zone_razor93", "ch")
					if rejoinCH != 0 and rejoinCH != pc.get_channel_id() then
						say(string.format(gameforge[lang].common.dungeon_26, rejoinCH))
						return
					end

					if rejoinCH != 0 and d.find(rejoinIDX) then
						if d.getf(rejoinIDX, "was_completed") == 0 then
							say(gameforge[lang].common.dungeon_4)
							local agree2 = select(gameforge[lang].common.yes, gameforge[lang].common.no)
							if agree2 == 2 then
								say_title(string.format("%s:", mob_name(9225)))
								say("")
								say(gameforge[lang].common.dungeon_3)
								return
							end

							local f = d.getf(rejoinIDX, "floor")
							if f == 1 then
								goahead = 0
								pc.warp(1594300, 118700, rejoinIDX)
							elseif f == 2 then
								goahead = 0
								pc.warp(1594300, 118700, rejoinIDX)
							elseif f == 3 or f == 4 then
								goahead = 0
								pc.warp(1618700, 117600, rejoinIDX)
							elseif f == 5 then
								goahead = 0
								pc.warp(1581600, 168500, rejoinIDX)
							elseif f == 6 or f == 7 then
								goahead = 0
								pc.warp(1594300, 118700, rejoinIDX)
							elseif f == 8 or f == 9 or f == 10 then
								goahead = 0
								pc.warp(1618700, 117600, rejoinIDX)
							elseif f == 11 or f == 12 then
								goahead = 0
								pc.warp(1581600, 168500, rejoinIDX)
							elseif f == 13 then
								goahead = 0
								pc.warp(1625100, 160200, rejoinIDX)
							end
						end
					end
				end
			end

			if goahead == 1 then
				pc.setf("owl_zone_razor93", "disconnect", 0)
				pc.setf("owl_zone_razor93", "idx", 0)
				pc.setf("owl_zone_razor93", "ch", 0)

				say(gameforge[lang].common.dungeon_5)
				say_reward(string.format(gameforge[lang].common.dungeon_6, 92))
				say_reward(string.format(gameforge[lang].common.dungeon_7, 112))
				say_reward(string.format("- %s: %d", item_name(30713), 1))
				if party.is_party() then
					say_reward(gameforge[lang].common.dungeon_8)
				end
				say(gameforge[lang].common.dungeon_9)
				local n = select(gameforge[lang].common.yes, gameforge[lang].common.no)
				say_title(string.format("%s:", mob_name(9225)))
				say("")
				if n == 2 then
					say(gameforge[lang].common.dungeon_3)
					return
				end

				local flag = string.format("ww_566_%d", pc.get_channel_id())
				local ww_flag = game.get_event_flag(flag) - get_global_time()
				if ww_flag > 0 then
					say(gameforge[lang].common.dungeon_28)
					say(string.format(gameforge[lang].common.dungeon_29, ww_flag))
					return
				end

				myname = pc.get_name()
				result, cooldown, name = d.check_entrance(92, 112, 30713, 0, 1, "owl_zone_razor93.cooldown")
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
					if myname == name then
						say(string.format(gameforge[lang].common.dungeon_18, 1, item_name(30713)))
					else
						say(string.format(gameforge[lang].common.dungeon_17, name, 1, item_name(30713)))
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
					d.remove_item(30713, 1, 0, 0, 30709, 255, 30708, 255, 30303, 255, 30707, 255, 0, 0, 0, 0, 900, "owl_zone_razor93")
					game.set_event_flag(string.format("ww_566_%d", pc.get_channel_id()), get_global_time() + 5)
					d.join_cords(18, 15943, 1187)
				end
			end
		end
	end
end
