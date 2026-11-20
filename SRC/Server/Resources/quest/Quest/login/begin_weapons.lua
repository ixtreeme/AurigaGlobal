quest beginer_weapons begin
    state start begin
        when "catalog.10.equipment_chest_f17334250c36".typed_use begin
            if pc.get_level() < 1 then return end
            if not pc.can_warp() then
                syschat(gameforge[pc.get_language()].reset_scroll._35)
                return
            end

            local by_class = {
                [0] = {{1, 58, 9, 1}, {1, 180, 9, 1}, {2, 123, 9, 1}},
                [1] = {{1, 121, 9, 1}, {1, 471, 9, 1}, {1, 58, 9, 1}, {2, 143, 9, 1}},
                [2] = {{1, 58, 9, 1}, {2, 163, 9, 1}},
                [3] = {{1, 213, 9, 1}, {1, 249, 9, 1}, {2, 183, 9, 1}},
            }
            local rewards = by_class[pc.get_job()]
            if not rewards then return end
            local common = {
                {22, 50, 0, 40},
                {5, 723, 0, 1},
                {5, 726, 0, 1},
                {5, 727, 0, 1},
                {5, 728, 0, 1},
                {5, 731, 0, 1},
                {5, 732, 0, 1},
                {5, 733, 0, 1},
                {5, 734, 0, 1},
                {5, 735, 0, 1},
                {5, 737, 0, 1},
            }
            for _, reward in ipairs(common) do rewards[#rewards + 1] = reward end
            local ok, reason = pc.typed_item_redeem(rewards, 0)
            if not ok then
                if reason == "inventory_full" then
                    say("Szabadíts fel helyet a leltárban. / Make room in your inventory.")
                else
                    say("A láda most nem nyitható ki. / This chest cannot be opened now.")
                end
            end
        end
    end
end
