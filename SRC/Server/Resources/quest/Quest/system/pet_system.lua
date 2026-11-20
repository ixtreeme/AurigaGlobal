quest pet_system begin
	state start begin

		function exchange_fixed_seal(code)
			if not pc.can_warp() then
				syschat(gameforge[pc.get_language()].reset_scroll._35)
				return
			end

			local category, local_id = game.typed_item_key(code)
			if category ~= 15 then
				syschat("Ez a petpecsét még nem váltható be.")
				return
			end

			local exchanged, reason = pc.typed_item_redeem({{category, local_id, 0, 1}}, 0)
			if exchanged then
				syschat("A petpecsét cseréje sikerült.")
			elseif reason == "inventory_full" then
				syschat("Szabadíts fel helyet a leltárban a petpecsét cseréjéhez.")
			else
				syschat("A petpecsétet most nem lehet beváltani.")
			end
		end

		when "catalog.10.noname_650864ec2eeb".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.noname_650864ec2eeb")
		end
		when "catalog.10.mini_lilith_inf_ca09c07f2662".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.mini_lilith_inf_ca09c07f2662")
		end
		when "catalog.10.poisoned_spirit_drake_inf_aa1a88aca44f".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.poisoned_spirit_drake_inf_aa1a88aca44f")
		end
		when "catalog.10.angelic_pegasus_inf_2c9706b33a89".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.angelic_pegasus_inf_2c9706b33a89")
		end
		when "catalog.10.luffy_02ddc31ba0c3".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.luffy_02ddc31ba0c3")
		end
		when "catalog.10.nogai_signet_permanent_9b4ae4ca31de".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.nogai_signet_permanent_9b4ae4ca31de")
		end
		when "catalog.10.valentino_cf79c2cb1e21".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.valentino_cf79c2cb1e21")
		end
		when "catalog.10.valentinia_636a456c9706".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.valentinia_636a456c9706")
		end
		when "catalog.10.donnie_permanent_4da820854b62".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.donnie_permanent_4da820854b62")
		end
		when "catalog.10.frank_permanent_7d86ccde8f37".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.frank_permanent_7d86ccde8f37")
		end
		when "catalog.10.gryph_edb7144d3fb4".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.gryph_edb7144d3fb4")
		end
		when "catalog.10.all_seeing_flamecat_453667dade55".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.all_seeing_flamecat_453667dade55")
		end
		when "catalog.10.all_seeing_frostcat_7848c2d45426".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.all_seeing_frostcat_7848c2d45426")
		end
		when "catalog.10.luffy_lenend_9d50822fd51d".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.luffy_lenend_9d50822fd51d")
		end
		when "catalog.10.luffy_osi_dde4d24bcce9".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.luffy_osi_dde4d24bcce9")
		end
		when "catalog.10.luffy_mythic_1d820cfb3e40".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.luffy_mythic_1d820cfb3e40")
		end
		when "catalog.10.dragon_epic_52f1175fe016".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.dragon_epic_52f1175fe016")
		end
		when "catalog.10.dragon_pet_legend_94a33e9c29b9".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.dragon_pet_legend_94a33e9c29b9")
		end
		when "catalog.10.dragon_pet_osi_805cf92ddd90".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.dragon_pet_osi_805cf92ddd90")
		end
		when "catalog.10.dragon_pet_mythic_50932e451ea5".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.dragon_pet_mythic_50932e451ea5")
		end
		when "catalog.10.mini_monkey_7a2fcfe1ef6f".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.mini_monkey_7a2fcfe1ef6f")
		end
		when "catalog.10.mini_spider_29afd884d914".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.mini_spider_29afd884d914")
		end
		when "catalog.10.mini_razador_9f0ce28f92fd".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.mini_razador_9f0ce28f92fd")
		end
		when "catalog.10.mini_nemere_2a8ade1a9ad2".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.mini_nemere_2a8ade1a9ad2")
		end
		when "catalog.10.dragonette_d9dfd5d1860b".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.dragonette_d9dfd5d1860b")
		end
		when "catalog.10.mini_meley_76c2d668fd10".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.mini_meley_76c2d668fd10")
		end
		when "catalog.10.mini_azrael_5f059c94bdf9".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.mini_azrael_5f059c94bdf9")
		end
		when "catalog.10.fat_mini_executor_9a63cffa6f5a".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.fat_mini_executor_9a63cffa6f5a")
		end
		when "catalog.10.baby_baashido_249643c0130b".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.baby_baashido_249643c0130b")
		end
		when "catalog.10.nessie_1541e400fbe8".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.nessie_1541e400fbe8")
		end
		when "catalog.10.bold_black_giant_orc_12f7ce370dd0".typed_use begin
			pet_system.exchange_fixed_seal("companion.fixed.bold_black_giant_orc_12f7ce370dd0")
		end

		when "catalog.10.noname_9f7fd3f181e8".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.noname_9f7fd3f181e8")
		end
		when "catalog.10.mini_lilith_24h_2f83fee88a5a".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.mini_lilith_24h_2f83fee88a5a")
		end
		when "catalog.10.mini_lilith_10d_2fe919e828e0".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.mini_lilith_10d_2fe919e828e0")
		end
		when "catalog.10.noname_6084f80d94af".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.noname_6084f80d94af")
		end
		when "catalog.10.noname_f60234176e90".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.noname_f60234176e90")
		end
		when "catalog.10.noname_37601ffbccf0".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.noname_37601ffbccf0")
		end
		when "catalog.10.noname_cfa68c011968".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.noname_cfa68c011968")
		end
		when "catalog.10.leonidas_signet_e61c2df3342b".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.leonidas_signet_e61c2df3342b")
		end
		when "catalog.10.khan_signet_d827fb39ed6e".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.khan_signet_d827fb39ed6e")
		end
		when "catalog.10.porky_signet_e8134b357132".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.porky_signet_e8134b357132")
		end
		when "catalog.10.rufus_signet_4606541c41fa".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.rufus_signet_4606541c41fa")
		end
		when "catalog.10.noname_631e65f4269a".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.noname_631e65f4269a")
		end
		when "catalog.10.noname_c6aabbc03782".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.noname_c6aabbc03782")
		end
		when "catalog.10.boney_5508bca0b5f9".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.boney_5508bca0b5f9")
		end
		when "catalog.10.jade_phoenix_signet_97e64cf1c212".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.jade_phoenix_signet_97e64cf1c212")
		end
		when "catalog.10.cooper_a511ae36623b".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.cooper_a511ae36623b")
		end
		when "catalog.10.flame_ghost_e299dc43c764".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.flame_ghost_e299dc43c764")
		end
		when "catalog.10.flame_ghost_588560361426".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.flame_ghost_588560361426")
		end
		when "catalog.10.leonidas_signet_permanent_1670a6d90e3f".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.leonidas_signet_permanent_1670a6d90e3f")
		end
		when "catalog.10.nogai_signet_15g_802a7e823842".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.nogai_signet_15g_802a7e823842")
		end
		when "catalog.10.nogai_signet_30_c9eb455a2471".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.nogai_signet_30_c9eb455a2471")
		end
		when "catalog.10.donnie_15g_65e21768e6eb".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.donnie_15g_65e21768e6eb")
		end
		when "catalog.10.donnie_30g_ddb996295cc3".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.donnie_30g_ddb996295cc3")
		end
		when "catalog.10.frank_15g_223d7c15ae00".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.frank_15g_223d7c15ae00")
		end
		when "catalog.10.frank_30g_f238510448b5".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.frank_30g_f238510448b5")
		end
		when "catalog.10.bonsai_087c02b3711d".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.bonsai_087c02b3711d")
		end
		when "catalog.10.nogai_bc3e8922998b".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.nogai_bc3e8922998b")
		end
		when "catalog.10.baby_santa_claus_7g_c93a40d497d3".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.baby_santa_claus_7g_c93a40d497d3")
		end
		when "catalog.10.christmas_reindeer_7g_49c94c6c148d".typed_use begin
			pet_system.exchange_fixed_seal("companion.timed.christmas_reindeer_7g_49c94c6c148d")
		end

		function get_pet_info(itemVnum)
			if pet_system.pet_info_map==nil then
				pet_system.pet_info_map = {
				-- [ITEM VNUM] MOB_VNUM, DEFAULT NAME
					--[53001] = {34001, pet_system.petname_34001, 0}, --			no bonus	120giorni
					-- [53002] = {34002, pet_system.petname_34002, 0}, --		 	+1500 HP e +15% danno, Permette di droppare le calze se lo si tiene al proprio fianco mentre si uccidono mob.				20giorni
					--[53003] = {34003, pet_system.petname_34003, 0}, --			no bonus	120giorni
					-- [53005] = {34004, pet_system.petname_34004, 1}, --			+1500 HP e +15% di danno, se il PG si trova dal 2° piano della Torre dei Demoni in su, o se si trova all'interno delle Catacombe del Diavolo	30giorni
					[53019] = {34019}, --			+1500 HP e +15% di danno	30giorni
					[53020] = {34017}, --			+1500 HP e +15% di danno	30giorni
					[53021] = {34018}, --			+1500 HP, +15% Forte contro Mostri	30giorni
					[53022] = {34021}, --			+1500 HP e +15% di danno	30giorni
					[53023] = {34022}, --			+1500 HP e +15% di danno	30giorni
					[53024] = {34023}, --			+1500 HP, +5% Trafiggente, +5% Resistenza Abilità		7giorni


					[53218] = {34023}, --			+1500 HP, +5% Trafiggente, +5% Resistenza Abilità		30giorni
					[53219] = {34023}, --			+1500 HP, +5% Trafiggente, +5% Resistenza Abilità		120giorni
					[53220] = {34024}, --			+1500 HP e +5% Critico, +10% Resistenza ai Danni Medi	30giorni
					[53221] = {34024}, --			+1500 HP e +5% Critico, +10% Resistenza ai Danni Medi	120giorni
					[53222] = {34026}, --			1500 HP, +15% danno		30giorni
					[53223] = {34027}, --			+1500 HP, +15% danno	30giorni
					[53224] = {34028}, --			+1500 HP, +15% danno	30giorni
					[53225] = {34029}, --			+15% danno, +5% danno critico, +15% danno magico	30giorni
					[53226] = {34030}, --			+1500 HP +5% Trafiggenti	120giorni
					[53227] = {34031}, --			+1500 HP +5% Critico		120giorni
					[53228] = {34033}, --			+1500 HP, +15% danno fisico e danno magico	30giorni
					[53229] = {34032}, --			+5% colpi critici, +15% danno fisico e danno magico	30giorni
					[53230] = {34034}, --			+5% colpi trafiggenti e +1500 HP	30giorni
					[53231] = {34035}, --			+5% colpi critici e +1500 HP	30giorni
					[53232] = {34039}, --			+1500 HP e +5% Trafiggenti		30giorni
					[53233] = {34055}, --			+5% colpi critici e +1500 HP	30giorni	Il pet raccoglierà automaticamente tutti gli oggetti a terra nelle sue vicinanze.
					[53234] = {34056}, --			+5% colpi trafiggenti e +1500 HP	30giorni	Il pet raccoglierà automaticamente tutti gli oggetti a terra nelle sue vicinanze.
					[53235] = {34057}, --			+5% colpi critici e +1500 HP	1giorno Il pet raccoglierà automaticamente tutti gli oggetti a terra nelle sue vicinanze.
					[53236] = {34060}, --			+15% Forte contro Mostri e +1500 HP		30giorni	Se insieme al Pet evocato si sta utilizzando un Icona Amuleto del Cioccolato.pngAmuleto del Cioccolato, si otterrà 50% Exp Bonus.
					[53237] = {34061}, --			+5% colpi critici e +1500 HP			30giorni	Se insieme al Pet evocato si sta utilizzando un Icona Amuleto del Cioccolato.pngAmuleto del Cioccolato, si otterrà 50% Exp Bonus.
					[53240] = {34063}, --			+5% colpi critici e +1500 HP		30giorni
					[53241] = {34062}, --			+1500 HP, +15% Forte contro Mostri	30giorni
					--[53242] = {34066, pet_system.petname_34066p, 0}, --			+5% colpi critici e +1500 HP		30giorni		Il pet raccoglierà automaticamente tutti gli oggetti a terra nelle sue vicinanze.
					--[53243] = {34066, pet_system.petname_34066pp, 0}, --			20% exp								30giorni
					--[53244] = {34067, pet_system.petname_34067, 0}, --			+5% colpi critici e +15% di danno		30giorni		Equipaggiandolo con un'armatura Vampiro+ , un elmo Vampiro+ e il Leccalecca garantisce un bonus di +20% Exp e + 5% Danni Medi e Abilità
					--[53245] = {34068, pet_system.petname_34068, 0}, --			+5% colpi trafiggenti e +15% di danno	30giorni		Equipaggiandolo con un'armatura Vampiro+ , un elmo Vampiro+ e il Leccalecca garantisce un bonus di +20% Exp e + 5% Danni Medi e Abilità
					--[53246] = {34069, pet_system.petname_34069, 0}, --			no bonus								1giorno			Equipaggiandolo con un'armatura ed elmo Vampiro garantisce un bonus di +20% Exp
					--[53247] = {34070, pet_system.petname_34070, 0}, --			+5% colpi critici e +5% Possibilità di Svenimento		30giorni	Equipaggiandolo con il Vestito da festa+, il Copricapo+ o il Basco+ si riceveranno, in aggiunta, due bonus del set. Aggiungendo un Anello della Felicità, si riceverà un altro bonus del set.
					-- [53248] = {34071, pet_system.petname_34071, 0}, --			+5% colpi trafiggenti e +5% Possibilità di Svenimento	1giorno		Equipaggiandolo con il Vestito da festa+, il Copricapo+ o il Basco+ si riceveranno, in aggiunta, due bonus del set. Aggiungendo un Anello della Felicità, si riceverà un altro bonus del set.
					--[53249] = {34072, pet_system.petname_34072, 0}, --			no bonus												30giorni	Equipaggiandolo con il vestito da festa, il copricapo o il basco si riceveranno, in aggiunta, un bonus del set. Aggiungendo un Anello della Felicità, si riceverà un altro bonus del set.
					[53284] = {34116}, --			no bonus							3600giorni
					-- GF specials


				}
			end

			return pet_system.pet_info_map[itemVnum]
		end

		function get_spawn_effect_file(idx)
			if pet_system.effect_table==nil then
				pet_system.effect_table = {
					[0] = nil,
					--[1] = "d:\\\\ymir work\\\\effect\\\\etc\\\\appear_die\\\\npc2_appear.mse",
				}
			end

			return pet_system.effect_table[idx]
		end

		when 53019.use or 53020.use or 53021.use or 53022.use or 53023.use or 53024.use or
			53218.use or 53219.use or 53220.use or 53221.use or 53222.use or 53223.use or
			53224.use or 53225.use or 53226.use or 53227.use or 53228.use or 53229.use or
			53230.use or 53231.use or 53232.use or 53233.use or 53234.use or 53235.use or
			53236.use or 53237.use or 53240.use or 53241.use or 53284.use or 53297.use
			begin
			if not pc.can_warp() then
				syschat(gameforge[pc.get_language()].reset_scroll._35)
				return
			end

			local pet_info = pet_system.get_pet_info(item.vnum)
			if null ~= pet_info then
				local mobVnum = pet_info[1]
				local spawn_effect_file_name = pet_system.get_spawn_effect_file(pet_info[3])
				if true == pet.is_summon(mobVnum) then
					if spawn_effect_file_name ~= nil then
						pet.spawn_effect(mobVnum, spawn_effect_file_name)
					end

					pet.unsummon(mobVnum)
				else
					if pet.count_summoned() < 1 then
						pet.summon(mobVnum, false)
					else
						syschat(gameforge[pc.get_language()].pet_system._1)
					end

					if spawn_effect_file_name ~= nil then
						pet.spawn_effect(mobVnum, spawn_effect_file_name)
					end
				end
			end
		end
	end
end
