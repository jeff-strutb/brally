/* br_mat3_port.h -- PORT-ONLY pre-include for src/core/geometry/br_mat3.c.
 *
 * slice3_44.h still prototypes BrMat4BuildScaledTransposed with the stale
 * signature of the unfiled copy in slice3_44.c, while br_mat3.c defines the
 * matched one (const BrMat4 *, BrMat4 *, const BrVec3 *). MSVC only warns;
 * clang refuses. Including the header here once, with that one prototype
 * renamed out of the way, lets br_mat3.c see the types without the clash.
 * The decomp source is not edited for the port.
 */
#define BrMat4BuildScaledTransposed BrMat4BuildScaledTransposed_slice3_44_proto
#include "slice3_44.h"
#undef BrMat4BuildScaledTransposed
