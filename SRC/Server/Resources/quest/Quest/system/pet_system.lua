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

        when "quest.pet_seal.arctic_khan".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.arctic_khan")
        end
        when "quest.pet_seal.polar_porky".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.polar_porky")
        end
        when "quest.pet_seal.frosty_rufus".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.frosty_rufus")
        end
        when "quest.pet_seal.sheldon".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.sheldon")
        end
        when "quest.pet_seal.cooper".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.cooper")
        end
        when "quest.pet_seal.panther_7d".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.panther_7d")
        end
        when "quest.pet_seal.panther_30d".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.panther_30d")
        end
        when "quest.pet_seal.panther_120d".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.panther_120d")
        end
        when "quest.pet_seal.leopard_30d".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.leopard_30d")
        end
        when "quest.pet_seal.leopard_120d".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.leopard_120d")
        end
        when "quest.pet_seal.pumpkin".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.pumpkin")
        end
        when "quest.pet_seal.little_broom".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.little_broom")
        end
        when "quest.pet_seal.snowflake".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.snowflake")
        end
        when "quest.pet_seal.sleigh_bell".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.sleigh_bell")
        end
        when "quest.pet_seal.cupid".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.cupid")
        end
        when "quest.pet_seal.wild_heart".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.wild_heart")
        end
        when "quest.pet_seal.blue_crescent".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.blue_crescent")
        end
        when "quest.pet_seal.red_crescent".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.red_crescent")
        end
        when "quest.pet_seal.eweriel".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.eweriel")
        end
        when "quest.pet_seal.ramblue".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.ramblue")
        end
        when "quest.pet_seal.baby_huashin".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.baby_huashin")
        end
        when "quest.pet_seal.bruce".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.bruce")
        end
        when "quest.pet_seal.wayne".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.wayne")
        end
        when "quest.pet_seal.robin".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.robin")
        end
        when "quest.pet_seal.valentino".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.valentino")
        end
        when "quest.pet_seal.valentinia".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.valentinia")
        end
        when "quest.pet_seal.moon_lantern_blue".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.moon_lantern_blue")
        end
        when "quest.pet_seal.moon_lantern_orange".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.moon_lantern_orange")
        end
        when "quest.pet_seal.poisoned_spirit_dragon".typed_use begin
            pet_system.exchange_fixed_seal("companion.timed.redesign.poisoned_spirit_dragon")
        end
        function choose(worn_time)
            local hu = pc.get_language() == "hu"
            if not pc.can_warp() then return end
            local pets = {
                {"Sarkvidéki Kán", "Arctis Khan", "1500 TP, 15% sebzés", "1,500 HP, +15% damage", "arctic_khan"},
                {"Sarki Porky", "Polar Porkie", "1500 TP, 15% sebzés", "1,500 HP, +15% damage", "polar_porky"},
                {"Fagyos Rufusz", "Frost Rufus", "1500 TP, 15% szörnyek ellen", "1,500 HP, +15% damage against monsters", "frosty_rufus"},
                {"Sheldon", "Sheldon", "1500 TP, 15% sebzés", "1,500 HP, +15% damage", "sheldon"},
                {"Cooper", "Cooper", "1500 TP, 15% sebzés", "1,500 HP, +15% damage", "cooper"},
                {"Párduckölyök", "Panther Cub", "1500 TP, 5% átható találat, 5% készségellenállás", "1,500 HP, +5% piercing chance, +5% skill resistance", "panther_7d"},
                {"Leopárdkölyök", "Leopard Cub", "1500 TP, 5% kritikus találat, 10% átlagkár-ellenállás", "1,500 HP, +5% critical chance, +10% normal damage resistance", "leopard_30d"},
                {"Tökfej", "Pumpkin Head", "1500 TP, 15% sebzés", "1,500 HP, +15% damage", "pumpkin"},
                {"Kis seprű", "Little Broomstick", "1500 TP, 15% sebzés", "1,500 HP, +15% damage", "little_broom"},
                {"Hópehely", "Snowflake", "1500 TP, 15% sebzés", "1,500 HP, +15% damage", "snowflake"},
                {"Száncsengő", "Snowbell", "15% sebzés, 5% kritikus sebzés, 15% mágikus sebzés", "+15% damage, +5% critical damage, +15% magic damage", "sleigh_bell"},
                {"Cupido", "Cupido", "1500 TP, 5% átható találat", "1,500 HP, +5% piercing chance", "cupid"},
                {"Tomboló szív", "Raging Heart", "1500 TP, 5% kritikus találat", "1,500 HP, +5% critical chance", "wild_heart"},
                {"Kék holdsarló", "Blue Crescent Moon", "1500 TP, 15% fizikai és mágikus sebzés", "1,500 HP, +15% physical and magic damage", "blue_crescent"},
                {"Piros holdsarló", "Red Crescent Moon", "5% kritikus találat, 15% fizikai és mágikus sebzés", "+5% critical chance, +15% physical and magic damage", "red_crescent"},
                {"Eweriel", "Eweriel", "1500 TP, 5% átható találat", "1,500 HP, +5% piercing chance", "eweriel"},
                {"Ramblue", "Ramblue", "1500 TP, 5% kritikus találat", "1,500 HP, +5% critical chance", "ramblue"},
                {"Baby Huashin", "Baby Huashin", "1500 TP, 5% átható találat", "1,500 HP, +5% piercing chance", "baby_huashin"},
                {"Bruce", "Bruce", "1500 TP, 5% kritikus találat, tárgyfelvétel", "1,500 HP, +5% critical chance, collects nearby items", "bruce"},
                {"Wayne", "Wayne", "1500 TP, 5% átható találat, tárgyfelvétel", "1,500 HP, +5% piercing chance, collects nearby items", "wayne"},
                {"Robin", "Robin", "1500 TP, 5% kritikus találat, tárgyfelvétel", "1,500 HP, +5% critical chance, collects nearby items", "robin"},
                {"Valentino", "Valentino", "1500 TP, 15% szörnyek ellen, csokoládéamulettel 50% tapasztalatbónusz", "1,500 HP, +15% damage against monsters, +50% experience with the Chocolate Amulet", "valentino"},
                {"Valentinia", "Valentinia", "1500 TP, 5% kritikus találat, csokoládéamulettel 50% tapasztalatbónusz", "1,500 HP, +5% critical chance, +50% experience with the Chocolate Amulet", "valentinia"},
                {"Holdlámpás (Kék)", "Lantern Moon (Blue)", "1500 TP, 5% kritikus találat", "1,500 HP, +5% critical chance", "moon_lantern_blue"},
                {"Holdlámpás (Narancs)", "Lantern Moon (Orange)", "1500 TP, 15% szörnyek ellen", "1,500 HP, +15% damage against monsters", "moon_lantern_orange"},
                {"Mergezett Szellem Sarkany", "Poisoned Spirit Dragon", "Bónusz nélküli társ", "Companion without stat bonuses", "poisoned_spirit_dragon"},
            }
            local first = 1
            while true do
                local last = math.min(first + 4, #pets)
                local options = {}
                for i = first, last do
                    options[#options + 1] = hu and pets[i][1] or pets[i][2]
                end
                local next_page = 0
                if last < #pets then
                    next_page = #options + 1
                    options[next_page] = hu and "Tovább" or "Next"
                end
                local prev_page = 0
                if first > 1 then
                    prev_page = #options + 1
                    options[prev_page] = hu and "Vissza" or "Previous"
                end
                options[#options + 1] = hu and "Mégse" or "Cancel"
                local selected = select_table(options)
                if not selected or selected < 1 or selected >= #options then return end
                if selected == next_page then
                    first = last + 1
                elseif selected == prev_page then
                    first = first - 5
                else
                    local pet = pets[first + selected - 1]
                    say(hu and pet[1] or pet[2])
                    say(hu and pet[3] or pet[4])
                    say(hu and "A pecsét meglévő idejét kapja meg." or "Your seal's remaining time carries over.")
                    if select(hu and "Beváltás" or "Redeem", hu and "Mégse" or "Cancel") == 1 then
                        local prefix = worn_time and "companion.timed.inherited." or "companion.timed.redesign."
                        pet_system.exchange_fixed_seal(prefix .. pet[5])
                    end
                    return
                end
            end
        end
        when "quest.pet_choice.deadline".typed_use begin
            pet_system.choose(false)
        end
        when "quest.pet_choice.remaining".typed_use begin
            pet_system.choose(true)
        end
    end
end
