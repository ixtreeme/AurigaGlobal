quest guild_rank_manager begin
	state start begin
		when letter begin
			if pc.is_gm() then
				send_letter(gameforge[pc.get_language()].guild_rank_manager._1)
			end
		end

		when info or button begin
			local lang = pc.get_language()
			say_title(string.format("%s:", gameforge[lang].guild_rank_manager._1))
			say("")
			say(gameforge[lang].guild_rank_manager._2)
			local a = select(gameforge[lang].guild_rank_manager._3, gameforge[lang].guild_rank_manager._4, gameforge[lang].guild_rank_manager._5)
			if a == 1 then
				server_timer( "guild_rank_manager",  1)
			elseif a == 2 then
				say_title(string.format("%s:", gameforge[lang].guild_rank_manager._1))
				say("")
				say(gameforge[lang].guild_rank_manager._6)
				local days = tonumber(input())
				clear_server_timer ( "guild_rank_manager" )
				server_loop_timer( "guild_rank_manager",  86400*days)
				syschat(gameforge[lang].guild_rank_manager._7)
			elseif a == 3 then
				clear_server_timer ( "guild_rank_manager" )
			end
		end

		when guild_rank_manager.server_timer begin
			local goldReward = {797, 800, 803}
			local silverReward = {798, 801, 804}
			local bronzeReward = {799, 802, 805}
			local guildsGOLD = game.mysql_query("SELECT * FROM guild WHERE trophies >= 50 ORDER BY trophies DESC LIMIT 3")
			for i = 1, table.getn(guildsGOLD) do
				game.give_guild_reward(guildsGOLD[i][1], 10, goldReward[i], 0, 1)
				notice_all(925, string.format("%s#%d", guildsGOLD[i][2], i))
			end
			
			local guildsSILVER = game.mysql_query("SELECT * FROM guild WHERE trophies >= 30 AND trophies < 50 ORDER BY trophies DESC LIMIT 3")
			for i = 1, table.getn(guildsSILVER) do
				game.give_guild_reward(guildsSILVER[i][1], 10, silverReward[i], 0, 1)
				notice_all(926, string.format("%s#%d", guildsSILVER[i][2], i))
			end
			
			local guildsBRONZE = game.mysql_query("SELECT * FROM guild WHERE trophies >= 5 AND trophies < 30 ORDER BY trophies DESC LIMIT 3")
			for i = 1, table.getn(guildsBRONZE) do
				game.give_guild_reward(guildsBRONZE[i][1], 10, bronzeReward[i], 0, 1)
				notice_all(927, string.format("%s#%d", guildsBRONZE[i][2], i))
			end
			
			game.reset_guild_war_stats()
		end
	end
end

