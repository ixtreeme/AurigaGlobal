--[[
-- Init - The Teleport Ring Class
]]

TeleportRing = {};

TeleportRing.ReturnData = function()
	if (TeleportRing.data == nil) then
		TeleportRing.data = {
			{
				["TeleportMapName"] = "Map1",
				["TeleportHasOtherIndexs"] = true,
				["TeleportTheOtherIndexes"] = {
					{["OtherIndexName"] = "Jinno", ["x"] = 963100, ["y"] = 276000},
					{["OtherIndexName"] = "Chunjo", ["x"] = 63200, ["y"] = 166700},
					{["OtherIndexName"] = "Shinsoo", ["x"] = 473900, ["y"] = 954600}},
				["TeleportLevelCheck"] = false,
				["TeleportItemCheck"] = false
			},
			{
				["TeleportMapName"] = "Map2",
				["TeleportHasOtherIndexs"] = true,
				["TeleportTheOtherIndexes"] = {
					{["OtherIndexName"] = "Jinno", ["x"] = 863800, ["y"] = 246000},
					{["OtherIndexName"] = "Chunjo", ["x"] = 145700, ["y"] = 239800},
					{["OtherIndexName"] = "Shinsoo", ["x"] = 352300, ["y"] = 882700}},
				["TeleportLevelCheck"] = false,
				["TeleportItemCheck"] = false
			},
			{
				["TeleportMapName"] = "Seungryong Valley",
				["TeleportHasOtherIndexs"] = false,
				["TeleportLevelCheck"] = false,
				["TeleportItemCheck"] = false,
				["x"] = 332700,
				["y"] = 746300
			},
			{
				["TeleportMapName"] = "Desert (Event map)",
				["TeleportHasOtherIndexs"] = false,
				["TeleportLevelCheck"] = false,
				["TeleportItemCheck"] = false,
				["x"] = 296300,
				["y"] = 547500
			},
			{
				["TeleportMapName"] = "Sohan",
				["TeleportHasOtherIndexs"] = false,
				["TeleportLevelCheck"] = false,
				["TeleportItemCheck"] = false,
				["x"] = 436100,
				["y"] = 215100
			},
			{
				["TeleportMapName"] = "Demon Tower - Devil's Catacombs",
				["TeleportHasOtherIndexs"] = true,
				["TeleportTheOtherIndexes"] = {
					{["OtherIndexName"] = "Demon Tower", ["x"] = 590400, ["y"] = 110500},
					{["OtherIndexName"] = "Devil's Catacombs", ["x"] = 591900, ["y"] = 100200}},
				["TeleportLevelCheck"] = false,
				["TeleportItemCheck"] = false
			},
			{
				["TeleportMapName"] = "Fire Map",
				["TeleportHasOtherIndexs"] = true,
				["TeleportTheOtherIndexes"] = {
					{["OtherIndexName"] = "Middle", ["x"] = 599300, ["y"] = 707200},
					{["OtherIndexName"] = "Metin Spawn 1", ["x"] = 663900, ["y"] = 631600},
					{["OtherIndexName"] = "Metin Spawn 2", ["x"] = 650300, ["y"] = 752600},
					{["OtherIndexName"] = "Metin Spawn 3", ["x"] = 683300, ["y"] = 694800}},
				["TeleportLevelCheck"] = false,
				["TeleportItemCheck"] = false
			},
			{
				["TeleportMapName"] = "World Boss Map",
				["TeleportHasOtherIndexs"] = false,
				["TeleportLevelCheck"] = false,
				["TeleportItemCheck"] = false,
				["x"] = 828100,
				["y"] = 763400
			},
			{
				["TeleportMapName"] = "V3",
				["TeleportHasOtherIndexs"] = false,
				["TeleportLevelCheck"] = true,
				["TeleportLevel"] = {75, 105},
				["TeleportItemCheck"] = false,
				["TeleportItems"] = {"material.verko", 1},
				["x"] = 145300,
				["y"] = 1323600
			},
			{
				["TeleportMapName"] = "V4",
				["TeleportHasOtherIndexs"] = false,
				["TeleportLevelCheck"] = true,
				["TeleportLevel"] = {75, 105},
				["TeleportItemCheck"] = false,
				["TeleportItems"] = {"material.verko", 1},
				["x"] = 181500,
				["y"] = 1220700
			},
			{
				["TeleportMapName"] = "Ghost Forest",
				["TeleportHasOtherIndexs"] = true,
				["TeleportTheOtherIndexes"] = {
					{["OtherIndexName"] = "Beginning", ["x"] = 290400, ["y"] = 5500},
					{["OtherIndexName"] = "The end", ["x"] = 289000, ["y"] = 41900}},
				["TeleportLevelCheck"] = false,
				["TeleportItemCheck"] = false
			},
			{
				["TeleportMapName"] = "Red Forest",
				["TeleportHasOtherIndexs"] = true,
				["TeleportTheOtherIndexes"] = {
					{["OtherIndexName"] = "Beginning", ["x"] = 1119900, ["y"] = 69200},
					{["OtherIndexName"] = "The end", ["x"] = 1120200, ["y"] = 7100}},
				["TeleportLevelCheck"] = false,
				["TeleportItemCheck"] = false
			},
			{
				["TeleportMapName"] = "The spiders' dungeons - level: 35-80", -- Teleport Map Name
				["TeleportHasOtherIndexs"] = true, -- true = citeste "TeleportTheOtherIndexes" de mai jos, adica cand dai click iti arata si alte optiuni de teleportare / false = opusul.
				["TeleportTheOtherIndexes"] = {
					{["OtherIndexName"] = "Spider Dungeon 2", ["x"] = 665600, ["y"] = 435200}, -- Optiunile de teleportare ale Mapei de mai sus (Pot fi adaugate oricate)
					{["OtherIndexName"] = "Spider Dungeon 3", ["x"] = 97600, ["y"] = 571100}},
				["TeleportLevelCheck"] = true, -- true = iti citeste limitele de nivel adaugate mai jos / false = opusul.
				["TeleportLevel"] = {35, 80}, -- nivel minim / nivel maxim (delimiteaza nivelele la care te poti teleporta in mapa respectiva)
				["TeleportItemCheck"] = false
			},
			{
				["TeleportMapName"] = "Beta Regions - Level: 90-120",
				["TeleportHasOtherIndexs"] = true,
				["TeleportTheOtherIndexes"] = {
					{["OtherIndexName"] = "Cape", ["x"] = 1085300, ["y"] = 1783300},
					{["OtherIndexName"] = "Nephrite", ["x"] = 1087400, ["y"] = 1646000},
					{["OtherIndexName"] = "Thunder", ["x"] = 1134700, ["y"] = 1653900},
					{["OtherIndexName"] = "Guatama", ["x"] = 1177600, ["y"] = 1664000}},
				["TeleportLevelCheck"] = true,
				["TeleportLevel"] = {90, 120},
				["TeleportItemCheck"] = false
			},
			{
				["TeleportMapName"] = "Enchanted Forest 95-120",
				["TeleportHasOtherIndexs"] = false,
				["TeleportLevelCheck"] = true,
				["TeleportLevel"] = {95, 120},
				["TeleportItemCheck"] = false,
				["TeleportItems"] = {"material.verko", 1},
				["x"] = 813300,
				["y"] = 1503200
			},
			{
				["TeleportMapName"] = "Finishing Region - Level: 85-120",
				["TeleportHasOtherIndexs"] = true,
				["TeleportTheOtherIndexes"] = {
					{["OtherIndexName"] = "Middle", ["x"] = 38400, ["y"] = 63900},
					{["OtherIndexName"] = "Up", ["x"] = 46900, ["y"] = 45400},
					{["OtherIndexName"] = "Left", ["x"] = 20800, ["y"] = 55700},
					{["OtherIndexName"] = "Down", ["x"] = 45200, ["y"] = 83400}},
				["TeleportLevelCheck"] = true,
				["TeleportLevel"] = {85, 120},
				["TeleportItemCheck"] = false
			},
		};
	end -- if

	return TeleportRing.data;
