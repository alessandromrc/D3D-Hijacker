#include "pch.h"
#include <windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <CString>
#include <atlstr.h>
#include "../MinHook/include/MinHook.h"
#include <mutex>

bool hijacked_d3d11 = false;
bool hijacked_dxgi = false;
std::mutex g_mutex;

typedef HMODULE(WINAPI* TYPE_LoadLibraryA)(LPCSTR);
TYPE_LoadLibraryA g_loadLibraryA_original = NULL;
HMODULE WINAPI LoadLibraryA_replacement(_In_ LPCSTR lpFileName)
{
	if (hijacked_d3d11== false) // avoid doing it again as our wrapper module might actually load real d3d11.dll!
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
					if (PathFileExistsA(dllPath))
					{
						lpFileName = dllPath;
						hijacked_d3d11 = true;
					}
					else
					{
						lpFileName = "C:\\Windows\\System32\\d3d11.dll"; // Fallback to original d3d11.dll
					}
				}
			}
		}
	}

	if (hijacked_dxgi == false) // avoid doing it again as our wrapper module might actually load real dxgi.dll!
	{
		if (strstr(lpFileName, "dxgi") != nullptr || strstr(lpFileName, "DXGI"))
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
					strcat_s(dllPath, "\\dxgi.dll");
					if (PathFileExistsA(dllPath)) // check if dxgi.dll exists
					{
						lpFileName = dllPath;
						hijacked_dxgi = true;
					}
				}
			}
		}
	}

	return g_loadLibraryA_original(lpFileName);
}


bool installHooks() {
	if (MH_Initialize() != MH_OK) {
		return false; // Proper error handling for MH_Initialize
	}

	if (MH_CreateHook(&LoadLibraryA, &LoadLibraryA_replacement, reinterpret_cast<LPVOID*>(&g_loadLibraryA_original)) != MH_OK) {
		MH_Uninitialize();
		return false; // Proper error handling for MH_CreateHook
	}

	if (MH_EnableHook(&LoadLibraryA) != MH_OK) {
		MH_Uninitialize();
		return false; // Proper error handling for MH_EnableHook
	}

	return true;
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
) {
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
		if (installHooks()) {
			std::thread([]() {
				std::this_thread::sleep_for(std::chrono::seconds(5)); // Adjust the delay if needed
				std::lock_guard<std::mutex> lock(g_mutex);
				if (hijacked_d3d11 || hijacked_dxgi) {
					if (MH_DisableHook(MH_ALL_HOOKS) == MH_OK) {
						MH_Uninitialize();
						HMODULE hModule = GetModuleHandle(NULL);
						FreeLibraryAndExitThread(hModule, 0);
					}
				}
				}).detach();
		}
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
