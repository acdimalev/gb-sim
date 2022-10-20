#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct
{ union
  { struct { uint8_t f, a; };
    uint16_t af;
  };
  union
  { struct { uint8_t c, b; };
    uint16_t bc;
  };
  union
  { struct { uint8_t e, d; };
    uint16_t de;
  };
  union
  { struct { uint8_t l, h; };
    uint16_t hl;
  };
  uint16_t pc;
  uint16_t sp;
} reg;

uint16_t cycles = 0;

enum flag
{ FLAG_Z = 1 << 7
, FLAG_N = 1 << 6
, FLAG_H = 1 << 5
, FLAG_C = 1 << 4
};

enum r8
{ R8_A
, R8_B
, R8_C
, R8_D
, R8_E
, R8_H
, R8_L
};

enum r16
{ R16_BC
, R16_DE
, R16_HL
};

uint8_t mem[1 << 16];

enum op
{ OP_ADC_A_R8
, OP_ADC_A_IHL
, OP_ADC_A_N8
, OP_ADD_A_R8
, OP_ADD_A_IHL
, OP_ADD_A_N8
, OP_AND_A_R8
, OP_AND_A_IHL
, OP_AND_A_N8
, OP_CP_A_R8
, OP_CP_A_IHL
, OP_CP_A_N8
, OP_DEC_R8
, OP_DEC_IHL
, OP_INC_R8
, OP_INC_IHL
, OP_OR_A_R8
, OP_OR_A_IHL
, OP_OR_A_N8
, OP_SBC_A_R8
, OP_SBC_A_IHL
, OP_SBC_A_N8
, OP_SUB_A_R8
, OP_SUB_A_IHL
, OP_SUB_A_N8
, OP_XOR_A_R8
, OP_XOR_A_IHL
, OP_XOR_A_N8

, OP_ADD_HL_R16
, OP_DEC_R16
, OP_INC_R16

, OP_BIT_U3_R8
, OP_BIT_U3_IHL
, OP_RES_U3_R8
, OP_RES_U3_IHL
, OP_SET_U3_R8
, OP_SET_U3_IHL
, OP_SWAP_R8
, OP_SWAP_IHL

, OP_RL_R8
, OP_RL_IHL
, OP_RLA
, OP_RLC_R8
, OP_RLC_IHL
, OP_RLCA
, OP_RR_R8
, OP_RR_IHL
, OP_RRA
, OP_RRC_R8
, OP_RRC_IHL
, OP_RRCA
, OP_SLA_R8
, OP_SLA_IHL
, OP_SRA_R8
, OP_SRA_IHL
, OP_SRL_R8
, OP_SRL_IHL

, OP_LD_R8_R8
, OP_LD_R8_N8
, OP_LD_R16_N16
, OP_LD_IHL_R8
, OP_LD_IHL_N8
, OP_LD_R8_IHL
, OP_LD_IR16_A
, OP_LD_IN16_A
, OP_LDH_IN16_A
, OP_LDH_IC_A
, OP_LD_A_IR16
, OP_LD_A_IN16
, OP_LDH_A_IN16
, OP_LDH_A_IC
, OP_LD_IHLI_A
, OP_LD_IHLD_A
, OP_LD_A_IHLI
, OP_LD_A_IHLD

, OP_CALL_N16
, OP_CALL_CC_N16
, OP_JP_HL
, OP_JP_N16
, OP_JP_CC_N16
, OP_JR_E8
, OP_JR_CC_E8
, OP_RET_CC
, OP_RET
, OP_RETI
, OP_RST_VEC

, OP_ADD_HL_SP
, OP_ADD_SP_E8
, OP_DEC_SP
, OP_INC_SP
, OP_LD_SP_N16
, OP_LD_IN16_SP
, OP_LD_HL_SPE8
, OP_LD_SP_HL
, OP_POP_AF
, OP_POP_R16
, OP_PUSH_AF
, OP_PUSH_R16

, OP_CCF
, OP_CPL
, OP_DAA
, OP_DI
, OP_EI
, OP_HALT
, OP_NOP
, OP_SCF
, OP_STOP
};

enum cc
{ CC_NZ
};

struct instruction
{ enum op op;
  uint16_t p1, p2;
};

struct program
{ size_t length;
  struct instruction instructions[0];
};

struct symbol
{ char name[16];
  int32_t value;
};


void _panic(int line)
{ printf("PANIC: %d\n", line);
  exit(-1);
}

#define panic _panic(__LINE__)


void status()
{ char
    z = reg.f & FLAG_Z ? 'Z' : '-'
  , n = reg.f & FLAG_N ? 'N' : '-'
  , h = reg.f & FLAG_H ? 'H' : '-'
  , c = reg.f & FLAG_C ? 'C' : '-'
  ;
  printf
  ( "A: %02x  F: %02x  (AF: %04x)\n"
    "B: %02x  C: %02x  (BC: %04x)\n"
    "D: %02x  E: %02x  (DE: %04x)\n"
    "H: %02x  L: %02x  (HL: %04x)\n"
    "PC: %04x  SP: %04x\n"
    "F: [%c%c%c%c]\n"
  , reg.a, reg.f, reg.af
  , reg.b, reg.c, reg.bc
  , reg.d, reg.e, reg.de
  , reg.h, reg.l, reg.hl
  , reg.pc
  , reg.sp
  , z, n, h, c
  );
}


static inline uint8_t _get_r8(enum r8 r)
{ switch (r)
  { case R8_A: return reg.a;
    case R8_B: return reg.b;
    case R8_C: return reg.c;
    case R8_D: return reg.d;
    case R8_E: return reg.e;
    case R8_H: return reg.h;
    case R8_L: return reg.l;
    default: panic;
  }
}


static inline uint16_t _get_r16(enum r16 r)
{ switch (r)
  { case R16_BC: return reg.bc;
    case R16_DE: return reg.de;
    case R16_HL: return reg.hl;
    default: panic;
  }
}


static inline void _set_r8(enum r8 r, uint8_t val)
{ switch (r)
  { case R8_A: reg.a = val; break;
    case R8_B: reg.b = val; break;
    case R8_C: reg.c = val; break;
    case R8_D: reg.d = val; break;
    case R8_E: reg.e = val; break;
    case R8_H: reg.h = val; break;
    case R8_L: reg.l = val; break;
    default: panic;
  }
}


static inline uint16_t _set_r16(enum r16 r, uint16_t val)
{ switch (r)
  { case R16_BC: reg.bc = val; break;
    case R16_DE: reg.de = val; break;
    case R16_HL: reg.hl = val; break;
    default: panic;
  }
}


// 8-bit arithmetic and logic instructions


static inline void _adc(uint8_t val)
{ uint16_t tmp = reg.a + val + (FLAG_C & reg.f ? 1 : 0);
  reg.f =
    (0xff & tmp ? 0 : FLAG_Z)
  | (0x10 & (reg.a ^ val ^ tmp) ? FLAG_H : 0)
  | (0x100 & tmp ? FLAG_C : 0)
  ;
  reg.a = tmp;
}

void adc_a_r8(enum r8 src)
{ _adc(_get_r8(src)); cycles += 1; }

void adc_a_ihl()
{ _adc(mem[reg.hl]); cycles += 2; }

void adc_a_n8(uint8_t val)
{ _adc(val); cycles += 2; }


static inline void _add(uint8_t val)
{ uint16_t tmp = reg.a + val;
  reg.f =
    (0xff & tmp ? 0 : FLAG_Z)
  | (0x10 & (reg.a ^ val ^ tmp) ? FLAG_H : 0)
  | (0x100 & tmp ? FLAG_C : 0)
  ;
  reg.a = tmp;
}

void add_a_r8(enum r8 src)
{ _add(_get_r8(src)); cycles += 1; }

