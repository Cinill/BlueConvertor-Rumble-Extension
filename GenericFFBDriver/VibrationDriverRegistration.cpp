#include "stdafx.h"
#include <fstream>
#include <filesystem>

#define DRIVER_x86 "GenericFFBDriver32.dll"
#define DRIVER_x64 "GenericFFBDriver64.dll"

#ifdef _WIN64
#define IS_WIN64 TRUE
#else
#define IS_WIN64 IsWow64()
#endif

using LPFN_ISWOW64PROCESS = BOOL(WINAPI *)(HANDLE, PBOOL);
LPFN_ISWOW64PROCESS fnIsWow64Process;

using LPFN_GETSYSTEMWOW64DIRECTORY = UINT(WINAPI *)(LPSTR, UINT);
LPFN_GETSYSTEMWOW64DIRECTORY fnGetSystemWow64Directory;

BOOL IsWow64()
{
	BOOL bIsWow64 = FALSE;

	//IsWow64Process is not available on all supported versions of Windows.
	//Use GetModuleHandle to get a handle to the DLL that contains the function
	//and GetProcAddress to get a pointer to the function if available.

	fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(
		GetModuleHandle(TEXT("kernel32")), "IsWow64Process");

	if (nullptr != fnIsWow64Process)
	{
		if (!fnIsWow64Process(GetCurrentProcess(), &bIsWow64))
		{
			HRESULT hr = HRESULT_FROM_WIN32(GetLastError());

			_ASSERT(hr == S_OK);
		}
	}
	return bIsWow64;
}


int RunCommand(LPSTR cmd)
{
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	// Start the child process. 
	if (!CreateProcessA(nullptr, // No module name (use command line)
	                    cmd, // Command line
	                    nullptr, // Process handle not inheritable
	                    nullptr, // Thread handle not inheritable
	                    FALSE, // Set handle inheritance to FALSE
	                    0, // No creation flags
	                    nullptr, // Use parent's environment block
	                    nullptr, // Use parent's starting directory 
	                    &si, // Pointer to STARTUPINFO structure
	                    &pi) // Pointer to PROCESS_INFORMATION structure
	)
	{
		printf("CreateProcess failed (%d).\n", GetLastError());
		return FALSE;
	}

	// Wait until child process exits.
	WaitForSingleObject(pi.hProcess, INFINITE);

	// Close process and thread handles. 
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return TRUE;
}

STDAPI RegisterVibrationDriver(void)
{
	BOOL isWin64 = IS_WIN64;
	char systemPath[MAX_PATH];
	char cmdLine[MAX_PATH];
	char modulePath[MAX_PATH];

	// Finding dll directory
	strcpy_s(modulePath, "");

	HMODULE hModule = GetModuleHandleA(DRIVER_x86);
	if (hModule == nullptr)
		hModule = GetModuleHandleA(DRIVER_x64);

	if (hModule != nullptr)
	{
		if (GetModuleFileNameA(hModule, modulePath, MAX_PATH) > 0)
		{
			char tmpBuffer[MAX_PATH];
			sprintf_s(tmpBuffer, "%s\\..\\", modulePath);

			GetFullPathNameA(tmpBuffer, MAX_PATH, modulePath, nullptr);
		}
	}

	if (isWin64)
	{
		// Must register both x86 and x64 dlls

		// x64 registration
		GetSystemDirectoryA(systemPath, MAX_PATH);
		sprintf_s(cmdLine, "\"%s\\regsvr32.exe\" /s \"%s%s\"", systemPath, modulePath, DRIVER_x64);
		RunCommand(cmdLine);

		// x86 registration
		fnGetSystemWow64Directory = (LPFN_GETSYSTEMWOW64DIRECTORY)GetProcAddress(
			GetModuleHandleA("kernel32"), "GetSystemWow64DirectoryA");
		fnGetSystemWow64Directory(systemPath, MAX_PATH);
		sprintf_s(cmdLine, "\"%s\\regsvr32.exe\" /s \"%s%s\"", systemPath, modulePath, DRIVER_x86);
		RunCommand(cmdLine);
	}
	else
	{
		// Register x86 only
		GetSystemDirectoryA(systemPath, MAX_PATH);
		sprintf_s(cmdLine, "\"%s\\regsvr32.exe\" /s \"%s%s\"", systemPath, modulePath, DRIVER_x64);
		RunCommand(cmdLine);
	}

	return S_OK;
}
