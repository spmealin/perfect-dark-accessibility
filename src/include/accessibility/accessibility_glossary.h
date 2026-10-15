#ifndef _IN_ACCESSIBILITY_GLOSSARY_H
#define _IN_ACCESSIBILITY_GLOSSARY_H

#include <PR/ultratypes.h>

/* Main-thread menu API. Catalog text is owned English accessibility vocabulary;
 * translate this catalog, not the game/ROM language banks, in a future locale layer. */
s32 accessibilityGlossaryGetCount(void);
const char *accessibilityGlossaryGetName(s32 index);
void accessibilityGlossaryPlay(s32 index);
void accessibilityGlossaryTick(void);
void accessibilityGlossaryStop(void);

#endif