end -- func

TeleportRing.TeleportIsLevelCheck = function(index)
	local data = TeleportRing.ReturnData();

	return data[index]["TeleportLevelCheck"];
end -- func

TeleportRing.TeleportIsItemsCheck = function(index)
	local data = TeleportRing.ReturnData();

	return data[index]["TeleportItemCheck"];
end -- func

TeleportRing.TeleportMapHasOtherIndex = function(index)
	local data = TeleportRing.ReturnData();

	return data[index]["TeleportHasOtherIndexs"];
end -- func

TeleportRing.InsertWindow = function()
	local data = TeleportRing.ReturnData(); local window = {};

	for index in ipairs(data) do table.insert(window, data[index]["TeleportMapName"]); end -- for
	table.insert(window, "Renunta");

	return window;
end -- func

TeleportRing.DisplayOtherIndexWindow = function(WinIndex)
	local data = TeleportRing.ReturnData(); local OtherWindow = {};

	for index in ipairs(data[WinIndex]["TeleportTheOtherIndexes"]) do table.insert(OtherWindow, data[WinIndex]["TeleportTheOtherIndexes"][index]["OtherIndexName"]); end -- for
	table.insert(OtherWindow, "Renunta");

	return OtherWindow;
end -- func

TeleportRing.isBetweenLevel = function(index)
	local data = TeleportRing.ReturnData(); local pLevel = pc.get_level();

	return pLevel >= data[index]["TeleportLevel"][1] and pLevel <= data[index]["TeleportLevel"][2];
