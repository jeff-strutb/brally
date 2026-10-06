# Cheats

*Boss Rally* was long believed to have **no cheat codes at all**: none are printed
in the manual, and none have ever circulated online. The decompilation turned up a
working cheat system hiding in plain sight: the game quietly keeps the last 32
keys you type, and the instant the tail of what you've typed spells one of six
code words, it fires. There's no cheat menu and no prompt, and there's a catch:
**keys only count while the mouse pointer is resting on the main menu's
"Credits" item.** The key-recording hook belongs to that one menu row and runs only
while the cursor is over it. Typing anywhere else in the menus does nothing, and
neither does moving to Credits with the keyboard or typing into a name box (the
name box reads each key and clears it). Case doesn't matter. A chime confirms the
code. Nothing changes on screen right away: each code just sets a flag, which
shows up later in the car and track selection screens, when you click Credits, or
when you crash.

Each code is a person's first name:

| Code | What it does |
|---|---|
| `hazel` | Unlocks **all tracks**: all six courses and their mirrored versions, twelve in all |
| `benjamin` | Unlocks **all cars**: all sixteen |
| `brielle` | Adds the **Bonus track** and its mirrored version to the track list |
| `lynette` | Adds an **unpainted copy of every unlocked car**: the same car drawn with no textures, a plain grey body. With `benjamin` too, the car list runs to 32 |
| `sophia` | Unlocks the game's **ending sequence** without finishing the championship: Credits then plays the end-of-game roll (the full staff list scrolling over the Stripmine and city tracks) instead of the normal credits |
| `madeleine` | Removes the **cap on body damage**: normally each of the car's eight dent zones stops denting once it reaches 256, and with this code dents keep piling up |

Each row was checked in the running game: every code was typed on Credits, and its
effect was observed in quick race's track and car lists, in a race, in the credits,
or in the car's dent counters during a crash. The flags behind them, in
`BRGlide.dll`: `hazel` sets `0x10AC5C50`, the gate on the track list (`0x100387F0`);
`benjamin` sets `0x10AC5C48` and `lynette` sets `0x10AC5C4C`, both read by the
car-list gate (`0x10038860`); `lynette`'s slots 16 to 31 set a flag that makes
`BrCarDrawVehicle` skip the car's textures; `brielle` sets `0x10AC5C54`, which
lengthens the track list by the Bonus slots; `sophia` sets `0x10AC5D98`, which
switches Credits to the ending; `madeleine` raises the dent cap at `0x100ABE44`
from 256 to 32767.
