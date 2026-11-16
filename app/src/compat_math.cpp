// Host math shims.
//
// clang lowers __builtin_roundeven[f] (used by simde's SSE _mm_round_* paths,
// which the ReXGlue runtime includes) to a libcall. The MSVC UCRT does not
// provide roundeven/roundevenf, so we supply them here. They are not reachable
// on the guest side; they only satisfy the host link.

#include <cmath>

namespace {

inline double RoundEven(double x, long double (*trunc_fn)(long double)) noexcept {
  (void)trunc_fn;
  double t = std::trunc(x);
  double d = x - t;
  if (d > 0.5) {
    t += 1.0;
  } else if (d < -0.5) {
    t -= 1.0;
  } else if (d == 0.5 || d == -0.5) {
    double half = t * 0.5;
    if (half != std::trunc(half)) {
      t += (d > 0.0) ? 1.0 : -1.0;
    }
  }
  return t;
}

}  // namespace

extern "C" double roundeven(double x) noexcept {
  return RoundEven(x, nullptr);
}

extern "C" float roundevenf(float x) noexcept {
  return static_cast<float>(RoundEven(static_cast<double>(x), nullptr));
}