void add_a_ihl()
{ _add(mem[reg.hl]); cycles += 2; }

void add_a_n8(uint8_t val)
{ _add(val); cycles += 2; }


static inline void _and(uint8_t val)
{ reg.a &= val;
  reg.f =
    (reg.a ? 0 : FLAG_Z)
  | FLAG_H
  ;
}

void and_a_r8(enum r8 src)
{ _and(_get_r8(src)); cycles += 1; }

void and_a_ihl()
{ _and(mem[reg.hl]); cycles += 2; }

void and_a_n8(uint8_t val)
{ _and(val); cycles += 2; }


// TODO: validate carry and half-carry
static inline void _cp(uint8_t val)
{ uint16_t tmp = reg.a - val;
  reg.f =
    (0xff & tmp ? 0 : FLAG_Z)
  | FLAG_N
  | (0x10 & (reg.a ^ val ^ tmp) ? FLAG_H : 0)
  | (0x100 & tmp ? FLAG_C : 0)
  ;
}

void cp_a_r8(enum r8 src)
{ _cp(_get_r8(src)); cycles += 1; }

void cp_a_ihl()
{ _cp(mem[reg.hl]); cycles += 2; }

void cp_a_n8(uint8_t val)
{ _cp(val); cycles += 2; }


static inline uint8_t _dec(uint8_t val)
{ uint8_t tmp = val - 1;
  reg.f =
    (tmp ? 0 : FLAG_Z)
  | FLAG_N
  | (0x10 & (val ^ tmp) ? FLAG_H : 0)
  | FLAG_C & reg.f
  ;
  return tmp;
}

void dec_r8(enum r8 dst)
{ _set_r8(dst, _dec(_get_r8(dst))); cycles += 1; }

void dec_ihl()
{ mem[reg.hl] = _dec(mem[reg.hl]); cycles += 3; }


static inline uint8_t _inc(uint8_t val)
{ uint8_t tmp = val + 1;
  reg.f =
    (tmp ? 0 : FLAG_Z)
  | (0x10 & (val ^ tmp) ? FLAG_H : 0)
  | FLAG_C & reg.f
  ;
  return tmp;
}

void inc_r8(enum r8 dst)
{ _set_r8(dst, _inc(_get_r8(dst))); cycles += 1; }

void inc_ihl()
{ mem[reg.hl] = _inc(mem[reg.hl]); cycles += 3; }


static inline void _or(uint8_t val)
{ reg.a |= val;
  reg.f =
    (reg.a ? 0 : FLAG_Z)
  ;
}

void or_a_r8(enum r8 src)
{ _or(_get_r8(src)); cycles += 1; }

void or_a_ihl()
{ _or(mem[reg.hl]); cycles += 2; }

void or_a_n8(uint8_t val)
{ _or(val); cycles += 2; }


static inline void _sbc(uint8_t val)
{ uint16_t tmp = reg.a - val - (FLAG_C & reg.f ? 1 : 0);
  reg.f =
    (0xff & tmp ? 0 : FLAG_Z)
  | FLAG_N
  | (0x10 & (reg.a ^ val ^ tmp) ? FLAG_H : 0)
  | (0x100 & tmp ? FLAG_C : 0)
  ;
  reg.a = tmp;
}

void sbc_a_r8(enum r8 src)
{ _sbc(_get_r8(src)); cycles += 1; }

void sbc_a_ihl()
{ _sbc(mem[reg.hl]); cycles += 2; }

void sbc_a_n8(uint8_t val)
{ _sbc(val); cycles += 2; }


static inline void _sub(uint8_t val)
{ uint16_t tmp = reg.a - val;
  reg.f =
    (0xff & tmp ? 0 : FLAG_Z)
  | FLAG_N
  | (0x10 & (reg.a ^ val ^ tmp) ? FLAG_H : 0)
  | (0x100 & tmp ? FLAG_C : 0)
  ;
  reg.a = tmp;
}

void sub_a_r8(enum r8 src)
{ _sub(_get_r8(src)); cycles += 1; }

void sub_a_ihl()
{ _sub(mem[reg.hl]); cycles += 2; }

void sub_a_n8(uint8_t val)
{ _sub(val); cycles += 2; }


static inline void _xor(uint8_t val)
{ reg.a ^= val;
  reg.f =
    (reg.a ? 0 : FLAG_Z)
  ;
}

void xor_a_r8(enum r8 src)
{ _xor(_get_r8(src)); cycles += 1; }

void xor_a_ihl()
{ _xor(mem[reg.hl]); cycles += 2; }

void xor_a_n8(uint8_t val)
{ _xor(val); cycles += 2; }


// 16-bit arithmetic and logic instructions


static inline void _add_hl(uint16_t val)
{ uint32_t tmp = reg.hl + val;
  reg.f =
    FLAG_Z & reg.f
  | (0x1000 & (reg.hl ^ val ^ tmp) ? FLAG_H : 0)
  | (0x10000 & tmp ? FLAG_C : 0)
  ;
  reg.hl = tmp;
}

void add_hl_r16(enum r16 src)
{ _add_hl(_get_r16(src)); cycles += 2; }


void dec_r16(enum r16 dst)
{ switch(dst)
  { case R16_BC: reg.bc--; break;
    case R16_DE: reg.de--; break;
    case R16_HL: reg.hl--; break;
    default: panic;
  }
  cycles += 2;
}

void inc_r16(enum r16 dst)
{ switch(dst)
  { case R16_BC: reg.bc++; break;
    case R16_DE: reg.de++; break;
    case R16_HL: reg.hl++; break;
    default: panic;
  }
  cycles += 2;
}


// bit operations instructions


static inline void _bit(uint8_t bit, uint8_t val)
{ bit &= 0b111;
  reg.f =
    ((1 << bit) & val ? 0 : FLAG_Z)
  | FLAG_H
  | FLAG_C & reg.f
  ;
}

void bit_u3_r8(uint8_t bit, enum r8 r)
{ _bit(bit, _get_r8(r)); cycles += 2; }

void bit_u3_ihl(uint8_t bit)
{ _bit(bit, mem[reg.hl]); cycles += 3; }


static inline uint8_t _res(uint8_t bit, uint8_t val)
{ bit &= 0b111;
  return val & ~(1 << bit);
}

void res_u3_r8(uint8_t bit, enum r8 r)
{ _set_r8(r, _res(bit, _get_r8(r))); cycles += 2; }

void res_u3_ihl(uint8_t bit)
{ mem[reg.hl] = _res(bit, mem[reg.hl]); cycles += 4; }


static inline uint8_t _set(uint8_t bit, uint8_t val)
{ bit &= 0b111;
  return val | (1 << bit);
}

void set_u3_r8(uint8_t bit, enum r8 r)
{ _set_r8(r, _set(bit, _get_r8(r))); cycles += 2; }

void set_u3_ihl(uint8_t bit)
{ mem[reg.hl] = _set(bit, mem[reg.hl]); cycles += 4; }


static inline uint8_t _swap(uint8_t val)
{ reg.f =
    val ? 0 : FLAG_Z
  ;
  return val << 4 | val >> 4;
}

void swap_r8(enum r8 r)
{ _set_r8(r, _swap(_get_r8(r))); cycles += 2; }

void swap_ihl()
{ mem[reg.hl] = _swap(mem[reg.hl]); cycles += 4; }


// bit shift instructions


static inline uint8_t _rl(uint8_t val)
{ uint16_t tmp = val << 1 | (FLAG_C & reg.f ? 1 : 0);
  reg.f =
    (0xff & tmp ? 0 : FLAG_Z)
  | (0x100 & tmp ? FLAG_C : 0)
  ;
  return tmp;
}

void rl_r8(enum r8 r)
{ _set_r8(r, _rl(_get_r8(r))); cycles += 2; }

