#pragma once
constexpr size_t SELF_HASH_LEN = 32; // 32 byte-os BLAKE2b elég, kevesebb adat a binárisban

bool LG_EnsureLaunchedFromPatcher();
//bool ::ComputeSelfTextHash(unsigned char out[SELF_HASH_LEN]);