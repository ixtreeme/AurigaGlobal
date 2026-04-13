#pragma once



#ifndef _WIN32

#ifndef __cplusplus
typedef unsigned char		bool;
#endif

typedef unsigned int		uint32_t;
typedef int			BOOL;
typedef unsigned char		uint8_t;
typedef unsigned short 		uint16_t;
typedef long			LONG;
typedef unsigned long		ULONG;
typedef int			INT;
typedef unsigned int		UINT;

typedef uint64_t			socket_t;

#else

struct timezone 
{
    int     tz_minuteswest; /* minutes west of Greenwich */
    int     tz_dsttime;     /* type of dst correction */
};

typedef SOCKET			socket_t;



#endif

