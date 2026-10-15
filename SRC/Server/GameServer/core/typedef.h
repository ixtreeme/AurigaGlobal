#ifndef __INC_METIN_II_GAME_TYPEDEF_H__
#define __INC_METIN_II_GAME_TYPEDEF_H__
#include <unordered_set>

class DESC;
typedef DESC* LPDESC;

class CLIENT_DESC;
typedef CLIENT_DESC* LPCLIENT_DESC;

class DESC_P2P;
typedef DESC_P2P* LPDESC_P2P;

typedef struct regen* LPREGEN;
typedef struct regen_exception* LPREGEN_EXCEPTION;

class SECTREE;
typedef SECTREE* LPSECTREE;
typedef std::list<LPSECTREE> LPSECTREE_LIST;

class SECTREE_MAP;
typedef SECTREE_MAP* LPSECTREE_MAP;

typedef struct pixel_position_s
{
	int32_t x, y, z;
} PIXEL_POSITION;
//#ifndef itertype
//#define itertype(v) __typeof((v).begin())
//#endif

#endif /* __INC_METIN_II_GAME_TYPEDEF_H__ */

