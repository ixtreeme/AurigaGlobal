quest change_yang begin
	state start begin
		when "catalog.10.noname_1a9d93083e50".typed_use begin
			if not pc.can_warp() then
				syschat(gameforge[pc.get_language()].reset_scroll._35)
				return
			end

			local ok, reason = pc.typed_item_redeem({
				{19, 210, 0, 1}, -- Itachi Sharingan
				{19, 211, 0, 1}, -- Mangekyou Sharingan
				{19, 212, 0, 1}, -- Madara Sharingan
				{19, 213, 0, 1}, -- Sasuke Sharingan
				{19, 214, 0, 1}, -- Kakashi Sharingan
				{19, 215, 0, 1}, -- Sarada Sharingan
				{19, 216, 0, 1}, -- Rinnegan
			}, 0)
			if not ok then
				if reason == "inventory_full" then
					syschat("Nincs eleg szabad hely a runakhoz.")
				else
					syschat("A runakat most nem lehet atvenni.")
				end
			end
		end
	end
end
