#include <cstdint>
#include <windows.h>

static uint8_t abCRCMagicCube[32] = { 3 };
static uint8_t abCRCXorTable[8] = { 102, 30, 188, 44, 39, 201, 43, 5 };
static uint8_t bMagicCubeIdx = 0;


uint8_t GetProcessCRCMagicCubePiece()
{
    uint8_t bPiece = uint8_t(abCRCMagicCube[bMagicCubeIdx] ^ abCRCXorTable[bMagicCubeIdx]);

    if (!(++bMagicCubeIdx & 7))
        bMagicCubeIdx = 0;

    return bPiece;
}

