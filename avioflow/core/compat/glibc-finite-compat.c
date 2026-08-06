/*
 * Compatibility shims for glibc's removed __*_finite math symbols.
 *
 * glibc < 2.31 exported __log_finite, __exp_finite and friends as aliases used
 * by code compiled with -ffinite-math-only. glibc 2.31 removed them
 * (https://sourceware.org/glibc/wiki/Release/2.31). The prebuilt libvorbis in
 * the FFmpeg package was compiled against an older glibc, so it still refers to
 * them, and a fully static link fails to resolve them on a modern host.
 *
 * The other language bindings do not hit this: they link the core into a shared
 * object, where undefined symbols are permitted and these references are never
 * reached at runtime unless Vorbis encoding is used. A Rust binary links
 * statically and must resolve every symbol, so provide them here.
 *
 * The finite variants only ever skipped the NaN and infinity checks, so
 * forwarding to the standard functions is behaviourally correct; it costs the
 * classification the caller had promised was unnecessary.
 *
 * Compiled only on Linux (see CMakeLists.txt). The symbols are weak so that a
 * host still providing them wins.
 */

#include <math.h>

#define AVIOFLOW_FINITE_ALIAS_1(name)                                          \
  __attribute__((weak)) double __##name##_finite(double x) { return name(x); }

#define AVIOFLOW_FINITE_ALIAS_1F(name)                                         \
  __attribute__((weak)) float __##name##f_finite(float x) { return name##f(x); }

AVIOFLOW_FINITE_ALIAS_1(log)
AVIOFLOW_FINITE_ALIAS_1(exp)
AVIOFLOW_FINITE_ALIAS_1F(acos)

__attribute__((weak)) double __pow_finite(double x, double y) {
  return pow(x, y);
}
