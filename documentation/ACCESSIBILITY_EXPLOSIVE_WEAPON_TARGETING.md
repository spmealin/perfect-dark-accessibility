# Explosive weapon targeting

## Behavior and boundaries

Exact aim at a world weapon that native `objDamage` can detonate uses the
existing lower interrupted destructible lock (90 ms sound / 10 ms silence).
This covers mines and proximity-mode Dragons without inventing destructibility
for ordinary dropped guns. Ownership is not an eligibility condition. Existing
target-indicator, gameplay-scope and shutdown gates still apply.

Only `src/accessibility/accessibility_targeting_game.c` changes runtime policy;
no established game file or upstream hook is changed. Native gunfire/explosion
immunity, invincibility, lifecycle, held-weapon exclusion, and the remote-mine/
homing-rocket exception are respected. This reports exact query alignment, not
the eventual trajectory or guaranteed detonation of a thrown projectile.
Threat Detector discovery remains separate from its copied combat candidate,
so contact/presence alerts survive while aimed explosives use the correct lock.
No new configuration, English speech, audio voice, allocation or raycast is added.

## Automated checks

From MinGW64, after configuring the normal build:

```sh
cc @build/CMakeFiles/pd.dir/includes_C.rsp -DPLATFORM_64BIT=1 -DVERSION=2 -DPAL=0 -D_LANGUAGE_C=1 -DACCESSIBILITY_PERFORMANCE_DIAGNOSTICS=0 -std=c11 -O2 -fwhole-program tools/tests/accessibility_weapon_target_test.c -o build/accessibility_weapon_target_test.exe -lm
build/accessibility_weapon_target_test.exe
cc @build/CMakeFiles/pd.dir/includes_C.rsp -DPLATFORM_64BIT=1 -DVERSION=2 -DPAL=0 -D_LANGUAGE_C=1 -DACCESSIBILITY_PERFORMANCE_DIAGNOSTICS=0 -std=c11 -O2 -fwhole-program tools/tests/accessibility_precision_mapping_test.c -o build/accessibility_precision_mapping_test.exe -lm
build/accessibility_precision_mapping_test.exe
```

The weapon fixture exercises the real adapter helpers with stubbed weapon
function, health and mortality queries: explosive families, primary/secondary
Dragon transition, ordinary-gun/device exclusions, damage immunity, lifecycle,
held versus attached props, and detector-copy independence. The core fixture
checks that a destructible category with a hostile detector relationship still
produces the interrupted 396 Hz lock, preserving hostile/body-region/glass cues.
These are semantic/unit checks, not real-world shot or listening tests.

Both fixtures passed, and the Windows x64 `ntsc-final` RelWithDebInfo game build
completed through MinGW64 with performance diagnostics off. The existing
`build/pd.ini` has accessibility, logging and targeting enabled, with independent
interrupted-target volume `0.12`. No game was launched or log session replaced;
blind-user listening/interaction acceptance remains pending. Existing upstream
compiler warnings were left unchanged.

## Owner acceptance test (pending)

1. Aim a firearm directly at a placed proximity mine and secondary Dragon.
   Expect the lower interrupted lock. Move aim away: it must stop.
2. Repeat with Threat Detector enabled: retain its contact alert but use the
   interrupted lock while on the explosive, not the solid hostile lock.
3. Shoot the item; after detonation no stale lock should remain.
4. Aim at an ordinary dropped primary Dragon or other gun: no new generic
   destructible lock. Pickups retain their usual F8 behavior.
5. Check an enemy, protected person and breakable pane for unchanged cues.

No new runtime session is claimed for this change. Use Shift+F2 for failures;
report session ID and capture number. Inspect `targeting/destroyable_object_aim`,
`targeting/weapon_threat_aim`, and alignment records (category, interrupted
pattern, carrier, volume source and raw-query alignment source). Historical
session logs are not acceptance evidence for the new code. Build integration
uses Windows x64 `ntsc-final`; normal performance diagnostics remain off.
Rollback is a revert of the implementation commit; no save/config migration is
needed. No regressions are known from automated checks; in-game acceptance is
still required.
