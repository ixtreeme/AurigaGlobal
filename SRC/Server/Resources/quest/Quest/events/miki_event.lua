quest random_50010_take begin
    state start begin
        when 20384.chat."Zoknik/Socks" begin
            local count = pc.typed_item_count(10, 993, 0)
            if count == nil then
                say("A leltár most nem elérhető. / Inventory is busy.")
                return
            end
            if count < 100 then
                say("Az ajándékhoz 100 zokni kell. / You need 100 stockings for a gift.")
                return
            end
            say("Beváltasz 100 zoknit egy ajándékra? / Exchange 100 stockings for a gift?")
            if select("Igen / Yes", "Mégsem / Cancel") ~= 1 then return end

            -- Category, item, refinement, minimum, maximum, weight and display names.
            local rewards = {
                {3, 693, 0, 1, 1, 1, "Eek Gyurüje", "Eek's Ring"},
                {10, 1059, 0, 1, 1, 1, "Tökéletes lélekkő", "Perfect Soul Stone"},
                {22, 52, 0, 1, 1, 1, "Mágikus kő", "Magic Stone"},
                {6, 564, 0, 1, 1, 1, "Sárkány pikkely", "Dragon Scales"},
                {6, 565, 0, 1, 1, 1, "Sárkány karom", "Dragon Claw"},
                {3, 739, 0, 1, 1, 1, "Hatalmas Nyalóka (1h)", "Mighty Lolly (1h)"},
                {15, 682, 0, 1, 1, 9, "Karácsonyi Oroszlán", "Xmass Lion", announce = true},
                {16, 9, 0, 20, 20, 1, "Utalvány (1 SÉ)", "Voucher (1 DC)"},
                {16, 9, 0, 30, 30, 1, "Utalvány (1 SÉ)", "Voucher (1 DC)"},
                {16, 9, 0, 40, 40, 1, "Utalvány (1 SÉ)", "Voucher (1 DC)"},
                {10, 964, 0, 1, 1, 1, "Utalvány (100 SÉ)", "Voucher (100 DC)"},
                {16, 9, 0, 10, 50, 40, "Utalvány (1 SÉ)", "Voucher (1 DC)"},
                {5, 346, 0, 2000, 2000, 1, "Megbűvölő tárgy", "Enchant Item"},
                {5, 772, 0, 1, 1, 1, "Legendás Bónuszoló", "Legendary bonus"},
                {5, 773, 0, 1, 1, 1, "Legendás Megváltoztató", "Legendary Changer"},
                {6, 63, 0, 5, 5, 1, "Birodalom Rúna", "Empire Rune"},
                {6, 63, 0, 10, 10, 1, "Birodalom Rúna", "Empire Rune"},
                {3, 738, 0, 1, 1, 1, "Félhold gyuru (2h", "Crescent Moon Ring (2h)"},
                {10, 895, 0, 1, 1, 1, "Csavart kulcs", "Twisted Key"},
                {10, 1352, 0, 1, 1, 1, "Nemere jegy", "Nemere Ticket"},
                {10, 965, 0, 5, 5, 1, "Nexoria Coin (10m Yang)"},
                {10, 965, 0, 10, 10, 1, "Nexoria Coin (10m Yang)"},
                {10, 965, 0, 20, 20, 1, "Nexoria Coin (10m Yang)"},
                {10, 1364, 0, 1, 1, 4, "Karácsonyi Run jegy", "Christmas Dungeon (Medium)"},
                {10, 1277, 0, 1, 1, 1, "Fejlesztoital (10%)", "Improvement Potion (10%)"},
                {10, 1276, 0, 1, 1, 1, "Fejlesztoital (5%)", "Improvement Potion (5%)"},
                {20, 45, 0, 1, 1, 1, "Különleges kosztüm bónusz (5%)", "Special Costume Enchant  (5%)"},
                {20, 46, 0, 1, 1, 1, "Különleges kosztüm bónusz (10%)", "Special Costume Enchant  (10%)"},
                {5, 698, 0, 1, 1, 1, "Talizmán megerösíto", "Reinforces Talismans"},
                {5, 699, 0, 1, 1, 1, "Talizmán büvölo", "Enchant Talismans"},
                {5, 625, 0, 1, 1, 1, "Jelmez büvölö", "Enchant Costumes"},
                {5, 626, 0, 1, 1, 1, "Fagyasztási bónusz", "Freeze Bonus"},
                {5, 627, 0, 1, 1, 1, "fagyasztási bónusz csere", "Refreeze Bonus"},
                {5, 628, 0, 1, 1, 1, "Felolvasztási bónusz", "Thaw Bonus"},
                {20, 40, 0, 1, 1, 1, "Vállpánt kosztüm büvölö", "Enchant Costumes Stole"},
                {3, 747, 0, 1, 1, 1, "Mágikus nyalóka (15d)", "Magic Lolly (15g)"},
                {10, 1278, 0, 1, 1, 1, "Fejlesztoital (15%)", "Improvement Potion (15%)"},
                {7, 254, 5, 1, 1, 1, "Paranoia Köve +5", "Stone of Paranoia+5"},
                {7, 259, 5, 1, 1, 1, "Háború Köve +5", "Stone of War+5"},
                {7, 265, 5, 1, 1, 1, "Metin elleni ko+5", "Stone of Metin+5"},
                {7, 271, 6, 1, 1, 1, "Boss Elleni  Kö+5", "Stone of Boss+5"},
                {7, 283, 5, 1, 1, 1, "Állatok Köve +5", "Stone of Animal+5"},
                {10, 1239, 0, 1, 1, 1, "Frank (30d)", "Frank (30g)"},
                {4, 1893, 0, 1, 1, 1, "Red Fegyver Effect III (7d)", "Red Weapon Effect III (7g)"},
                {4, 1895, 0, 1, 1, 1, "Red Páncél Effect III (7d)", "Red Armor Effect III (7g)"},
                {3, 468, 0, 1, 1, 1, "Ryuga Pepsi-je", "Profiq's Pepsi"},
                {5, 651, 0, 1, 1, 1, "A bölcsesség fozete (1h)", "Potion of Wisdom (1h)"},
                {5, 700, 0, 1, 1, 1, "Talizmánerösíto", "Strengthens Talismans"},
                {8, 400, 0, 1, 1, 1, "Sámán ládája", "Shamano's Chest"},
                {8, 399, 0, 1, 1, 1, "Sura ládája", "Sura's Chest"},
                {8, 398, 0, 1, 1, 1, "Ninja ládája", "Ninja's Chest"},
                {8, 397, 0, 1, 1, 1, "Harcos ládája", "Warrior's Chest"},
                {19, 178, 0, 1, 1, 1, "Úri Vállszalag Jelmez (alapveto)", "Costumes Lord Sash (basic)"},
                {10, 907, 0, 1, 1, 1, "Sárga Zsugorított fej", "Shrunken Head"},
                {10, 912, 0, 1, 1, 1, "Pók-kulcs", "Arachnids Key"},
                {10, 919, 0, 1, 1, 1, "Tündérek köve", "Stone of the Dryads"},
                {10, 924, 0, 1, 1, 1, "Owl jegy", "Owl pass"},
                {10, 1347, 0, 1, 1, 1, "Permiso de Meley", "Meley-Erlaubnis"},
                {10, 1351, 0, 1, 1, 1, "Razador jegy", "Razador Ticket"},
                {3, 776, 0, 1, 1, 1, "Ochao jegy", "Ochao ticket)"},
                {10, 1385, 0, 1, 1, 1, "Rúna jegy", "Ticket of the Runes"},
                {4, 1516, 0, 1, 1, 3, "Mikulás kardja (30 napos)", "Sword of Santa Claus (30 Days)"},
                {4, 1517, 0, 1, 1, 3, "Mikulás torei (30 napos)", "Santa's Daggers (30 Days)"},
                {4, 1518, 0, 1, 1, 3, "Mikulás íj (30 napos)", "Santa's Bow (30 Days)"},
                {4, 1519, 0, 1, 1, 3, "Mikulás nagykardja (30 napos)", "Santa's Greatsword (30 Days)"},
                {4, 1520, 0, 1, 1, 3, "Mikulás harangja (30 napos)", "Santa's Bell (30 Days)"},
                {4, 1523, 0, 1, 1, 3, "Mikulás páncélja (f) (30d)", "Santa's Armor (M) (30d)"},
                {4, 1522, 0, 1, 1, 3, "Mikulás páncélja (n) (30d)", "Santa's Armor (F) (30d)"},
                {4, 1574, 0, 1, 1, 3, "Mikulás haja (n) (30e)", "Santa's Hair (F) (30d)"},
                {4, 1575, 0, 1, 1, 3, "Mikulás haja (f) (30d)", "Santa's Hair (M) (30)"},
                {19, 225, 0, 1, 1, 1, "Karácsonyi Szárny (7napos)", "Christmas Stole Costume (7G)"},
                {5, 702, 0, 1, 1, 1, "Narancssárga Ampulla", "Orange Ampoule"},
                {5, 703, 0, 1, 1, 1, "Sárga Ampulla", "Yellow Ampoule"},
                {5, 704, 0, 1, 1, 1, "Kék Ampulla", "Blue Ampoule"},
                {5, 705, 0, 1, 1, 1, "Red Ampulla", "Red Ampoule"},
                {5, 706, 0, 1, 1, 1, "Zöld Ampulla", "Green Ampoule"},
                {5, 707, 0, 1, 1, 1, "Violet Ampulla", "Violet Ampoule"},
                {5, 708, 0, 1, 1, 1, "Kék Harmat", "Blue Dew"},
                {5, 709, 0, 1, 1, 1, "Narancssárga Harmat", "Orange Dew"},
                {5, 710, 0, 1, 1, 1, "Fehér Harmat", "White Dew"},
                {5, 711, 0, 1, 1, 1, "Sárga Harmat", "Yellow Dew"},
                {5, 712, 0, 1, 1, 1, "Red Harmat", "Red Dew"},
                {5, 313, 0, 5, 5, 1, "Ördögűző tekercs", "Exorcism Scroll"},
                {5, 313, 0, 10, 10, 1, "Ördögűző tekercs", "Exorcism Scroll"},
                {5, 139, 0, 5, 5, 1, "Koncentrált olvasás", "Concentrated Reading"},
                {5, 139, 0, 10, 10, 1, "Koncentrált olvasás", "Concentrated Reading"},
                {20, 41, 0, 1, 1, 1, "Sharingan"},
                {20, 42, 0, 1, 1, 1, "Amaterasu"},
                {20, 43, 0, 1, 1, 1, "Rinnegan"},
                {6, 678, 0, 1, 1, 1, "Kutatói Elixír +", "Researcher's Elixir +"},
                {6, 679, 0, 1, 1, 1, "Kutatói Elixír", "Researcher's Elixir"},
                {3, 526, 0, 1, 1, 1, "Textil öv+0", "Linen Belt +0"},
                {10, 1061, 0, 1, 1, 1, "Elhárítás lélekkő", "Stone of Souls Counterattack"},
                {5, 657, 0, 1, 1, 1, "Tolvaj kesztyu (1h)", "Thief's Gloves (1h)"},
                {5, 655, 0, 1, 1, 1, "Tapasztalat gyürü (1h)", "Experience Ring (1h)"},
                {8, 393, 0, 1, 1, 1, "Fokozó könyv láda", "Boost Book Chest"},
                {8, 392, 0, 1, 1, 1, "Elhárító könyv láda", "Ward Book Chest"},
                {10, 1365, 0, 1, 1, 1, "Karácsonyi Run jegy (Prémium)", "Christmas Dungeon (Extreme)"},
                {22, 31, 0, 10, 10, 1, "Áldástekercs", "Blessing Scroll"},
                {22, 31, 0, 20, 20, 1, "Áldástekercs", "Blessing Scroll"},
                {5, 789, 0, 1, 1, 1, "Arany gyümölcs", "Golden Fruit"},
                {3, 161, 5, 1, 1, 1, "Mennyei Könny Fülbev.+5", "Heaven's Tear Earrings +5"},
            }
            local total = 0
            for _, reward in ipairs(rewards) do total = total + reward[6] end
            local roll = number(1, total)
            local reward
            for _, candidate in ipairs(rewards) do
                roll = roll - candidate[6]
                if roll <= 0 then
                    reward = candidate
                    break
                end
            end
            local category, item, level = reward[1], reward[2], reward[3]
            local amount = number(reward[4], reward[5])
            local ok, reason = pc.typed_item_exchange(
                {{10, 993, 0, 100}}, {{category, item, level, amount}}, 0)
            if not ok then
                if reason == "inventory_full" then
                    say("Szabadíts fel helyet a leltárban. / Make room in your inventory.")
                elseif reason == "materials" then
                    say("Már nincs nálad 100 zokni. / You no longer have 100 stockings.")
                else
                    say("A beváltás most nem sikerült. / Exchange unavailable.")
                end
                return
            end

            local fallback = reward[7]
            if pc.get_language() ~= "hu" and reward[8] then fallback = reward[8] end
            if reward.announce then notice_all(2242, pc.get_name(), fallback) end
            say("Sikeresen beváltottál 100 zoknit. / Exchanged 100 stockings.")
            say_reward(string.format("%d × %s", amount, game.typed_item_name(category, item, fallback)))
        end
    end
end
