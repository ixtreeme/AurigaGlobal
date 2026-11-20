quest alter_of_power begin
	state start begin
		when 20077.click with npc.get_guild() == pc.get_guild() and pc.is_guild_master() begin
			say_title(string.format("%s:", mob_name(npc.get_race())))
			say("")
			local lang = pc.get_language()
			say(gameforge[lang].alter_of_power._1)
			say(gameforge[lang].alter_of_power._2)
			say(gameforge[lang].alter_of_power._3)
			say(gameforge[lang].alter_of_power._4)
			say(gameforge[lang].alter_of_power._5)
			say(gameforge[lang].alter_of_power._6)
			if pc.getqf("build_level") == 0 then
				pc.setqf("build_level", guild.level(pc.get_guild()))
			end

			select(gameforge[lang].common.wait)
			say_title(string.format("%s:", mob_name(npc.get_race())))
			say("")
			say(gameforge[lang].alter_of_power._7)
			if pc.getqf("build_level") < guild.level(pc.get_guild()) or guild.level(pc.get_guild()) >= 20 then
				say(gameforge[lang].alter_of_power._7)
				say(gameforge[lang].alter_of_power._8)
				say(gameforge[lang].alter_of_power._9)
				say(gameforge[lang].alter_of_power._10)
				say(gameforge[lang].alter_of_power._11)
				say(gameforge[lang].alter_of_power._12)
				say_reward(gameforge[lang].alter_of_power._13)
				say_reward(gameforge[lang].alter_of_power._14)
				say_reward(gameforge[lang].alter_of_power._15)
				say_reward(gameforge[lang].alter_of_power._16)
				local s = select(gameforge[lang].alter_of_power._17, gameforge[lang].alter_of_power._18)
				say_title(string.format("%s:", mob_name(npc.get_race())))
				say("")
				if s == 1 then
					local paid = pc.typed_item_exchange({{6, 755, 0, 10}, {6, 756, 0, 10}, {6, 757, 0, 15}}, {}, 25000000)
					if paid then
						say(gameforge[lang].alter_of_power._19)
						say(gameforge[lang].alter_of_power._20)
						building.reconstruct(14062)
						pc.setqf("build_level", guild.level(pc.get_guild()))
						char_log(0, "GUILD_BUILDING", "alter_of_power 14062 constructed")
					else
						say(gameforge[lang].alter_of_power._21)
						say(gameforge[lang].alter_of_power._22)
					end
				elseif s == 2 then
					say(gameforge[lang].alter_of_power._23)
				end
			else
				say(gameforge[lang].alter_of_power._24)
				say(gameforge[lang].alter_of_power._25)
			end
		end

		when 20078.click with npc.get_guild() == pc.get_guild() and pc.is_guild_master() begin
			say_title(string.format("%s:", mob_name(npc.get_race())))
			say("")
			local lang = pc.get_language()
			say(gameforge[lang].alter_of_power._26)
			say(gameforge[lang].alter_of_power._27)
			say(gameforge[lang].alter_of_power._28)
			select(gameforge[lang].common.wait)
			say_title(string.format("%s:", mob_name(npc.get_race())))
			say("")
			say(gameforge[lang].alter_of_power._29)
			if pc.getqf("build_level") < guild.level(pc.get_guild()) or guild.level(pc.get_guild()) >= 20 then
				say(gameforge[lang].alter_of_power._30)
				say(gameforge[lang].alter_of_power._31)
				say(gameforge[lang].alter_of_power._32)
				say_reward(gameforge[lang].alter_of_power._33)
				say_reward(gameforge[lang].alter_of_power._34)
				say_reward(gameforge[lang].alter_of_power._35)
				say_reward(gameforge[lang].alter_of_power._36)
				local s = select(gameforge[lang].alter_of_power._37, gameforge[lang].alter_of_power._38)
				say_title(string.format("%s:", mob_name(npc.get_race())))
				say("")
				if s == 1 then
					local paid = pc.typed_item_exchange({{6, 755, 0, 15}, {6, 756, 0, 20}, {6, 757, 0, 20}}, {}, 30000000)
					if paid then
						say(gameforge[lang].alter_of_power._39)
						say(gameforge[lang].alter_of_power._40)
						say(gameforge[lang].alter_of_power._41)
						say(gameforge[lang].alter_of_power._42)
						building.reconstruct(14063)
						pc.setqf("build_level", guild.level(pc.get_guild()))
						char_log(0, "GUILD_BUILDING", "alter_of_power 14063 constructed")
					else
						say(gameforge[lang].alter_of_power._43)
						say(gameforge[lang].alter_of_power._44)
					end
				elseif s == 2 then
					say(gameforge[lang].alter_of_power._45)
				end
			end
		end

		when 20079.click with npc.get_guild() == pc.get_guild() and pc.is_guild_master() begin
			say_title(string.format("%s:", mob_name(npc.get_race())))
			say("")
			local lang = pc.get_language()
			say(gameforge[lang].alter_of_power._46)
			say(gameforge[lang].alter_of_power._47)
			say(gameforge[lang].alter_of_power._48)
			say("La gilda possiede gia l'altare")
			say("di livello massimo.")
			say("Non puoi migliorarlo ulteriormente.")
			say("")
		end

		when 20077.click or 20078.click or 20079.click with npc.get_guild() == pc.get_guild() and pc.is_guild_master() != true begin
			say_title(string.format("%s:", mob_name(npc.get_race())))
			say("")
			local lang = pc.get_language()
			say(gameforge[lang].alter_of_power._49)
			say(gameforge[lang].alter_of_power._50)
		end
	end
end