void rl_ihl()
{ mem[reg.hl] = _rl(mem[reg.hl]); cycles += 4; }

void rla()
{ uint16_t tmp = reg.a << 1 | (FLAG_C & reg.f ? 1 : 0);
  reg.f =
    (0x100 & tmp ? FLAG_C : 0)
  ;
  reg.a = tmp;
  cycles += 1;
}


static inline uint8_t _rlc(uint8_t val)
{ uint16_t tmp = val << 1 | (0x80 & val ? 1 : 0);
  reg.f =
    (0xff & tmp ? 0 : FLAG_Z)
  | (0x100 & tmp ? FLAG_C : 0)
  ;
  return tmp;
}

void rlc_r8(enum r8 r)
{ _set_r8(r, _rlc(_get_r8(r))); cycles += 2; }

void rlc_ihl()
{ mem[reg.hl] = _rlc(mem[reg.hl]); cycles += 4; }

void rlca()
{ uint16_t tmp = reg.a << 1 | (0x80 & reg.a ? 1 : 0);
  reg.f =
    (0x100 & tmp ? FLAG_C : 0)
  ;
  reg.a = tmp;
  cycles += 1;
}


static inline uint8_t _rr(uint8_t val)
{ bool carry = 1 & val;
  val = (FLAG_C & reg.f ? 0x80 : 0) | val >> 1;
  reg.f =
    (val ? 0 : FLAG_Z)
  | (carry ? FLAG_C : 0)
  ;
  return val;
}

void rr_r8(enum r8 r)
{ _set_r8(r, _rr(_get_r8(r))); cycles += 2; }

void rr_ihl()
{ mem[reg.hl] = _rr(mem[reg.hl]); cycles += 4; }

void rra()
{ bool carry = 1 & reg.a;
  reg.a = (FLAG_C & reg.f ? 0x80 : 0) | reg.a >> 1;
  reg.f =
    (carry ? FLAG_C : 0)
  ;
  cycles += 1;
}


static inline uint8_t _rrc(uint8_t val)
{ bool carry = 1 & val;
  val = (carry ? 0x80 : 0) | val >> 1;
  reg.f =
    (val ? 0 : FLAG_Z)
  | (carry ? FLAG_C : 0)
  ;
  return val;
}

void rrc_r8(enum r8 r)
{ _set_r8(r, _rrc(_get_r8(r))); cycles += 2; }

void rrc_ihl()
{ mem[reg.hl] = _rrc(mem[reg.hl]); cycles += 4; }

void rrca()
{ bool carry = 1 & reg.a;
  reg.a = (carry ? 0x80 : 0) | reg.a >> 1;
  reg.f =
    (carry ? FLAG_C : 0)
  ;
  cycles += 1;
}


static inline uint8_t _sla(uint8_t val)
{ uint16_t tmp = val << 1;
  reg.f =
    (0xff & tmp ? 0 : FLAG_Z)
  | (0x100 & tmp ? FLAG_C : 0)
  ;
  return tmp;
}

void sla_r8(enum r8 r)
{ _set_r8(r, _sla(_get_r8(r))); cycles += 2; }

void sla_ihl()
{ mem[reg.hl] = _sla(mem[reg.hl]); cycles += 4; }


static inline uint8_t _sra(uint8_t val)
{ bool carry = 1 & val;
  val = 0x80 & val | val >> 1;
  reg.f =
    (val ? 0 : FLAG_Z)
  | (carry ? FLAG_C : 0)
  ;
  return val;
}

void sra_r8(enum r8 r)
{ _set_r8(r, _sra(_get_r8(r))); cycles += 2; }

void sra_ihl()
{ mem[reg.hl] = _sra(mem[reg.hl]); cycles += 4; }


static inline uint8_t _srl(uint8_t val)
{ bool carry = 1 & val;
  val >>= 1;
  reg.f =
    (val ? 0 : FLAG_Z)
  | (carry ? FLAG_C : 0)
  ;
  return val;
}

void srl_r8(enum r8 r)
{ _set_r8(r, _srl(_get_r8(r))); cycles += 2; }

void srl_ihl()
{ mem[reg.hl] = _srl(mem[reg.hl]); cycles += 4; }


// load instructions


void ld_r8_r8(enum r8 dst, enum r8 src)
{ _set_r8(dst, _get_r8(src));
  cycles += 1;
}

void ld_r8_n8(enum r8 dst, uint8_t val)
{ _set_r8(dst, val);
  cycles += 2;
}

void ld_r16_n16(enum r16 dst, uint16_t val)
{ _set_r16(dst, val);
  cycles += 3;
}


void ld_ihl_r8(enum r8 src)
{ mem[reg.hl] = _get_r8(src);
  cycles += 2;
}

void ld_ihl_n8(uint8_t val)
{ mem[reg.hl] = val;
  cycles += 3;
}

void ld_r8_ihl(enum r8 dst)
{ _set_r8(dst, mem[reg.hl]);
  cycles += 2;
}


void ld_ir16_a(enum r16 idst)
{ mem[_get_r16(idst)] = reg.a;
  cycles += 2;
}

void ld_in16_a(uint16_t idst)
{ mem[idst] = reg.a;
  cycles += 4;
}

void ldh_in16_a(uint16_t idst)
{ if (0xff00 > idst || 0xffff < idst) panic;
  mem[idst] = reg.a;
  cycles += 3;
}

void ldh_ic_a()
{ mem[0xff00 | reg.c] = reg.a;
  cycles += 2;
}

void ld_a_ir16(enum r16 isrc)
{ reg.a = mem[_get_r16(isrc)];
  cycles += 2;
}

void ld_a_in16(uint16_t isrc)
{ reg.a = mem[isrc];
  cycles += 4;
}

void ldh_a_in16(uint16_t isrc)
{ if (0xff00 > isrc || 0xffff < isrc) panic;
  reg.a = mem[isrc];
  cycles += 3;
}

void ldh_a_ic()
{ reg.a = mem[0xff00 | reg.c];
  cycles += 2;
}


void ld_ihli_a()
{ mem[reg.hl++] = reg.a;
  cycles += 2;
}

void ld_ihld_a()
{ mem[reg.hl--] = reg.a;
  cycles += 2;
}

void ld_a_ihli()
{ reg.a = mem[reg.hl++];
  cycles += 2;
}

void ld_a_ihld()
{ reg.a = mem[reg.hl--];
  cycles += 2;
}


// stack operations instructions


static inline int16_t _spe8(int8_t val)
{ uint16_t tmp = reg.sp + val;
  reg.f =
    (0x10 & (reg.sp ^ val ^ tmp) ? FLAG_H : 0)
  | (0x100 & (reg.sp ^ val ^ tmp) ? FLAG_C : 0)
  ;
  return tmp;
}

void add_hl_sp()
{ _add_hl(reg.sp); cycles += 2; }

void add_sp_e8(int8_t val)
{ reg.sp = _spe8(val); cycles += 4; }


void dec_sp()
{ reg.sp--; cycles += 2; }

void inc_sp()
{ reg.sp++; cycles += 2; }


void ld_sp_n16(uint16_t val)
{ reg.sp = val; cycles += 3; }

void ld_in16_sp(uint16_t idst)
{ mem[idst+0 & 0xffff] = reg.sp;
  mem[idst+1 & 0xffff] = reg.sp >> 8;
  cycles += 5;
}

void ld_hl_spe8(int8_t val)
{ reg.hl = _spe8(val); cycles += 3; }

void ld_sp_hl()
{ reg.sp = reg.hl; cycles += 2; }


static inline uint16_t _pop()
{ uint16_t tmp = mem[reg.sp++];
  tmp |= mem[reg.sp++] << 8;
  return tmp;
}

void pop_af()
{ reg.af = _pop(); cycles += 3; }

void pop_r16(enum r16 r)
{ _set_r16(r, _pop()); cycles += 3; }


