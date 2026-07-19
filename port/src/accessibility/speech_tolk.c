#include <windows.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <PR/ultratypes.h>
#include "system.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_speech_backend.h"

typedef void (__cdecl *TolkLoadFn)(void);
typedef bool (__cdecl *TolkIsLoadedFn)(void);
typedef void (__cdecl *TolkUnloadFn)(void);
typedef const wchar_t *(__cdecl *TolkDetectScreenReaderFn)(void);
typedef bool (__cdecl *TolkHasSpeechFn)(void);
typedef bool (__cdecl *TolkHasBrailleFn)(void);
typedef bool (__cdecl *TolkOutputFn)(const wchar_t *text, bool interrupt);
typedef bool (__cdecl *TolkSilenceFn)(void);

static HMODULE g_TolkModule = NULL;
static TolkLoadFn g_TolkLoad = NULL;
static TolkIsLoadedFn g_TolkIsLoaded = NULL;
static TolkUnloadFn g_TolkUnload = NULL;
static TolkDetectScreenReaderFn g_TolkDetectScreenReader = NULL;
static TolkHasSpeechFn g_TolkHasSpeech = NULL;
static TolkHasBrailleFn g_TolkHasBraille = NULL;
static TolkOutputFn g_TolkOutput = NULL;
static TolkSilenceFn g_TolkSilence = NULL;
static s32 g_TolkInitialized = 0;
static s32 g_TolkLoaded = 0;
static s32 g_TolkAvailable = 0;
static char *g_TolkReaderName = NULL;

static char *speechTolkFormatWindowsError(DWORD errornum)
{
	char *message = NULL;
	DWORD length;

	length = FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
					| FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, errornum, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPSTR)&message, 0, NULL);

	if (!length || !message) {
		return NULL;
	}

	while (length && (message[length - 1] == '\r' || message[length - 1] == '\n')) {
		message[--length] = '\0';
	}

	return message;
}

static void speechTolkLogWindowsFailure(const char *operation, DWORD errornum,
		const char *path)
{
	char *message = speechTolkFormatWindowsError(errornum);

	sysLogPrintf(LOG_WARNING, "accessibility speech: %s failed: %s (%lu)",
			operation, message ? message : "unknown Windows error",
			(unsigned long)errornum);
	accessibilityLogEvent("speech", "backend_unavailable",
			"operation=%s windows_error=%lu windows_message=%s path=%s",
			operation, (unsigned long)errornum,
			message ? message : "", path ? path : "");

	if (message) {
		LocalFree(message);
	}
}

static char *speechTolkWideToUtf8(const wchar_t *value, DWORD *errornum)
{
	char *result;
	int length;

	if (!value) {
		return NULL;
	}

	SetLastError(ERROR_SUCCESS);
	length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1,
			NULL, 0, NULL, NULL);

	if (!length) {
		if (errornum) {
			*errornum = GetLastError();
		}
		return NULL;
	}

	result = malloc((size_t)length);

	if (!result) {
		if (errornum) {
			*errornum = ERROR_NOT_ENOUGH_MEMORY;
		}
		return NULL;
	}

	if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1,
			result, length, NULL, NULL)) {
		if (errornum) {
			*errornum = GetLastError();
		}
		free(result);
		return NULL;
	}

	return result;
}

static wchar_t *speechTolkUtf8ToWide(const char *value, s32 *units, DWORD *errornum)
{
	wchar_t *result;
	int length;

	if (!value || !value[0]) {
		if (errornum) {
			*errornum = ERROR_INVALID_DATA;
		}
		return NULL;
	}

	SetLastError(ERROR_SUCCESS);
	length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1,
			NULL, 0);

	if (!length) {
		if (errornum) {
			*errornum = GetLastError();
		}
		return NULL;
	}

	result = malloc((size_t)length * sizeof(*result));

	if (!result) {
		if (errornum) {
			*errornum = ERROR_NOT_ENOUGH_MEMORY;
		}
		return NULL;
	}

	if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1,
			result, length)) {
		if (errornum) {
			*errornum = GetLastError();
		}
		free(result);
		return NULL;
	}

	if (units) {
		*units = length - 1;
	}

	return result;
}