end -- func

local function fare(index)
	local item = TeleportRing.ReturnData()[index]["TeleportItems"]
	if not item or type(item[1]) ~= "string" or type(item[2]) ~= "number"
		or item[2] < 1 or item[2] ~= math.floor(item[2]) then return end
	local category, local_id = game.typed_item_key(item[1])
	if category then return category, local_id, item[2] end
end

TeleportRing.hasItemNeeded = function(index)
	local category, local_id, amount = fare(index)
	return category ~= nil and (pc.typed_item_count(category, local_id, 0, true) or 0) >= amount
end

local function payAndWarp(index, destination)
	if not pc.can_warp() then
		say("Asteapta 10 secunde pentru a te putea teleporta.")
		return false
	end
	if TeleportRing.TeleportIsItemsCheck(index) then
		local category, local_id, amount = fare(index)
		if not category or not pc.typed_item_exchange({{category, local_id, 0, amount}}, {}, 0, true) then
			return false
		end
	end
	return pc.warp(destination.x, destination.y)
end

-- The main data window of the ring

TeleportRing.MainWindow = function()
	local data = TeleportRing.ReturnData(); local InsertedWindowTable = TeleportRing.InsertWindow();

	say_title(string.format("%s:", game.typed_item_name(10, 180, "Teleport Ring")))
	local FirstWindow = select_table(InsertedWindowTable);
	if (FirstWindow == #InsertedWindowTable) then
		return false;
	else
		say_title(string.format("%s:", game.typed_item_name(10, 180, "Teleport Ring")))
		if TeleportRing.TeleportIsLevelCheck(FirstWindow) then
			if (not TeleportRing.isBetweenLevel(FirstWindow)) then
				say(string.format("Nivelul tau trebuie sa fie intre %d si %d pentru[ENTER]a putea intra in aceasta mapa.", data[FirstWindow]["TeleportLevel"][1], data[FirstWindow]["TeleportLevel"][2]))
				return false;
			end -- if
		end -- if

		if TeleportRing.TeleportIsItemsCheck(FirstWindow) then
			if (not TeleportRing.hasItemNeeded(FirstWindow)) then
				local category, local_id, amount = fare(FirstWindow)
				if category then
					say(string.format("Trebuie sa deti x%d - %s pentru[ENTER]a putea intra in aceasta mapa.", amount, game.typed_item_name(category, local_id, "Bloodstone")))
				end
				return false;
			end -- if
		end -- if

		if TeleportRing.TeleportMapHasOtherIndex(FirstWindow) then
			local OtherInsertedWindowTable = TeleportRing.DisplayOtherIndexWindow(FirstWindow);

			local otherWind = select_table(OtherInsertedWindowTable);
			if (otherWind == #OtherInsertedWindowTable) then
				return false;
			else
				return payAndWarp(FirstWindow, data[FirstWindow]["TeleportTheOtherIndexes"][otherWind])
			end
		end
		return payAndWarp(FirstWindow, data[FirstWindow])
	end
end
