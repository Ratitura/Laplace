#ifndef LAPLACE_CONFIG_H
#define LAPLACE_CONFIG_H

#include <stddef.h>

#ifndef LAPLACE_DEBUG
#define LAPLACE_DEBUG 0
#endif

#if defined(_WIN32)
#define LAPLACE_PLATFORM_WINDOWS 1
#else
#define LAPLACE_PLATFORM_WINDOWS 0
#endif

#if defined(__clang__)
#define LAPLACE_COMPILER_CLANG 1
#else
#define LAPLACE_COMPILER_CLANG 0
#endif

#define LAPLACE_BUILD_DEBUG ((LAPLACE_DEBUG) != 0)
#define LAPLACE_BUILD_RELEASE ((LAPLACE_DEBUG) == 0)
#define LAPLACE_CACHELINE_BYTES ((size_t)64u)

/* ---------- HV backend selection (Phase 03 / Phase 03.1) ---------- */

/*
 * LAPLACE_HV_BACKEND selects the *availability* of the ISPC backend.
 * When set to ISPC, the ISPC kernels are compiled and linked.  Which
 * operations actually dispatch to ISPC is governed by the per-operation
 * policy macros below.
 *
 * Values:
 *   0 = SCALAR  — scalar-only, no ISPC.  Always available.
 *   1 = ISPC    — ISPC backend is available.
 *                  Requires ISPC compilation support in build.
 *
 * The scalar implementation remains the truth-preserving reference baseline
 * regardless of which backend is active.  ISPC outputs must match scalar
 * outputs bit-exactly.
 *
 * This define is set as a PRIVATE compile flag by the build system (xmake)
 * on laplace_core.  External code (tests, benchmarks, dependents) should
 * NOT inspect this macro for backend identification.  Use the public
 * function laplace_hv_backend_name() declared in laplace/hv.h instead.
 */
#define LAPLACE_HV_BACKEND_SCALAR 0
#define LAPLACE_HV_BACKEND_ISPC   1

#ifndef LAPLACE_HV_BACKEND
#define LAPLACE_HV_BACKEND LAPLACE_HV_BACKEND_SCALAR
#endif

/* ---------- Per-operation backend policy ---------- */

/*
 * Per-operation ISPC dispatch policy.
 *
 * When LAPLACE_HV_BACKEND == LAPLACE_HV_BACKEND_ISPC, each first-wave
 * operation can independently choose scalar or ISPC dispatch.  This
 * allows selecting the optimal backend per operation based on measured
 * performance characteristics.
 *
 * Policy values:
 *   0 = use scalar backend for this operation
 *   1 = use ISPC backend for this operation
 *
 * When LAPLACE_HV_BACKEND == LAPLACE_HV_BACKEND_SCALAR, these macros
 * are ignored — all operations use scalar unconditionally.
 *
 * Defaults (based on Phase 03.2 benchmark data, 16384-bit, avx2-i32x8):
 *   BIND:         ISPC (scalar 129 ns → ISPC 66 ns, 1.95× speedup)
 *   XOR_POPCOUNT: ISPC (scalar 392 ns → ISPC 328 ns, 1.20× speedup)
 *   POPCOUNT:     ISPC (scalar 321 ns → ISPC 290 ns, 1.11× speedup)
 *
 * These defaults can be overridden at compile time to force scalar for
 * specific operations (e.g., for parity testing or when a specific
 * ISPC kernel is found to have a bug).
 *
 * Example override: -DLAPLACE_HV_OP_POPCOUNT_USE_ISPC=0
 *   Forces popcount to use scalar even when ISPC backend is available.
 */

#ifndef LAPLACE_HV_OP_BIND_USE_ISPC
#define LAPLACE_HV_OP_BIND_USE_ISPC 1
#endif

#ifndef LAPLACE_HV_OP_XOR_POPCOUNT_USE_ISPC
#define LAPLACE_HV_OP_XOR_POPCOUNT_USE_ISPC 1
#endif

#ifndef LAPLACE_HV_OP_POPCOUNT_USE_ISPC
#define LAPLACE_HV_OP_POPCOUNT_USE_ISPC 1
#endif

#endif
