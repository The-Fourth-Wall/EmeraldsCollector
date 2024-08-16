#include "collector_base/collector_base.module.spec.h"

int main(void) {
  cspec_run_suite("all", { T_collector_base(); });
}
