#ifndef __INC_METIN_II_GAME_TYPEDEF_H__
#define __INC_METIN_II_GAME_TYPEDEF_H__
#include <unordered_set>

class DESC;
#ifdef USE_DEBUG_PTR
typedef DebugPtr<DESC> LPDESC;
#else
typedef DESC* LPDESC;
#endif

class CLIENT_DESC;
#ifdef USE_DEBUG_PTR
typedef DebugPtr<CLIENT_DESC> LPCLIENT_DESC;
#else
typedef CLIENT_DESC* LPCLIENT_DESC;
#endif

class DESC_P2P;
#ifdef USE_DEBUG_PTR
typedef DebugPtr<DESC_P2P> LPDESC_P2P;
#else
typedef DESC_P2P* LPDESC_P2P;
#endif

typedef struct regen* LPREGEN;
typedef struct regen_exception* LPREGEN_EXCEPTION;

class SECTREE;
#ifdef USE_DEBUG_PTR
typedef DebugPtr<SECTREE> LPSECTREE;
#else
typedef SECTREE* LPSECTREE;
#endif
typedef std::list<LPSECTREE> LPSECTREE_LIST;

class SECTREE_MAP;
#ifdef USE_DEBUG_PTR
typedef DebugPtr<SECTREE_MAP> LPSECTREE_MAP;
#else
typedef SECTREE_MAP* LPSECTREE_MAP;
#endif

class CDungeon;
#ifdef USE_DEBUG_PTR
typedef DebugPtr<CDungeon> LPDUNGEON;
#else
typedef CDungeon* LPDUNGEON;
#endif

typedef struct pixel_position_s
{
	int32_t x, y, z;
} PIXEL_POSITION;
//#ifndef itertype
//#define itertype(v) __typeof((v).begin())
//#endif

#endif /* __INC_METIN_II_GAME_TYPEDEF_H__ */

