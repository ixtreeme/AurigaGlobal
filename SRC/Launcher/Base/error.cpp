#include "StdAfx.h"

#include <cstdio>
#include <ctime>
#include <winsock.h>
#include <imagehlp.h>

FILE * fException;


BOOL CALLBACK EnumerateLoadedModulesProc(PCSTR ModuleName, ULONG ModuleBase, ULONG ModuleSize, PVOID UserContext)
{
	uint32_t offset = *((uint32_t*)UserContext);

	if (offset >= ModuleBase && offset <= ModuleBase + ModuleSize)
	{
		fprintf(fException, "%s", ModuleName);
		//__idx += sprintf(__msg+__idx, "%s", ModuleName);
		return FALSE;
	}
	else
		return TRUE;
}

LONG __stdcall EterExceptionFilter(_EXCEPTION_POINTERS* pExceptionInfo)
{
	HANDLE		hProcess	= GetCurrentProcess();
	HANDLE		hThread		= GetCurrentThread();

	fException = fopen("ErrorLog.txt", "wt");
	if (fException)
	{
		char module_name[256];
		time_t module_time;

		HMODULE hModule = GetModuleHandle(nullptr);

		GetModuleFileName(hModule, module_name, sizeof(module_name));
		module_time = (time_t)GetTimestampForLoadedLibrary(hModule);

		fprintf(fException, "Module Name: %s\n", module_name);
		fprintf(fException, "Time Stamp: %s\n", ctime(&module_time));
		fprintf(fException, "\n");
		fprintf(fException, "Exception Type: 0x%08x\n", pExceptionInfo->ExceptionRecord->ExceptionCode);
		fprintf(fException, "\n");


		CONTEXT&	context		= *pExceptionInfo->ContextRecord;

		fprintf(fException, "eax: 0x%08llu\tebx: 0x%08llu\n", context.Rax, context.Rbx);
		fprintf(fException, "ecx: 0x%08llu\tedx: 0x%08llu\n", context.Rcx, context.Rdx);
		fprintf(fException, "esi: 0x%08llu\tedi: 0x%08llu\n", context.Rsi, context.Rdi);
		fprintf(fException, "ebp: 0x%08llu\tesp: 0x%08llu\n", context.Rbp, context.Rsp);
		fprintf(fException, "\n");


		STACKFRAME stackFrame = {0,};
		stackFrame.AddrPC.Offset	= context.Rip;
		stackFrame.AddrPC.Mode		= AddrModeFlat;
		stackFrame.AddrStack.Offset	= context.Rsp;
		stackFrame.AddrStack.Mode	= AddrModeFlat;
		stackFrame.AddrFrame.Offset	= context.Rbp;
		stackFrame.AddrFrame.Mode	= AddrModeFlat;

		for (int i=0; i < 512 && stackFrame.AddrPC.Offset; ++i)
		{
			if (StackWalk(IMAGE_FILE_MACHINE_I386, hProcess, hThread, &stackFrame, &context, nullptr, nullptr, nullptr, nullptr) != FALSE)
			{
				fprintf(fException, "0x%08llu\t", stackFrame.AddrPC.Offset);
				//__idx+=sprintf(__msg+__idx, "0x%08x\t", stackFrame.AddrPC.Offset);
				EnumerateLoadedModules(hProcess, (PENUMLOADED_MODULES_CALLBACK) EnumerateLoadedModulesProc, &stackFrame.AddrPC.Offset);
				fprintf(fException, "\n");

				//__idx+=sprintf(__msg+__idx,  "\n");
			}
			else
			{
				break;
			}
		}

		fprintf(fException, "\n");
		fflush(fException);

		fclose(fException);
		fException = nullptr;

		WinExec("errorlog.exe",SW_SHOW);


	}

	return EXCEPTION_EXECUTE_HANDLER;
}

void SetEterExceptionHandler()
{
	SetUnhandledExceptionFilter(EterExceptionFilter);
}