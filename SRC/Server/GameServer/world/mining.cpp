#include "stdafx.h"
#include "mining.h"

bool mining::IsVeinOfOre(uint32_t race)
{
    return (race >= 20047 && race <= 20059) || (race >= 30301 && race <= 30306);
}
