quest baule_pet begin
    state start begin
        when "catalog.10.pet_chest_b732c2d6c215".typed_use begin
            local language = pc.get_language()
            if not pc.can_warp() then
                syschat(gameforge[language].reset_scroll._35)
                return
            end
            local eggs = {
                {"catalog.10.monkey_egg_14db4db705d4", "Majomtojás", "Monkey Egg"},
                {"catalog.10.spider_egg_b185cafc5293", "Póktojás", "Spider Egg"},
                {"catalog.10.razador_s_egg_b796e5196cfd", "Razador tojása", "Razador's Egg"},
                {"catalog.10.nemere_s_egg_692e1966250c", "Nemere tojása", "Nemere's Egg"},
                {"catalog.10.blue_dragon_egg_84ea722ed564", "Kék sárkánytojás", "Blue Dragon Egg"},
                {"catalog.10.red_dragon_egg_769a16049932", "Vörös sárkánytojás", "Red Dragon Egg"},
                {"catalog.10.mini_azrael_s_egg_70087e8b0275", "Mini Azrael-tojás", "Mini Azrael's Egg"},
                {"catalog.10.fat_mini_executor_egg_65101c70fdd2", "Dagi Minihóhér-tojás", "Fat Mini Executor Egg"},
                {"catalog.10.baby_baashido_s_egg_b39f47097c09", "Kis Baashido tojás", "Baby Baashido's Egg"},
                {"catalog.10.nessie_egg_5cd70e3b5057", "Nessie tojás", "Nessie Egg"},
                {"catalog.10.exedyar_s_egg_beca25c2160e", "Exedyar tojása", "Exedyar's Egg"},
            }
            local choices = {}
            for _, egg in ipairs(eggs) do
                if not pc.typed_pet_egg_key(egg[1]) then
                    say("A petláda most nem érhető el. / The pet chest is currently unavailable.")
                    return
                end
                choices[#choices + 1] = language == "hu" and egg[2] or egg[3]
            end
            choices[#choices + 1] = gameforge[language].common.close
            say(language == "hu" and "Válassz egy pettojást." or "Choose a pet egg.")
            local egg = eggs[select_table(choices)]
            if not egg then return end

            local category, item = pc.typed_pet_egg_key(egg[1])
            if not category then
                say("A tojás most nem érhető el. / This egg is currently unavailable.")
                return
            end
            local ok, reason = pc.typed_item_redeem({{category, item, 0, 1}}, 0)
            if ok then
                say(language == "hu" and "A tojást a leltárból tudod kikeltetni." or "You can hatch the egg from your inventory.")
                say(language == "hu" and "A fejlődés feltételeit a pet adatlapján nézheted meg."
                    or "Check your pet's details to see its evolution requirements.")
            elseif reason == "inventory_full" then
                say(language == "hu" and "Szabadíts fel helyet a leltárban." or "Make room in your inventory.")
            else
                say(language == "hu" and "A petláda most nem nyitható ki." or "The pet chest cannot be opened now.")
            end
        end
    end
end
