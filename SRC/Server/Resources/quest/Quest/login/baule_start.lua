quest baule_start begin
	state start begin
		when "catalog.10.apprentice_chest_i_2156821f2841".typed_use begin
			local item_title = game.typed_item_use_name("Apprentice Chest I")
			local lang = pc.get_language()
			if pc.get_level() >= 1 then
				if not pc.can_warp() then
					syschat(gameforge[lang].reset_scroll._35)
					return
				end

				local rewards = {}
				rewards[#rewards + 1] = {5, 410, 0, 1}--Nap Elixír (Nagy)
				rewards[#rewards + 1] = {5, 414, 0, 1}--Hold Elixír (Nagy)
				rewards[#rewards + 1] = {5, 649, 0, 250}--Zöld Mágia
				rewards[#rewards + 1] = {10, 795, 0, 200}--Bátorság köpeny
				rewards[#rewards + 1] = {5, 713, 0, 3}--Zöld Harmat
				rewards[#rewards + 1] = {4, 1869, 0, 1}
				rewards[#rewards + 1] = {4, 1871, 0, 1}
				rewards[#rewards + 1] = {15, 638, 0, 1}--Fóka Vihar tigris
				rewards[#rewards + 1] = {10, 180, 0, 1}--Geci
				if pc.get_sex() == 0 then
					rewards[#rewards + 1] = {4, 1377, 0, 1}--Sivatagi harcos (f, fek)
					rewards[#rewards + 1] = {4, 1432, 0, 1}--Fekete turbán (F)
				elseif pc.get_sex() == 1 then
					rewards[#rewards + 1] = {4, 1378, 0, 1}--Sivatagi harcos (n, fek)
					rewards[#rewards + 1] = {4, 1433, 0, 1}--Fekete turbán (N)
				end
				local job = pc.get_job()
				if job == 0 then
					rewards[#rewards + 1] = {4, 1071, 0, 1}--Aqua-penge
					rewards[#rewards + 1] = {4, 1074, 0, 1}--Aqua-glefe
				elseif job == 1 then
					rewards[#rewards + 1] = {4, 1072, 0, 1}--Aqua-tõr
					rewards[#rewards + 1] = {4, 1073, 0, 1}--Aqua-íj
					rewards[#rewards + 1] = {4, 1071, 0, 1}--Aqua-penge
				elseif job == 2 then
					rewards[#rewards + 1] = {4, 1071, 0, 1}--Aqua-penge
				elseif job == 3 then
					rewards[#rewards + 1] = {4, 1075, 0, 1}--Aqua-harang
					rewards[#rewards + 1] = {4, 1076, 0, 1}--Aqua-legyezõ
				end
				local opened, reason = pc.typed_item_redeem(rewards, 0)
				if not opened then
					if reason == "inventory_full" then
						chat("Szabadits fel helyet a leltarban. / Make room in your inventory.")
					else
						say(gameforge[lang].common.unknownerr)
					end
					return
				end
				chat(string.format(gameforge[lang].baule_start._1, item_title))
				chat(gameforge[lang].baule_start._2)
				notice_all(904, pc.get_name())
			else
				say_title(string.format("%s:", item_title))
				say("")
				say(string.format(gameforge[lang].baule_start._3, 1))
			end
		end

		when "catalog.10.expert_chest_i_2c455ec85700".typed_use begin
			local item_title = game.typed_item_use_name("Expert Chest I")
			local lang = pc.get_language()
			if pc.get_level() >= 30 then
				if not pc.can_warp() then
					syschat(gameforge[lang].reset_scroll._35)
					return
				end

				local rewards = {}
				rewards[#rewards + 1] = {10, 1043, 0, 1}--Szakértõ ládája III
				rewards[#rewards + 1] = {5, 783, 0, 5}--Koncentrált olvasás
				rewards[#rewards + 1] = {5, 313, 0, 5}--Ördögûzõ tekercs
				rewards[#rewards + 1] = {5, 657, 0, 1}--Tolvaj kesztyû(1ó)
				rewards[#rewards + 1] = {5, 655, 0, 1}--Tapasztalat gyûrû(1ó)
				rewards[#rewards + 1] = {5, 738, 0, 500}--Tárgy büvölö (nem átadható)
				local opened, reason = pc.typed_item_redeem(rewards, 0)
				if not opened then
					if reason == "inventory_full" then
						chat("Szabadits fel helyet a leltarban. / Make room in your inventory.")
					else
						say(gameforge[lang].common.unknownerr)
					end
					return
				end
				chat(string.format(gameforge[lang].baule_start._1, item_title))
				chat(gameforge[lang].baule_start._2)
			else
				say_title(string.format("%s:", item_title))
				say("")
				say(string.format(gameforge[lang].baule_start._3, 30))
			end
		end

		when "catalog.10.expert_chest_iii_56ce4a1d2759".typed_use begin
			local item_title = game.typed_item_use_name("Expert Chest III")
			local lang = pc.get_language()
			if pc.get_level() >= 50 then
				if not pc.can_warp() then
					syschat(gameforge[lang].reset_scroll._35)
					return
				end

				local rewards = {}
				rewards[#rewards + 1] = {10, 1045, 0, 1}--láda
				rewards[#rewards + 1] = {5, 738, 0, 100}--Tárgy büvölö (nem átadható)
				rewards[#rewards + 1] = {8, 36, 0, 15}--Harcos láda
				rewards[#rewards + 1] = {5, 787, 0, 3}--Zen-bab
				rewards[#rewards + 1] = {6, 126, 0, 4}--Orkfog
				rewards[#rewards + 1] = {5, 716, 0, 5}--Sárkány Isten támadása
				rewards[#rewards + 1] = {5, 715, 0, 5}--Átüto harc
				rewards[#rewards + 1] = {5, 714, 0, 5}--Kritikus küzdelem
				rewards[#rewards + 1] = {7, 311, 5, 1}--Démonok köve+5
				local opened, reason = pc.typed_item_redeem(rewards, 0)
				if not opened then
					if reason == "inventory_full" then
						chat("Szabadits fel helyet a leltarban. / Make room in your inventory.")
					else
						say(gameforge[lang].common.unknownerr)
					end
					return
				end
				chat(string.format(gameforge[lang].baule_start._1, item_title))
				chat(gameforge[lang].baule_start._2)
			else
				say_title(string.format("%s:", item_title))
				say("")
				say(string.format(gameforge[lang].baule_start._3, 50))
			end
		end

		when "catalog.10.master_chest_ii_a91f9a06bbf8".typed_use begin
			local item_title = game.typed_item_use_name("Master Chest II")
			local lang = pc.get_language()
			if pc.get_level() >= 75 then
				if not pc.can_warp() then
					syschat(gameforge[lang].reset_scroll._35)
					return
				end

				local rewards = {}
				rewards[#rewards + 1] = {5, 346, 0, 100}--Megbûvölõ tárgy
				rewards[#rewards + 1] = {5, 702, 0, 1}--Narancssárga Ampulla
				rewards[#rewards + 1] = {5, 787, 0, 5}--Zen-bab
				rewards[#rewards + 1] = {5, 716, 0, 10}--Sárkány Isten támadása
				rewards[#rewards + 1] = {5, 715, 0, 10}--Átüto harc
				rewards[#rewards + 1] = {5, 730, 0, 1}--Zöld Ampulla
				rewards[#rewards + 1] = {5, 727, 0, 1}--Sárga Ampulla
				local opened, reason = pc.typed_item_redeem(rewards, 0)
				if not opened then
					if reason == "inventory_full" then
						chat("Szabadits fel helyet a leltarban. / Make room in your inventory.")
					else
						say(gameforge[lang].common.unknownerr)
					end
					return
				end
				chat(string.format(gameforge[lang].baule_start._1, item_title))
				chat(gameforge[lang].baule_start._2)
			else
				say_title(string.format("%s:", item_title))
				say("")
				say(string.format(gameforge[lang].baule_start._3, 75))
			end
		end

		when "catalog.10.master_chest_iii_5235af93858b".typed_use begin
			local item_title = game.typed_item_use_name("Master Chest III")
			local lang = pc.get_language()
			if pc.get_level() >= 100 then
				if not pc.can_warp() then
					syschat(gameforge[lang].reset_scroll._35)
					return
				end

				local rewards = {}
				rewards[#rewards + 1] = {10, 1047, 0, 1}
				rewards[#rewards + 1] = {5, 740, 0, 1}
				rewards[#rewards + 1] = {5, 741, 0, 1}
				rewards[#rewards + 1] = {5, 742, 0, 1}
				rewards[#rewards + 1] = {14, 33, 0, 1}
				rewards[#rewards + 1] = {20, 40, 0, 1}
				rewards[#rewards + 1] = {5, 728, 0, 1}
				local opened, reason = pc.typed_item_redeem(rewards, 0)
				if not opened then
					if reason == "inventory_full" then
						chat("Szabadits fel helyet a leltarban. / Make room in your inventory.")
					else
						say(gameforge[lang].common.unknownerr)
					end
					return
				end
				chat(string.format(gameforge[lang].baule_start._1, item_title))
				chat(gameforge[lang].baule_start._2)
			else
				say_title(string.format("%s:", item_title))
				say("")
				say(string.format(gameforge[lang].baule_start._3, 100))
			end
		end

		when "catalog.10.grand_master_chest_eacd7c87af35".typed_use begin
			local item_title = game.typed_item_use_name("Grand Master Chest")
			local lang = pc.get_language()
			if pc.get_level() >= 120 then
				if not pc.can_warp() then
					syschat(gameforge[lang].reset_scroll._35)
					return
				end

				local rewards = {}
				rewards[#rewards + 1] = {5, 729, 0, 1}
				rewards[#rewards + 1] = {5, 730, 0, 1}
				rewards[#rewards + 1] = {5, 731, 0, 1}
				rewards[#rewards + 1] = {17, 104, 0, 1}
				rewards[#rewards + 1] = {14, 33, 0, 1}
				-- The retired alchemy changer has no typed reward.
				rewards[#rewards + 1] = {20, 40, 0, 1}
				rewards[#rewards + 1] = {5, 741, 0, 1}
				local opened, reason = pc.typed_item_redeem(rewards, 0)
				if not opened then
					if reason == "inventory_full" then
						chat("Szabadits fel helyet a leltarban. / Make room in your inventory.")
					else
						say(gameforge[lang].common.unknownerr)
					end
					return
				end
				chat(string.format(gameforge[lang].baule_start._1, item_title))
				chat(gameforge[lang].baule_start._2)
			else
				say_title(string.format("%s:", item_title))
				say("")
				say(string.format(gameforge[lang].baule_start._3, 120))
			end
		end
	end
end
