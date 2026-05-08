import uiscriptlocale

# 12 x 16 = 192 slot
SLOT_X_COUNT = 12
SLOT_Y_COUNT = 16

SLOT_STEP = 32
SLOT_BLOCK = 33

TITLE_H = 30
TOP_Y = 30
PADDING = 5

LIST_W = 170
SCROLL_W = 12
GAP = 8

GRID_W = SLOT_BLOCK * SLOT_X_COUNT
GRID_H = SLOT_BLOCK * SLOT_Y_COUNT

GRID_X = PADDING + LIST_W + 2 + SCROLL_W + GAP
WIN_W = PADDING + LIST_W + 2 + SCROLL_W + GAP + GRID_W + PADDING
WIN_H = TOP_Y + GRID_H + 10

window = {
	"name": "MountInventoryWindow",

	"x": SCREEN_WIDTH - 400,
	"y": SCREEN_HEIGHT - 600,

	"style": ("movable", "float",),

	"width": WIN_W,
	"height": WIN_H,

	"type": "board",

	"children":
	(
		{
			"name": "ExpandBtn",
			"type": "button",
			"x": 78,
			"y": 15,
		},

		{
			"name": "MountInventoryLayer",
			"type": "board",
			"style": ("attach", "float"),
			"x": 0,
			"y": 0,
			"width": WIN_W,
			"height": WIN_H,

			"children":
			(
				{
					"name": "MinimizeBtn",
					"type": "button",
					"x": 2,
					"y": 15,
					"width": 10,
				},

				{
					"name": "MountInventoryBoard",
					"type": "board",
					"style": ("attach", "float"),
					"x": 0,
					"y": 0,
					"width": WIN_W,
					"height": WIN_H,

					"children":
					(
						{
							"name": "TitleBar",
							"type": "titlebar",
							"style": ("attach",),
							"x": 0,
							"y": 0,
							"width": WIN_W - 2,
							"color": "yellow",
							"children":
							(
								{
									"name": "TitleName",
									"type": "text",
									"x": 0,
									"y": 3,
									"text": "Account Mount Inventory",
									"text_horizontal_align": "center",
									"horizontal_align": "center",
								},
							),
						},

						# LEFT LIST
						{
							"name": "MountInventoryList",
							"type": "listboxex",
							"x": PADDING,
							"y": TOP_Y,
							"width": LIST_W,
							"height": GRID_H,
							"itemsize_x": LIST_W,
							"itemsize_y": 16,
							"itemstep": 16,
							"viewcount": int(GRID_H / 16),
						},

						# LIST SCROLLBAR
						{
							"name": "MountInventoryListScroll",
							"type": "scrollbar",
							"x": PADDING + LIST_W + 2,
							"y": TOP_Y,
							"size": GRID_H,
						},

						# RIGHT GRID (SLOTS)
						{
							"name": "MountInventorySlot",
							"type": "grid_table",
							"x": GRID_X,
							"y": TOP_Y,
							"start_index": 0,
							"x_count": SLOT_X_COUNT,
							"y_count": SLOT_Y_COUNT,
							"x_step": SLOT_STEP,
							"y_step": SLOT_STEP,
							"image": "d:/ymir work/ui/public/Slot_Base.sub",
						},

						{
							"name": "MountInventoryCloseButton",
							"type": "button",
							"x": WIN_W - 22 - 24,
							"y": 0,
							"text": "X",
							"default_image": "d:/ymir work/ui/public/small_button_01.sub",
							"over_image": "d:/ymir work/ui/public/small_button_02.sub",
							"down_image": "d:/ymir work/ui/public/small_button_03.sub",
						},
					),
				},
			),
		},
	),
}
