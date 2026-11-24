quest events_mgr begin
	state start begin
		-- -- christmas event
		-- when 20031.chat."Ho ho ho!" with game.get_event_flag("christmas_event") == 1 begin
			-- local lang = pc.get_language()
			
			-- say_title(string.format("%s:", mob_name(20031)))
			-- say("")
			-- say(string.format(gameforge[lang].events_mgr._19, pc.get_name()))
			-- local menu = select(gameforge[lang].events_mgr._20, gameforge[lang].common.close)
			-- if menu == 1 then
				-- npc.open_shop(71)
				-- setskin(NOWINDOW)
			-- -- elseif menu == 2 then
				-- -- command("cube open")
				-- -- setskin(NOWINDOW)
			-- -- elseif menu == 3 then
				-- -- cmdchat("BINARY_WHEEL_ASKOPEN")
				-- -- setskin(NOWINDOW)
			-- end
		-- end
        -- when 20031.chat."Mutasd az idot!" begin
            -- local time = os.date("*t")
            -- say_title("Szerver ido")
            -- say(string.format("Mostani szerver ido:\\n%04d-%02d-%02d %02d:%02d:%02d",
                -- time.year, time.month, time.day,
                -- time.hour, time.min, time.sec))
        -- end
		-- when 20031.chat."get_channel_id!" begin
			-- local ch = get_channel_id()
			-- say_title(string.format("Channel ID: %d", ch))
			-- say("")
		-- end
		-- -- end christmas event

		-- ox event
		when 20011.chat."Shop" begin
			npc.open_shop(68)
			setskin(NOWINDOW)
		end
		-- when kill with not npc.is_pc() and game.get_event_flag("christmas_event") == 1 begin
			-- local talan = math.random(1,250)
			-- if talan == 6 then
				-- game.drop_item_with_ownership(78201, 1)
			-- end
			-- if talan == 7 then
				-- game.drop_item_with_ownership(78202, 1)
			-- end
			-- if talan == 8 then
				-- game.drop_item_with_ownership(78203, 1)
			-- end
			-- if talan == 9 then
				-- game.drop_item_with_ownership(78204, 1)
			-- end
			-- if talan == 10 then
				-- game.drop_item_with_ownership(78205, 1)
			-- end
			-- if talan == 11 then
				-- game.drop_item_with_ownership(78206, 1)
			-- end
		-- end

		when 20011.chat."OX-verseny" begin
			local lang = pc.get_language()
			local stat = game.get_event_flag("oxevent_status")
			say_title(string.format("%s:", mob_name(20011))) 
			say("")
			say(gameforge[lang].events_mgr._2)
			say(gameforge[lang].events_mgr._3)
			say(gameforge[lang].events_mgr._4)
			say("")
			if stat == 0 then 
				say(gameforge[lang].events_mgr._5)
				say(gameforge[lang].events_mgr._6)
			elseif stat == 1 then 
				say(gameforge[lang].events_mgr._7)
				say(gameforge[lang].events_mgr._8)
				say("")
				local what = select(gameforge[lang].events_mgr._9, gameforge[lang].events_mgr._10, gameforge[lang].events_mgr._11) 
				if what == 1 then 
					say_title(string.format("%s:", mob_name(20011))) 
					say("")
					say(gameforge[lang].events_mgr._12)
					say("")
					wait() 
					pc.warp(896500, 24600) 
				elseif what == 2 then 
					say_title(string.format("%s:", mob_name(20011))) 
					say("")
					say(gameforge[lang].events_mgr._13)
					say(gameforge[lang].events_mgr._14)
					say("")
					wait()
					pc.warp(896300, 28900) 
				end 
			elseif stat == 2 then
				say(gameforge[lang].events_mgr._15)
				say(gameforge[lang].events_mgr._16)
				say(gameforge[lang].events_mgr._17)
				local agree = select(gameforge[lang].common.yes, gameforge[lang].common.no) 
				if agree == 1 then 
					say_title(string.format("%s:", mob_name(20011))) 
					say("")
					say(gameforge[lang].events_mgr._18)
					say("")
					wait()
					pc.warp(896300, 28900)
				end
			end 
		end
		-- end ox event

		-- dungeon bonus
		-- when 5001.kill begin
			-- game.drop_item_with_ownership(89001, 1)
		-- end

		-- when 5002.kill begin
			-- game.drop_item_with_ownership(89002, 1)
		-- end

		-- when 5003.kill begin
			-- game.drop_item_with_ownership(89003, 1)
		-- end
		-- end dungeon bonus

		when login begin
			local lang = pc.get_language()
			-- dungeon bonus
			local bonus = 10 + game.get_event_flag("dungeon_bonus")
			if bonus > 100 then
				bonus = 100
			end
			
			notice(string.format(gameforge[lang].events_mgr._1, bonus))
			-- end dungeon bonus
		end

		when "catalog.10.helper_scroll_8488f792c8d2".typed_use begin
			local rewards = {}
			if pc.get_sex() == 0 then
				table.insert(rewards, {4, 1483, 0, 1}) -- Szellem Kosztum (Ferfi)
			else
				table.insert(rewards, {4, 1482, 0, 1}) -- Szellem Kosztum (Noi)
			end

			local weapons = {
				[0] = {{4, 1476, 0, 1}, {4, 1479, 0, 1}}, -- Kard, pallos
				[1] = {{4, 1476, 0, 1}, {4, 1477, 0, 1}, {4, 1478, 0, 1}}, -- Kard, tor, ij
				[2] = {{4, 1476, 0, 1}}, -- Kard
				[3] = {{4, 1480, 0, 1}, {4, 1481, 0, 1}}, -- Harang, legyezo
			}
			for _, weapon in ipairs(weapons[pc.get_job()] or {}) do
				table.insert(rewards, weapon)
			end
			table.insert(rewards, {19, 206, 0, 1}) -- Szellem Szarny (vegtelen)
			local ok, reason = pc.typed_item_redeem(rewards, 0)
			if not ok then
				if reason == "inventory_full" then
					syschat("Nincs eleg szabad hely a Szellem csomaghoz.")
				else
					syschat("A Szellem csomagot most nem lehet atvenni.")
				end
			end
		end



			------------------------------------------------------------------
			when 20023.chat.":EVENTEK!" with pc.is_gm() begin

				say_title("GM panel:")
				say("")
				say("Válaszd ki, mit szeretnél csinálni!")
				say("")

				local e = select("Arany Béka Spawn (371 db)", "Tanaka spawn (371 db)", "Mégse")
				if e == 3 then return end

				-- spawnolt mobok számlálója
				--game.set_event_flag("event_spawn_left", 371)

				local startX = 81
				local endX   = 450
				local startY = 66
				local endY   = 450

				-- GM hely, ahova nem spawnolunk
				local gm_x1, gm_y1 = 249, 233
				local gm_x2, gm_y2 = 298, 278

				local mob_vnum = 124     -- Arany Béka
				if e == 2 then
					mob_vnum = 5000      -- Tanaka
				end

				local count = 0

				for x = startX, endX, 20 do
					for y = startY, endY, 20 do

						local in_gm_zone =
							x >= gm_x1 and x <= gm_x2 and
							y >= gm_y1 and y <= gm_y2

						if not in_gm_zone and count < 371 then
							mob.spawn(mob_vnum, x, y, 1, 1, 1)
							count = count + 1
						end
					end
				end

				if mob_vnum == 124 then
					big_notice_in_map("[Event] 400 Golden Frogs have been spawned!")
					big_notice_in_map("No new spawns will occur until you kill the last one!")
				else
					big_notice_in_map("[Event] 400 Tanakas have been spawned!")
					big_notice_in_map("No new spawns will occur until you kill the last one!")
				end
			end



			------------------------------------------------------------------
			-- KILL HANDLER – Ha az összes spawnolt mob meghalt
			------------------------------------------------------------------
			-- when 125.kill or 5000.kill begin

				-- local left = game.get_event_flag("event_spawn_left")

				-- if left > 0 then
					-- left = left - 1
					-- game.set_event_flag("event_spawn_left", left)
				-- end

				-- if left == 0 then
					-- big_notice_in_map(" Eventmap clean! ")
				-- end
			-- end



		when 20023.chat.":RAZOR:EZT HAGYJÁTOK!" with pc.is_gm() begin
			say_title("Event Manager:")
			say("")
			say("Scegli un evento:")
			local events = select("Dungeon Boss Bonus", " OX", "Chiudi")
			if events == 1 then
				local s = game.get_event_flag("dungeon_bonus")
				say_title("Event Manager:")
				say("")
				if s != 0 then
					say("Az esemény jelenleg aktív.")
					say("")
					say(string.format("Valószínuség: +%d", s))
					say("")
					say("Cosa vuoi fare?")
					local what = select("Deaktiválás", "Változás valószínusége", "Chiudi")
					if what == 1 then
						say_title("Event Manager:")
						say("")
						say("Biztosan deaktiválja az eseményt?")
						local agree = select("Si", "No")
						if agree == 2 then
							return
						end
						
						say_title("Event Manager:")
						say("")
						say("Az eseményt deaktiválták.")
						game.set_event_flag("dungeon_bonus", 0)
						notice_all(1295, "10")
					elseif what == 2 then
						say_title("Event Manager:")
						say("")
						say("Add meg a dungeon bonusz valoszinuseget")
						say(" ")
						say("min: 1")
						say("max: 90")
						local prob = input()
						say_title("Event Manager:")
						say("")
						if prob == nil or prob == "" then
							say("Az érték érvénytelen.")
							return
						end
						
						prob = tonumber(prob)
						if prob < 1 then
							say("A valószínuség kisebb, mint 1.")
						elseif prob > 90 then
							say("A valószínuség nagyobb, mint 90.")
						end
						say(string.format("Beírtad a valószínuséget: %d", prob))
						say("")
						say("Biztosan folytatja?")
						local agree = select("Si", "No")
						if agree == 2 then
							return
						end
						
						say_title("Event Manager:")
						say("")
						say(string.format("A valószínuség erre módosult %d.", prob))
						game.set_event_flag("dungeon_bonus", prob)
						notice_all(1295, string.format("%d", prob + 10))
					end
				else
					say("Az esemény jelenleg deaktiválva van.")
					say("")
					say("Szeretné aktiválni?")
					say("")
					local agree = select("Si", "No")
					if agree == 2 then
						return
					end
					
					say_title("Event Manager:")
					say("")
					say("add meg a borton bonusz valoszinuseget ")
					say(" .")
					say("min: 1")
					say("max: 90")
					local prob = input()
					say_title("Event Manager:")
					say("")
					if prob == nil or prob == "" then
						say("Az érték érvénytelen.")
						return
					end
					
					prob = tonumber(prob)
					if prob < 1 then
						say("kissebb mint 1.")
					elseif prob > 90 then
						say("nagyobb mint 90.")
					end
					say(string.format("Beírtad a valószínuséget: %d", prob))
					say("")
					say("Biztosan folytatja?")
					local agree = select("Si", "No")
					if agree == 2 then
						return
					end
					
					say_title("Event Manager:")
					say("")
					say("Az esemény elindult.")
					game.set_event_flag("dungeon_bonus", prob)
					notice_all(1295, string.format("%d", prob + 10))
				end
			elseif events == 2 then
				say_title("Event Manager:")
				say("")
				if pc.get_map_index() != 113 then
					say("ox mappon kell lenned")
					say("/go ox.")
					say("vagy")
					say(" /warp 8963 289")
					return
				end
				
				local s = oxevent.get_status()
				if s == 0 then
					say("mit akarsz csinálni?")
					local what = select("Aktiválja", "vissza")
					if what == 1 then
						say_title("Event Manager:")
						say("")
						local res = oxevent.open()
						if res == 1 then
							say("A kérdések betöltése sikeresen befejezodött.") 
							say("A felhasználók most már csatlakozhatnak az eseményhez.")
							notice_all(912, "") 
							notice_all(913, "") 
						else
							say("A kérdések betöltése nem sikerült.")
							say("Ellenorizze a kérdésfájlt.") 
						end
					end
				elseif s == 1 then
					say(string.format("Résztvevok száma: %d.", oxevent.get_attender()))
					say("")
					say("mit akarsz csinálni?")
					local what = select("belépés lezarasa", "Chiudi")
					if what == 1 then
						oxevent.close()
						say_title("Event Manager:")
						say("")
						say("Az OX versenyre való nevezés letiltva.")
						notice_all(914, "")
						notice_all(915, "")
					end
				elseif s == 2 then
					say("Az esemény elkezdodött.")
					say("")
					say("mit akarsz csinálni?")
					local what = select("Tegyen fel kérdést", "Nyeremény kiosztása", "event vege", "Chiudi")
					if what == 1 then
						say_title("Event Manager:")
						say("")
						local res = oxevent.quiz(1, 30)
						if res == 1 then 
							say("A kérdések sikeresen betöltve.")
						else
							say("A kérdések betöltése nem sikerült.")
						end
					elseif what == 2 then
						say_title("OX jutalom")
						say("Add meg a targy kataloguskodjat.")
						local code = input()
						if not code or not string.match(code, "%S") then return end
						code = string.match(code, "^%s*(.-)%s*$")
						say_title("OX jutalom")
						say("Darabszam jatekosonkent:")
						local count = tonumber(input() or "")
						if not count or count < 1 or count > 4294967295 or count ~= math.floor(count) then
							say("Ervenytelen darabszam.")
							return
						end
						say_title("OX jutalom")
						say(string.format("%s, %d db jatekosonkent", code, count))
						if select("Kiosztas", "Megse") == 1 then
							if oxevent.give_typed_item(code, 0, count) then
								say("A jutalom mentese elindult.")
							else
								say("A kiosztas nem indithato. Ellenorizd a kodot es az esemeny allapotat.")
							end
						end
					elseif what == 3 then
						notice_all(917, "")
						oxevent.end_event()
						say_title("Event Manager:")
						say("")
						say(" OX vége.")
					end
				else
					say("Kérdés van folyamatban, kérjük, várjon...") 
				end
			end
		end
	end
end

