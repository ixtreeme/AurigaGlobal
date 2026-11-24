quest metin_event begin
    state start begin
        function MAP_DATA()
            return {
                { idx = 41, coords = { -- map1
                    {488,499}, {326,310}, {112,167}, {459,141},
                    {711,125}, {980,104}, {1139,139}, {1157,362},
                    {1062,524}, {1082,788}, {1153,1048}, {1053,1222},
                    {848,1181}, {661,1234}, {465,1210}, {295,998},
                    {506,917}, {714,1029}, {896,1029}, {1129,828},
                    {1073,461}, {908,431}, {896,295}, {707,244},
                    {556,320}, {397,280}, {337,433}, {118,358},
                    {45,629}, {51,763}, {149,878}, {765,667}
                }},
                { idx = 363, coords = { -- orcs
                    {153,104}, {642,137}, {1431,85}, {858,267},
                    {527,336}, {187,388}, {128,571}, {111,993},
                    {299,920}, {222,643}, {397,611}, {562,768},
                    {748,708}, {915,661}, {986,815}, {1210,856},
                    {1146,696}, {1420,989}, {1454,1299}, {1370,1447},
                    {1136,1441}, {1214,1276}, {1083,1150}, {783,1108},
                    {713,1176}, {465,1356}, {309,1167}, {91,1103},
                    {87,1145}, {165,1402}, {1262,611}, {1077,538}
                }},
            }
        end

        function METIN_VNUM()
            return 8060
        end

        function get_random_coord(coords)
            local r = number(1, table.getn(coords))
            return coords[r]
        end

        when 20023.chat."Helloween Event indítása" begin
            if not pc.is_gm() then
                say("Nincs jogosultságod az esemény indítására.")
                return
            end
            if game.get_event_flag("metin_event_active") == 1 then
                say("Az esemény már folyamatban van!")
                return
            end

            game.set_event_flag("metin_event_active", 1)
            say("Halloween Event elindítva CH99-en.")

            local maps = metin_event.MAP_DATA()
            for i = 1, table.getn(maps) do
                local mi = maps[i]
                for j = 1, 20 do
                    local pos = metin_event.get_random_coord(mi.coords)
                    spawn_mob_in_map(metin_event.METIN_VNUM(), 1, false, mi.idx, pos[1], pos[2])
                end
            end
        end

        when kill with not npc.is_pc() and npc.get_race() == 8060 and game.get_event_flag("metin_event_active") == 1 begin
            local map_idx = pc.get_map_index()
            server_timer("respawn_8060", 5*60, map_idx)

            -- Példa drop (testre szabhatod):
            game.drop_typed_item_with_ownership(10, 1013, 0, 1)
            game.drop_typed_item_with_ownership(10, 910, 0, 5)
        end

        when 20023.chat."Helloween Event leállítása" begin
            if not pc.is_gm() then return end
            if game.get_event_flag("metin_event_active") == 0 then
                say("Az esemény nincs elindítva.")
                return
            end
            game.set_event_flag("metin_event_active", 0)
            say("Event leállítva CH99-en.")
        end

        when respawn_8060.server_timer begin
            local map_idx = get_server_timer_arg()
            local maps = metin_event.MAP_DATA()
            for i = 1, table.getn(maps) do
                local mi = maps[i]
                if mi.idx == map_idx then
                    local pos = metin_event.get_random_coord(mi.coords)
                    spawn_mob_in_map(metin_event.METIN_VNUM(), 1, false, mi.idx, pos[1], pos[2])
                    break
                end
            end
        end
    end
end
