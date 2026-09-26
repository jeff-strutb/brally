/* ports/macos/include/windows.h -- PORT-ONLY, empty.
 *
 * Some decomp TUs include <windows.h> outside their BR_MATCHING_BUILD block
 * even though everything that uses it is inside (net/br_netopen.c). The
 * harness build compiles only the non-matching arms, so an empty header is
 * all they need. The decomp source is not edited for the port.
 */
