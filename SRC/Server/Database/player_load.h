#pragma once

#include <common/tables.h>

// Normalize the legacy skill slot and reconstruct the unspent secondary
// budget from every supported secondary skill when loading a player.
void NormalizeSecondarySkills(TPlayerTable& player);