static inline void _push(uint16_t val)
{ mem[--reg.sp] = val >> 8;
  mem[--reg.sp] = val;
}

void push_af()
{ _push(reg.af); cycles += 4; }

void push_r16(enum r16 r)
{ _push(_get_r16(r)); cycles += 4; }


// miscellaneous instructions


void ccf()
{ reg.f =
    FLAG_Z & reg.f
  | (FLAG_C & reg.f ? 0 : FLAG_C)
  ;
  cycles += 1;
}

void cpl()
{ reg.a = ~reg.a;
  reg.f |= FLAG_N | FLAG_H;
  cycles += 1;
}

void daa()
{ uint16_t tmp = reg.a;
  tmp += (0x0f & tmp > 0x09) || (FLAG_H & reg.f) ? 0x06 : 0;
  tmp += (0xf0 & tmp > 0x90) || (FLAG_C & reg.f) ? 0x60 : 0;
  reg.f =
    (0xff & tmp ? 0 : FLAG_Z)
  | FLAG_N & reg.f
  | (0x100 & tmp ? FLAG_C : 0)
  ;
  reg.a = tmp;
  cycles += 1;
}


void di()
{ panic; }

void ei()
{ panic; }

void halt()
{ panic; }


void nop()
{ cycles += 1; }

void scf()
{ reg.f =
    FLAG_Z & reg.f
  | FLAG_C;
  ;
  cycles += 1;
}


void stop()
{ panic; }


#define listsize(list) (sizeof(list) / sizeof(*list))


void run_program(struct program *program)
{ cycles = 0;
  uint16_t pc = 0;
  while (program->length > pc)
  { struct instruction *x = &program->instructions[pc++];
    switch (x->op)
    { case OP_ADC_A_R8: adc_a_r8(x->p1); break;
      case OP_ADC_A_IHL: adc_a_ihl(); break;
      case OP_ADC_A_N8: adc_a_n8(x->p1); break;
      case OP_ADD_A_R8: add_a_r8(x->p1); break;
      case OP_ADD_A_IHL: add_a_ihl(); break;
      case OP_ADD_A_N8: add_a_n8(x->p1); break;
      case OP_AND_A_R8: and_a_r8(x->p1); break;
      case OP_AND_A_IHL: and_a_ihl(); break;
      case OP_AND_A_N8: and_a_n8(x->p1); break;
      case OP_CP_A_R8: cp_a_r8(x->p1); break;
      case OP_CP_A_IHL: cp_a_ihl(); break;
      case OP_CP_A_N8: cp_a_n8(x->p1); break;
      case OP_DEC_R8: dec_r8(x->p1); break;
      case OP_DEC_IHL: dec_ihl(); break;
      case OP_INC_R8: inc_r8(x->p1); break;
      case OP_INC_IHL: inc_ihl(); break;
      case OP_OR_A_R8: or_a_r8(x->p1); break;
      case OP_OR_A_IHL: or_a_ihl(); break;
      case OP_OR_A_N8: or_a_n8(x->p1); break;
      case OP_SBC_A_R8: sbc_a_r8(x->p1); break;
      case OP_SBC_A_IHL: sbc_a_ihl(); break;
      case OP_SBC_A_N8: sbc_a_n8(x->p1); break;
      case OP_SUB_A_R8: sub_a_r8(x->p1); break;
      case OP_SUB_A_IHL: sub_a_ihl(); break;
      case OP_SUB_A_N8: sub_a_n8(x->p1); break;
      case OP_XOR_A_R8: xor_a_r8(x->p1); break;
      case OP_XOR_A_IHL: xor_a_ihl(); break;
      case OP_XOR_A_N8: xor_a_n8(x->p1); break;

      case OP_ADD_HL_R16: add_hl_r16(x->p1); break;
      case OP_DEC_R16: dec_r16(x->p1); break;
      case OP_INC_R16: inc_r16(x->p1); break;

      case OP_BIT_U3_R8: bit_u3_r8(x->p1, x->p2); break;
      case OP_BIT_U3_IHL: bit_u3_ihl(x->p1); break;
      case OP_RES_U3_R8: res_u3_r8(x->p1, x->p2); break;
      case OP_RES_U3_IHL: res_u3_ihl(x->p1); break;
      case OP_SET_U3_R8: set_u3_r8(x->p1, x->p2); break;
      case OP_SET_U3_IHL: set_u3_ihl(x->p1); break;
      case OP_SWAP_R8: swap_r8(x->p1); break;
      case OP_SWAP_IHL: swap_ihl(); break;

      case OP_RL_R8: rl_r8(x->p1); break;
      case OP_RL_IHL: rl_ihl(); break;
      case OP_RLA: rla(); break;
      case OP_RLC_R8: rlc_r8(x->p1); break;
      case OP_RLC_IHL: rlc_ihl(); break;
      case OP_RLCA: rlca(); break;
      case OP_RR_R8: rr_r8(x->p1); break;
      case OP_RR_IHL: rr_ihl(); break;
      case OP_RRA: rra(); break;
      case OP_RRC_R8: rrc_r8(x->p1); break;
      case OP_RRC_IHL: rrc_ihl(); break;
      case OP_RRCA: rrca(); break;
      case OP_SLA_R8: sla_r8(x->p1); break;
      case OP_SLA_IHL: sla_ihl(); break;
      case OP_SRA_R8: sra_r8(x->p1); break;
      case OP_SRA_IHL: sra_ihl(); break;
      case OP_SRL_R8: srl_r8(x->p1); break;
      case OP_SRL_IHL: srl_ihl(); break;

      case OP_LD_R8_R8: ld_r8_r8(x->p1, x->p2); break;
      case OP_LD_R8_N8: ld_r8_n8(x->p1, x->p2); break;
      case OP_LD_R16_N16: ld_r16_n16(x->p1, x->p2); break;
      case OP_LD_IHL_R8: ld_ihl_r8(x->p1); break;
      case OP_LD_IHL_N8: ld_ihl_n8(x->p1); break;
      case OP_LD_R8_IHL: ld_r8_ihl(x->p1); break;
      case OP_LD_IR16_A: ld_ir16_a(x->p1); break;
      case OP_LD_IN16_A: ld_in16_a(x->p1); break;
      case OP_LDH_IN16_A: ldh_in16_a(x->p1); break;
      case OP_LDH_IC_A: ldh_ic_a(); break;
      case OP_LD_A_IR16: ld_a_ir16(x->p1); break;
      case OP_LD_A_IN16: ld_a_in16(x->p1); break;
      case OP_LDH_A_IN16: ldh_a_in16(x->p1); break;
      case OP_LDH_A_IC: ldh_a_ic(); break;
      case OP_LD_IHLI_A: ld_ihli_a(); break;
      case OP_LD_IHLD_A: ld_ihld_a(); break;
      case OP_LD_A_IHLI: ld_a_ihli(); break;
      case OP_LD_A_IHLD: ld_a_ihld(); break;

      case OP_CALL_N16: panic;
      case OP_CALL_CC_N16: panic;
      case OP_JP_HL: panic;
      case OP_JP_N16: panic;
      case OP_JP_CC_N16: panic;
      case OP_JR_E8: panic;
      case OP_JR_CC_E8:
      { bool flag = false;
        switch (x->p1)
        { case CC_NZ: flag = !(FLAG_Z & reg.f); break;
        }
        if (flag) { pc += x->p2; cycles += 3; } else { cycles += 2; }
        break;
      }
      case OP_RET_CC: panic;
      case OP_RET: panic;
      case OP_RETI: panic;
      case OP_RST_VEC: panic;

      case OP_ADD_HL_SP: add_hl_sp(); break;
      case OP_ADD_SP_E8: add_sp_e8(x->p1); break;
      case OP_DEC_SP: dec_sp(); break;
      case OP_INC_SP: inc_sp(); break;
      case OP_LD_SP_N16: ld_sp_n16(x->p1); break;
      case OP_LD_IN16_SP: ld_in16_sp(x->p1); break;
      case OP_LD_HL_SPE8: ld_hl_spe8(x->p1); break;
      case OP_LD_SP_HL: ld_sp_hl(); break;
      case OP_POP_AF: pop_af(); break;
      case OP_POP_R16: pop_r16(x->p1); break;
      case OP_PUSH_AF: push_af(); break;
      case OP_PUSH_R16: push_r16(x->p1); break;

      case OP_CCF: ccf(); break;
      case OP_CPL: cpl(); break;
      case OP_DAA: daa(); break;
      case OP_DI: di(); break;
      case OP_EI: ei(); break;
      case OP_HALT: halt(); break;
      case OP_NOP: nop(); break;
      case OP_SCF: scf(); break;
      case OP_STOP: stop(); break;

      default: panic;
    }
  }
  // printf("cycles: %d\n", cycles);
}


