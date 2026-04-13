#pragma once

class CHARACTER;
class CItem;

namespace item_change
{
	// Returns true if the item use was handled (and UseItem should stop).
	bool HandleUse(CHARACTER* ch, CItem* item);
}
