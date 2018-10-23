#include <stdio.h>
#include <stdint.h>
#include "monitor.h"

void init_mmio();
void init_device();
work_mode_t init_monitor(int, char *[]);
void cpu_exec(uint64_t);

int main(int argc, char *argv[]) {
  init_mmio();
  /* Initialize the monitor. */
  init_monitor(argc, argv);
  init_device();
  cpu_exec(-1);
  return 0;
}