static wchar_t *speechTolkGetDllPath(DWORD *errornum)
{
	wchar_t *path = NULL;
	size_t capacity = 256;
	DWORD length;
	wchar_t *slash;
	size_t directorylength;
	static const wchar_t filename[] = L"Tolk.dll";

	for (;;) {
		wchar_t *resized = realloc(path, capacity * sizeof(*path));

		if (!resized) {
			free(path);
			if (errornum) {
				*errornum = ERROR_NOT_ENOUGH_MEMORY;
			}
			return NULL;
		}

		path = resized;
		SetLastError(ERROR_SUCCESS);
		length = GetModuleFileNameW(NULL, path, (DWORD)capacity);

		if (!length) {
			if (errornum) {
				*errornum = GetLastError();
			}
			free(path);
			return NULL;
		}

		if (length < capacity) {
			path[length] = L'\0';
			break;
		}

		if (capacity > 32768) {
			if (errornum) {
				*errornum = ERROR_BUFFER_OVERFLOW;
			}
			free(path);
			return NULL;
		}

		capacity *= 2;
	}

	slash = wcsrchr(path, L'\\');

	if (!slash) {
		slash = wcsrchr(path, L'/');
	}

	if (!slash) {
		if (errornum) {
			*errornum = ERROR_BAD_PATHNAME;
		}
		free(path);
		return NULL;
	}

	directorylength = (size_t)(slash - path) + 1;
	{
		wchar_t *resized = realloc(path,
				(directorylength + sizeof(filename) / sizeof(filename[0])) * sizeof(*path));

		if (!resized) {
			if (errornum) {
				*errornum = ERROR_NOT_ENOUGH_MEMORY;
			}
			free(path);
			return NULL;
		}

		path = resized;
	}

	memcpy(path + directorylength, filename, sizeof(filename));
	return path;
}

static FARPROC speechTolkResolveExport(const char *name, s32 *missing)
{
	FARPROC result = GetProcAddress(g_TolkModule, name);

	accessibilityLogEvent("speech", "export_resolution",
			"name=%s resolved=%d address=%p", name, result != NULL, result);

	if (!result) {
		(*missing)++;
	}

	return result;
}

static void speechTolkClearFunctions(void)
{
	g_TolkLoad = NULL;
	g_TolkIsLoaded = NULL;
	g_TolkUnload = NULL;
	g_TolkDetectScreenReader = NULL;
	g_TolkHasSpeech = NULL;
	g_TolkHasBraille = NULL;
	g_TolkOutput = NULL;
	g_TolkSilence = NULL;
}

s32 accessibilitySpeechBackendInit(void)
{
	wchar_t *dllpath;
	char *dllpathutf8 = NULL;
	const wchar_t *readername;
	DWORD errornum = ERROR_SUCCESS;
	s32 missing = 0;
	s32 hasspeech;
	s32 hasbraille;
	u64 started;

	if (g_TolkInitialized) {
		return g_TolkAvailable;
	}

	g_TolkInitialized = 1;
	dllpath = speechTolkGetDllPath(&errornum);

	if (!dllpath) {
		speechTolkLogWindowsFailure("resolve executable directory", errornum, "");
		return 0;
	}

	dllpathutf8 = speechTolkWideToUtf8(dllpath, &errornum);
	SetLastError(ERROR_SUCCESS);
	started = sysGetMicroseconds();
	g_TolkModule = LoadLibraryW(dllpath);
	errornum = g_TolkModule ? ERROR_SUCCESS : GetLastError();
	accessibilityLogEvent("speech", "dll_load_result",
			"loaded=%d path=%s module=%p windows_error=%lu elapsed_us=%llu",
			g_TolkModule != NULL, dllpathutf8 ? dllpathutf8 : "",
			g_TolkModule, (unsigned long)errornum,
			(unsigned long long)(sysGetMicroseconds() - started));

	if (!g_TolkModule) {
		speechTolkLogWindowsFailure("load Tolk.dll", errornum,
				dllpathutf8 ? dllpathutf8 : "");
		free(dllpathutf8);
		free(dllpath);
		return 0;
	}

	free(dllpathutf8);
	free(dllpath);

	g_TolkLoad = (TolkLoadFn)(uintptr_t)speechTolkResolveExport("Tolk_Load", &missing);
	g_TolkIsLoaded = (TolkIsLoadedFn)(uintptr_t)speechTolkResolveExport("Tolk_IsLoaded", &missing);
	g_TolkUnload = (TolkUnloadFn)(uintptr_t)speechTolkResolveExport("Tolk_Unload", &missing);
	g_TolkDetectScreenReader = (TolkDetectScreenReaderFn)(uintptr_t)speechTolkResolveExport("Tolk_DetectScreenReader", &missing);
	g_TolkHasSpeech = (TolkHasSpeechFn)(uintptr_t)speechTolkResolveExport("Tolk_HasSpeech", &missing);
	g_TolkHasBraille = (TolkHasBrailleFn)(uintptr_t)speechTolkResolveExport("Tolk_HasBraille", &missing);
	g_TolkOutput = (TolkOutputFn)(uintptr_t)speechTolkResolveExport("Tolk_Output", &missing);
	g_TolkSilence = (TolkSilenceFn)(uintptr_t)speechTolkResolveExport("Tolk_Silence", &missing);

	if (missing) {
		accessibilityLogEvent("speech", "backend_unavailable",
				"reason=missing_exports missing_count=%d", missing);
		FreeLibrary(g_TolkModule);
		g_TolkModule = NULL;
		speechTolkClearFunctions();
		return 0;
	}

	started = sysGetMicroseconds();
	g_TolkLoad();
	g_TolkLoaded = g_TolkIsLoaded();

	if (!g_TolkLoaded) {
		accessibilityLogEvent("speech", "backend_unavailable",
				"reason=tolk_not_loaded elapsed_us=%llu",
				(unsigned long long)(sysGetMicroseconds() - started));
		g_TolkUnload();
		FreeLibrary(g_TolkModule);
		g_TolkModule = NULL;
		speechTolkClearFunctions();
		return 0;
	}

	readername = g_TolkDetectScreenReader();
	hasspeech = readername && g_TolkHasSpeech();
	hasbraille = readername && g_TolkHasBraille();

	if (readername) {
		g_TolkReaderName = speechTolkWideToUtf8(readername, &errornum);
	}

	g_TolkAvailable = readername && hasspeech && g_TolkReaderName;
	accessibilityLogEvent("speech", "backend_detected",
			"tolk_loaded=%d reader=%s has_speech=%d has_braille=%d available=%d elapsed_us=%llu",
			g_TolkLoaded, g_TolkReaderName ? g_TolkReaderName : "",
			hasspeech, hasbraille, g_TolkAvailable,
			(unsigned long long)(sysGetMicroseconds() - started));

	if (!g_TolkAvailable) {
		accessibilityLogEvent("speech", "backend_unavailable",
				"reason=%s reader=%s has_speech=%d has_braille=%d name_conversion_error=%lu",
				readername ? hasspeech ? "reader_name_conversion" : "reader_has_no_speech" : "no_active_reader",
				g_TolkReaderName ? g_TolkReaderName : "", hasspeech, hasbraille,
				(unsigned long)errornum);
	}

	return g_TolkAvailable;
}

