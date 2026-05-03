import uiscriptlocale

window = {
	"name" : "CostumeEquipmentDialog",
	"x" : 0,
	"y" : 0,
	"width" : 167,
	"height" : 189,
	"children" :
	(
		{
			"name" : "ExpandButton",
			"type" : "button",
			"x" : 2,
			"y" : 20,
			"default_image" : "d:/ymir work/ui/game/belt_inventory/btn_expand_normal.tga",
			"over_image" : "d:/ymir work/ui/game/belt_inventory/btn_expand_over.tga",
			"down_image" : "d:/ymir work/ui/game/belt_inventory/btn_expand_down.tga",
			"disable_image" : "d:/ymir work/ui/game/belt_inventory/btn_expand_disabled.tga",
		},
		{
			"name" : "CostumeEquipmentLayer",
			"x" : 0,
			"y" : 2,
			"width" : 167,
			"height" : 187,
			"children" :
			(
				{
					"name" : "MinimizeButton",
					"type" : "button",
					"x" : -2,
					"y" : 20,
					"default_image" : "d:/ymir work/ui/game/belt_inventory/btn_minimize_normal.tga",
					"over_image" : "d:/ymir work/ui/game/belt_inventory/btn_minimize_over.tga",
					"down_image" : "d:/ymir work/ui/game/belt_inventory/btn_minimize_down.tga",
					"disable_image" : "d:/ymir work/ui/game/belt_inventory/btn_minimize_disabled.tga",
				},
				{
					"name" : "CostumeEquipmentBoard",
					"type" : "board",
					"style" : ("attach", "float"),
					"x" : 10,
					"y" : 0,
					"width" : 157,
					"height" : 187,
					"children" :
					(
						{
							"name" : "Costume_Base",
							"type" : "image",
							"x" : 0,
							"y" : 0,
							"image" : "d:/ymir work/ui/costume_bg2.tga",
							"children" :
							(
								{
									"name" : "CostumeEquipmentSlot",
									"type" : "slot",
									"x" : 4,
									"y" : 3,
									"width" : 155,
									"height" : 187,
									"slot" : (
										{"index":11, "x":58, "y":55, "width":32, "height":64}, # Costume
										{"index":12, "x":58, "y": 9, "width":32, "height":32}, # Hair
										{"index":17, "x":106, "y":10, "width":32, "height":32}, # Sash
										{"index":18, "x":10, "y":24, "width":32, "height":96}, # Weapon
										{"index":19, "x":106, "y":55, "width":32, "height":32}, # Skin Pet
										{"index":20, "x":106, "y":99, "width":32, "height":32}, # Skin Mount
										{"index":21, "x":60, "y":142, "width":32, "height":32}, # Effetto Armatura
										{"index":22, "x":10, "y":142, "width":32, "height":32}, # Effetto Arma
									),
								},
							),
						},
					),
				},
			),
		},
	),
}