/* brally_main.c -- THE ENTRY POINT, and nothing else.
 *
 * WHY THIS FILE EXISTS
 *
 * port/host/brally.c holds every wiring decision this port makes: which hook
 * tables get installed, which module context each builder is bound to, and
 * which object the frame loop reads as "the current phase". It also used to
 * hold `main`, and that one fact put all of it outside the test gate --
 * `tools/regress.sh` runs test BINARIES, a test binary brings its own `main`,
 * and two `main`s do not link. So no suite linked port/host at all.
 *
 * That was not a theoretical gap. An agent unified 0x10AA2904 -- six host
 * objects for the one dword the menu writes and the frame loop reads -- and
 * then mutation-tested the fix by putting the whole split back. 131 suites
 * passed and `./build/brally -all` still reported 16/16. The tree could not
 * tell a wired front end from an unwired one.
 *
 * Moving four lines here is the whole structural change. brally.c is now an
 * ordinary object file that a test can link, `BrHostWireAll` is callable, and
 * port/tests/test_host_wiring.c asserts what the wiring produced. Nothing
 * about WHAT the wiring does moved or changed.
 */

int BrHostMain(int argc, char **argv);

int main(int argc, char **argv)
{
    return BrHostMain(argc, argv);
}
