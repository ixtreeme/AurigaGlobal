quest cooldown_reset begin
    state start begin
        when "catalog.10.dungeon_cd_reset_0f8f1b52bcef".typed_use begin
            if pc.in_dungeon() then
                say("A boss halála után a visszaszámlálás újraindul.")
            end
            say_title("Kazamata várakozásának törlése")
            local dungeons = {
                { key = "owl_zone_razor93",               name = "Bagoly Dungeon" },
                { key = "dungeonorchi",                   name = "Ork Dungeon" },
                { key = "slime_dungeon",                  name = "Slime Dungeon" },
                { key = "dungeon_spider",                 name = "Pok Dungeon" },
                { key = "catacombe",                      name = "Katakomba" },
                { key = "dragonlair_zone",                name = "Sarkany Lair" },
                { key = "meleylair_zone",                 name = "Meley Lair" },
                { key = "razador_zone",                   name = "Razador Run" },
                { key = "nemere_zone",                    name = "Nemere Run" },
                { key = "temple_ochao",                   name = "Ochao Templom" },
                { key = "enchanted_forest",               name = "Varazs Erdo" },
                { key = "nightmare_zone_razor93_prepare", name = "Nightmare Zone" },
                { key = "dungeonpyramid_razor93",         name = "Pyramid Run" },
                { key = "tritontemple_razor93",           name = "Triton Temple" },
                { key = "valentine_dungeon",              name = "Valentin-napi kazamata" },
                { key = "halloween2022",                name = "Bloody Cathedral" },
                { key = "vikingdungeon_zone",           name = "Viking Dungeon" },
            }
            local active = {}
            local menu = {}
            for _, dungeon in ipairs(dungeons) do
                local deadline = pc.getf(dungeon.key, "cooldown")
                local remaining = deadline - get_global_time()
                if remaining > 0 then
                    local hours = math.floor(remaining / 3600)
                    local minutes = math.floor((remaining % 3600) / 60)
                    active[#active + 1] = { key = dungeon.key, name = dungeon.name, deadline = deadline }
                    menu[#menu + 1] = string.format("%s - %d óra %d perc", dungeon.name, hours, minutes)
                end
            end
            if #active == 0 then
                say("Egyik kazamatának sincs aktív várakozása.")
                return
            end
            menu[#menu + 1] = "Mégsem"
            say("Válaszd ki a kazamatát.")
            local selected = active[select_table(menu)]
            if not selected then return end
            local current = pc.getf(selected.key, "cooldown")
            if current <= get_global_time() then
                say("A kazamata várakozása közben lejárt.")
                return
            end
            if current ~= selected.deadline then
                say("Közben megváltozott a várakozási idő. Nyisd meg újra a listát.")
                return
            end
            if pc.typed_reset_dungeon_cooldown(selected.key) ~= 1 then
                say("A tárgy most nem használható.")
                return
            end
            say(string.format("A(z) %s várakozása törölve.", selected.name))
        end
    end
end
