quest heavenly_refine begin
    state start begin
        when 20072.chat."Refine Stones" begin
            -- Native ore and refined item keys, in the original menu order.
            local menu = {
                {10, 1074, 6, 693, "Mennyei könny érc", "Mennyei könny"},
                {10, 1062, 6, 681, "Gyémánt kő", "Gyémánt"},
                {10, 1063, 6, 682, "Borostyánkő", "Borostyánkő"},
                {10, 1064, 6, 683, "Megkövesedett törzs", "Fosszilis fa"},
                {10, 1065, 6, 684, "Réz érc", "Réz"},
                {10, 1066, 6, 685, "Ezüst érc", "Ezüst"},
                {10, 1067, 6, 686, "Arany érc", "Arany"},
                {10, 1068, 6, 687, "Jáde érc", "Jáde"},
                {10, 1069, 6, 688, "Ébenfa érc", "Ébenfa"},
                {10, 1070, 6, 689, "Igazgyöngy darab", "Gyöngyszem"},
                {10, 1071, 6, 690, "Fehérarany érc", "Fehér arany"},
                {10, 1072, 6, 691, "Kristály érc", "Kristály"},
                {10, 1073, 6, 692, "Kvarz érc", "Ametiszt"},
                {10, 1075, 6, 694, "Lélekkristály érc", "Lélekkristály"},
                {10, 1076, 6, 695, "Rubin érc", "Rubint"},
                {10, 1077, 6, 696, "Gránát érc", "Gránát"},
                {10, 1078, 6, 697, "Smaragd érc", "Smaragd"},
                {10, 1079, 6, 698, "Zafír érc", "Zafír"},
                {10, 1080, 6, 699, "Turmalin érc", "Turmalin"},
            }

            say_title("Mennyei finomítás")
            addimage(30, 10, "ore.tga")
            say("Válaszd ki, mit szeretnél finomítani.")

            local options = {}
            for i = 1, #menu do
                options[i] = menu[i][6]
            end
            options[#menu + 1] = "Mégse"
            local recipe = menu[select_table(options)]
            if not recipe then
                return
            end

            local yang_cost = 500000
            local ore_count = 100
            local chance = 60
            say_title(recipe[6])
            say_reward(string.format("%d db %s", ore_count, recipe[5]))
            say_reward("1 db Metinkő")
            say_reward("500 000 Yang")
            say_reward(string.format("Siker esélye %d%%", chance))
            say("Sikertelen finomításkor is elfogy az érc, a Metinkő és a Yang.")
            if select("Finomítás", "Mégse") ~= 1 then
                return
            end

            if pc.get_gold() < yang_cost then
                say("Nincs elég Yangod.")
                return
            end
            if (pc.typed_item_count(recipe[1], recipe[2], 0, true) or 0) < ore_count then
                say(string.format("Nincs nálad %d db %s.", ore_count, recipe[5]))
                return
            end

            -- Native socket-stone keys and levels accepted by this altar.
            -- Preserve the original order when multiple eligible stones are present.
            local stones = {
                {20, 0}, {21, 0}, {22, 0}, {23, 0}, {24, 0}, {25, 0}, {26, 0}, {27, 0},
                {28, 0}, {29, 0}, {30, 0}, {31, 0}, {32, 0}, {33, 0}, {34, 0}, {35, 0},
                {36, 0}, {204, 1}, {20, 1}, {21, 1}, {22, 1}, {23, 1}, {24, 1}, {25, 1},
                {26, 1}, {27, 1}, {28, 1}, {29, 1}, {30, 1}, {31, 1}, {32, 1}, {33, 1},
                {205, 2}, {206, 2}, {207, 2}, {208, 2}, {20, 2}, {21, 2}, {22, 2}, {23, 2},
                {24, 2}, {25, 2}, {26, 2}, {27, 2}, {28, 2}, {29, 2}, {30, 2}, {31, 2},
                {32, 2}, {33, 2}, {209, 3}, {210, 3}, {211, 3}, {212, 3}, {20, 3}, {21, 3},
                {22, 3}, {23, 3}, {24, 3}, {25, 3}, {26, 3}, {27, 3}, {28, 3}, {29, 3},
                {30, 3}, {31, 3}, {32, 3}, {33, 3},
            }
            local stone
            for i = 1, #stones do
                local candidate = stones[i]
                if (pc.typed_item_count(7, candidate[1], candidate[2], true) or 0) > 0 then
                    stone = candidate
                    break
                end
            end
            if not stone then
                say("Nincs nálad a finomításhoz használható Metinkő.")
                return
            end

            local costs = {{recipe[1], recipe[2], 0, ore_count}, {7, stone[1], stone[2], 1}}
            local success = number(1, 100) <= chance
            local rewards = success and {{recipe[3], recipe[4], 0, 1}} or {}
            local committed, reason = pc.typed_item_exchange(costs, rewards, yang_cost, true)
            if not committed then
                if reason == "inventory_full" then
                    say("Nincs hely a finomított tárgynak.")
                elseif reason == "materials" then
                    say("A finomításhoz már nincs elég érced vagy Metinköved.")
                elseif reason == "yang" then
                    say("Nincs elég Yangod.")
                else
                    say("A finomítást nem sikerült elindítani. Ellenőrizd a készleted, majd próbáld újra.")
                end
                return
            end
            if success then
                say("Sikeres finomítás. Megkaptad: "..recipe[6]..".")
            else
                say("A finomítás sikertelen volt.")
            end
        end
    end
end
