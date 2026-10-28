/* Allocation-free PCM/menu regression, not an audibility or blind-user test.
 * Build in MinGW64 using the game's include response file and SDL2 flags. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#define SDL_MAIN_HANDLED
#include "types.h"
#include "../../port/src/accessibility/accessibility_tone_ownership.c"
#include "../../port/src/accessibility/accessibility_tone.c"
#include "../../src/accessibility/accessibility_glossary.c"
#include "../../port/src/accessibility/accessibility_options.c"

static u64 testclock;
static s32 testenabled = 1;
static s32 testpops;

char *langGet(s32 textid) { assert(textid == L_OPTIONS_213); return "Back"; }
void menuPopDialog(void) { testpops++; }

u64 sysGetMicroseconds(void) { return testclock; }
s32 accessibilityIsEnabled(void) { return testenabled; }
f32 accessibilityGetEnemyFrequency(void) { return 900; }
f32 accessibilityGetVirtualCaneVolume(void) { return .184f; }
f32 accessibilityGetTargetingVolume(void) { return .15f; }
f32 accessibilityGetInterruptedTargetingVolume(void) { return .12f; }
void accessibilityGetEnemyTuning(f32 *a, f32 *b, f32 *c, f32 *volume)
{ if (volume) *volume = .25f; }
void accessibilityGetMarkerTuning(f32 *range, f32 *volume)
{ if (volume) *volume = 1; }
void accessibilityGetCombatRadarTuning(f32 *a, f32 *b, f32 *volume)
{ if (volume) *volume = .2f; }
void accessibilityGetProjectileHazardTuning(f32 *a, f32 *volume)
{ if (volume) *volume = .25f; }
void accessibilityGetVirtualCanePitch(f32 *nearhz, f32 *farhz)
{ *nearhz = 600; *farhz = 300; }
void accessibilityLogEvent(const char *category, const char *event,
		const char *format, ...) { }

static s32 nonzero(const s16 *pcm, s32 samples)
{
	s32 i;
	for (i = 0; i < samples; i++) if (pcm[i]) return 1;
	return 0;
}

static void simulateWorldSuppression(void)
{
	/* These normally run every paused/menu frame and must not cancel a demo. */
	accessibilityToneSet(0, 0);
	accessibilityToneStopChirp();
	accessibilityToneStopTargetPresence();
	accessibilityToneStopThreatAlert();
	accessibilityToneStopCompass();
	accessibilityToneStopWeaponFunction();
	accessibilityToneSetHazard(0, 0, 0, 0);
	accessibilityToneSetCaneEdge(0, 0, 0);
	accessibilityToneStopCombat();
	accessibilityToneStopProjectiles();
	accessibilityToneStopTracker();
	accessibilityToneStopFriendly();
	accessibilityToneStopDoors();
	accessibilityToneStopRadar();
	accessibilityToneStopHillBeacon();
	accessibilityToneStopCane();
	accessibilityToneStopMarkers();
	accessibilityToneStopLandmarks();
}

