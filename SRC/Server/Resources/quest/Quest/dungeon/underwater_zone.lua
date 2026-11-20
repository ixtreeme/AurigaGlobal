quest underwater_zone begin
	state start begin
		function clear(arg)
			clear_server_timer("underwater_prepare", arg)
			clear_server_timer("underwater_checker", arg)
			clear_server_timer("underwater_warp", arg)
			clear_server_timer("underwater_end", arg)
			clear_server_timer("underwater_complete", arg)
			if d.find(arg) then
				d.setf(arg, "was_completed", 1)
				d.clear_regen(arg)
				d.kill_all(arg)
				d.exit_all_lobby(arg, 1)
			end
		end

		when underwater_complete.server_timer begin
			underwater_zone.clear(get_server_timer_arg())
		end

		when underwater_end.server_timer begin
			local arg = get_server_timer_arg()
			d.notice(arg, 1040, "", true)--Az ido lejárt, sajnos elbuktál. Teleportálnak téged
			d.notice(arg, 1041, "", true)--ki a runról.
			underwater_zone.clear(arg)
		end

		when 3962.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 670000 and idx < 680000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 13 then
					d.setf(idx, "was_completed", 1)
					d.complete(3962, 3600, 67, "underwater_zone")
					if party.is_party() then
							pc.typed_item_grant(8, 424, 0, 1)--Hidra ládája
							pc.typed_item_grant(10, 965, 0, 2)--coin
							pc.typed_item_grant(6, 117, 0, 5)--f gyö
							pc.typed_item_grant(6, 118, 0, 5)--k gy
							pc.typed_item_grant(6, 119, 0, 5)--v gy
							pc.typed_item_grant(6, 346, 0, 50)--Metin ko szilánk
							pc.typed_item_grant(5, 346, 0, 200)--1-5 fori 20db
							pc.typed_item_grant(6, 39, 0, 2)--Aqua krist�ly
							pc.typed_item_grant(6, 41, 0, 2)--Istenek Itala
						--notice_all(1064, pc.get_name())
						notice_all(2092, party.get_leader_name())
					else
						pc.typed_item_grant(8, 424, 0, 1)--Hidra ládája
						pc.typed_item_grant(10, 965, 0, 1)--coin
						pc.typed_item_grant(6, 117, 0, 2)--f gyö
						pc.typed_item_grant(6, 118, 0, 2)--k gy
						pc.typed_item_grant(6, 119, 0, 2)--v gy
						pc.typed_item_grant(6, 346, 0, 25)--Metin ko szilánk
						pc.typed_item_grant(5, 346, 0, 100)--1-5 fori 20db
						pc.typed_item_grant(6, 39, 0, 1)--Aqua krist�ly
						pc.typed_item_grant(6, 41, 0, 1)--Istenek Itala
						notice_all(2093, pc.get_name())
					end
					d.kill_all(idx)
					d.notice(idx, 1068, "", true)--Gratulálunk, a Runt befejezted!
					--d.notice(idx, 1261, "", true)--30 másodperc múlva teleportálnak ki.
					--server_timer("underwater_complete", 30, idx)
				end

			end
		end

		when "catalog.3.zin_bong_in_key_f29d442e6a2e".typed_use with pc.in_dungeon() and pc.get_map_index() >= 670000 and pc.get_map_index() < 680000 begin
			local idx = pc.get_map_index()
			if idx >= 670000 and idx < 680000 then
				if d.getf(idx, "floor") == 12 then
					if not d.typed_item_target(20362) then return end
					if not pc.typed_dungeon_item_redeem({}, 0) then return end
					npc.purge()
					d.setf(idx, "floor", 13)
					d.clear_regen(idx)
					d.kill_all(idx)
					d.typed_item_cleanup("catalog.3.zin_bong_in_key_f29d442e6a2e")
					d.spawn_mob(idx, 3962, 175, 188)
					d.set_regen_file(idx, "data/dungeon/water/regenboss.json")
					d.notice(idx, 1032, "", true)--A pecsétet felnyitották
					d.notice(idx, 1033, "", true)--Hamarosan feljutsz a kilencedik emeletre.
					server_timer("underwater_warp", 2, idx)
				end
			end
		end

		when 2005.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 670000 and idx < 680000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 11 then
					if number(1, 100) > 95 then -- 5%
						d.setf(idx, "floor", 12)
						game.drop_typed_item(3, 678, 0, 1)
					end
				end
			end
		end

		when "catalog.10.zin_sa_gui_tower_map_cbbc4093c677".typed_use with pc.in_dungeon() and pc.get_map_index() >= 670000 and pc.get_map_index() < 680000 begin
			local idx = pc.get_map_index()
			if idx >= 670000 and idx < 680000 then
				if d.getf(idx, "floor") == 10 then
					if not pc.typed_dungeon_item_redeem({}, 0) then return end
					d.setf(idx, "floor", 11)
					d.typed_item_cleanup("catalog.10.zin_sa_gui_tower_map_cbbc4093c677")
					d.spawn_mob(idx, 20362, 397, 192, 60)
					d.set_regen_file(idx, "data/dungeon/water/regen8.json")
					d.notice(idx, 1029, "", true)--Megtalálta a helyes térképet,
					d.notice(idx, 1030, "", true)--Rövidesen feljut a 8. emeletre.
					server_timer("underwater_warp", 2, idx)
				end
			end
		end

		when 3965.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 670000 and idx < 680000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 9 then
					d.setf(idx, "floor", 10)
					game.drop_typed_item(10, 901, 0, 1)
				end
			end
		end

		when 395.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 670000 and idx < 680000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 8 then
					local c = d.getf(idx, "count") + 1
					if c == 4 then
						d.setf(idx, "floor", 9)
						d.notice(idx, 1028, "", true)--Keresd meg a térképet, hogy tovább mehess
						d.spawn_mob(idx, 3965, 619, 179)
					else
						d.setf(idx, "count", c)
					end
				end
			end
		end

		when 20090.chat."To Next floor!" with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 670000 and idx < 680000 then
				if d.getf(idx, "floor") == 7 then
				d.setf(idx, "floor", 8)
				local lang = pc.get_language()

				say_title(string.format("%s:", mob_name(npc.get_race())))
				say("Thank u my hero*-*")
				say("Szopjambe neked*-*")
				-- say(gameforge[lang].underwater_zone._1)
				-- say(gameforge[lang].underwater_zone._2)
				-- say(gameforge[lang].underwater_zone._3)
				wait()
				setskin(NOWINDOW)
				d.setf(idx, "count", 0)
				d.spawn_mob(idx, 395, 642, 195)
				d.spawn_mob(idx, 395, 626, 151)
				d.spawn_mob(idx, 395, 587, 181)
				d.spawn_mob(idx, 395, 615, 207)
				d.notice(idx, 1022, "", true)--Hamarosan a következo emeletre teleportálnak.
				server_timer("underwater_warp", 2, idx)
				npc.purge()
				end
			end
		end

		when 3956.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 670000 and idx < 680000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 6 then
					d.setf(idx, "floor", 7)
					d.notice(idx, 1026, "", true)--Az NPC megjelent, beszélj vele hogy engedjen tovább.
					d.spawn_mob(idx, 20090, 387, 426)
				end
			end
		end

		when "catalog.3.key_stone_b7ab7bf48f31".typed_use with pc.in_dungeon() and pc.get_map_index() >= 670000 and pc.get_map_index() < 680000 begin
			local idx = pc.get_map_index()
			if idx >= 670000 and idx < 680000 then
				if d.getf(idx, "floor") == 5 then
					if not d.typed_item_target(9425) then return end
					if not pc.typed_dungeon_item_redeem({}, 0) then return end
					npc.purge()

					local c = d.getf(idx, "count2") + 1
					if c == 5 then
						d.setf(idx, "floor", 6)
						d.clear_regen(idx)
						d.kill_all(idx)
						d.spawn_mob(idx, 3956, 387, 426)

						d.typed_item_cleanup("catalog.3.key_stone_b7ab7bf48f31")

						d.notice(idx, 1021, "", true)--Minden pecsétet felnyitottak.
						d.notice(idx, 1022, "", true)--Hamarosan a következo emeletre teleportálnak.
						server_timer("underwater_warp", 2, idx)
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

		when 2001.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 670000 and idx < 680000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 5 then
					local c = d.getf(idx, "count1") + 1
					if c == 8 then
						d.setf(idx, "count1", 0)
						game.drop_typed_item(3, 695, 0, 1)
					else
						d.setf(idx, "count1", c)
					end
				end
			end
		end

		when 394.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 670000 and idx < 680000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 4 then
					if d.getf(idx, "unique_vid") == npc.get_vid() then
						d.setf(idx, "floor", 5)
						d.setf(idx, "count1", 0)
						d.setf(idx, "count2", 0)
						d.clear_regen(idx)
						d.kill_all(idx)
						d.spawn_mob(idx, 9425, 398, 191)
						d.spawn_mob(idx, 9425, 427, 220)
						d.spawn_mob(idx, 9425, 370, 165)
						d.spawn_mob(idx, 9425, 427, 164)
						d.spawn_mob(idx, 9425, 368, 222)
						d.set_regen_file(idx, "data/dungeon/water/regen5.json")
						d.notice(idx, 1015, "", true)--A megfelelo metin elpusztult, hamarosan a következo emeletre teleportálnak.
						server_timer("underwater_warp", 2, idx)
					else
						d.notice(idx, 1016, "", true)--Egy hamis metin eltunik...
					end
				end
			end
		end

		when 3691.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 670000 and idx < 680000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 3 then
					d.setf(idx, "floor", 4)

					local stonepos = {
								[1] = {["x"] = 190, ["y"] = 430},
								[2] = {["x"] = 188, ["y"] = 382},
								[3] = {["x"] = 145, ["y"] = 388},
								[4] = {["x"] = 120, ["y"] = 426}
					}

					local random_num = number(1, table.getn(stonepos))
					for index, position in ipairs(stonepos) do
						local vid = d.spawn_mob(idx, 394, position["x"], position["y"])
						if random_num == index then
							d.setf(idx, "unique_vid", vid)
						end
					end

					d.notice(idx, 1013, "", true)--Pusztítsd el a megfelelo metint, hogy tovább menj.
				end
			end
		end

		when 3690.kill with pc.in_dungeon() begin
			local idx = pc.get_map_index()
			if idx >= 670000 and idx < 680000 then
				if d.getf(idx, "was_completed") != 0 then return end
				if d.getf(idx, "floor") == 2 then
					d.kill_all(idx)
					d.setf(idx, "floor", 3)
					d.spawn_mob(idx, 3691, 164, 420)
					d.notice(idx, 1097, "", true)--Hamarosan a következo emeletre jutsz.
					server_timer("underwater_warp", 2, idx)
				end
			end
		end

		when underwater_warp.server_timer begin
			local arg = get_server_timer_arg()
			if d.find(arg) then
				clear_server_timer("underwater_warp", arg)
				local f = d.getf(arg, "floor")
				if f == 2 then
					d.jump_all(arg, 4989, 884)
				elseif f == 3 then
					d.jump_all(arg, 5025, 703)
				elseif f == 5 then
					d.jump_all(arg, 5243, 435)
					d.notice(arg, 1017, "", true)--Nyissd ki az összes pecsétet, hogy tovább mehess.
				elseif f == 6 then
					d.jump_all(arg, 5235, 670)
					d.notice(arg, 2095, "", true)--Öld meg a BOSS-t hogy az NPC kiszabaduljon.
				elseif f == 8 then
					d.jump_all(arg, 5494, 456)
				elseif f == 11 then
					d.jump_all(arg, 5289, 462)
					d.notice(arg, 1031, "", true)--Keresd meg a megfelelo kulcsot az emelethez.
				elseif f == 13 then
					d.jump_all(arg, 5064, 456)
					d.notice(arg, 2096, "", true)
				end
			else
				underwater_zone.clear(arg)
			end
		end

		when underwater_checker.server_timer begin
			local arg = get_server_timer_arg()
			if d.find(arg) then
				local f = d.getf(arg, "floor")
				if f == 1 then
					if d.count_monster(arg) == 0 then
						clear_server_timer("underwater_checker", arg)
						d.setf(arg, "floor", 2)
						d.spawn_mob(arg, 3690, 159, 653)
						d.notice(arg, 1097, "", true)--Hamarosan a következo emeletre jutsz.
						server_timer("underwater_warp", 2, arg)
					end
				end
			else
				underwater_zone.clear(arg)
			end
		end

		when underwater_prepare.server_timer begin
			local arg = get_server_timer_arg()
			if d.find(arg) then
				d.regen_file(arg, "data/dungeon/water/regen2.json")
				server_timer("underwater_end", 2999, arg)
				server_loop_timer("underwater_checker", 2, arg)
				d.notice(arg, 1128, string.format("%d", 50), true)--Mostantól kezdve %s percetek lesz arra, hogy befejezzétek a várbörtönt.
				d.notice(arg, 930, "", true)--	Öld meg az emelet összes szörnyét!
			else
				underwater_zone.clear(arg)
			end
		end

		when logout begin
			local idx = pc.get_map_index()
			if idx >= 670000 and idx < 680000 then
				pc.setf("underwater_zone", "disconnect", get_global_time() + 300)
			end
		end

		when login begin
			local idx = pc.get_map_index()
			if idx == 67
			then
				pc.warp(535279, 1428280)
			elseif idx >= 670000 and idx < 680000 then
				pc.set_warp_location(219, 5352, 14282)
				pc.setf("underwater_zone", "idx", idx)
				pc.setf("underwater_zone", "ch", pc.get_channel_id())
				if d.getf(idx, "floor") == 0 then
					d.setf(idx, "floor", 1)
					server_timer("underwater_prepare", 1, idx)
					d.setf(idx, "was_completed", 0)
				end
			end
		end

		when 20013.chat."UnderWater Run" begin
			local lang = pc.get_language()

			say_title(string.format("%s:", mob_name(20013)))
			say("")
			say(string.format(gameforge[lang].common.dungeon_1, pc.get_name()))
			-- local mapIdx = pc.get_map_index()
			-- if mapIdx != 219 then
				-- return
			-- end

			say(gameforge[lang].common.dungeon_2)
			local agree = select(gameforge[lang].common.yes, gameforge[lang].common.no)
			say_title(string.format("%s:", mob_name(20013)))
			say("")
			if agree == 2 then
				say(gameforge[lang].common.dungeon_3)
				return
			end

			local goahead = 1
			local rejoinTIME = pc.getf("underwater_zone", "disconnect") - get_global_time()
			if rejoinTIME > 0 then
				local rejoinIDX = pc.getf("underwater_zone", "idx")
				if rejoinIDX > 0 then
					local rejoinCH = pc.getf("underwater_zone", "ch")
					if rejoinCH != 0 and rejoinCH != pc.get_channel_id() then
						say(string.format(gameforge[lang].common.dungeon_26, rejoinCH))
						return
					end

					if rejoinCH != 0 and d.find(rejoinIDX) then
						if d.getf(rejoinIDX, "was_completed") == 0 then
							say(gameforge[lang].common.dungeon_4)
							local agree2 = select(gameforge[lang].common.yes, gameforge[lang].common.no)
							if agree2 == 2 then
								say_title(string.format("%s:", mob_name(20013)))
								say("")
								say(gameforge[lang].common.dungeon_3)
								return
							end

							local f = d.getf(rejoinIDX, "floor")
							if f == 1 then
								goahead = 0
								pc.warp(486400,	25600, rejoinIDX)
							elseif f == 2 then
								goahead = 0
								pc.warp(502500, 70300, rejoinIDX)
							elseif f == 3 or f == 4 then
								goahead = 0
								pc.warp(524300, 43500, rejoinIDX)
							elseif f == 5 then
								goahead = 0
								pc.warp(523500, 67000, rejoinIDX)
							elseif f == 6 or f == 7 then
								goahead = 0
								pc.warp(546900, 68700, rejoinIDX)
							elseif f == 8 or f == 9 or f == 10 then
								goahead = 0
								pc.warp(549400, 45600, rejoinIDX)
							elseif f == 11 or f == 12 then
								goahead = 0
								pc.warp(528900, 46200, rejoinIDX)
							elseif f == 13 then
								goahead = 0
								pc.warp(506400, 45600, rejoinIDX)
							end
						end
					end
				end
			end

			if goahead == 1 then
				pc.setf("underwater_zone", "disconnect", 0)
				pc.setf("underwater_zone", "idx", 0)
				pc.setf("underwater_zone", "ch", 0)

				say(gameforge[lang].common.dungeon_5)
				say_reward(string.format(gameforge[lang].common.dungeon_6, 140))
				say_reward(string.format(gameforge[lang].common.dungeon_7, 140))
				--say_reward(string.format("- %s: %d", item_name(10951), 1))
				if party.is_party() then
					say_reward(gameforge[lang].common.dungeon_8)
				end
				say(gameforge[lang].common.dungeon_9)
				local n = select(gameforge[lang].common.yes, gameforge[lang].common.no)
				say_title(string.format("%s:", mob_name(20013)))
				say("")
				if n == 2 then
					say(gameforge[lang].common.dungeon_3)
					return
				end

				local flag = string.format("ww_67_%d", pc.get_channel_id())
				local ww_flag = game.get_event_flag(flag) - get_global_time()
				if ww_flag > 0 then
					say(gameforge[lang].common.dungeon_28)
					say(string.format(gameforge[lang].common.dungeon_29, ww_flag))
					return
				end

				myname = pc.get_name()
				result, cooldown, name = d.typed_check_entrance()
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
					-- if myname == name then
						-- say(string.format(gameforge[lang].common.dungeon_18, 1, item_name(0)))
					-- else
						-- say(string.format(gameforge[lang].common.dungeon_17, name, 1, item_name(0)))
					-- end
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
					if not d.typed_enter() then
						say(gameforge[lang].common.unknownerr)
					end
				end
			end
		end
	end
end
