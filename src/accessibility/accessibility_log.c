#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <PR/ultratypes.h>
#include "fs.h"
#include "system.h"
#include "accessibility/accessibility_log.h"

#define ACCESSIBILITY_LOG_SCHEMA 1

static FILE *g_AccessibilityLogFile = NULL;
static uint64_t g_AccessibilityLogSequence = 0;
static char g_AccessibilityLogSession[32];
static s32 g_AccessibilityLogFailureReported = 0;

static size_t accessibilityLogUtf8SequenceLength(const unsigned char *value)
{
	const unsigned char first = value[0];

	if (first >= 0xc2 && first <= 0xdf
			&& value[1] >= 0x80 && value[1] <= 0xbf) {
		return 2;
	}

	if (first >= 0xe0 && first <= 0xef
			&& value[1] >= 0x80 && value[1] <= 0xbf
			&& value[2] >= 0x80 && value[2] <= 0xbf
			&& (first != 0xe0 || value[1] >= 0xa0)
			&& (first != 0xed || value[1] <= 0x9f)) {
		return 3;
	}

	if (first >= 0xf0 && first <= 0xf4
			&& value[1] >= 0x80 && value[1] <= 0xbf
			&& value[2] >= 0x80 && value[2] <= 0xbf
			&& value[3] >= 0x80 && value[3] <= 0xbf
			&& (first != 0xf0 || value[1] >= 0x90)
			&& (first != 0xf4 || value[1] <= 0x8f)) {
		return 4;
	}

	return 0;
}

static void accessibilityLogWarn(const char *operation, s32 errornum)
{
	if (!g_AccessibilityLogFailureReported) {
		g_AccessibilityLogFailureReported = 1;

		if (errornum) {
			sysLogPrintf(LOG_WARNING, "accessibility log: %s failed for %s: %s (%d)",
					operation, fsFullPath(ACCESSIBILITY_LOG_PATH), strerror(errornum), errornum);
		} else {
			sysLogPrintf(LOG_WARNING, "accessibility log: %s failed for %s",
					operation, fsFullPath(ACCESSIBILITY_LOG_PATH));
		}
	}
}

static void accessibilityLogWriteJsonString(const char *value)
{
	const unsigned char *ptr = (const unsigned char *)(value ? value : "");

	fputc('"', g_AccessibilityLogFile);

	while (*ptr) {
		size_t utf8length;

		switch (*ptr) {
		case '"':
			fputs("\\\"", g_AccessibilityLogFile);
			break;
		case '\\':
			fputs("\\\\", g_AccessibilityLogFile);
			break;
		case '\b':
			fputs("\\b", g_AccessibilityLogFile);
			break;
		case '\f':
			fputs("\\f", g_AccessibilityLogFile);
			break;
		case '\n':
			fputs("\\n", g_AccessibilityLogFile);
			break;
		case '\r':
			fputs("\\r", g_AccessibilityLogFile);
			break;
		case '\t':
			fputs("\\t", g_AccessibilityLogFile);
			break;
		default:
			if (*ptr < 0x20) {
				fprintf(g_AccessibilityLogFile, "\\u%04x", (u32)*ptr);
			} else if (*ptr < 0x80) {
				fputc(*ptr, g_AccessibilityLogFile);
			} else {
				utf8length = accessibilityLogUtf8SequenceLength(ptr);

				if (utf8length) {
					fwrite(ptr, 1, utf8length, g_AccessibilityLogFile);
					ptr += utf8length;
					continue;
				}

				// Preserve invalid octets as JSON escapes instead of emitting invalid JSON.
				fprintf(g_AccessibilityLogFile, "\\u%04x", (u32)*ptr);
			}
			break;
		}

		ptr++;
	}

	fputc('"', g_AccessibilityLogFile);
}

static void accessibilityLogCloseAfterFailure(const char *operation)
{
	FILE *file = g_AccessibilityLogFile;
	s32 errornum = errno;

	g_AccessibilityLogFile = NULL;

	if (file) {
		fclose(file);
	}

	accessibilityLogWarn(operation, errornum);
}

