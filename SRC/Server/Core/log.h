#ifndef __INC_LIBTHECORE_LOG_H__
#define __INC_LIBTHECORE_LOG_H__

#ifdef __cplusplus
#include "Logging.hpp"
#endif

#ifdef __cplusplus
extern "C"
{
#endif
    extern int log_init(void);
    extern void log_destroy(void);
    extern void log_rotate(void);

	extern void log_set_level(unsigned int level);

	extern void log_set_expiration_days(unsigned int days);

    extern void pt_log(const char *format, ...);

#ifdef __cplusplus
}

template <typename... Args>
inline void sys_err(fmt::format_string<Args...> fmt, Args&&... args)
{
	LOG_ERROR(fmt, std::forward<Args>(args)...);
}
#endif

#endif
