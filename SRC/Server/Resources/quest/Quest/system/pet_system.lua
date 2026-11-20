quest pet_system begin
	state start begin
		function get_pet_info(itemVnum)
			if pet_system.pet_info_map==nil then
				pet_system.pet_info_map = {
				-- [ITEM VNUM] MOB_VNUM, DEFAULT NAME
					--[53001] = {34001, pet_system.petname_34001, 0}, --			no bonus	120giorni
					-- [53002] = {34002, pet_system.petname_34002, 0}, --		 	+1500 HP e +15% danno, Permette di droppare le calze se lo si tiene al proprio fianco mentre si uccidono mob.				20giorni
					--[53003] = {34003, pet_system.petname_34003, 0}, --			no bonus	120giorni
					-- [53005] = {34004, pet_system.petname_34004, 1}, --			+1500 HP e +15% di danno, se il PG si trova dal 2° piano della Torre dei Demoni in su, o se si trova all'interno delle Catacombe del Diavolo	30giorni
					[53006] = {34009}, --			+1500 HP e +15% di danno, se il PG si trova dal 2° piano della Torre dei Demoni in su, o se si trova all'interno delle Catacombe del Diavolo	200giorni(non implementato sull offy ma c'è)
					[53007] = {34010}, --			+1500 HP e +15% danno, Permette di droppare le calze se lo si tiene al proprio fianco mentre si uccidono mob.				20giorni
					[53008] = {34011}, --			+1500 HP e +15% di danno	30giorni
					[53009] = {34012}, --			+1500 HP e +15% di danno	30giorni
					[53010] = {34008}, --			+1500 HP e +15% di danno	30giorni
					[53011] = {34007}, --			+1500 HP e +15% di danno	120giorni
					[53012] = {34005}, --			+15% danno e +1500 HP		30giorni
					[53013] = {34006}, --			+1500 HP e +15% di danno	30giorni
					[53014] = {34013}, --			+1500 HP e +15% di danno	30giorni
					[53015] = {34014}, --			+1500 HP e +15% di danno	30giorni
					[53016] = {34015}, --			+1500 HP e +15% di danno	30giorni
					[53017] = {34016}, --			+30% di Exp e +15% di Danno	120giorni
					[53018] = {34020}, --			+1500 HP e +15% di danno	30giorni
					[53019] = {34019}, --			+1500 HP e +15% di danno	30giorni
					[53020] = {34017}, --			+1500 HP e +15% di danno	30giorni
					[53021] = {34018}, --			+1500 HP, +15% Forte contro Mostri	30giorni
					[53022] = {34021}, --			+1500 HP e +15% di danno	30giorni
					[53023] = {34022}, --			+1500 HP e +15% di danno	30giorni
					[53024] = {34023}, --			+1500 HP, +5% Trafiggente, +5% Resistenza Abilità		7giorni
					[53025] = {34024}, --			+1500 HP e +5% Critico, +10% Resistenza ai Danni Medi	7giorni
					[53026] = {34008}, --

					[53290] = {34201},
					[53291] = {34202},
					[53292] = {34203},
					[53293] = {34204},
					[53294] = {34205},
					[53295] = {34206},
					[53296] = {34207},

					[53027] = {34008}, --
					[53028] = {34008}, --
					[53029] = {34094}, --
					[53030] = {34094}, --
					[53031] = {34094}, --
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
					[53238] = {34058}, --			no bonus								1giorno		Se insieme al Pet evocato si sta utilizzando un Icona Amuleto del Cioccolato.pngAmuleto del Cioccolato, si otterrà 20% Exp Bonus.
					[53239] = {34059}, --			no bonus								1giorno		Se insieme al Pet evocato si sta utilizzando un Icona Amuleto del Cioccolato.pngAmuleto del Cioccolato, si otterrà 20% Exp Bonus.
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
					[53248] = {34084}, --			Donnie ???
					[53249] = {34084}, --			Donnie ???
					[53250] = {34084}, --			Donnie ???
					[53251] = {34085}, --					Frank ???
					[53252] = {34085}, --					Frank ???
					[53253] = {34085}, --					Frank ???
					[53256] = {34066}, --			no bonus							3600giorni
					[53263] = {34093}, --			no bonus							3600giorni
					[53264] = {34094}, --			no bonus							3600giorni
					[53282] = {34114}, --			no bonus							3600giorni
					[53283] = {34115}, --			no bonus							3600giorni
					[53284] = {34116}, --			no bonus							3600giorni
					-- GF specials
					[38200] = {34006}, --			Sigillo Rufus ???
					[38201] = {34006}, --			Sigillo Rufus ???
					[48301] = {34100},
					[48311] = {34100},
					[48321] = {34100},
					[49010] = {34116},
					[49050] = {34117},
					[60102] = {34118},
					[60104] = {34119},

					[55701] = {34041},
					[55702] = {34045},
					[55703] = {34049},
					[55704] = {34053},
					[55705] = {34036},
					[55706] = {34064},
					[55707] = {34073},
					[55708] = {34075},
					[55709] = {34080},
					[55710] = {34082},
					[55711] = {34095},

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

		when 38200.use or 38201.use or
			53006.use or 53007.use or 53008.use or 53009.use or
			53010.use or 53011.use or 53012.use or 53013.use or 53014.use or 53015.use or 53016.use or 53017.use or 53018.use or 53019.use or
			53020.use or 53021.use or 53022.use or 53023.use or 53024.use or 53025.use or -- 53004.use or 53026.use or
			53218.use or 53219.use or
			53220.use or 53221.use or 53222.use or 53223.use or 53224.use or 53225.use or 53226.use or 53227.use or 53228.use or 53229.use or
			53230.use or 53231.use or 53232.use or 53233.use or 53234.use or 53235.use or 53236.use or 53237.use or 53238.use or 53239.use or
			53240.use or 53241.use or 53256.use or 53263.use or 53264.use or 53282.use or 53283.use or 53284.use or
			53026.use or 53027.use or 53028.use or 53029.use or 53030.use or 53031.use or 53248.use or 53249.use or 53250.use or 53251.use or
			53252.use or 53253.use or 48301.use or 48311.use or 48321.use or 49010.use or 49050.use or
			60102.use or 60104.use
			or 55701.use
			or 55702.use
			or 55703.use
			or 55704.use
			or 55705.use
			or 55706.use
			or 55707.use
			or 55708.use
			or 55709.use
			or 55710.use
			or 55711.use
			or 53290.use
			or 53291.use
			or 53292.use
			or 53293.use
			or 53294.use
			or 53295.use
			or 53296.use
			or 53297.use
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