s32 accessibilityLogInit(void)
{
	time_t now;

	if (g_AccessibilityLogFile) {
		return 1;
	}

	g_AccessibilityLogFailureReported = 0;
	g_AccessibilityLogSequence = 0;
	now = time(NULL);
	snprintf(g_AccessibilityLogSession, sizeof(g_AccessibilityLogSession),
			"%" PRIu64, (uint64_t)now);

	errno = 0;
	g_AccessibilityLogFile = fsFileOpenWrite(ACCESSIBILITY_LOG_PATH);

	if (!g_AccessibilityLogFile) {
		accessibilityLogWarn("open", errno);
		return 0;
	}

	return 1;
}

void accessibilityLogShutdown(void)
{
	FILE *file;
	s32 flushresult;
	s32 closeresult;
	s32 errornum;

	if (!g_AccessibilityLogFile) {
		return;
	}

	file = g_AccessibilityLogFile;
	g_AccessibilityLogFile = NULL;
	errno = 0;
	flushresult = fflush(file);
	errornum = errno;
	closeresult = fclose(file);

	if (!errornum) {
		errornum = errno;
	}

	if (flushresult != 0 || closeresult != 0) {
		accessibilityLogWarn("close", errornum);
	}
}

s32 accessibilityLogIsOpen(void)
{
	return g_AccessibilityLogFile != NULL;
}

void accessibilityLogEvent(const char *category, const char *event, const char *fmt, ...)
{
	static const char formaterror[] = "<message formatting failed>";
	static const char allocationerror[] = "<message allocation failed>";
	const char *message = "";
	char *allocated = NULL;
	va_list args;
	va_list copy;
	s32 length;
	uint64_t sequence;
	uint64_t timestamp;

	if (!g_AccessibilityLogFile) {
		return;
	}

	sequence = g_AccessibilityLogSequence++;
	timestamp = (uint64_t)sysGetMicroseconds();

	if (fmt) {
		va_start(args, fmt);
		va_copy(copy, args);
		length = vsnprintf(NULL, 0, fmt, copy);
		va_end(copy);

		if (length < 0) {
			message = formaterror;
		} else {
			allocated = malloc((size_t)length + 1);

			if (allocated) {
				if (vsnprintf(allocated, (size_t)length + 1, fmt, args) < 0) {
					free(allocated);
					allocated = NULL;
					message = formaterror;
				} else {
					message = allocated;
				}
			} else {
				message = allocationerror;
			}
		}

		va_end(args);
	}

	fputs("{\"schema\":", g_AccessibilityLogFile);
	fprintf(g_AccessibilityLogFile, "%d", ACCESSIBILITY_LOG_SCHEMA);
	fputs(",\"seq\":", g_AccessibilityLogFile);
	fprintf(g_AccessibilityLogFile, "%" PRIu64, sequence);
	fputs(",\"session\":", g_AccessibilityLogFile);
	accessibilityLogWriteJsonString(g_AccessibilityLogSession);
	fputs(",\"t_us\":", g_AccessibilityLogFile);
	fprintf(g_AccessibilityLogFile, "%" PRIu64, timestamp);
	fputs(",\"build_hash\":", g_AccessibilityLogFile);
	accessibilityLogWriteJsonString(VERSION_HASH);
	fputs(",\"build_branch\":", g_AccessibilityLogFile);
	accessibilityLogWriteJsonString(VERSION_BRANCH);
	fputs(",\"target\":", g_AccessibilityLogFile);
	accessibilityLogWriteJsonString(VERSION_TARGET);
	fputs(",\"arch\":", g_AccessibilityLogFile);
	accessibilityLogWriteJsonString(VERSION_ARCH);
	fputs(",\"rom_config\":", g_AccessibilityLogFile);
	accessibilityLogWriteJsonString(VERSION_ROMID);
	fputs(",\"build_type\":", g_AccessibilityLogFile);
	accessibilityLogWriteJsonString(VERSION_BUILD);
	fputs(",\"category\":", g_AccessibilityLogFile);
	accessibilityLogWriteJsonString(category);
	fputs(",\"event\":", g_AccessibilityLogFile);
	accessibilityLogWriteJsonString(event);
	fputs(",\"message\":", g_AccessibilityLogFile);
	accessibilityLogWriteJsonString(message);
	fputs("}\n", g_AccessibilityLogFile);

	free(allocated);

	if (fflush(g_AccessibilityLogFile) != 0 || ferror(g_AccessibilityLogFile)) {
		accessibilityLogCloseAfterFailure("write");
	}
}
