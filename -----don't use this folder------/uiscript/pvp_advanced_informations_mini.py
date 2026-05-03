FACE = "d:/ymir work/ui/game/windows/box_face.sub"

window = {
	"name" : "PvPInformationsMini",
	"x" : 0,
	"y" : 0,
	"style" : ("float",),
	"width" : 53,
	"height" : 53,
	"children" :
	(
		{
			"name" : "Board",
			"type" : "window", 
			"x" : 0,
			"y" : 0,
			"width" : 53,
			"height" : 53,
			"children" :
			(
				{
					"name" : "CharacterSlot",
					"type" : "image",
					"x" : 3,
					"y" : 0,
					"width" : 32,
					"height" : 32,
					"image" : FACE,
				},
				{
					"name" : "CharacterIcon",
					"type" : "image",
					"x" : 7,
					"y" : 5,
					"width" : 32,
					"height" : 32,
					"image" : FACE,
				},
			),
		},
	),
}

