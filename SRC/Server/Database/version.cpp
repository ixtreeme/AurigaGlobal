#include <stdio.h>
#include <stdlib.h>
#include "Core/Logging.hpp"

void WriteVersion()
{
#ifndef _WIN32
	FILE* fp(fopen("VERSION.txt", "w"));

	if (NULL != fp)
	{
		fprintf(fp, "__DB_VERSION__: %s\n", __DB_VERSION__);
		fprintf(fp, "%s@%s:%s\n", __USER__, __HOSTNAME__, __PWD__);
		fclose(fp);
	}
	else
	{
		LOG_ERROR("cannot open VERSION.txt");
		exit(0);
	}
#endif
}

