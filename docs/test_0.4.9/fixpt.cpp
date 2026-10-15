// verifica offline: per ogni normalizzato float n in [0,1] (passo 1e-6 + bordi), P = from(n) e un punto fisso: from(to(P)) == P ?
#include <cmath>
#include <cstdio>
#include <algorithm>
template <class F, class T> int check (float a, float b, F from, T to) { int bad = 0, tot = 0; for (double x = 0; x <= 1.0; x += 1e-6) { const float n = (float) x, P = from (a, b, n); ++tot; if (from (a, b, to (a, b, P)) != P) ++bad; } std::printf ("  %g..%g: %d/%d non fissi\n", a, b, bad, tot); return bad; }
int main() {
  auto fF = [] (float a, float b, float n) { return a * std::pow (b / a, n); }; auto tF = [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); };
  auto fD = [] (float a, float b, float n) { return std::clamp ((float) ((double) a * std::pow ((double) b / a, (double) n)), a, b); }; auto tD = [] (float a, float b, float v) { return (float) (std::log ((double) v / a) / std::log ((double) b / a)); };
  for (auto r : { std::pair<float,float> { 10.f, 30000.f }, { 0.025f, 40.f }, { 0.1f, 200.f }, { 5.f, 2000.f } }) { std::printf ("float:"); check (r.first, r.second, fF, tF); std::printf ("double:"); check (r.first, r.second, fD, tD); } }
