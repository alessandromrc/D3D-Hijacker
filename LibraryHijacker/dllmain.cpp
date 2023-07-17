#include "pch.h"
#include <windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <CString>
#include <atlstr.h>
#include "../MinHook/include/MinHook.h"

bool hijacked = false;

typedef HMODULE(WINAPI* TYPE_LoadLibraryA)(LPCSTR);
TYPE_LoadLibraryA g_loadLibraryA_original = NULL;
HMODULE WINAPI LoadLibraryA_replacement(_In_ LPCSTR lpFileName)
{
	if (hijacked == false) // avoid doing it again as our wrapper module might actually load real d3d11.dll!
	{
		if (strstr(lpFileName, "D3D11") != nullptr || strstr(lpFileName, "d3d11"))
		{
			char path[MAX_PATH];
			if (GetModuleFileNameA(NULL, path, MAX_PATH) != 0)
			{
				char dllPath[MAX_PATH];
				strcpy_s(dllPath, path);
				char* lastSlash = strrchr(dllPath, '\\');
				if (lastSlash != NULL)
				{
					*lastSlash = '\0';
					strcat_s(dllPath, "\\d3d11.dll");
					lpFileName = dllPath;
					hijacked = true;
				}
			}
		}
	}
	return g_loadLibraryA_original(lpFileName);
}

bool installHooks()
{
	// Initialize MinHook.
	if (MH_Initialize() != MH_OK)
	{
		return false;
	}
	if (MH_CreateHook(
		(LPVOID)LoadLibraryA,
		(LPVOID)LoadLibraryA_replacement,
		(LPVOID*)&g_loadLibraryA_original
	) != MH_OK)
	{
		return false;
	}
	if (MH_EnableHook((LPVOID)LoadLibraryA) != MH_OK)
	{
		return false;
	}

	return true;
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	HANDLE hProcess = GetCurrentProcess();
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		installHooks();
		std::thread([]() {
			if (hijacked)
			{
				if (MH_DisableHook(MH_ALL_HOOKS) == MH_OK)
				{
					MH_Uninitialize();
					HMODULE hModule = GetModuleHandle(NULL);
					FreeLibraryAndExitThread(hModule, 0);
				}
			}
			}).detach();
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}