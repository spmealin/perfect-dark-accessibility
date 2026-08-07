# Special-device target audit

This document records the campaign-wide semantic audit behind the
accessibility targeting registry in
`src/accessibility/accessibility_targeting_game.c`. It is a coverage guide for
playtesting, not a substitute for the mission scripts.

## Contract

The centered target tone identifies a prop which the current mission script
accepts for the equipped special item. Registry entries are keyed by stage,
right-hand weapon, setup tag, expected prop type, and minimum difficulty. This
deliberately avoids model-name and broad interactable heuristics: visually
similar scenery is not made a valid target merely because it looks like a
computer, vehicle, door, or pillar.

The tone reports target identity only. Native interaction range, placement,
trajectory, inventory, difficulty, objective state, and success checks remain
authoritative. A tone on a surface does not promise that a thrown device will
land there or that pressing use from the current position will work.

## Implemented point-target contracts

| Mission or exercise | Equipped item | Native target tags | Native script contract |
| --- | --- | --- | --- |
| CI Data Uplink training | Data Uplink | `0x30` | Exercise terminal |
| CI ECM Mine training | ECM Mine | `0x32` | Exercise hub placement surface |
| CI Door Decoder training | Door Decoder | `0x35` | Exercise panel |
| DataDyne Central: Defection | ECM Mine | `0x03`, `0x04` | `if_weapon_thrown_on_object` for the security and external communications hubs |
| DataDyne Central: Defection | Data Uplink | `0x0c` | `if_chr_activated_object` download PC followed by the equipped-Uplink check |
| DataDyne Research: Investigation | Data Uplink | `0x0a` | Security-door Uplink PC |
| Area 51: Infiltration | Comms Rider | `0x07` | `if_weapon_thrown_on_object` antenna |
| Area 51: Infiltration | Explosives | `0x10` | Radar terminal activation followed by the equipped-Explosives check |
| Area 51: Rescue | Data Uplink | `0x01`, `0x02`, `0x03` | The three virus terminals, each with activation and equipped-Uplink checks |
| Area 51: Escape (Special Agent and above) | Alien Medpack/Auto-Surgeon | `0x00` | Hoverbed activation followed by the equipped-Auto-Surgeon check; Agent bypasses the point-activation step |
| G5 Building: Reconnaissance | Door Decoder | `0x11` | Safe keypad activation followed by the equipped-Decoder check |
| Chicago: Stealth | Remote Mine | `0x08`, `0x09` | Upper and lower fire-door placement surfaces |
| Chicago: Stealth | Reprogrammer (`WEAPON_DATAUPLINK`) | `0x0a` | Objective hovercab activation followed by the equipped-Reprogrammer check |
| Chicago: Stealth | Tracer Bug | `0x0c` | `if_weapon_thrown_on_object` limousine |
| Carrington Institute: Defense | Data Uplink | `0x3c` | Skedar shuttle activation followed by the equipped-Uplink check |
| Skedar Ruins: Battle Shrine | Target Amplifier | `0x01`, `0x02`, `0x03` | The three `if_weapon_thrown_on_object` pillar checks |

The Chicago mine surfaces are door props. The registry records
`PROPTYPE_DOOR` for those two rows and `PROPTYPE_OBJ` for the other targets so
that a reused or malformed tag cannot silently admit the wrong semantic prop.
The Escape row starts at Special Agent because Agent difficulty bypasses the
hoverbed activation check even though the mission item can exist there.

## Audited mission items without point-target rows

These items were found in mission inventories or setup scripts but do not have
an object under the crosshairs that determines success, so assigning a target
tone would misrepresent the game contract:

- Air Base's Horizon Scanner, Crash Site's President Scanner, Villa's
  R-Tracker, and Pelagic II's X-Ray Scanner interactions use their own
  view or scanner semantics. Their accessibility output belongs to the
  corresponding scanner subsystem rather than this registry.
- Deep Sea's Backup Disk completes its restore step when the player is within
  the scripted Dr. Caroll area with the disk equipped. It does not activate or
  attach to a tagged prop.
- Mr. Blonde's Revenge plants the Skedar Bomb from an in-room equipped-item
  check. There is no aim-tested destination prop.
- Air Base and Air Force One suitcases, Air Base flight plans, Pelagic II
  research tapes, G5's DAT tape, Attack Ship's necklace, keys, disguises, and
  similar mission inventory are carried, collected, or delivered without an
  aim-tested special-device destination.
- Ordinary Remote/Proximity Mines outside Chicago remain ordinary weapon
  placement. No mission script names a unique correct target surface for them.
- DataDyne Central: Extraction, Villa, Air Base, Air Force One, Crash Site,
  Pelagic II, Deep Sea, Attack Ship, Maian SOS, War!, and Duel expose no
  additional tagged object-plus-equipped-special-item contract in their setup
  scripts. Investigation's maintenance terminals are ordinary interactables;
  only its Uplink PC requires the special-device profile.

CamSpy photograph objectives are also excluded from the static registry. They
already use the generic live `criteria_holograph` adapter, which follows the
engine's framing, range, render, health, and completion rules in any stage.

## Maintenance rule

When a mission script is changed or a missed objective is reported, search the
setup for all of the following before adding a row:

1. the item grant, pickup, or intro inventory entry;
2. `if_chr_weapon_equipped`, `if_weapon_thrown_on_object`, or an equivalent
   native use condition;
3. the exact target tag and its setup prop type;
4. completion, failure, removal, invisibility, and difficulty gates.

Add a row only when the script establishes a stable point-target contract.
Record any non-point interaction here so a later audit does not turn it into a
false target. Runtime acceptance across every difficulty and co-op variant is
still required; this audit establishes code coverage, not blind-user
acceptance.
