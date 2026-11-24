-- ____________________________
-- |   |   |   |   |   |   |   |
-- | R | A | Z | O | R | 9 | 3 |--2025.12.29
-- |___|___|___|___|___|___|___|

quest leveldown_66_to_65 begin
    state start begin
        when "catalog.10.level_reset_66_to_65_c0f8a197800f".typed_use begin
            local result = pc.typed_reset_level()
            if result == 1 then
                say("A szinted visszaállt 65-re. / Your level is 65.")
            elseif result == 2 then
                say("Vedd le a cipőt, karkötőt, nyakláncot és fülbevalót!")
                say("Take off your shoes, bracelet, necklace, and earrings!")
            elseif result == 3 then
                say("Ezt a tárgyat csak Map1-en használhatod. / Use this item on Map1.")
            elseif result == 4 then
                say("Ezt a tárgyat 66-os szinten használhatod. / Requires level 66.")
            elseif result == 5 then
                say("Nincs elég szabad pontod a visszaállításhoz.")
                say("Szükséges: 1 képességpont, 1 mellékképességpont")
                say("és a szinten belüli haladástól függően 0–3 státuszpont.")
                say("Requires 1 unspent skill point, 1 sub-skill point")
                say("and 0–3 stat points, depending on level progress.")
            else
                say("A tárgy most nem használható. / Item unavailable.")
            end
        end
    end
end
