#define __LIBTHECORE__
#include "stdafx.h"
#include "Logging.hpp"

#ifndef _WIN32
#define SYSLOG_FILENAME "syslog"
#define SYSERR_FILENAME "syserr"
#define PTS_FILENAME "PTS"
#else
#define SYSLOG_FILENAME "syslog.txt"
#define SYSERR_FILENAME "syserr.txt"
#define PTS_FILENAME "PTS.txt"
#endif

typedef struct log_file_s * LPLOGFILE;
typedef struct log_file_s LOGFILE;

struct log_file_s
{
    char*	filename;
    FILE*	fp;

    int		last_hour;
    int		last_day;
};

LPLOGFILE	log_file_sys = NULL;
LPLOGFILE	log_file_err = NULL;
LPLOGFILE	log_file_pt = NULL;
static char	log_dir[32] = { 0, };
unsigned int log_keep_days = 3;

LPLOGFILE log_file_init(const char* filename, const char *openmode);
void log_file_destroy(LPLOGFILE logfile);
void log_file_rotate(LPLOGFILE logfile);
void log_file_check(LPLOGFILE logfile);
void log_file_set_dir(const char *dir);

static unsigned int log_level_bits = 0;

namespace
{
constexpr std::size_t LOG_FORMAT_BUFFER_SIZE = 4096;

void FormatLegacyLogMessage(char* buffer, std::size_t bufferSize, const char* format, va_list args)
{
	if (!buffer || bufferSize == 0)
		return;

	if (!format)
	{
		buffer[0] = '\0';
		return;
	}

	vsnprintf(buffer, bufferSize, format, args);
	buffer[bufferSize - 1] = '\0';
}
}

void log_set_level(unsigned int bit)
{
	log_level_bits |= bit;
}

void log_unset_level(unsigned int bit)
{
	log_level_bits &= ~bit;
}

void log_set_expiration_days(unsigned int days)
{
	log_keep_days = days;
}

int log_get_expiration_days(void)
{
	return log_keep_days;
}

int log_init(void)
{
	log_file_set_dir("./log");

	try
	{
		logging::Init(log_dir);
	}
	catch (const std::exception&)
	{
		return false;
	}

	log_file_pt = log_file_init(PTS_FILENAME, "w");
	if (NULL == log_file_pt)
		return false;

	return true;

}

void log_destroy(void)
{
	log_file_destroy(log_file_pt);

	log_file_sys = NULL;
	log_file_err = NULL;
	log_file_pt = NULL;

	logging::Shutdown();
}

void log_rotate(void)
{
	log_file_check(log_file_pt);

	log_file_rotate(log_file_pt);
}

void _sys_err(const char *func, int line, const char *format, ...)
{
	va_list args;
	char buf[LOG_FORMAT_BUFFER_SIZE];

	va_start(args, format);
	FormatLegacyLogMessage(buf, sizeof(buf), format, args);
	va_end(args);

	if (auto errorLogger = logging::GetErrorLogger())
	{
		errorLogger->log(
			spdlog::source_loc{"", line, func ? func : ""},
			spdlog::level::err,
			"{}",
			buf);
		errorLogger->flush();
	}

	if (auto logger = logging::GetLogger())
		logger->error("SYSERR: {}: {}", func ? func : "", buf);
}

static char sys_log_header_string[33] = { 0, };

void sys_log_header(const char *header)
{
	strncpy(sys_log_header_string, header, 32);
}

void sys_log(unsigned int bit, const char *format, ...)
{
	va_list	args;
	char buf[LOG_FORMAT_BUFFER_SIZE];

	if (bit != 0 && !(log_level_bits & bit))
		return;

	va_start(args, format);
	FormatLegacyLogMessage(buf, sizeof(buf), format, args);
	va_end(args);

	if (auto logger = logging::GetLogger())
		logger->info("{}{}", sys_log_header_string, buf);
}

void pt_log(const char *format, ...)
{
	va_list	args;

	if (!log_file_pt)
		return;

	va_start(args, format);
	vfprintf(log_file_pt->fp, format, args);
	va_end(args);

	fputc('\n', log_file_pt->fp);
	fflush(log_file_pt->fp);
}

LPLOGFILE log_file_init(const char * filename, const char * openmode)
{
	LPLOGFILE logfile;
	FILE * fp;
	struct tm curr_tm;
	time_t time_s;

	time_s = time(0);
	curr_tm = *localtime(&time_s);

	fp = fopen(filename, openmode);

	if (!fp)
		return NULL;

	logfile = (LPLOGFILE) malloc(sizeof(LOGFILE));
#ifdef _WIN32
	logfile->filename = _strdup(filename);
#else
	logfile->filename = strdup(filename);
#endif
	logfile->fp	= fp;
	logfile->last_hour = curr_tm.tm_hour;
	logfile->last_day = curr_tm.tm_mday;

	return (logfile);
}

void log_file_destroy(LPLOGFILE logfile)
{
	if (logfile == NULL) {
		return;
	}

	if (logfile->filename)
	{
		free(logfile->filename);
		logfile->filename = NULL;
	}

	if (logfile->fp)
	{
		fclose(logfile->fp);
		logfile->fp = NULL;
	}

	free(logfile);
}

