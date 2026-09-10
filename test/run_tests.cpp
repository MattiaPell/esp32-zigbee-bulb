// Host test runner: calls every suite and reports the totals.

#include <cstdio>

#include "check.h"
#include "tests.h"

int main() {
  runJsonLiteTests();
  runPresetTests();
  runAdaptiveTests();

  const int failed = check::failures();
  const int total = check::checks();
  if (failed == 0) {
    std::printf("OK: %d checks passed\n", total);
    return 0;
  }
  std::printf("FAILED: %d of %d checks failed\n", failed, total);
  return 1;
}
