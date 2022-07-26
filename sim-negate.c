#include "gb-sim.h"


int main(int argc, char **argv)
{ struct symbol symbols[] = {};

  struct program *program =
    parse_program_file(symbols, listsize(symbols), "sim-negate.asm");

  for (int i = 0; i < 32; i++)
  { int8_t x = rand();
    reg.a = x;
    run_program(program);
    int8_t y = reg.a;
    printf("%4d %4d  %s\n", x, y, y == -x ? "good" : "bad");
  }

  return 0;
}
