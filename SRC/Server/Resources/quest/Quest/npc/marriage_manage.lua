quest marriage_manage begin
	state start begin
		function is_equip_wedding_dress()
			return marriage.typed_attire()
		end

		function wedding_attire_name(job, fallback)
			local attire = {[0] = 193, [1] = 195, [2] = 194, [3] = 196}
			if not attire[job] then return fallback end
			return game.typed_item_name(2, attire[job], fallback)
		end

		when 9006.click begin
			say_title(string.format("%s:", mob_name(npc.get_race())))
			say("")
			local lang = pc.get_language()
			say(gameforge[lang].common.choose)
			local s = select(gameforge[lang].marriage_manage._1, gameforge[lang].marriage_manage._2, gameforge[lang].marriage_manage._3, gameforge[lang].marriage_manage._178, gameforge[lang].common.close)
			if s == 1 then
				say_title(string.format("%s:", mob_name(npc.get_race())))
				say("")
				if pc.is_engaged_or_married() then
					say(gameforge[lang].marriage_manage._4)
					return
				end

				if not npc.lock() then
					say(gameforge[lang].marriage_manage._5)
					return
				end

				if pc.level < 25 then
					say(gameforge[lang].marriage_manage._6)
					say(gameforge[lang].marriage_manage._7)
					say(gameforge[lang].marriage_manage._8)
					npc.unlock()
					return
				end

				local m_ring_num = (pc.typed_item_count(10, 1310, 0) or 0)
				local m_has_ring = m_ring_num > 0
				if not m_has_ring then
					say(gameforge[lang].marriage_manage._9)
					say(gameforge[lang].marriage_manage._10)
					say("")
					say(game.typed_item_name(10, 1310, gameforge[lang].marriage_manage._11))
					say(gameforge[lang].marriage_manage._12)
					npc.unlock()
					return
				end

				if not marriage_manage.is_equip_wedding_dress() then
					say(gameforge[lang].marriage_manage._13)
					say(gameforge[lang].marriage_manage._14)
					say(gameforge[lang].marriage_manage._15)
					say("")
					say(marriage_manage.wedding_attire_name(pc.get_job(), gameforge[lang].marriage_manage._16))
					say_reward(gameforge[lang].marriage_manage._17)

					npc.unlock()
					return
				end

				if pc.get_money() < 1000000 then
					say(gameforge[lang].marriage_manage._18)
					say(gameforge[lang].marriage_manage._19)
					say(gameforge[lang].marriage_manage._20)
					say(gameforge[lang].marriage_manage._21)
					npc.unlock()
					return
				end

				say(gameforge[lang].marriage_manage._22)
				say(gameforge[lang].marriage_manage._23)
				say("")
				say_reward(gameforge[lang].marriage_manage._24)
				local sname = input()
				say_title(string.format("%s:", mob_name(npc.get_race())))
				say("")
				if sname == "" then
					say(gameforge[lang].marriage_manage._25)
					say(gameforge[lang].marriage_manage._26)
					npc.unlock()
					return
				end

				local u_vid = find_pc_by_name(sname)
				local m_vid = pc.get_vid()
				if u_vid == 0 then
					say(gameforge[lang].marriage_manage._27)
					say("")
					say_reward(string.format(gameforge[lang].marriage_manage._28, sname))
					npc.unlock()
					return
				end

				if not npc.is_near_vid(u_vid , 10) then
					say(gameforge[lang].marriage_manage._29)
					say(gameforge[lang].marriage_manage._30)
					say(gameforge[lang].marriage_manage._31)
					say("")
					say_reward(string.format(gameforge[lang].marriage_manage._32, sname))
					npc.unlock()
					return
				end

				local old = pc.select(u_vid)
				local u_level = pc.get_level()
				local u_lang = pc.get_language()
				local u_job = pc.get_job()
				local u_name = pc.name
				local u_gold = pc.get_money()
				local u_married = pc.is_married()
				local u_has_ring = (pc.typed_item_count(10, 1310, 0) or 0) > 0
				local u_wear = marriage_manage.is_equip_wedding_dress()
				pc.select(old)
				local m_level = pc.get_level()
				if u_vid == m_vid then
					say(gameforge[lang].marriage_manage._33)
					say(gameforge[lang].marriage_manage._34)
					say(gameforge[lang].marriage_manage._35)
					say("")
					say_reward(gameforge[lang].marriage_manage._36)
					npc.unlock()
					return
				end

				if u_married then
					say(gameforge[lang].marriage_manage._37)
					say(gameforge[lang].marriage_manage._38)
					say(gameforge[lang].marriage_manage._39)
					say("")
					say_reward(string.format(gameforge[lang].marriage_manage._40, sname))
					npc.unlock()
					return
				end

				if u_level <= 25 then
					say(gameforge[lang].marriage_manage._41)
					say(gameforge[lang].marriage_manage._42)
					say(gameforge[lang].marriage_manage._43)
					say("")
					say_reward(gameforge[lang].marriage_manage._44)
					npc.unlock()
					return
				end

				if m_level - u_level > 15 or u_level - m_level > 15 then
					say(gameforge[lang].marriage_manage._45)
					say(gameforge[lang].marriage_manage._46)
					say(gameforge[lang].marriage_manage._47)
					say("")
					say_reward(gameforge[lang].marriage_manage._48)
					npc.unlock()
					return
				end

				if not u_has_ring then
					say(gameforge[lang].marriage_manage._49)
					say(gameforge[lang].marriage_manage._50)
					say(game.typed_item_name(10, 1310, gameforge[lang].marriage_manage._11))
					say_reward(gameforge[lang].marriage_manage._51)
					npc.unlock()
					return
				end

				if not u_wear then
					say(gameforge[lang].marriage_manage._52)
					say(gameforge[lang].marriage_manage._53)
					say(gameforge[lang].marriage_manage._54)
					say("")
					say(marriage_manage.wedding_attire_name(u_job, gameforge[lang].marriage_manage._16))
					say_reward(gameforge[lang].marriage_manage._17)

					npc.unlock()
					return
				end

				local ok_sign = confirm(u_vid, string.format(gameforge[u_lang].marriage_manage._55, pc.name), 30)
				if ok_sign == CONFIRM_OK then
					local m_name = pc.name
					if marriage.typed_engage(u_vid) then
						say(gameforge[lang].marriage_manage._60)
						say("")
						say(gameforge[lang].marriage_manage._61)
						say(gameforge[lang].marriage_manage._62)
						say(gameforge[lang].marriage_manage._63)
						say(gameforge[lang].marriage_manage._64)
						say("")
						say_reward(gameforge[lang].marriage_manage._65)
						select(gameforge[lang].common.wait)
						setskin(NOWINDOW)
					else
						say(gameforge[lang].common.unknownerr)
					end
				else
					say(gameforge[lang].marriage_manage._56)
					say(gameforge[lang].marriage_manage._57)
					say(gameforge[lang].marriage_manage._58)
					say("")
					say_reward(gameforge[lang].marriage_manage._59)
				end

				npc.unlock()
			elseif s == 2 then
				say_title(string.format("%s:", mob_name(npc.get_race())))
				say("")
				if not pc.is_engaged() then
					say(gameforge[lang].marriage_manage._4)
					return
				end

				say(gameforge[lang].marriage_manage._66)
				say(gameforge[lang].marriage_manage._67)
				say(gameforge[lang].marriage_manage._68)
				say("")
				say(gameforge[lang].marriage_manage._69)
				say(gameforge[lang].marriage_manage._70)
				select(gameforge[lang].common.wait)
				setskin(NOWINDOW)
				marriage.warp_to_my_marriage_map()
			elseif s == 3 then
				say_title(string.format("%s:", mob_name(npc.get_race())))
				say("")
				if pc.is_engaged() then
					say(gameforge[lang].marriage_manage._4)
					return
				end

				local t = marriage . get_wedding_list()
				if table . getn ( t)== 0 then
					say(gameforge[lang].marriage_manage._71)
				else
					local wedding_names = {}
					table.foreachi(t, function(n , p) wedding_names[n] = p[3].." - "..p[4].."!" end)
					wedding_names[table.getn(t) + 1] = gameforge[lang].common.confirm
					local s = select_table(wedding_names)
					if s != table.getn(wedding_names) then
						marriage.join_wedding(t[s][1], t[s][2])
					end
				end
			elseif s == 4 then
				setskin(NOWINDOW)
				npc.open_shop(6)
			else
				setskin(NOWINDOW)
			end
		end

		when 9011.click begin
			say_title(string.format("%s:", mob_name(npc.get_race())))
			say("")
			if not pc.is_engaged() and not pc.is_married() then
				say(gameforge[lang].marriage_manage._95)
				say(gameforge[lang].marriage_manage._96)
				return
			end

			local lang = pc.get_language()
			say(gameforge[lang].common.choose)
			local s = select(gameforge[lang].marriage_manage._72, gameforge[lang].marriage_manage._97, gameforge[lang].marriage_manage._98, gameforge[lang].marriage_manage._99, gameforge[lang].marriage_manage._100, gameforge[lang].marriage_manage._101, gameforge[lang].marriage_manage._102, gameforge[lang].common.close)
			if s == 1 then
				say_title(string.format("%s:", mob_name(npc.get_race())))
				say("")
				if not pc.is_engaged() and not marriage.in_my_wedding() then
					say(gameforge[lang].marriage_manage._4)
					return
				end

				if not npc.lock() then
					say(gameforge[lang].marriage_manage._73)
					say(gameforge[lang].marriage_manage._74)
					say(gameforge[lang].marriage_manage._75)
					return
				end

				say(gameforge[lang].marriage_manage._76)
				say(gameforge[lang].marriage_manage._77)
				say(gameforge[lang].marriage_manage._78)
				say("")
				local sname = input()
				local u_vid = find_pc_by_name(sname)
				local m_vid = pc.get_vid()

				say_title(string.format("%s:", mob_name(npc.get_race())))
				say("")
				if u_vid == 0 then
					say(gameforge[lang].marriage_manage._79)
					say(gameforge[lang].marriage_manage._80)
					say("")
					say_reward(string.format(gameforge[lang].marriage_manage._81, sname))
					npc.unlock()
					return
				end

				if not npc.is_near_vid(u_vid , 10) then
					say(gameforge[lang].marriage_manage._82)
					say(gameforge[lang].marriage_manage._83)
					say("")
					say_reward(string.format(gameforge[lang].marriage_manage._84, sname))
					npc.unlock()
					return
				end

				if u_vid == m_vid then
					say(gameforge[lang].marriage_manage._85)
					say("")
					say_reward(gameforge[lang].marriage_manage._86)
					npc.unlock()
					return
				end

				if u_vid != marriage.find_married_vid() then
					say(gameforge[lang].marriage_manage._87)
					say(gameforge[lang].marriage_manage._88)
					npc.unlock()
					return
				end

				local ok_sign = confirm(u_vid , string.format(gameforge[lang].marriage_manage._94, pc.name), 30)
				if ok_sign != CONFIRM_OK then
					say(gameforge[lang].marriage_manage._91)
					say(gameforge[lang].marriage_manage._92)
					say(gameforge[lang].marriage_manage._93)
					npc.unlock()
					return
				end

				say(gameforge[lang].marriage_manage._89)
				say("")
				marriage.set_to_marriage()
				say("")
				say_reward(gameforge[lang].marriage_manage._90)
				npc.unlock()
			elseif s == 2 then
				if not pc.is_engaged() and not marriage.in_my_wedding() then
					say_title(string.format("%s:", mob_name(npc.get_race())))
					say("")
					say(gameforge[lang].marriage_manage._4)
					return
				end

				marriage.wedding_music(true, "wedding.mp3")
				setskin(NOWINDOW)
			elseif s == 3 then
				if not pc.is_engaged() and not marriage.in_my_wedding() then
					say_title(string.format("%s:", mob_name(npc.get_race())))
					say("")
					say(gameforge[lang].marriage_manage._4)
					return
				end

				if marriage.wedding_is_playing_music() then
					marriage.wedding_music(false, "default")
				end

				setskin(NOWINDOW)
			elseif s == 4 then
				if not pc.is_engaged() and not marriage.in_my_wedding() then
					say_title(string.format("%s:", mob_name(npc.get_race())))
					say("")
					say(gameforge[lang].marriage_manage._4)
					return
				end

				marriage.wedding_dark(true)
				setskin(NOWINDOW)
			elseif s == 5 then
				if not pc.is_engaged() and not marriage.in_my_wedding() then
					say_title(string.format("%s:", mob_name(npc.get_race())))
					say("")
					say(gameforge[lang].marriage_manage._4)
					return
				end

				marriage.wedding_snow(true)
				setskin(NOWINDOW)
			elseif s == 6 then
				if not pc.is_engaged() and not marriage.in_my_wedding() then
					say_title(string.format("%s:", mob_name(npc.get_race())))
					say("")
					say(gameforge[lang].marriage_manage._4)
					return
				end

				marriage.wedding_snow(false)
				setskin(NOWINDOW)
			elseif s == 7 then
				say_title(string.format("%s:", mob_name(npc.get_race())))
				say("")
				if not pc.is_engaged() and not marriage.in_my_wedding() then
					say(gameforge[lang].marriage_manage._4)
					return
				end

				if not npc.lock() then
					say(gameforge[lang].marriage_manage._103)
					return
				end

				say(gameforge[lang].marriage_manage._104)
				say(gameforge[lang].marriage_manage._105)
				say("")
				say(gameforge[lang].marriage_manage._106)
				say(gameforge[lang].marriage_manage._107)
				say("")
				local r = select(gameforge[lang].common.yes, gameforge[lang].common.no)
				if r == 1 then
					local u_vid = marriage.find_married_vid()
					say_title(string.format("%s:", mob_name(npc.get_race())))
					say("")
					if u_vid == 0 then
						say(gameforge[lang].marriage_manage._108)
						say(gameforge[lang].marriage_manage._109)
						say(gameforge[lang].marriage_manage._110)
						say(gameforge[lang].marriage_manage._111)
						npc.unlock()
						return
					end

					say(gameforge[lang].marriage_manage._112)
					say(gameforge[lang].marriage_manage._113)
					say(gameforge[lang].marriage_manage._114)
					say(gameforge[lang].marriage_manage._115)
					local ok_sign = confirm (u_vid, gameforge[lang].marriage_manage._116, 30)
					if ok_sign == CONFIRM_OK then
						say(gameforge[lang].marriage_manage._119)
						select(gameforge[lang].common.close)
						marriage.end_wedding()
					else
						say(gameforge[lang].marriage_manage._117)
						say(gameforge[lang].marriage_manage._118)
					end
				end

				npc.unlock()
			else
				setskin(NOWINDOW)
			end
		end

		function check_divorce_time()
			local DIVORCE_LIMIT_TIME = 86400
			if marriage.get_married_time() < 86400 then
				say_title ( "Aiutante Matrimonio:" )
				say("" )
				say("Hai divorziato da poco." )
				say("" )
				say("Per sposarti o divorziare" )
				say("nuovamente devi aspettare." )
				say("" )
				return false
			end
			return true
		end

		when 11000.chat."Agreed Divorce" or 11002.chat."Agreed Divorce" or 11004.chat."Agreed Divorce" with pc.is_married() begin
			say_title(string.format("%s:", mob_name(npc.get_race())))
			say("")
			local lang = pc.get_language()
			if marriage.get_married_time() < 86400 then
				say(gameforge[lang].marriage_manage._120)
				say(gameforge[lang].marriage_manage._121)
				return
			end

			local u_vid = marriage.find_married_vid()
			if u_vid == 0 or not npc.is_near_vid (u_vid, 10)then
				say(gameforge[lang].marriage_manage._122)
				say(gameforge[lang].marriage_manage._123)
				return
			end

			say(gameforge[lang].marriage_manage._124)
			say(gameforge[lang].marriage_manage._125)
			say("")
			say(gameforge[lang].marriage_manage._126)
			say(gameforge[lang].marriage_manage._127)
			say("")
			local s = select (gameforge[lang].marriage_manage._128, gameforge[lang].marriage_manage._129)
			if s == 1 then
				say_title(string.format("%s:", mob_name(npc.get_race())))
				say("")
				local m_enough_money = pc.get_gold() > 500000
				local m_have_ring = (pc.typed_item_count(10, 1587, 0) or 0) > 0
				local old = pc.select(u_vid)
				local u_enough_money = pc.get_gold() > 500000
				local u_have_ring = (pc.typed_item_count(10, 1587, 0) or 0) > 0
				pc.select(old)

				if not m_have_ring then
					say(gameforge[lang].marriage_manage._130)
					say(game.typed_item_name(10, 1587, gameforge[lang].marriage_manage._131))
					say(gameforge[lang].marriage_manage._132)
					return
				end

				if not u_have_ring then
					say(gameforge[lang].marriage_manage._133)
					say(game.typed_item_name(10, 1587, gameforge[lang].marriage_manage._131))
					say(gameforge[lang].marriage_manage._132)
					return
				end

				if not m_enough_money then
					say(gameforge[lang].marriage_manage._134)
					say(gameforge[lang].marriage_manage._135)
					say(gameforge[lang].marriage_manage._136)
					return
				end

				if not u_enough_money then
					say(gameforge[lang].marriage_manage._134)
					say(gameforge[lang].marriage_manage._137)
					say(gameforge[lang].marriage_manage._136)
					return
				end

				say(gameforge[lang].marriage_manage._138)
				say(gameforge[lang].marriage_manage._139)
				say(gameforge[lang].marriage_manage._140)
				say(gameforge[lang].marriage_manage._141)
				say("")
				local c = select(gameforge[lang].marriage_manage._142, gameforge[lang].marriage_manage._143)
				say_title(string.format("%s:", mob_name(npc.get_race())))
				say("")
				if 2 == c then
					say(gameforge[lang].marriage_manage._144)
					say(gameforge[lang].marriage_manage._145)
					say(gameforge[lang].marriage_manage._146)
					return
				end

				local ok_sign = confirm(u_vid , string.format(gameforge[lang].marriage_manage._147, pc.name), 30)
				if ok_sign == CONFIRM_OK then
					local m_enough_money = pc.get_gold() > 500000
					local m_have_ring = (pc.typed_item_count(10, 1587, 0) or 0) > 0
					local old = pc.select(u_vid)
					local u_enough_money = pc.get_gold() > 500000
					local u_have_ring = (pc.typed_item_count(10, 1587, 0) or 0) > 0
					pc.select(old)
					if m_have_ring and m_enough_money and u_have_ring and u_enough_money and marriage.typed_mutual_divorce(u_vid) then
						say(gameforge[lang].marriage_manage._148)
						say(gameforge[lang].marriage_manage._149)
						say(gameforge[lang].marriage_manage._150)
						say("")
						say_reward(gameforge[lang].marriage_manage._150)
					else
						say(gameforge[lang].marriage_manage._152)
						say(gameforge[lang].marriage_manage._153)
					end
				else
					say(gameforge[lang].marriage_manage._154)
					say(gameforge[lang].marriage_manage._155)
					say(gameforge[lang].marriage_manage._156)
				end
			end
		end

		when 11000.chat."Unilateral Divorce" or 11002.chat."Unilateral Divorce" or 11004.chat."Unilateral Divorce" with pc.is_married() begin
			say_title(string.format("%s:", mob_name(npc.get_race())))
			say("")
			local lang = pc.get_language()
			if marriage.get_married_time() < 86400 then
				say(gameforge[lang].marriage_manage._120)
				say(gameforge[lang].marriage_manage._121)
				return
			end

			say(gameforge[lang].marriage_manage._157)
			say(gameforge[lang].marriage_manage._158)
			say(gameforge[lang].marriage_manage._159)
			say("")
			say(gameforge[lang].marriage_manage._160)
			say("")
			local s = select (gameforge[lang].marriage_manage._161, gameforge[lang].marriage_manage._162)
			if s == 2 then
				return
			end

			say_title(string.format("%s:", mob_name(npc.get_race())))
			say("")
			if pc.get_gold() < 1000000 then
				say(gameforge[lang].marriage_manage._163)
				say(gameforge[lang].marriage_manage._164)
				say(gameforge[lang].marriage_manage._165)
				return
			end

			say(gameforge[lang].marriage_manage._166)
			say(gameforge[lang].marriage_manage._167)
			local c = select(gameforge[lang].common.yes, gameforge[lang].common.no)
			say_title(string.format("%s:", mob_name(npc.get_race())))
			say("")
			if c == 2 then
				say(gameforge[lang].marriage_manage._168)
				say("")
				say(gameforge[lang].marriage_manage._169)
				say(gameforge[lang].marriage_manage._170)
				say(gameforge[lang].marriage_manage._171)
				say("")
				say_reward(gameforge[lang].marriage_manage._172)
				return
			end

			if not marriage.typed_unilateral_divorce() then
				say(gameforge[lang].common.unknownerr)
				return
			end
			say(gameforge[lang].marriage_manage._173)
			say(gameforge[lang].marriage_manage._174)
			say(gameforge[lang].marriage_manage._175)
			say(gameforge[lang].marriage_manage._176)
			say("")
			say_reward(gameforge[lang].marriage_manage._177)
		end
	end
end
