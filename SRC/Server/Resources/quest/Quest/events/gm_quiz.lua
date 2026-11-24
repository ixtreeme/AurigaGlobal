quest gm_quiz begin
    state start begin
        function ask_question()
            say_title("Kvíz mester")
            say("Válaszd ki a kérdést:")
            local c = select(
                "Kézzel írom be",
                "Gondoltam egy számra 1-100",
                "Gondoltam egy számra 1-1000",
                "Mégse"
            )

            if c == 1 then
                say_reward("Írd be a kérdést (pl.: Gondoltam egy számra):")
                local q = input()
                if q == "" then
                    return nil, nil, nil
                end
                return q, nil, nil                -- kézi kérdés
            elseif c == 2 then
                return nil, 1, 100                -- sablon: 1–100 (q == nil!)
            elseif c == 3 then
                return nil, 1, 1000               -- sablon: 1–1000 (q == nil!)
            end

            return nil, nil, nil                  -- Mégse
        end

        function choose_reward()
            say_title("Kvíz mester")
            local choice = select("Auriga Coin (10m Yang) ×1", "Auriga Coin (10m Yang) ×2",
                "Legendás Bónuszoló ×1", "Legendás Megváltoztató ×1", "Egyedi tárgy", "Mégse")
            if choice == 1 or choice == 2 then
                return "catalog.16.auriga_coin_10m_yang_fa8097313261", choice
            elseif choice == 3 then
                return "catalog.5.legendary_bonus_043af960ba20", 1
            elseif choice == 4 then
                return "catalog.5.legendary_changer_8e172122f0ec", 1
            elseif choice == 5 then
                say("Írd be a típusos katalógusban szereplő tárgykódot.")
                local code = input()
                if not code or not game.typed_item_key(code) then
                    say("Ez a tárgy nem található az aktív katalógusban.")
                    return
                end
                say("Hány darabot kapjon a nyertes?")
                local count = tonumber(input())
                if not count or count ~= math.floor(count) or count <= 0 or count > 4294967295 then
                    say("Adj meg pozitív egész darabszámot.")
                    return
                end
                return code, count
            end
        end

        when 20086.chat."Kviz mester" begin
            say_title("Kvíz mester")
            if not pc.is_gm() then
                say("Ezt a panelt csak GM használhatja.")
                return
            end
            local action = select("Új kvíz indítása", "Futó kvíz lezárása", "Kilépés")
            if action == 2 then
                game.cancel_typed_quiz()
                return
            elseif action ~= 1 then
                return
            end
            local question, minimum, maximum = gm_quiz.ask_question()
            if not question and not minimum then return end
            say_reward("Írd be a helyes választ egész számként.")
            local answer = math.tointeger(input())
            if not answer or (minimum and (answer < minimum or answer > maximum)) then
                say("A helyes válasz legyen egész szám a megadott tartományban.")
                return
            end

            local code, count = gm_quiz.choose_reward()
            if not code then return end
            local category, id = game.typed_item_key(code)
            if not category then
                say("A jutalom jelenleg nem elérhető.")
                return
            end
            say_title("Összegzés")
            say(question or string.format("Számtalálós (%d–%d)", minimum, maximum))
            say(string.format("Helyes válasz: %d", answer))
            say_reward(string.format("Jutalom: %s ×%d", game.typed_item_name(category, id), count))
            if select("Küldés", "Mégse") == 1 then
                game.start_typed_quiz(answer, code, count, maximum or 0, question or "")
            end
        end
    end
end