enum isn_token
{ ADC_TOK
, ADD_TOK
, AND_TOK
, CP_TOK
, DEC_TOK
, INC_TOK
, OR_TOK
, SBC_TOK
, SUB_TOK
, XOR_TOK

, BIT_TOK
, RES_TOK
, SET_TOK
, SWAP_TOK

, RL_TOK
, RLA_TOK
, RLC_TOK
, RLCA_TOK
, RR_TOK
, RRA_TOK
, RRC_TOK
, RRCA_TOK
, SLA_TOK
, SRA_TOK
, SRL_TOK

, LD_TOK
, LDH_TOK

, CALL_TOK
, JP_TOK
, JR_TOK
, RET_TOK
, RETI_TOK
, RST_TOK

, POP_TOK
, PUSH_TOK

, CCF_TOK
, CPL_TOK
, DAA_TOK
, DI_TOK
, EI_TOK
, HALT_TOK
, NOP_TOK
, SCF_TOK
, STOP_TOK
};

struct
{ char string[8];
  enum isn_token value;
} isn_tokens[] =
{ "adc", ADC_TOK
, "add", ADD_TOK
, "and", AND_TOK
, "cp", CP_TOK
, "dec", DEC_TOK
, "inc", INC_TOK
, "or", OR_TOK
, "sbc", SBC_TOK
, "sub", SUB_TOK
, "xor", XOR_TOK

, "bit", BIT_TOK
, "res", RES_TOK
, "set", SET_TOK
, "swap", SWAP_TOK

, "rl", RL_TOK
, "rla", RLA_TOK
, "rlc", RLC_TOK
, "rlca", RLCA_TOK
, "rr", RR_TOK
, "rra", RRA_TOK
, "rrc", RRC_TOK
, "rrca", RRCA_TOK
, "sla", SLA_TOK
, "sra", SRA_TOK
, "srl", SRL_TOK

, "ld", LD_TOK
, "ldh", LDH_TOK

, "call", CALL_TOK
, "jp", JP_TOK
, "jr", JR_TOK
, "ret", RET_TOK
, "reti", RETI_TOK
, "rst", RST_TOK

, "pop", POP_TOK
, "push", PUSH_TOK

, "ccf", CCF_TOK
, "cpl", CPL_TOK
, "daa", DAA_TOK
, "di", DI_TOK
, "ei", EI_TOK
, "halt", HALT_TOK
, "nop", NOP_TOK
, "scf", SCF_TOK
, "stop", STOP_TOK
};
size_t n_isn_tokens = listsize(isn_tokens);

enum arg_token_type
{ NONE_TOK_TYPE
, R16_TOK_TYPE
, N16_TOK_TYPE
, R8_TOK_TYPE
, N8_TOK_TYPE
, IR16_TOK_TYPE
, IHLI_TOK_TYPE
, CC_TOK_TYPE
, ANON_LABEL_TOK_TYPE
, IHL_TOK_TYPE
, U3_TOK_TYPE
, IN16_TOK_TYPE
, IC_TOK_TYPE
, IHLD_TOK_TYPE
, E8_TOK_TYPE
, SP_TOK_TYPE
, SPE8_TOK_TYPE
, AF_TOK_TYPE
};

struct
{ char string[8];
  enum arg_token_type type;
  int32_t value;
} arg_tokens[] =
{ "", NONE_TOK_TYPE, 0
, "hl", R16_TOK_TYPE, R16_HL
, "de", R16_TOK_TYPE, R16_DE
, "c", R8_TOK_TYPE, R8_C
, "a", R8_TOK_TYPE, R8_A
, "[de]", IR16_TOK_TYPE, R16_DE
, "[hli]", IHLI_TOK_TYPE, 0
, "nz", CC_TOK_TYPE, CC_NZ
, ":-", ANON_LABEL_TOK_TYPE, -1
, "l", R8_TOK_TYPE, R8_L
, "h", R8_TOK_TYPE, R8_H
, "[hl]", IHL_TOK_TYPE, 0
, "[hld]", IHLD_TOK_TYPE, 0
, "sp", SP_TOK_TYPE, 0
, "af", AF_TOK_TYPE, 0
, "b", R8_TOK_TYPE, R8_B
, ":--", ANON_LABEL_TOK_TYPE, -2
, "d", R8_TOK_TYPE, R8_D
, "bc", R16_TOK_TYPE, R16_BC
};
size_t n_arg_tokens = listsize(arg_tokens);

struct arg_token
{ enum arg_token_type type;
  int32_t value;
};


size_t nextdelim(char *delim, char *string)
{ int i = 0;
  while (true)
  { if (0 == string[i]) return i;
    for (char *c = delim; *c; c++)
      if (*c == string[i]) return i;
    i++;
  }
}


struct arg_token parse_arg
( struct symbol *symbols, size_t n_symbols
, char *string, size_t n
)
{ int i;
  for (i = 0; n_arg_tokens > i; i++)
    if (n == strlen(arg_tokens[i].string) && !strncmp(arg_tokens[i].string, string, n)) break;
  if (n_arg_tokens != i)
    return (struct arg_token){ arg_tokens[i].type, arg_tokens[i].value };

  for (i = 0; n_symbols > i; i++)
    if (n == strlen(symbols[i].name) && !strncmp(symbols[i].name, string, n)) break;
  if (n_symbols != i)
  { uint16_t value = symbols[i].value;
    if (0 <= value && 8 > value)
      return (struct arg_token){ U3_TOK_TYPE, value };
    if (-128 <= value && 128 > value)
      return (struct arg_token){ E8_TOK_TYPE, value };
    if (-128 <= value && 256 > value)
      return (struct arg_token){ N8_TOK_TYPE, value };
    if (-32768 <= value && 65536 > value)
      return (struct arg_token){ N16_TOK_TYPE, value };
  }
  panic;
}


