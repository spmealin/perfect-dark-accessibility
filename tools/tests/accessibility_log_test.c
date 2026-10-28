/* Logger-only regression. Anonymous temporary files, never the player's log. */
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <PR/ultratypes.h>

static int failflush, failwrite, failalloc, failformat, formatcalls;
static int outstanding, allocations, frees, warnings, flushes;
static u64 clockus;
static FILE *testfile;

static int testFlush(FILE *file)
{
	flushes++;
	clockus += 2000;
	if (failflush) { errno = EIO; return EOF; }
	return fflush(file);
}
static int testError(FILE *file) { return failwrite || ferror(file); }
static void *testMalloc(size_t size)
{
	void *value;
	if (failalloc) return NULL;
	value = malloc(size);
	if (value) { outstanding++; allocations++; }
	return value;
}
static void testFree(void *value)
{
	if (value) { outstanding--; frees++; }
	free(value);
}
static int testFormat(char *buffer, size_t size, const char *format, va_list args)
{
	if (++formatcalls == failformat) return -1;
	return vsnprintf(buffer, size, format, args);
}

#define fflush testFlush
#define ferror testError
#define malloc testMalloc
#define free testFree
#define vsnprintf testFormat
#include "../../src/accessibility/accessibility_log.c"
#undef fflush
#undef ferror
#undef malloc
#undef free
#undef vsnprintf

u64 sysGetMicroseconds(void) { clockus += 10; return clockus; }
FILE *fsFileOpenWrite(const char *name)
{
	assert(!strcmp(name, ACCESSIBILITY_LOG_PATH));
	testfile = tmpfile();
	assert(testfile);
	return testfile;
}
const char *fsFullPath(const char *path) { return "anonymous-test-file"; }
void sysLogPrintf(s32 level, const char *format, ...) { warnings++; }

static char output[32768];
static const char *readOutput(void)
{
	size_t count;
	assert(!fflush(testfile));
	assert(!fseek(testfile, 0, SEEK_SET));
	count = fread(output, 1, sizeof(output) - 1, testfile);
	output[count] = 0;
	assert(!fseek(testfile, 0, SEEK_END));
	return output;
}

int main(void)
{
	char large[6001];
	int before;
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	struct accessibilitylogdiagnostics d;
#endif
	memset(large, 'x', sizeof(large) - 1);
	large[sizeof(large) - 1] = 0;
	assert(accessibilityLogInit());
	assert(accessibilityLogInit());
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	accessibilityLogGetDiagnostics(NULL);
	accessibilityLogReportPerformance("empty");
	assert(g_AccessibilityLogSequence == 0);
#endif
	accessibilityLogEvent("fixture", "escaped", "%s %d", "quote\"\\\n\t\xc3\xa9\xff", 7);
	assert(flushes == 1); /* Original first-event flush. */
	accessibilityLogEventMessage(NULL, NULL, NULL);
	assert(flushes == 1);
	accessibilityLogEvent("fixture", "large", "%s", large);
	assert(outstanding == 0 && allocations == 1 && frees == 1);
	assert(strstr(readOutput(), "\\\"\\\\\\n\\t\xc3\xa9\\u00ff 7"));
	assert(strstr(output, large));
	assert(strstr(output, "\"seq\":0"));
	assert(strstr(output, "\"seq\":2"));
	assert(strstr(output, "\"category\":\"\",\"event\":\"\",\"message\":\"\""));
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	accessibilityLogGetDiagnostics(&d);
	assert(d.events == 3 && d.formatcalls == 2);
	assert(d.allocations == 1 && d.frees == 1 && d.allocatedbytes == sizeof(large));
	assert(d.flushes == 1 && d.flushus >= 2000 && d.writeus > 0 && d.formatus > 0);
	assert(!strcmp(d.worstwritecategory, "fixture"));
	accessibilityLogReportPerformance("fixture");
	assert(strstr(readOutput(), "\"event\":\"log_window\""));
	assert(strstr(output, "events=3 format_calls=2"));
	accessibilityLogGetDiagnostics(&d);
	assert(d.events == 1 && d.formatcalls == 1); /* Summary belongs to next window. */
#else
	assert(!strstr(readOutput(), "log_window"));
#endif
	before = flushes;
	accessibilityLogEventMessage("lifecycle", "fixture", "flush now");
	assert(flushes == before + 1);
	clockus += 1000001;
	accessibilityLogEventMessage("fixture", "interval", "flush interval");
	assert(flushes == before + 2);
	failalloc = 1;
	accessibilityLogEvent("fixture", "allocation_failure", "%s", large);
	failalloc = 0;
	assert(outstanding == 0);
	assert(strstr(readOutput(), "<message allocation failed>"));
	failformat = formatcalls + 2; /* Fail the oversized second formatting pass. */
	accessibilityLogEvent("fixture", "format_failure", "%s", large);
	failformat = 0;
	assert(outstanding == 0 && allocations == 2 && frees == 2);
	assert(strstr(readOutput(), "<message formatting failed>"));
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	accessibilityLogGetDiagnostics(&d);
	assert(d.allocationfailures == 1 && d.allocations == 1 && d.frees == 1);
#endif
	before = flushes;
	accessibilityLogShutdown();
	assert(flushes == before + 1);
	accessibilityLogShutdown();
	assert(!accessibilityLogIsOpen() && outstanding == 0);
	assert(accessibilityLogInit());
	failwrite = 1;
	accessibilityLogEventMessage("fixture", "write_failure", "nonfatal");
	assert(!accessibilityLogIsOpen() && warnings == 1);
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	accessibilityLogGetDiagnostics(&d); assert(d.iofailures == 1);
#endif
	accessibilityLogEvent("fixture", "closed", "%s", large);
	assert(outstanding == 0 && warnings == 1);
	failwrite = 0;
	assert(accessibilityLogInit());
	failflush = 1;
	accessibilityLogEventMessage("lifecycle", "flush_failure", "nonfatal");
	assert(!accessibilityLogIsOpen() && warnings == 2);
	failflush = 0;
	accessibilityLogShutdown();
	puts("Logger: JSON/UTF-8, long messages, frees, flush policy, failures and diagnostic windows passed.");
	return 0;
}
