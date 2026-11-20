quest rune begin
	state start begin
		when login or levelup with pc.get_level() >= 42 begin
			local lang = pc.get_language()
			say("Susanoo system")
			say("Susanoo system unlocked")
			cmdchat("rune 2")
			set_state(gotoinfo)
		end
	end

	state gotoremind begin
		when login or levelup with pc.get_level() >= 42 begin
			local lang = pc.get_language()
			say("Beta test")
			cmdchat("rune 2")
			set_state(gotoinfo)
		end
	end

	state gotoinfo begin
		when login begin
			if pc.get_level() >= 42 then
				cmdchat("rune 2")
			else
				set_state(gotoremind)
			end
		end

		when letter begin
			send_letter("Susanoo");
		end

		when button or info begin
			local lang = pc.get_language()
			say_title("Susanoo")
			say("Press button J to open the window")
			say("U get all items for Susanoo")
			wait()
			say_title("Susanoo")
			say("")
			local ok, reason = pc.typed_item_exchange({}, {
				{19, 210, 0, 1}, {19, 211, 0, 1}, {19, 212, 0, 1}, {19, 213, 0, 1},
				{19, 214, 0, 1}, {19, 215, 0, 1}, {19, 216, 0, 1},
			}, 0)
			if not ok then
				if reason == "inventory_full" then
					say("Szabadits fel helyet a leltarban. / Make room in your inventory.")
				else
					say("A jutalom most nem veheto at. / The reward cannot be claimed right now.")
				end
				return
			end
			cmdchat("rune 1")
			clear_letter()
			set_state(__COMPLETE__)
		end
	end

	state __COMPLETE__ begin
		when login begin
			if pc.get_level() >= 42 then
				cmdchat("rune 1")
			else
				set_state(gotoinfo)
			end
		end
	end
end
