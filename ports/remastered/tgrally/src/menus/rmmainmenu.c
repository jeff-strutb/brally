/* rmmainmenu.c (port): Quit on Top Gear Rally's main menu, as Top Gear Rally
 * Remastered has it on a desktop.
 *
 * The cartridge's main menu ends at Options: a console is switched off, not
 * quit.  The app closes as its window's close does (rm_quit,
 * ports/remastered/platform/mux.c), as Boss Rally's own menus' Quit does
 * there; the row is the last, after Options, its icon the console (the hand-
 * built one of the Remaster Options' rows: rmoptions.c).  On the iPhone, where
 * an app does not close itself, there is no such row.
 *
 * mainmenu.c (port) asks for the screen's items (rm_mainmenu_items) and hands
 * a chosen row past the game's own to rm_mainmenu_row. */
#include "tgr/common.h"
#include "tgr/menu.h"
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

/* -- declarations -- */
int rm_quit(void);                      /* the app's (ports/remastered/platform/options.h) */
void *rm_options_icon_console(void);   /* rmoptions.c */
/* -- end declarations -- */

#if defined(__APPLE__) && TARGET_OS_IPHONE
#define RM_QUIT_ROW 0
#else
#define RM_QUIT_ROW 1
#endif

static MenuItem s_quit;
static char s_quit_label[] = "Quit";
static MenuItem *s_items[16];
static int s_game_rows;

/* the screen's rows: the game's, then Quit (NULL-terminated) */
MenuItem **rm_mainmenu_items(MenuItem **game)
{
  int n = 0;

  while (game[n] != 0 && n < 14) {
    s_items[n] = game[n];
    n++;
  }
  s_game_rows = n;
  if (RM_QUIT_ROW && n > 0) {
    s_quit.label = s_quit_label;
    s_quit.flags = game[n - 1]->flags;  /* a row like Options: A chooses it */
    s_quit.icon = rm_options_icon_console();
    s_quit.iconRomStart = 0;
    s_quit.iconRomEnd = 0;
    s_items[n++] = &s_quit;
  }
  s_items[n] = 0;
  return s_items;
}

/* a row the game's switch does not know, chosen: Quit closes the app */
/* 1 while the main menu runs its carousel: the menus begin there, so B (the
 * cartridge's way back to the intro) does nothing and its prompt is not shown
 * (mainmenu.c, frontbuttons.c) */
int rm_menu_no_back;

void rm_mainmenu_row(int row)
{
  if (RM_QUIT_ROW && row == s_game_rows) {
    rm_quit();
  }
}
