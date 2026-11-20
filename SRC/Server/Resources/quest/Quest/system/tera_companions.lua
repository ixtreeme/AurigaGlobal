quest tera_companions begin
    state start begin
        function exchange(code)
            local category, local_id
            if game.typed_item_key then
                category, local_id = game.typed_item_key(code)
            end
            if category ~= 15 then
                syschat("A petpecsét cseréje még nem érhető el. / Pet seal exchange is not available yet.")
                return
            end
            local ok, reason = pc.typed_item_redeem({{category, local_id, 0, 1}}, 0)
            if ok then
                syschat("A petpecsét cseréje sikerült. Az új pecséttel megidézheted a petet. / Use the new seal to summon your pet.")
            elseif reason == "inventory_full" then
                syschat("Szabadíts fel helyet a leltárban. / Make room in your inventory.")
            else
                syschat("A petpecsét most nem cserélhető. / The pet seal cannot be exchanged now.")
            end
        end

        when "catalog.10.seal_baba_hooka_721b4fe5fc3b".typed_use begin
            tera_companions.exchange("companion.tera.seal_baba_hooka_721b4fe5fc3b")
        end
        when "catalog.10.seal_beautiful_pixie_a17731dd6864".typed_use begin
            tera_companions.exchange("companion.tera.seal_beautiful_pixie_a17731dd6864")
        end
        when "catalog.10.seal_pet_durion_6c129459a50c".typed_use begin
            tera_companions.exchange("companion.tera.seal_pet_durion_6c129459a50c")
        end
        when "catalog.10.seal_popori_boy03_e70893a8c861".typed_use begin
            tera_companions.exchange("companion.tera.seal_popori_boy03_e70893a8c861")
        end
        when "catalog.10.seal_popori_boy03b_23968143c874".typed_use begin
            tera_companions.exchange("companion.tera.seal_popori_boy03b_23968143c874")
        end
        when "catalog.10.seal_bisque_lady_1180e1adfa36".typed_use begin
            tera_companions.exchange("companion.tera.seal_bisque_lady_1180e1adfa36")
        end
        when "catalog.10.seal_boss_ghilliedhu_b_1d8f72f4df11".typed_use begin
            tera_companions.exchange("companion.tera.seal_boss_ghilliedhu_b_1d8f72f4df11")
        end
        when "catalog.10.seal_cat_60dd_dd3e9323e03a".typed_use begin
            tera_companions.exchange("companion.tera.seal_cat_60dd_dd3e9323e03a")
        end
        when "catalog.10.seal_dumperkan_fighter_5aae91208778".typed_use begin
            tera_companions.exchange("companion.tera.seal_dumperkan_fighter_5aae91208778")
        end
        when "catalog.10.seal_dying_ghillie_dhu_6648dbf658b6".typed_use begin
            tera_companions.exchange("companion.tera.seal_dying_ghillie_dhu_6648dbf658b6")
        end
        when "catalog.10.seal_eatingplant_af31393c67cc".typed_use begin
            tera_companions.exchange("companion.tera.seal_eatingplant_af31393c67cc")
        end
        when "catalog.10.seal_enchanteddoll_2cadaa64dc3d".typed_use begin
            tera_companions.exchange("companion.tera.seal_enchanteddoll_2cadaa64dc3d")
        end
        when "catalog.10.seal_ev_creature_frog_5960b67b5ebd".typed_use begin
            tera_companions.exchange("companion.tera.seal_ev_creature_frog_5960b67b5ebd")
        end
        when "catalog.10.seal_fire_tinydragon_4a1d2155efa1".typed_use begin
            tera_companions.exchange("companion.tera.seal_fire_tinydragon_4a1d2155efa1")
        end
        when "catalog.10.seal_fire_tinydragon_black_3aba45b20406".typed_use begin
            tera_companions.exchange("companion.tera.seal_fire_tinydragon_black_3aba45b20406")
        end
        when "catalog.10.seal_fire_tinydragon_green_da68f52d9d5c".typed_use begin
            tera_companions.exchange("companion.tera.seal_fire_tinydragon_green_da68f52d9d5c")
        end
        when "catalog.10.seal_water_melon_224bf071cfd4".typed_use begin
            tera_companions.exchange("companion.tera.seal_water_melon_224bf071cfd4")
        end
        when "catalog.10.seal_ghost_3e2a_502ced287cc3".typed_use begin
            tera_companions.exchange("companion.tera.seal_ghost_3e2a_502ced287cc3")
        end
        when "catalog.10.seal_ghost2_d01472482b12".typed_use begin
            tera_companions.exchange("companion.tera.seal_ghost2_d01472482b12")
        end
        when "catalog.10.seal_goral_90ab4d95dda2".typed_use begin
            tera_companions.exchange("companion.tera.seal_goral_90ab4d95dda2")
        end
        when "catalog.10.seal_homunculus_smurf_pet_0f636a958f52".typed_use begin
            tera_companions.exchange("companion.tera.seal_homunculus_smurf_pet_0f636a958f52")
        end
        when "catalog.10.seal_hooka_toad_a_e5eb7f51143e".typed_use begin
            tera_companions.exchange("companion.tera.seal_hooka_toad_a_e5eb7f51143e")
        end
        when "catalog.10.seal_hooka_toad_b_dcecb7ea81bb".typed_use begin
            tera_companions.exchange("companion.tera.seal_hooka_toad_b_dcecb7ea81bb")
        end
        when "catalog.10.seal_hooka_toad_c_09de258c213e".typed_use begin
            tera_companions.exchange("companion.tera.seal_hooka_toad_c_09de258c213e")
        end
        when "catalog.10.seal_lizard_55f5_afbbf72ef4e9".typed_use begin
            tera_companions.exchange("companion.tera.seal_lizard_55f5_afbbf72ef4e9")
        end
        when "catalog.10.seal_lizard_d96c_bfd9d1f97900".typed_use begin
            tera_companions.exchange("companion.tera.seal_lizard_d96c_bfd9d1f97900")
        end
        when "catalog.10.seal_monkey_e5ec4c138c12".typed_use begin
            tera_companions.exchange("companion.tera.seal_monkey_e5ec4c138c12")
        end
        when "catalog.10.seal_orcan_minimi_ab710bfb7d3c".typed_use begin
            tera_companions.exchange("companion.tera.seal_orcan_minimi_ab710bfb7d3c")
        end
        when "catalog.10.seal_animal_bear_ab6cb7b833d5".typed_use begin
            tera_companions.exchange("companion.tera.seal_animal_bear_ab6cb7b833d5")
        end
        when "catalog.10.seal_animal_cat_82bdd9909b4a".typed_use begin
            tera_companions.exchange("companion.tera.seal_animal_cat_82bdd9909b4a")
        end
        when "catalog.10.seal_animal_guinea_pig_fad07c525b18".typed_use begin
            tera_companions.exchange("companion.tera.seal_animal_guinea_pig_fad07c525b18")
        end
        when "catalog.10.seal_animal_panda_6fe9022552a7".typed_use begin
            tera_companions.exchange("companion.tera.seal_animal_panda_6fe9022552a7")
        end
        when "catalog.10.seal_animal_rabbit_eff91f966781".typed_use begin
            tera_companions.exchange("companion.tera.seal_animal_rabbit_eff91f966781")
        end
        when "catalog.10.seal_animal_raccoon_0a1a635ce961".typed_use begin
            tera_companions.exchange("companion.tera.seal_animal_raccoon_0a1a635ce961")
        end
        when "catalog.10.seal_animal_thibetanus_d5ce3b5f9e6a".typed_use begin
            tera_companions.exchange("companion.tera.seal_animal_thibetanus_d5ce3b5f9e6a")
        end
        when "catalog.10.seal_animal_wolf_6f1d396d2421".typed_use begin
            tera_companions.exchange("companion.tera.seal_animal_wolf_6f1d396d2421")
        end
        when "catalog.10.seal_bat_d6459e3a5735".typed_use begin
            tera_companions.exchange("companion.tera.seal_bat_d6459e3a5735")
        end
        when "catalog.10.seal_bunny_in_the_moon_a_500d62c65e35".typed_use begin
            tera_companions.exchange("companion.tera.seal_bunny_in_the_moon_a_500d62c65e35")
        end
        when "catalog.10.seal_cat_574a_d41a2ca18c60".typed_use begin
            tera_companions.exchange("companion.tera.seal_cat_574a_d41a2ca18c60")
        end
        when "catalog.10.seal_cat_b_804c012766d2".typed_use begin
            tera_companions.exchange("companion.tera.seal_cat_b_804c012766d2")
        end
        when "catalog.10.seal_durion_c6a396cee838".typed_use begin
            tera_companions.exchange("companion.tera.seal_durion_c6a396cee838")
        end
        when "catalog.10.seal_full_moon_a_4dfabf2248ab".typed_use begin
            tera_companions.exchange("companion.tera.seal_full_moon_a_4dfabf2248ab")
        end
        when "catalog.10.seal_ghost_3a5b_6c18ebe8f33a".typed_use begin
            tera_companions.exchange("companion.tera.seal_ghost_3a5b_6c18ebe8f33a")
        end
        when "catalog.10.seal_golden_pig_bcd0922d7a9f".typed_use begin
            tera_companions.exchange("companion.tera.seal_golden_pig_bcd0922d7a9f")
        end
        when "catalog.10.seal_mushroom_ff47a5d31157".typed_use begin
            tera_companions.exchange("companion.tera.seal_mushroom_ff47a5d31157")
        end
        when "catalog.10.seal_pig_kitty_541795d9e022".typed_use begin
            tera_companions.exchange("companion.tera.seal_pig_kitty_541795d9e022")
        end
        when "catalog.10.seal_shark_blue_285d25a6380c".typed_use begin
            tera_companions.exchange("companion.tera.seal_shark_blue_285d25a6380c")
        end
        when "catalog.10.seal_shark_pink_d4982c7f1322".typed_use begin
            tera_companions.exchange("companion.tera.seal_shark_pink_d4982c7f1322")
        end
        when "catalog.10.seal_slime_1da315ac4948".typed_use begin
            tera_companions.exchange("companion.tera.seal_slime_1da315ac4948")
        end
        when "catalog.10.seal_spirit_of_nature_a_8ff9d0d5f147".typed_use begin
            tera_companions.exchange("companion.tera.seal_spirit_of_nature_a_8ff9d0d5f147")
        end
        when "catalog.10.seal_spirit_of_nature_b_a527aa07b6f2".typed_use begin
            tera_companions.exchange("companion.tera.seal_spirit_of_nature_b_a527aa07b6f2")
        end
        when "catalog.10.seal_vehicle_cat_b_babf597cd2d3".typed_use begin
            tera_companions.exchange("companion.tera.seal_vehicle_cat_b_babf597cd2d3")
        end
        when "catalog.10.seal_vehicle_cat_c_822469f7dd00".typed_use begin
            tera_companions.exchange("companion.tera.seal_vehicle_cat_c_822469f7dd00")
        end
        when "catalog.10.seal_pigling_8f36a53be97a".typed_use begin
            tera_companions.exchange("companion.tera.seal_pigling_8f36a53be97a")
        end
        when "catalog.10.seal_pirate_synuuth_f39576589520".typed_use begin
            tera_companions.exchange("companion.tera.seal_pirate_synuuth_f39576589520")
        end
        when "catalog.10.seal_popori_boy_7209b5967c23".typed_use begin
            tera_companions.exchange("companion.tera.seal_popori_boy_7209b5967c23")
        end
        when "catalog.10.seal_popori_boy02_07c2f67079cd".typed_use begin
            tera_companions.exchange("companion.tera.seal_popori_boy02_07c2f67079cd")
        end
        when "catalog.10.seal_popori_boy03_0259dd65960d".typed_use begin
            tera_companions.exchange("companion.tera.seal_popori_boy03_0259dd65960d")
        end
        when "catalog.10.seal_sheep_aa3a8542cfc6".typed_use begin
            tera_companions.exchange("companion.tera.seal_sheep_aa3a8542cfc6")
        end
        when "catalog.10.seal_ssalissali_king_pet_79c3a7d87f5c".typed_use begin
            tera_companions.exchange("companion.tera.seal_ssalissali_king_pet_79c3a7d87f5c")
        end
        when "catalog.10.seal_stone_head_6a4459427b02".typed_use begin
            tera_companions.exchange("companion.tera.seal_stone_head_6a4459427b02")
        end
        when "catalog.10.seal_summoned_hellhound_66bb60aadc41".typed_use begin
            tera_companions.exchange("companion.tera.seal_summoned_hellhound_66bb60aadc41")
        end
        when "catalog.10.seal_tiny_dragon_3c69ea80a06a".typed_use begin
            tera_companions.exchange("companion.tera.seal_tiny_dragon_3c69ea80a06a")
        end
        when "catalog.10.seal_tiny_dragon_black_4c8dc2a0e144".typed_use begin
            tera_companions.exchange("companion.tera.seal_tiny_dragon_black_4c8dc2a0e144")
        end
        when "catalog.10.seal_tiny_dragon_gold_96d7d4e8da8f".typed_use begin
            tera_companions.exchange("companion.tera.seal_tiny_dragon_gold_96d7d4e8da8f")
        end
        when "catalog.10.seal_tiny_dragon_green_eb5c5ee56505".typed_use begin
            tera_companions.exchange("companion.tera.seal_tiny_dragon_green_eb5c5ee56505")
        end
        when "catalog.10.seal_tiny_dragon_purple_1c5e84e1cb73".typed_use begin
            tera_companions.exchange("companion.tera.seal_tiny_dragon_purple_1c5e84e1cb73")
        end
        when "catalog.10.seal_tiny_dragon_red_9760da0a701f".typed_use begin
            tera_companions.exchange("companion.tera.seal_tiny_dragon_red_9760da0a701f")
        end
        when "catalog.10.seal_tiny_dragon_silver_22245ba89730".typed_use begin
            tera_companions.exchange("companion.tera.seal_tiny_dragon_silver_22245ba89730")
        end
    end
end
