#include <windows.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wchar.h>
#include <PR/ultratypes.h>
#include <prism.h>
#include "platform.h"
#include "config.h"
#include "system.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_speech_backend.h"

#define SPEECH_PRISM_SETTING_LENGTH 32

typedef PrismConfig (__cdecl *PrismConfigInitFn)(void);
typedef PrismContext *(__cdecl *PrismInitFn)(PrismConfig *config);
typedef void (__cdecl *PrismShutdownFn)(PrismContext *context);
typedef PrismBackend *(__cdecl *PrismRegistryCreateFn)(PrismContext *context,
		PrismBackendId id);
typedef void (__cdecl *PrismBackendFreeFn)(PrismBackend *backend);
typedef const char *(__cdecl *PrismBackendNameFn)(PrismBackend *backend);
typedef uint64_t (__cdecl *PrismBackendGetFeaturesFn)(PrismBackend *backend);
typedef PrismError (__cdecl *PrismBackendInitializeFn)(PrismBackend *backend);
typedef PrismError (__cdecl *PrismBackendOutputFn)(PrismBackend *backend,
		const char *text, bool interrupt);
typedef PrismError (__cdecl *PrismBackendStopFn)(PrismBackend *backend);
typedef const char *(__cdecl *PrismErrorStringFn)(PrismError error);

struct speechPrismCandidate {
	const char *setting;
	PrismBackendId id;
};

static const struct speechPrismCandidate g_SpeechPrismScreenReaders[] = {
	{ "nvda", PRISM_BACKEND_NVDA },
	{ "pc_talker", PRISM_BACKEND_PC_TALKER },
	{ "zdsr", PRISM_BACKEND_ZDSR },
	{ "boy_pc_reader", PRISM_BACKEND_BOY_PC_READER },
	{ "jaws", PRISM_BACKEND_JAWS },
	{ "zoomtext", PRISM_BACKEND_ZOOM_TEXT },
	{ "sense_reader", PRISM_BACKEND_SENSE_READER },
	{ "system_access", PRISM_BACKEND_SYSTEM_ACCESS },
	{ "window_eyes", PRISM_BACKEND_WINDOW_EYES },
};

static char g_SpeechPrismBackendSetting[SPEECH_PRISM_SETTING_LENGTH] = "auto";
static char g_SpeechPrismFallbackSetting[SPEECH_PRISM_SETTING_LENGTH] = "onecore";
static HMODULE g_SpeechPrismModule = NULL;
static PrismContext *g_SpeechPrismContext = NULL;
static PrismBackend *g_SpeechPrismBackend = NULL;
static PrismConfigInitFn g_PrismConfigInit = NULL;
static PrismInitFn g_PrismInit = NULL;
static PrismShutdownFn g_PrismShutdown = NULL;
static PrismRegistryCreateFn g_PrismRegistryCreate = NULL;
static PrismBackendFreeFn g_PrismBackendFree = NULL;
static PrismBackendNameFn g_PrismBackendName = NULL;
static PrismBackendGetFeaturesFn g_PrismBackendGetFeatures = NULL;
static PrismBackendInitializeFn g_PrismBackendInitialize = NULL;
static PrismBackendOutputFn g_PrismBackendOutput = NULL;
static PrismBackendStopFn g_PrismBackendStop = NULL;
static PrismErrorStringFn g_PrismErrorString = NULL;
static s32 g_SpeechPrismInitialized = 0;
static s32 g_SpeechPrismAvailable = 0;
static char g_SpeechPrismBackendName[64];

PD_CONSTRUCTOR static void speechPrismConfigInit(void)
{
	configRegisterString("Accessibility.SpeechBackend",
			g_SpeechPrismBackendSetting, sizeof(g_SpeechPrismBackendSetting));
	configRegisterString("Accessibility.SpeechFallback",
			g_SpeechPrismFallbackSetting, sizeof(g_SpeechPrismFallbackSetting));
}

static const char *speechPrismErrorName(PrismError error)
{
	if (g_PrismErrorString) {
		return g_PrismErrorString(error);
	}

	return "unknown Prism error";
}