struct program *parse_program
( struct symbol *symbols, size_t n_symbols
, char *text
)
{ int max_instructions = 256;
  struct program *program =
    malloc(sizeof(struct program) + max_instructions * sizeof(struct instruction));
  program->length = 0;
  int max_anonymous_labels = max_instructions;
  int anonymous_labels[max_anonymous_labels];
  size_t n_anonymous_labels = 0;

  char *line = text;

  while (0 != *line)
  { int i = 0, n;
    if (':' == line[i])
    { anonymous_labels[n_anonymous_labels++] = program->length;
      if (max_anonymous_labels == n_anonymous_labels) panic;
      i++;
    }
    n = nextdelim("\n", line);
printf("%.*s\n", n, line);
    // skip whitespace
    while (' ' == line[i]) i++;
    // match instruction
    n = nextdelim(" \n", &line[i]);
    enum isn_token isn_token;
    { int j = 0;
      for (; n_isn_tokens > j; j++)
        if (n == strlen(isn_tokens[j].string) && !strncmp(isn_tokens[j].string, &line[i], n)) break;
      if (n_isn_tokens == j) panic;
      isn_token = isn_tokens[j].value;
    }
    i += n+1;
    struct arg_token first_arg_token;
    if (' ' == line[i-1])
    { // parse first parameter
      n = nextdelim(",\n", &line[i]);
      first_arg_token = parse_arg(symbols, n_symbols, &line[i], n);
      i += n+1;
    }
    else
    { first_arg_token = (struct arg_token){ NONE_TOK_TYPE, 0 };
    }
    struct arg_token second_arg_token;
    if (',' == line[i-1])
    { while (' ' == line[i]) i++;
      // parse second parameter
      n = nextdelim("\n", &line[i]);
      second_arg_token = parse_arg(symbols, n_symbols, &line[i], n);
      i += n+1;
    }
    else
    { second_arg_token = (struct arg_token){ NONE_TOK_TYPE, 0 };
    }

    void append(enum op op, uint16_t p1, uint16_t p2)
    { program->instructions[program->length++] =
        (struct instruction){ op, p1, p2 };
      if (max_instructions == program->length) panic;
    }

    // pattern match
    switch (isn_token)
    { case ADC_TOK:
      switch (first_arg_token.type)
      { case R8_TOK_TYPE:
        switch (second_arg_token.type)
        { case NONE_TOK_TYPE:
            append(OP_ADC_A_R8, first_arg_token.value, 0);
            break;
          case R8_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_ADC_A_R8, second_arg_token.value, 0);
            break;
          case IHL_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_ADC_A_IHL, 0, 0);
            break;
          case U3_TOK_TYPE:
          case E8_TOK_TYPE:
          case N8_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_ADC_A_N8, second_arg_token.value, 0);
            break;
          default:
            panic;
        } break;
        case IHL_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_ADC_A_IHL, 0, 0);
          break;
        case U3_TOK_TYPE:
        case E8_TOK_TYPE:
        case N8_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_ADC_A_N8, first_arg_token.value, 0);
          break;
        default:
          panic;
      } break;
      case ADD_TOK:
      switch (first_arg_token.type)
      { case R8_TOK_TYPE:
        switch (second_arg_token.type)
        { case NONE_TOK_TYPE:
            append(OP_ADD_A_R8, first_arg_token.value, 0);
            break;
          case R8_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_ADD_A_R8, second_arg_token.value, 0);
            break;
          case IHL_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_ADD_A_IHL, 0, 0);
            break;
          case U3_TOK_TYPE:
          case E8_TOK_TYPE:
          case N8_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_ADD_A_N8, second_arg_token.value, 0);
            break;
          default:
            panic;
        } break;
        case IHL_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_ADD_A_IHL, 0, 0);
          break;
        case U3_TOK_TYPE:
        case E8_TOK_TYPE:
        case N8_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_ADD_A_N8, first_arg_token.value, 0);
          break;
        case R16_TOK_TYPE:
        switch (second_arg_token.type)
        { case R16_TOK_TYPE:
            if (R16_HL != first_arg_token.value) panic;
            append(OP_ADD_HL_R16, second_arg_token.value, 0);
            break;
          case SP_TOK_TYPE:
            if (R16_HL != first_arg_token.value) panic;
            append(OP_ADD_HL_SP, second_arg_token.value, 0);
            break;
          default:
            panic;
        } break;
        case SP_TOK_TYPE:
        switch (second_arg_token.type)
        { case U3_TOK_TYPE:
          case E8_TOK_TYPE:
            append(OP_ADD_SP_E8, second_arg_token.value, 0);
            break;
          default:
            panic;
        } break;
        default:
          panic;
      } break;
      case AND_TOK:
      switch (first_arg_token.type)
      { case R8_TOK_TYPE:
        switch (second_arg_token.type)
        { case NONE_TOK_TYPE:
            append(OP_AND_A_R8, first_arg_token.value, 0);
            break;
          case R8_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_AND_A_R8, second_arg_token.value, 0);
            break;
          case IHL_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_AND_A_IHL, 0, 0);
            break;
          case U3_TOK_TYPE:
          case E8_TOK_TYPE:
          case N8_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_AND_A_N8, second_arg_token.value, 0);
            break;
          default:
            panic;
        } break;
        case IHL_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_AND_A_IHL, 0, 0);
          break;
        case U3_TOK_TYPE:
        case E8_TOK_TYPE:
        case N8_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_AND_A_N8, first_arg_token.value, 0);
          break;
        default:
          panic;
      } break;
      case CP_TOK:
      switch (first_arg_token.type)
      { case R8_TOK_TYPE:
        switch (second_arg_token.type)
        { case NONE_TOK_TYPE:
            append(OP_CP_A_R8, first_arg_token.value, 0);
            break;
          case R8_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_CP_A_R8, second_arg_token.value, 0);
            break;
          case IHL_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_CP_A_IHL, 0, 0);
            break;
          case U3_TOK_TYPE:
          case E8_TOK_TYPE:
          case N8_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_CP_A_N8, second_arg_token.value, 0);
            break;
          default:
            panic;
        } break;
        case IHL_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_CP_A_IHL, 0, 0);
          break;
        case U3_TOK_TYPE:
        case E8_TOK_TYPE:
        case N8_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_CP_A_N8, first_arg_token.value, 0);
          break;
        default:
          panic;
      } break;
      case DEC_TOK:
      switch (first_arg_token.type)
      { case R8_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_DEC_R8, first_arg_token.value, 0);
          break;
        case IHL_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_DEC_IHL, 0, 0);
          break;
        case R16_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_DEC_R16, first_arg_token.value, 0);
          break;
        case SP_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_DEC_SP, 0, 0);
          break;
        default:
          panic;
      } break;
      case INC_TOK:
      switch (first_arg_token.type)
      { case R8_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_INC_R8, first_arg_token.value, 0);
          break;
        case IHL_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_INC_IHL, 0, 0);
          break;
        case R16_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_INC_R16, first_arg_token.value, 0);
          break;
        case SP_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_INC_SP, 0, 0);
        default:
          panic;
      } break;
      case OR_TOK:
      switch (first_arg_token.type)
      { case R8_TOK_TYPE:
        switch (second_arg_token.type)
        { case NONE_TOK_TYPE:
            append(OP_OR_A_R8, first_arg_token.value, 0);
            break;
          case R8_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_OR_A_R8, second_arg_token.value, 0);
            break;
          case IHL_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_OR_A_IHL, 0, 0);
            break;
          case U3_TOK_TYPE:
          case E8_TOK_TYPE:
          case N8_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_OR_A_N8, second_arg_token.value, 0);
            break;
          default:
            panic;
        } break;
        case IHL_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_OR_A_IHL, 0, 0);
          break;
        case U3_TOK_TYPE:
        case E8_TOK_TYPE:
        case N8_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_OR_A_N8, first_arg_token.value, 0);
          break;
        default:
          panic;
      } break;
      case SBC_TOK:
      switch (first_arg_token.type)
      { case R8_TOK_TYPE:
        switch (second_arg_token.type)
        { case NONE_TOK_TYPE:
            append(OP_SBC_A_R8, first_arg_token.value, 0);
            break;
          case R8_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_SBC_A_R8, second_arg_token.value, 0);
            break;
          case IHL_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_SBC_A_IHL, 0, 0);
            break;
          case U3_TOK_TYPE:
          case E8_TOK_TYPE:
          case N8_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_SBC_A_N8, second_arg_token.value, 0);
            break;
          default:
            panic;
        } break;
        case IHL_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_SBC_A_IHL, 0, 0);
          break;
        case U3_TOK_TYPE:
        case E8_TOK_TYPE:
        case N8_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_SBC_A_N8, first_arg_token.value, 0);
          break;
        default:
          panic;
      } break;
      case SUB_TOK:
      switch (first_arg_token.type)
      { case R8_TOK_TYPE:
        switch (second_arg_token.type)
        { case NONE_TOK_TYPE:
            append(OP_SUB_A_R8, first_arg_token.value, 0);
            break;
          case R8_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_SUB_A_R8, second_arg_token.value, 0);
            break;
          case IHL_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_SUB_A_IHL, 0, 0);
            break;
          case U3_TOK_TYPE:
          case E8_TOK_TYPE:
          case N8_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_SUB_A_N8, second_arg_token.value, 0);
            break;
          default:
            panic;
        } break;
        case IHL_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_SUB_A_IHL, 0, 0);
          break;
        case U3_TOK_TYPE:
        case E8_TOK_TYPE:
        case N8_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_SUB_A_N8, first_arg_token.value, 0);
          break;
        default:
          panic;
      } break;
      case XOR_TOK:
      switch (first_arg_token.type)
      { case R8_TOK_TYPE:
        switch (second_arg_token.type)
        { case NONE_TOK_TYPE:
            append(OP_XOR_A_R8, first_arg_token.value, 0);
            break;
          case R8_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_XOR_A_R8, second_arg_token.value, 0);
            break;
          case IHL_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_XOR_A_IHL, 0, 0);
            break;
          case U3_TOK_TYPE:
          case E8_TOK_TYPE:
          case N8_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_XOR_A_N8, second_arg_token.value, 0);
            break;
          default:
            panic;
        } break;
        case IHL_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_XOR_A_IHL, 0, 0);
          break;
        case U3_TOK_TYPE:
        case E8_TOK_TYPE:
        case N8_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_XOR_A_N8, first_arg_token.value, 0);
          break;
        default:
          panic;
      } break;

      case BIT_TOK:
        if (U3_TOK_TYPE != first_arg_token.type) panic;
        switch (second_arg_token.type)
        { case R8_TOK_TYPE:
            append(OP_BIT_U3_R8, first_arg_token.value, second_arg_token.value);
            break;
          case IHL_TOK_TYPE:
            append(OP_BIT_U3_IHL, first_arg_token.value, 0);
            break;
          default:
            panic;
        }
        break;
      case RES_TOK:
        if (U3_TOK_TYPE != first_arg_token.type) panic;
        switch (second_arg_token.type)
        { case R8_TOK_TYPE:
            append(OP_RES_U3_R8, first_arg_token.value, second_arg_token.value);
            break;
          case IHL_TOK_TYPE:
            append(OP_RES_U3_IHL, first_arg_token.value, 0);
            break;
          default:
            panic;
        }
        break;
      case SET_TOK:
        if (U3_TOK_TYPE != first_arg_token.type) panic;
        switch (second_arg_token.type)
        { case R8_TOK_TYPE:
            append(OP_SET_U3_R8, first_arg_token.value, second_arg_token.value);
            break;
          case IHL_TOK_TYPE:
            append(OP_SET_U3_IHL, first_arg_token.value, 0);
            break;
          default:
            panic;
        }
        break;
      case SWAP_TOK:
        if (NONE_TOK_TYPE != second_arg_token.type) panic;
        switch (first_arg_token.type)
        { case R8_TOK_TYPE:
            append(OP_SWAP_R8, first_arg_token.value, 0);
            break;
          case IHL_TOK_TYPE:
            append(OP_SWAP_IHL, 0, 0);
            break;
          default:
            panic;
        }
        break;

      case RL_TOK:
        if (NONE_TOK_TYPE != second_arg_token.type) panic;
        switch (first_arg_token.type)
        { case R8_TOK_TYPE:
            append(OP_RL_R8, first_arg_token.value, 0);
            break;
          case IHL_TOK_TYPE:
            append(OP_RL_IHL, 0, 0);
            break;
          default:
            panic;
        }
        break;
      case RLA_TOK:
        if (NONE_TOK_TYPE != first_arg_token.type) panic;
        if (NONE_TOK_TYPE != second_arg_token.type) panic;
        append(OP_RLA, 0, 0);
        break;
      case RLC_TOK:
        if (NONE_TOK_TYPE != second_arg_token.type) panic;
        switch (first_arg_token.type)
        { case R8_TOK_TYPE:
            append(OP_RLC_R8, first_arg_token.value, 0);
            break;
          case IHL_TOK_TYPE:
            append(OP_RLC_IHL, 0, 0);
            break;
          default:
            panic;
        }
        break;
      case RLCA_TOK:
        if (NONE_TOK_TYPE != first_arg_token.type) panic;
        if (NONE_TOK_TYPE != second_arg_token.type) panic;
        append(OP_RLCA, 0, 0);
        break;
      case RR_TOK:
        if (NONE_TOK_TYPE != second_arg_token.type) panic;
        switch (first_arg_token.type)
        { case R8_TOK_TYPE:
            append(OP_RR_R8, first_arg_token.value, 0);
            break;
          case IHL_TOK_TYPE:
            append(OP_RR_IHL, 0, 0);
            break;
          default:
            panic;
        }
        break;
      case RRA_TOK:
        if (NONE_TOK_TYPE != first_arg_token.type) panic;
        if (NONE_TOK_TYPE != second_arg_token.type) panic;
        append(OP_RRA, 0, 0);
        break;
      case RRC_TOK:
        if (NONE_TOK_TYPE != second_arg_token.type) panic;
        switch (first_arg_token.type)
        { case R8_TOK_TYPE:
            append(OP_RRC_R8, first_arg_token.value, 0);
            break;
          case IHL_TOK_TYPE:
            append(OP_RRC_IHL, 0, 0);
            break;
          default:
            panic;
        }
        break;
      case RRCA_TOK:
        if (NONE_TOK_TYPE != first_arg_token.type) panic;
        if (NONE_TOK_TYPE != second_arg_token.type) panic;
        append(OP_RRCA, 0, 0);
        break;
      case SLA_TOK:
        if (NONE_TOK_TYPE != second_arg_token.type) panic;
        switch (first_arg_token.type)
        { case R8_TOK_TYPE:
            append(OP_SLA_R8, first_arg_token.value, 0);
            break;
          case IHL_TOK_TYPE:
            append(OP_SLA_IHL, 0, 0);
            break;
          default:
            panic;
        }
        break;
      case SRA_TOK:
        if (NONE_TOK_TYPE != second_arg_token.type) panic;
        switch (first_arg_token.type)
        { case R8_TOK_TYPE:
            append(OP_SRA_R8, first_arg_token.value, 0);
            break;
          case IHL_TOK_TYPE:
            append(OP_SRA_IHL, 0, 0);
            break;
          default:
            panic;
        }
        break;
      case SRL_TOK:
        if (NONE_TOK_TYPE != second_arg_token.type) panic;
        switch (first_arg_token.type)
        { case R8_TOK_TYPE:
            append(OP_SRL_R8, first_arg_token.value, 0);
            break;
          case IHL_TOK_TYPE:
            append(OP_SRL_IHL, 0, 0);
            break;
          default:
            panic;
        }
        break;

      case LD_TOK:
      switch (first_arg_token.type)
      { case R8_TOK_TYPE:
        switch (second_arg_token.type)
        { case R8_TOK_TYPE:
            append(OP_LD_R8_R8, first_arg_token.value, second_arg_token.value);
            break;
          case U3_TOK_TYPE:
          case E8_TOK_TYPE:
          case N8_TOK_TYPE:
            append(OP_LD_R8_N8, first_arg_token.value, second_arg_token.value);
            break;
          case IHL_TOK_TYPE:
            append(OP_LD_R8_IHL, first_arg_token.value, second_arg_token.value);
            break;
          case IR16_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_LD_A_IR16, second_arg_token.value, 0);
            break;
          case IN16_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_LD_A_IN16, second_arg_token.value, 0);
            break;
          case IHLI_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_LD_A_IHLI, 0, 0);
            break;
          case IHLD_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_LD_A_IHLD, 0, 0);
            break;
          default:
            panic;
        } break;
        case R16_TOK_TYPE:
        switch (second_arg_token.type)
        { case U3_TOK_TYPE:
          case E8_TOK_TYPE:
          case N8_TOK_TYPE:
          case N16_TOK_TYPE:
            append(OP_LD_R16_N16, first_arg_token.value, second_arg_token.value);
            break;
          case SPE8_TOK_TYPE:
            if (R16_HL != first_arg_token.value) panic;
            append(OP_LD_HL_SPE8, second_arg_token.value, 0);
            break;
          default:
            panic;
        } break;
        case IHL_TOK_TYPE:
        switch (second_arg_token.type)
        { case R8_TOK_TYPE:
            append(OP_LD_IHL_R8, second_arg_token.value, 0);
            break;
          case U3_TOK_TYPE:
          case E8_TOK_TYPE:
          case N8_TOK_TYPE:
            append(OP_LD_IHL_N8, second_arg_token.value, 0);
            break;
        } break;
        case IR16_TOK_TYPE:
          if (R8_TOK_TYPE != second_arg_token.type) panic;
          if (R8_A != second_arg_token.value) panic;
          append(OP_LD_IR16_A, first_arg_token.value, 0);
          break;
        case IN16_TOK_TYPE:
        switch (second_arg_token.type)
        { case R8_TOK_TYPE:
            if (R8_A != second_arg_token.value) panic;
            append(OP_LD_IN16_A, first_arg_token.value, 0);
            break;
          case SP_TOK_TYPE:
            append(OP_LD_IN16_SP, first_arg_token.value, 0);
            break;
          default:
            panic;
        } break;
        case IHLI_TOK_TYPE:
          if (R8_TOK_TYPE != second_arg_token.type) panic;
          if (R8_A != second_arg_token.value) panic;
          append(OP_LD_IHLI_A, 0, 0);
          break;
        case IHLD_TOK_TYPE:
          if (R8_TOK_TYPE != second_arg_token.type) panic;
          if (R8_A != second_arg_token.value) panic;
          append(OP_LD_IHLD_A, 0, 0);
          break;
        case SP_TOK_TYPE:
        switch (second_arg_token.type)
        { case U3_TOK_TYPE:
          case E8_TOK_TYPE:
          case N8_TOK_TYPE:
          case N16_TOK_TYPE:
            append(OP_LD_SP_N16, second_arg_token.value, 0);
            break;
          case R16_TOK_TYPE:
            if (R16_HL != second_arg_token.value) panic;
            append(OP_LD_SP_HL, 0, 0);
            break;
          default:
            panic;
        } break;
        default:
          panic;
      } break;
      case LDH_TOK:
      switch (first_arg_token.type)
      { case IN16_TOK_TYPE:
          if (R8_TOK_TYPE != second_arg_token.type) panic;
          if (R8_A != second_arg_token.value) panic;
          append(OP_LDH_IN16_A, first_arg_token.value, 0);
          break;
        case IC_TOK_TYPE:
          if (R8_TOK_TYPE != second_arg_token.type) panic;
          if (R8_A != second_arg_token.value) panic;
          append(OP_LDH_IC_A, 0, 0);
          break;
        case R8_TOK_TYPE:
        switch (second_arg_token.type)
        { case IN16_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_LDH_A_IN16, second_arg_token.value, 0);
            break;
          case IC_TOK_TYPE:
            if (R8_A != first_arg_token.value) panic;
            append(OP_LDH_A_IC, 0, 0);
            break;
          default:
            panic;
        } break;
        default:
          panic;
      } break;

      case CALL_TOK:
        panic;
      case JP_TOK:
        panic;
      case JR_TOK:
      switch (first_arg_token.type)
      { case U3_TOK_TYPE:
        case E8_TOK_TYPE:
          panic;
        case ANON_LABEL_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          panic;
        case CC_TOK_TYPE:
        switch (second_arg_token.type)
        { case U3_TOK_TYPE:
          case E8_TOK_TYPE:
            panic;
          case ANON_LABEL_TOK_TYPE:
          { int j = n_anonymous_labels + second_arg_token.value;
            if (0 > j || n_anonymous_labels <= j) panic;
            int x = anonymous_labels[j] - program->length - 1;
            append(OP_JR_CC_E8, first_arg_token.value, x);
            break;
          }
          default:
            panic;
        } break;
        default:
          panic;
      } break;
      case RET_TOK:
        panic;
      case RETI_TOK:
        panic;
      case RST_TOK:
        panic;

      case POP_TOK:
      switch (first_arg_token.type)
      { case AF_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_POP_AF, 0, 0);
          panic;
        case R16_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_POP_R16, first_arg_token.value, 0);
          panic;
        default:
          panic;
      } break;
      case PUSH_TOK:
      switch (first_arg_token.type)
      { case AF_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_PUSH_AF, 0, 0);
          panic;
        case R16_TOK_TYPE:
          if (NONE_TOK_TYPE != second_arg_token.type) panic;
          append(OP_PUSH_R16, first_arg_token.value, 0);
          panic;
        default:
          panic;
      } break;

      case CCF_TOK:
        if (NONE_TOK_TYPE != first_arg_token.type) panic; 
        if (NONE_TOK_TYPE != second_arg_token.type) panic; 
        append(OP_CCF, 0, 0);
        break;
      case CPL_TOK:
        if (NONE_TOK_TYPE != first_arg_token.type) panic; 
        if (NONE_TOK_TYPE != second_arg_token.type) panic; 
        append(OP_CPL, 0, 0);
        break;
      case DAA_TOK:
        if (NONE_TOK_TYPE != first_arg_token.type) panic; 
        if (NONE_TOK_TYPE != second_arg_token.type) panic; 
        append(OP_DAA, 0, 0);
        break;
      case DI_TOK:
        if (NONE_TOK_TYPE != first_arg_token.type) panic; 
        if (NONE_TOK_TYPE != second_arg_token.type) panic; 
        append(OP_DI, 0, 0);
        break;
      case EI_TOK:
        if (NONE_TOK_TYPE != first_arg_token.type) panic; 
        if (NONE_TOK_TYPE != second_arg_token.type) panic; 
        append(OP_EI, 0, 0);
        break;
      case HALT_TOK:
        if (NONE_TOK_TYPE != first_arg_token.type) panic; 
        if (NONE_TOK_TYPE != second_arg_token.type) panic; 
        append(OP_HALT, 0, 0);
        break;
      case NOP_TOK:
        if (NONE_TOK_TYPE != first_arg_token.type) panic; 
        if (NONE_TOK_TYPE != second_arg_token.type) panic; 
        append(OP_NOP, 0, 0);
        break;
      case SCF_TOK:
        if (NONE_TOK_TYPE != first_arg_token.type) panic; 
        if (NONE_TOK_TYPE != second_arg_token.type) panic; 
        append(OP_SCF, 0, 0);
        break;
      case STOP_TOK:
        if (NONE_TOK_TYPE != first_arg_token.type) panic; 
        if (NONE_TOK_TYPE != second_arg_token.type) panic; 
        append(OP_STOP, 0, 0);
        break;
      default: panic;
    }
    line += i;
  }

  return program;
}


struct program *parse_program_file
( struct symbol *symbols, size_t n_symbols
, char *filename
)
{ char code[1 << 16];
  int f = open(filename, O_RDONLY);
  if (!f) panic;
  size_t n = read(f, code, sizeof(code));
  if (sizeof(code) == n) panic;
  code[n] = 0;
  close(f);
  return parse_program(symbols, n_symbols, code);
}
