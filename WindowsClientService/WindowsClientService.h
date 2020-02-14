#pragma once

#ifndef _WCS_H
#define _WCS_H

#include <Windows.h>


extern SERVICE_STATUS gSvcStatus;
extern SERVICE_STATUS_HANDLE gSvcStatusHandle;
extern HANDLE ghSvcStopEvent;

VOID WINAPI SvcMain(DWORD dwArgc, LPTSTR* lpszArgv);

VOID SvcInit(DWORD dwArgc, LPTSTR* lpszArgv);

VOID ReportSvcStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint);

int SendingMessage();

_declspec(dllexport) void start();

#endif