void log_file_check(LPLOGFILE logfile)
{
	if (logfile == NULL)
		return;

	struct stat	sb;

	if (stat(logfile->filename, &sb) != 0 && errno == ENOENT)
	{
		fclose(logfile->fp);
		logfile->fp = fopen(logfile->filename, "a+");
	}
}

void log_file_delete_old(const char *filename)
{
	struct stat		sb;
	int			num1, num2;
	char		buf[32];
	char		system_cmd[64];
	struct tm		new_tm;

	if (stat(filename, &sb) == -1)
	{
		return;
	}

	if (!S_ISDIR(sb.st_mode))
		return;

	new_tm = *tm_calc(NULL, -(int)log_keep_days);
	sprintf(buf, "%04d%02d%02d", new_tm.tm_year + 1900, new_tm.tm_mon + 1, new_tm.tm_mday);
	num1 = atoi(buf);
#ifndef _WIN32
	{
		struct dirent ** namelist;
		int	n;

		n = scandir(filename, &namelist, 0, alphasort);

		if (n < 0)
			perror("scandir");
		else
		{
			char name[MAXNAMLEN+1];

			while (n-- > 0)
			{
				strncpy(name, namelist[n]->d_name, 255);
				name[255] = '\0';

				free(namelist[n]);

				if (*name == '.')
					continue;

				if (!isdigit(*name))
					continue;

				num2 = atoi(name);

				if (num2 <= num1)
				{
					sprintf(system_cmd, "rm -rf %s/%s", filename, name);
					system(system_cmd);

					sys_log(0, "%s: SYSTEM_CMD: %s", __FUNCTION__, system_cmd);
					fprintf(stderr, "%s: SYSTEM_CMD: %s %s:%d %s:%d\n", __FUNCTION__, system_cmd, buf, num1, name, num2);
				}
			}
		}

		free(namelist);
	}
#else
	{
		WIN32_FIND_DATA fdata;
		HANDLE			hFind;
		if ((hFind = FindFirstFile(filename, &fdata)) != INVALID_HANDLE_VALUE)
		{
			do
			{
				if (!isdigit(*fdata.cFileName))
					continue;

				num2 = atoi(fdata.cFileName);

				if (num2 <= num1)
				{
					sprintf(system_cmd, "del %s\\%s", filename, fdata.cFileName);
					system(system_cmd);

					sys_log(0, "SYSTEM_CMD: %s", system_cmd);
				}
			}
			while (FindNextFile(hFind, &fdata));
		}
	}
#endif
}

void log_file_rotate(LPLOGFILE logfile)
{
	if (logfile == NULL)
		return;

    struct tm	curr_tm;
	time_t	time_s;
    char	dir[128];
    char	system_cmd[128];

    time_s = time(0);
    curr_tm = *localtime(&time_s);

    if (curr_tm.tm_mday != logfile->last_day)
    {
	struct tm new_tm;
	new_tm = *tm_calc(&curr_tm, -3);

#ifndef _WIN32
	sprintf(system_cmd, "rm -rf %s/%04d%02d%02d", log_dir, new_tm.tm_year + 1900, new_tm.tm_mon + 1, new_tm.tm_mday);
#else
	sprintf(system_cmd, "del %s\\%04d%02d%02d", log_dir, new_tm.tm_year + 1900, new_tm.tm_mon + 1, new_tm.tm_mday);
#endif
	system(system_cmd);

	sys_log(0, "SYSTEM_CMD: %s", system_cmd);

	logfile->last_day = curr_tm.tm_mday;
    }

    if (curr_tm.tm_hour != logfile->last_hour)
    {
		struct stat	stat_buf;
		snprintf(dir, 128, "%s/%04d%02d%02d", log_dir, curr_tm.tm_year + 1900, curr_tm.tm_mon + 1, curr_tm.tm_mday);

		if (stat(dir, &stat_buf) != 0 || S_ISDIR(stat_buf.st_mode))
		{
#ifdef _WIN32
			CreateDirectory(dir, NULL);
#else
			mkdir(dir, S_IRWXU);
#endif
		}

		sys_log(0, "SYSTEM: LOG ROTATE (%04d-%02d-%02d %d)", 
				curr_tm.tm_year + 1900, curr_tm.tm_mon + 1, curr_tm.tm_mday, logfile->last_hour);

		fclose(logfile->fp);

#ifndef _WIN32
		snprintf(system_cmd, 128, "mv %s %s/%s.%02d", logfile->filename, dir, logfile->filename, logfile->last_hour);
#else
		snprintf(system_cmd, 128, "move %s %s\\%s.%02d", logfile->filename, dir, logfile->filename, logfile->last_hour);
#endif
		system(system_cmd);

		logfile->last_hour = curr_tm.tm_hour;
		logfile->fp = fopen(logfile->filename, "a+");
	}
}

void log_file_set_dir(const char *dir)
{
    strcpy(log_dir, dir);
    log_file_delete_old(log_dir);
}