void accessibilitySpeechBackendShutdown(void)
{
	u64 started;
	s32 cancelled;

	if (!g_TolkInitialized) {
		return;
	}

	if (g_TolkLoaded && g_TolkSilence) {
		started = sysGetMicroseconds();
		cancelled = g_TolkSilence();
		accessibilityLogEvent("speech", "cancel_result",
				"cancelled=%d reason=shutdown backend=%s elapsed_us=%llu",
				cancelled, g_TolkReaderName ? g_TolkReaderName : "",
				(unsigned long long)(sysGetMicroseconds() - started));
	}

	started = sysGetMicroseconds();

	if (g_TolkLoaded && g_TolkUnload) {
		g_TolkUnload();
	}

	g_TolkLoaded = 0;
	g_TolkAvailable = 0;
	free(g_TolkReaderName);
	g_TolkReaderName = NULL;

	if (g_TolkModule) {
		FreeLibrary(g_TolkModule);
		g_TolkModule = NULL;
	}

	speechTolkClearFunctions();
	accessibilityLogEvent("speech", "dll_unload_result",
			"backend=tolk elapsed_us=%llu",
			(unsigned long long)(sysGetMicroseconds() - started));
}

s32 accessibilitySpeechBackendIsAvailable(void)
{
	return g_TolkAvailable;
}

const char *accessibilitySpeechBackendGetName(void)
{
	return g_TolkReaderName ? g_TolkReaderName : "";
}

s32 accessibilitySpeechBackendOutput(const char *text, s32 interrupt)
{
	wchar_t *widetext;
	DWORD errornum = ERROR_SUCCESS;
	s32 units = 0;
	s32 result;
	u64 started = sysGetMicroseconds();

	if (!g_TolkAvailable || !g_TolkOutput) {
		return 0;
	}

	widetext = speechTolkUtf8ToWide(text, &units, &errornum);

	if (!widetext) {
		speechTolkLogWindowsFailure("convert speech UTF-8", errornum, "");
		accessibilityLogEvent("speech", "output_conversion",
				"converted=0 utf8_bytes=%llu windows_error=%lu text=%s",
				(unsigned long long)(text ? strlen(text) : 0), (unsigned long)errornum,
				text ? text : "");
		return 0;
	}

	accessibilityLogEvent("speech", "output_conversion",
			"converted=1 utf8_bytes=%llu utf16_units=%d elapsed_us=%llu text=%s",
			(unsigned long long)strlen(text), units,
			(unsigned long long)(sysGetMicroseconds() - started), text);
	started = sysGetMicroseconds();
	result = g_TolkOutput(widetext, interrupt != 0);
	accessibilityLogEvent("speech", "backend_output_result",
			"accepted=%d interrupt=%d utf16_units=%d elapsed_us=%llu",
			result, interrupt != 0, units,
			(unsigned long long)(sysGetMicroseconds() - started));
	free(widetext);

	return result;
}

s32 accessibilitySpeechBackendCancel(void)
{
	if (!g_TolkLoaded || !g_TolkSilence) {
		return 0;
	}

	return g_TolkSilence();
}
