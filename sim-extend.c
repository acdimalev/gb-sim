#include "gb-sim.h"


int main(int argc, char **argv)
{ struct symbol symbols[] = {};

  struct program *program =
    parse_program_file(symbols, listsize(symbols), "sim-extend.asm");

  for (int i = 0; i < 32; i++)
  { int8_t x = rand();
    reg.a = x;
    run_program(program);
    int16_t y = reg.hl;
    printf("%4d %6d  %s\n", x, y, y == x ? "good" : "bad");
  }

  return 0;
}
