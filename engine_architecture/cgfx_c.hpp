#pragma once
// =============================================================================
// cgfx_c.hpp — the ONE place cgfx is included from C++.
//
// cgfx headers are plain C with NO extern "C" guards, so a C++ TU that includes
// them directly would name-mangle the prototypes and fail to link
// (FOUNDATIONS sec 3a). Wrapping the include in extern "C" fixes the linkage.
//
// Included ONLY by render/*.cpp and assets/*.cpp (and asset_gpu.hpp, which is
// itself included only by those .cpp files). NEVER by a header reachable from
// core/ systems/ physics/ scene/ examples/.
//
// Reminder (FOUNDATIONS sec 3b): build cgfx descriptors as NAMED zero-init C++
// locals; the C compound-literal idiom &(CgfxDesc){...} does not compile in C++.
// =============================================================================
extern "C" {
#include "cgfx.h"
}
