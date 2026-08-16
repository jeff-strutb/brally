/* br_gamestep.h -- the GAME-STEP FUNCTION POINTER at Glide 0x106E79F4.
 *
 * THE RACE IS NOT A PHASE.  The phase the menus reach is the in-race HUD
 * overlay; the race itself is whatever this slot points at.  A previous pass
 * established that and this header does not re-derive it -- it only supplies
 * the three tiny functions that read and write the slot, whose addresses were
 * found by searching .text for references to the global:
 *
 *   0x1002E302  21 B   `return step == arg;`      the identity test
 *   0x1002E317  13 B   `step = arg;`              THE SETTER
 *   0x1002E324  11 B   `step();`                  the invoker
 *
 * and the three values that are ever stored:
 *
 *   0x10019A70   the RACE step.  It calls the menu frame itself, so the phase
 *                machinery port/host/brally.c already drives is a SUBROUTINE
 *                of the race, not a sibling.
 *   0x10032680   return to the front end.
 *   0x10008D60   the debug printf, used as a null step.
 *
 * The Win32 pump reaches the slot through a jump table on an engine-state
 * global; state 2 calls the installed step.  BrGameStepPump models that one
 * arm and nothing else -- the other arms are not ported and it says so by
 * returning a distinct result for them rather than pretending to run.
 *
 * DEVIATION: the slot is a host function pointer, not a 32-bit address, so
 * the three original values cannot be STORED in it.  BrGameStepId maps a
 * pointer back to which of the three it is, which is what makes "the race
 * step is installed" checkable from a harness instead of assumed.
 */
#ifndef BR_GAMESTEP_H
#define BR_GAMESTEP_H

/* The engine-state values the pump's jump table indexes.  Only state 2 is
 * modelled; the names are positional because only its behaviour is known. */
#define BR_GAMESTATE_STEP   2

typedef void (*BrGameStepFn)(void);

/* Which of the three the slot currently holds. */
enum {
    BR_GAMESTEP_NONE = 0,   /* NULL -- nothing installed                    */
    BR_GAMESTEP_RACE,       /* 0x10019A70                                   */
    BR_GAMESTEP_FRONTEND,   /* 0x10032680                                   */
    BR_GAMESTEP_NULL,       /* 0x10008D60, the debug printf as a null step  */
    BR_GAMESTEP_OTHER       /* a pointer the original never stored          */
};

/* 0x1002E317, 13 bytes.  Registering a body against one of the three original
 * addresses is what lets BrGameStepId answer; pass BR_GAMESTEP_OTHER (or just
 * do not register) for anything else. */
void         BrGameStepSet(BrGameStepFn pfn);
void         BrGameStepRegister(BrGameStepFn pfn, int id);

/* 0x1002E302, 21 bytes. */
int          BrGameStepIs(BrGameStepFn pfn);
/* 0x1002E324, 11 bytes.  Returns 0 if the slot is NULL and 1 if it ran. */
int          BrGameStepInvoke(void);

BrGameStepFn BrGameStepGet(void);
int          BrGameStepId(void);
const char  *BrGameStepName(int id);

/* The pump's state dispatch.  Returns 1 when the state was 2 and the step
 * ran, 0 when the state was 2 and nothing is installed, and -1 for any other
 * state -- which is "this arm is not ported", stated rather than silently
 * treated as a no-op. */
int          BrGameStepPump(int state);

#endif /* BR_GAMESTEP_H */
