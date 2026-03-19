#ifndef LAPLACE_COMPILER_H
#define LAPLACE_COMPILER_H

#if defined(_MSC_VER)
#define LAPLACE_FORCE_INLINE __forceinline
#define LAPLACE_NOINLINE __declspec(noinline)
#define LAPLACE_LIKELY(x) (x)
#define LAPLACE_UNLIKELY(x) (x)
#define LAPLACE_UNREACHABLE() __assume(0)
#elif defined(__clang__) || defined(__GNUC__)
#define LAPLACE_FORCE_INLINE __attribute__((always_inline)) inline
#define LAPLACE_NOINLINE __attribute__((noinline))
#define LAPLACE_LIKELY(x) __builtin_expect(!!(x), 1)
#define LAPLACE_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define LAPLACE_UNREACHABLE() __builtin_unreachable()
#else
#define LAPLACE_FORCE_INLINE inline
#define LAPLACE_NOINLINE
#define LAPLACE_LIKELY(x) (x)
#define LAPLACE_UNLIKELY(x) (x)
#define LAPLACE_UNREACHABLE() ((void)0)
#endif

#endif