int main(void)
{
	s16 silence[512] = {0}; /* 256 stereo frames. */
	s16 gameaudio[512];
	s32 index, frame, audible, other;
	union handlerdata data = {0};
	const s16 *pcm;

	assert(accessibilityGlossaryGetCount() > 80);
	assert(g_Glossary[accessibilityGlossaryGetCount() - 1].kind == OBJECT);
	assert(g_Glossary[accessibilityGlossaryGetCount() - 1].variant == 1);
	assert(!*accessibilityGlossaryGetName(-1));
	assert(!*accessibilityGlossaryGetName(accessibilityGlossaryGetCount()));
	glossaryListHandler(MENUOP_GETOPTIONCOUNT, NULL, &data);
	assert(data.list.value == accessibilityGlossaryGetCount() + 1);
	glossaryListHandler(MENUOP_GETOPTGROUPCOUNT, NULL, &data);
	assert(data.list.value == 0);
	assert(g_GlossaryItems[0].flags & MENUITEMFLAG_LIST_SILENTSELECT);
	assert(g_AccessibilityOptionsItems[2].flags & MENUITEMFLAG_SELECTABLE_CLOSESDIALOG);
	assert(g_AccessibilityOptionsItems[2].param2 == L_OPTIONS_213);
	data.list.value = accessibilityGlossaryGetCount();
	assert(!strcmp((char *)glossaryListHandler(MENUOP_GETOPTIONTEXT, NULL, &data), "Back"));
	accessibilityGlossaryPlay(4);
	glossaryListHandler(MENUOP_LISTITEMFOCUS, NULL, &data);
	assert(!accessibilityToneOwnershipIsPreview());
	glossaryListHandler(MENUOP_SET, NULL, &data);
	assert(testpops == 1);
	assert(g_Glossary[5].durationms == 4000 && g_Glossary[5].variant == 1);

	for (index = 0; index < accessibilityGlossaryGetCount(); index++) {
		u64 started = testclock;
		for (other = 0; other < index; other++) {
			assert(strcmp(accessibilityGlossaryGetName(index),
					accessibilityGlossaryGetName(other)));
		}
		data.list.value = index;
		glossaryListHandler(MENUOP_LISTITEMFOCUS, NULL, &data);
		assert(!nonzero(accessibilityToneMix(silence, sizeof(silence)), 512));
		glossaryListHandler(MENUOP_SET, NULL, &data);
		audible = 0;
		for (frame = 0; frame < g_Glossary[index].durationms + 250; frame += 10) {
			simulateWorldSuppression();
			pcm = accessibilityToneMix(silence, sizeof(silence));
			audible |= nonzero(pcm, 512);
			testclock = started + (u64)(frame + 10) * 1000;
			glossaryDialogHandler(MENUOP_TICK, NULL, NULL);
		}
		if (!audible) fprintf(stderr, "Silent cue: %d %s\n", index, g_Glossary[index].name);
		assert(audible);
		glossaryDialogHandler(MENUOP_CLOSE, NULL, NULL);
		assert(!nonzero(accessibilityToneMix(silence, sizeof(silence)), 512));
	}

	/* A held preview must terminate on the audio clock without any menu ticks. */
	accessibilityGlossaryPlay(15); /* Continuous enemy-body lock. */
	for (frame = 0; frame < 120; frame++) {
		pcm = accessibilityToneMix(silence, sizeof(silence));
		if (frame > 100) assert(!nonzero(pcm, 512));
	}
	accessibilityGlossaryStop();
	assert(!nonzero(accessibilityToneMix(silence, sizeof(silence)), 512));
	for (frame = 0; frame < 512; frame++) gameaudio[frame] = 1234;
	accessibilityGlossaryPlay(15);
	for (frame = 0; frame < 120; frame++) {
		pcm = accessibilityToneMix(gameaudio, sizeof(gameaudio));
		if (frame > 100) assert(!memcmp(pcm, gameaudio, sizeof(gameaudio)));
	}
	accessibilityGlossaryStop();
	assert(!memcmp(accessibilityToneMix(gameaudio, sizeof(gameaudio)),
			gameaudio, sizeof(gameaudio)));

	/* Replacing a drone with a chirp cannot leave the previous lane enabled. */
	accessibilityGlossaryPlay(4);
	accessibilityToneMix(silence, sizeof(silence));
	accessibilityGlossaryPlay(1);
	assert(!SDL_AtomicGet(&g_AccessibilityFriendlyEnabled[0]));
	assert(SDL_AtomicGet(&g_AccessibilityChirpEnabled));
	accessibilityGlossaryStop();
	assert(!nonzero(accessibilityToneMix(silence, sizeof(silence)), 512));

	/* No saved scanner/mode setters exist in this module; disabled stays inert. */
	testenabled = 0;
	accessibilityGlossaryPlay(0);
	assert(!accessibilityToneOwnershipIsPreview());
	assert(!nonzero(accessibilityToneMix(silence, sizeof(silence)), 512));
	printf("Audio glossary: %d entries passed catalog/menu/PCM/isolation/deadline checks.\n",
			accessibilityGlossaryGetCount());
	return 0;
}
