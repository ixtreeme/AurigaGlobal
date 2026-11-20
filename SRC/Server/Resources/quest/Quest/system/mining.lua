quest mining begin
    state start begin
        when 20047.click or
             20048.click or
             20049.click or
             20050.click or
             20051.click or
             20052.click or
             20053.click or
             20054.click or
             20055.click or
             20056.click or
             20057.click or
             20058.click or
             20059.click or
             30301.click or
             30302.click or
             30303.click or
             30304.click or
             30305.click or
             30306.click begin
            if pc.is_mount() then
                notice("Send back your Mount first!")
            else
                pc.mining()
            end
        end
    end
end
