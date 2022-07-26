#include "gb-sim.h"


int main(int argc, char **argv)
{ uint16_t dst = 0xc000;
  uint16_t src = 0x0100;
  mem[src+0] = 'h';
  mem[src+1] = 'e';
  mem[src+2] = 'l';
  mem[src+3] = 'l';
  mem[src+4] = 'o';
  uint8_t len = 5;

  struct symbol symbols[] =
  { "dst", dst
  , "src", src
  , "len", len
  };

  struct program *program =
    parse_program_file(symbols, listsize(symbols), "sim-hello.asm");

  run_program(program);
  printf("%d cycles\n", cycles);
  status();
  printf("%s\n", &mem[dst]);
  return 0;
}