static char *speechPrismFormatWindowsError(DWORD errornum)
{
	char *message = NULL;
	DWORD length = FormatMessageA(
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

static void speechPrismLogWindowsFailure(const char *operation, DWORD errornum,
		const char *path)
{
	char *message = speechPrismFormatWindowsError(errornum);

	sysLogPrintf(LOG_WARNING, "accessibility speech: %s failed: %s (%lu)",
			operation, message ? message : "unknown Windows error",
			(unsigned long)errornum);
	accessibilityLogEvent("speech", "backend_unavailable",
			"provider=prism operation=%s windows_error=%lu windows_message=%s path=%s",
			operation, (unsigned long)errornum, message ? message : "",
			path ? path : "");

	if (message) {
		LocalFree(message);
	}
}

static char *speechPrismWideToUtf8(const wchar_t *value)
{
	char *result;
	int length;

	if (!value) {
		return NULL;
	}

	length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1,
			NULL, 0, NULL, NULL);

	if (!length) {
		return NULL;
	}

	result = malloc((size_t)length);

	if (!result) {
		return NULL;
	}

	if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1,
			result, length, NULL, NULL)) {
		free(result);
		return NULL;
	}

	return result;
}

static wchar_t *speechPrismGetDllPath(DWORD *errornum)
{
	wchar_t *path = NULL;
	size_t capacity = 256;
	DWORD length;
	wchar_t *slash;
	size_t directorylength;
	static const wchar_t filename[] = L"prism.dll";

	for (;;) {
		wchar_t *resized = realloc(path, capacity * sizeof(*path));

		if (!resized) {
			free(path);
			*errornum = ERROR_NOT_ENOUGH_MEMORY;
			return NULL;
		}

		path = resized;
		SetLastError(ERROR_SUCCESS);
		length = GetModuleFileNameW(NULL, path, (DWORD)capacity);

		if (!length) {
			*errornum = GetLastError();
			free(path);
			return NULL;
		}

		if (length < capacity) {
			path[length] = L'\0';
			break;
		}

		if (capacity > 32768) {
			*errornum = ERROR_BUFFER_OVERFLOW;
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
		*errornum = ERROR_BAD_PATHNAME;
		free(path);
		return NULL;
	}

	directorylength = (size_t)(slash - path) + 1;
	{
		wchar_t *resized = realloc(path,
				(directorylength + sizeof(filename) / sizeof(filename[0]))
						* sizeof(*path));

		if (!resized) {
			*errornum = ERROR_NOT_ENOUGH_MEMORY;
			free(path);
			return NULL;
		}

		path = resized;
	}

	memcpy(path + directorylength, filename, sizeof(filename));
	return path;
}

static FARPROC speechPrismResolveExport(const char *name, s32 *missing)
{
	FARPROC result = GetProcAddress(g_SpeechPrismModule, name);

	accessibilityLogEvent("speech", "export_resolution",
			"provider=prism name=%s resolved=%d address=%p",
			name, result != NULL, result);

	if (!result) {
		(*missing)++;
	}

	return result;
}

static void speechPrismClearFunctions(void)
{
	g_PrismConfigInit = NULL;
	g_PrismInit = NULL;
	g_PrismShutdown = NULL;
	g_PrismRegistryCreate = NULL;
	g_PrismBackendFree = NULL;
	g_PrismBackendName = NULL;
	g_PrismBackendGetFeatures = NULL;
	g_PrismBackendInitialize = NULL;
	g_PrismBackendOutput = NULL;
	g_PrismBackendStop = NULL;
	g_PrismErrorString = NULL;
}

static s32 speechPrismWindowsScreenReaderActive(void)
{
	BOOL active = FALSE;
	BOOL queried = SystemParametersInfoW(SPI_GETSCREENREADER, 0, &active, 0);

	accessibilityLogEvent("speech", "windows_screen_reader_probe",
			"queried=%d active=%d windows_error=%lu", queried != FALSE,
			active != FALSE, queried ? 0ul : (unsigned long)GetLastError());
	return queried && active;
}

static s32 speechPrismTryBackend(const struct speechPrismCandidate *candidate,
		const char *selectionclass)
{
	PrismBackend *backend;
	uint64_t features;
	PrismError error;
	const char *name;
	u64 started = sysGetMicroseconds();

	backend = g_PrismRegistryCreate(g_SpeechPrismContext, candidate->id);

	if (!backend) {
		accessibilityLogEvent("speech", "backend_probe",
				"provider=prism candidate=%s class=%s created=0 elapsed_us=%llu",
				candidate->setting, selectionclass,
				(unsigned long long)(sysGetMicroseconds() - started));
		return 0;
	}

	features = g_PrismBackendGetFeatures(backend);
	name = g_PrismBackendName(backend);

	if (!(features & PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME)
			|| !(features & PRISM_BACKEND_SUPPORTS_OUTPUT)) {
		accessibilityLogEvent("speech", "backend_probe",
				"provider=prism candidate=%s name=%s class=%s created=1 runtime_supported=%d supports_output=%d selected=0 elapsed_us=%llu",
				candidate->setting, name ? name : "", selectionclass,
				(features & PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME) != 0,
				(features & PRISM_BACKEND_SUPPORTS_OUTPUT) != 0,
				(unsigned long long)(sysGetMicroseconds() - started));
		g_PrismBackendFree(backend);
		return 0;
	}

	error = g_PrismBackendInitialize(backend);

	if (error != PRISM_OK && error != PRISM_ERROR_ALREADY_INITIALIZED) {
		accessibilityLogEvent("speech", "backend_probe",
				"provider=prism candidate=%s name=%s class=%s created=1 runtime_supported=1 initialized=0 error=%d error_name=%s selected=0 elapsed_us=%llu",
				candidate->setting, name ? name : "", selectionclass, error,
				speechPrismErrorName(error),
				(unsigned long long)(sysGetMicroseconds() - started));
		g_PrismBackendFree(backend);
		return 0;
	}

	g_SpeechPrismBackend = backend;
	g_SpeechPrismAvailable = 1;
	snprintf(g_SpeechPrismBackendName, sizeof(g_SpeechPrismBackendName),
			"%s", name ? name : candidate->setting);
	accessibilityLogEvent("speech", "backend_detected",
			"provider=prism candidate=%s backend=%s class=%s features=%llu available=1 elapsed_us=%llu",
			candidate->setting, g_SpeechPrismBackendName, selectionclass,
			(unsigned long long)features,
			(unsigned long long)(sysGetMicroseconds() - started));
	return 1;
}

static const struct speechPrismCandidate *speechPrismFindCandidate(
		const char *setting)
{
	static const struct speechPrismCandidate other[] = {
		{ "uia", PRISM_BACKEND_UIA },
		{ "onecore", PRISM_BACKEND_ONE_CORE },
		{ "sapi", PRISM_BACKEND_SAPI },
	};
	size_t i;

	for (i = 0; i < sizeof(g_SpeechPrismScreenReaders)
			/ sizeof(g_SpeechPrismScreenReaders[0]); i++) {
		if (!strcasecmp(setting, g_SpeechPrismScreenReaders[i].setting)) {
			return &g_SpeechPrismScreenReaders[i];
		}
	}

	for (i = 0; i < sizeof(other) / sizeof(other[0]); i++) {
		if (!strcasecmp(setting, other[i].setting)) {
			return &other[i];
		}
	}

	return NULL;
}

static s32 speechPrismTryScreenReaders(void)
{
	static const struct speechPrismCandidate uia = {
		"uia", PRISM_BACKEND_UIA
	};
	size_t i;

	for (i = 0; i < sizeof(g_SpeechPrismScreenReaders)
			/ sizeof(g_SpeechPrismScreenReaders[0]); i++) {
		if (speechPrismTryBackend(&g_SpeechPrismScreenReaders[i],
				"screen_reader")) {
			return 1;
		}
	}

	if (speechPrismWindowsScreenReaderActive()
			&& speechPrismTryBackend(&uia, "screen_reader_uia")) {
		return 1;
	}

	return 0;
}

static s32 speechPrismTryFallback(void)
{
	static const struct speechPrismCandidate onecore = {
		"onecore", PRISM_BACKEND_ONE_CORE
	};
	static const struct speechPrismCandidate sapi = {
		"sapi", PRISM_BACKEND_SAPI
	};

	if (!strcasecmp(g_SpeechPrismFallbackSetting, "none")) {
		return 0;
	}

	if (!strcasecmp(g_SpeechPrismFallbackSetting, "sapi")) {
		return speechPrismTryBackend(&sapi, "fallback");
	}

	if (strcasecmp(g_SpeechPrismFallbackSetting, "onecore")) {
		accessibilityLogEvent("speech", "configuration_warning",
				"setting=Accessibility.SpeechFallback value=%s replacement=onecore",
				g_SpeechPrismFallbackSetting);
	}

	if (speechPrismTryBackend(&onecore, "fallback")) {
		return 1;
	}

	return speechPrismTryBackend(&sapi, "fallback_secondary");
}

static s32 speechPrismSelectBackend(void)
{
	const struct speechPrismCandidate *candidate;

	if (!strcasecmp(g_SpeechPrismBackendSetting, "none")) {
		return 0;
	}

	if (!strcasecmp(g_SpeechPrismBackendSetting, "auto")) {
		return speechPrismTryScreenReaders() || speechPrismTryFallback();
	}

	if (!strcasecmp(g_SpeechPrismBackendSetting, "screenreader")) {
		return speechPrismTryScreenReaders();
	}

	candidate = speechPrismFindCandidate(g_SpeechPrismBackendSetting);

	if (candidate) {
		return speechPrismTryBackend(candidate, "forced");
	}

	accessibilityLogEvent("speech", "configuration_warning",
			"setting=Accessibility.SpeechBackend value=%s replacement=auto",
			g_SpeechPrismBackendSetting);
	return speechPrismTryScreenReaders() || speechPrismTryFallback();
}

s32 accessibilitySpeechBackendInit(void)
{
	wchar_t *dllpath;
	char *dllpathutf8;
	DWORD errornum = ERROR_SUCCESS;
	s32 missing = 0;
	u64 started;
	PrismConfig config;

	if (g_SpeechPrismInitialized) {
		return g_SpeechPrismAvailable;
	}

	g_SpeechPrismInitialized = 1;
	dllpath = speechPrismGetDllPath(&errornum);

	if (!dllpath) {
		speechPrismLogWindowsFailure("resolve executable directory", errornum, "");
		return 0;
	}

	dllpathutf8 = speechPrismWideToUtf8(dllpath);
	SetLastError(ERROR_SUCCESS);
	started = sysGetMicroseconds();
	g_SpeechPrismModule = LoadLibraryW(dllpath);
	errornum = g_SpeechPrismModule ? ERROR_SUCCESS : GetLastError();
	accessibilityLogEvent("speech", "dll_load_result",
			"provider=prism version=%s loaded=%d path=%s module=%p windows_error=%lu elapsed_us=%llu",
			PRISM_VERSION_STRING, g_SpeechPrismModule != NULL,
			dllpathutf8 ? dllpathutf8 : "", g_SpeechPrismModule,
			(unsigned long)errornum,
			(unsigned long long)(sysGetMicroseconds() - started));

	if (!g_SpeechPrismModule) {
		speechPrismLogWindowsFailure("load prism.dll", errornum,
				dllpathutf8 ? dllpathutf8 : "");
		free(dllpathutf8);
		free(dllpath);
		return 0;
	}

	free(dllpathutf8);
	free(dllpath);

	g_PrismConfigInit = (PrismConfigInitFn)(uintptr_t)speechPrismResolveExport("prism_config_init", &missing);
	g_PrismInit = (PrismInitFn)(uintptr_t)speechPrismResolveExport("prism_init", &missing);
	g_PrismShutdown = (PrismShutdownFn)(uintptr_t)speechPrismResolveExport("prism_shutdown", &missing);
	g_PrismRegistryCreate = (PrismRegistryCreateFn)(uintptr_t)speechPrismResolveExport("prism_registry_create", &missing);
	g_PrismBackendFree = (PrismBackendFreeFn)(uintptr_t)speechPrismResolveExport("prism_backend_free", &missing);
	g_PrismBackendName = (PrismBackendNameFn)(uintptr_t)speechPrismResolveExport("prism_backend_name", &missing);
	g_PrismBackendGetFeatures = (PrismBackendGetFeaturesFn)(uintptr_t)speechPrismResolveExport("prism_backend_get_features", &missing);
	g_PrismBackendInitialize = (PrismBackendInitializeFn)(uintptr_t)speechPrismResolveExport("prism_backend_initialize", &missing);
	g_PrismBackendOutput = (PrismBackendOutputFn)(uintptr_t)speechPrismResolveExport("prism_backend_output", &missing);
	g_PrismBackendStop = (PrismBackendStopFn)(uintptr_t)speechPrismResolveExport("prism_backend_stop", &missing);
	g_PrismErrorString = (PrismErrorStringFn)(uintptr_t)speechPrismResolveExport("prism_error_string", &missing);

	if (missing) {
		accessibilityLogEvent("speech", "backend_unavailable",
				"provider=prism reason=missing_exports missing_count=%d", missing);
		FreeLibrary(g_SpeechPrismModule);
		g_SpeechPrismModule = NULL;
		speechPrismClearFunctions();
		return 0;
	}

	config = g_PrismConfigInit();
	config.availability_callback = NULL;
	started = sysGetMicroseconds();
	g_SpeechPrismContext = g_PrismInit(&config);
	accessibilityLogEvent("speech", "provider_init_result",
			"provider=prism version=%s initialized=%d requested_backend=%s fallback=%s elapsed_us=%llu",
			PRISM_VERSION_STRING, g_SpeechPrismContext != NULL,
			g_SpeechPrismBackendSetting, g_SpeechPrismFallbackSetting,
			(unsigned long long)(sysGetMicroseconds() - started));

	if (!g_SpeechPrismContext) {
		accessibilityLogEvent("speech", "backend_unavailable",
				"provider=prism reason=context_initialization_failed");
		return 0;
	}

	if (!speechPrismSelectBackend()) {
		accessibilityLogEvent("speech", "backend_unavailable",
				"provider=prism reason=no_selected_backend requested_backend=%s fallback=%s",
				g_SpeechPrismBackendSetting, g_SpeechPrismFallbackSetting);
	}

	return g_SpeechPrismAvailable;
}

void accessibilitySpeechBackendShutdown(void)
{
	u64 started;
	PrismError error;

	if (!g_SpeechPrismInitialized) {
		return;
	}

	started = sysGetMicroseconds();

	if (g_SpeechPrismBackend) {
		error = g_PrismBackendStop(g_SpeechPrismBackend);
		accessibilityLogEvent("speech", "cancel_result",
				"cancelled=%d reason=shutdown provider=prism backend=%s error=%d error_name=%s elapsed_us=%llu",
				error == PRISM_OK, g_SpeechPrismBackendName, error,
				speechPrismErrorName(error),
				(unsigned long long)(sysGetMicroseconds() - started));
		g_PrismBackendFree(g_SpeechPrismBackend);
		g_SpeechPrismBackend = NULL;
	}

	if (g_SpeechPrismContext) {
		g_PrismShutdown(g_SpeechPrismContext);
		g_SpeechPrismContext = NULL;
	}

	g_SpeechPrismAvailable = 0;
	g_SpeechPrismBackendName[0] = '\0';

	if (g_SpeechPrismModule) {
		FreeLibrary(g_SpeechPrismModule);
		g_SpeechPrismModule = NULL;
	}

	speechPrismClearFunctions();
	accessibilityLogEvent("speech", "dll_unload_result",
			"provider=prism elapsed_us=%llu",
			(unsigned long long)(sysGetMicroseconds() - started));
}

s32 accessibilitySpeechBackendIsAvailable(void)
{
	return g_SpeechPrismAvailable;
}

const char *accessibilitySpeechBackendGetName(void)
{
	return g_SpeechPrismBackendName;
}

s32 accessibilitySpeechBackendOutput(const char *text, s32 interrupt)
{
	PrismError error;
	u64 started = sysGetMicroseconds();

	if (!g_SpeechPrismAvailable || !g_SpeechPrismBackend || !text || !text[0]) {
		return 0;
	}

	error = g_PrismBackendOutput(g_SpeechPrismBackend, text, interrupt != 0);
	accessibilityLogEvent("speech", "backend_output_result",
			"provider=prism accepted=%d backend=%s interrupt=%d error=%d error_name=%s utf8_bytes=%llu elapsed_us=%llu",
			error == PRISM_OK, g_SpeechPrismBackendName, interrupt != 0,
			error, speechPrismErrorName(error),
			(unsigned long long)strlen(text),
			(unsigned long long)(sysGetMicroseconds() - started));
	return error == PRISM_OK;
}

s32 accessibilitySpeechBackendCancel(void)
{
	PrismError error;

	if (!g_SpeechPrismAvailable || !g_SpeechPrismBackend) {
		return 0;
	}

	error = g_PrismBackendStop(g_SpeechPrismBackend);
	return error == PRISM_OK;
}
