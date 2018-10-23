#ifndef __REG_H__
#define __REG_H__

#include <stddef.h>
#include "common.h"

#define CPU_INIT_PC 0xbfc00000

typedef struct {
  uint32_t gpr[32];
  uint32_t hi, lo;
  vaddr_t pc;
} CPU_state;

typedef union {
  union {
	struct { // common
	  uint32_t op     : 7;
	  uint32_t rd     : 5;
	  uint32_t func3  : 3;
	  uint32_t rs1    : 5;
	  uint32_t rs2    : 5;
	  uint32_t func7  : 7;
	};

/* placeholder */
#define CAT_IMPL(a, b) a ## b
#define CAT(a, b) CAT_IMPL(a, b)
#define __ CAT(_, __LINE__)

	// I-type
	union {
	  struct {
		uint32_t __       : 20;
		uint32_t imm_11_0 : 12;
	  };

	  struct {
		uint32_t __        : 20;
		int32_t  simm_11_0 : 12;
	  };

	  struct {
		uint32_t __        : 20;
		uint32_t shamt     : 5;
	  };
	} I;

	// S-type
	union {
	  struct {
		uint32_t __       : 7;
		uint32_t imm_4_0  : 5;
		uint32_t __       : 13;
		uint32_t imm_11_5 : 7;
	  };

	  struct {
		uint32_t __       : 31;
		uint32_t simm_11  : 1;
	  };
	} S;

	// B-type
	union {
	  struct {
		uint32_t __       : 7;
		uint32_t imm_11   : 1;
		uint32_t imm_4_1  : 4;
		uint32_t __       : 13;
		uint32_t imm_10_5 : 6;
		uint32_t imm_12   : 1;
	  };

	  struct {
		uint32_t __     : 31;
		int32_t simm_12 : 1;
	  };
	} B;
	
	// U-type
	union {
	  struct {
		uint32_t __        : 12;
		uint32_t imm_31_12 : 20;
	  };

	  struct {
		uint32_t __         : 12;
		int32_t  simm_31_12 : 20;
	  };
	} U;

	// J-type
	union {
	  struct {
		uint32_t __        : 12;
		uint32_t imm_19_12 : 8;
		uint32_t imm_J_11  : 1;
		uint32_t imm_10_1  : 10;
		uint32_t imm_20    : 1;
	  };

	  struct {
		uint32_t __        : 31;
		int32_t  simm_20   : 1;
	  };
	} J;
  };
#undef __
  uint32_t val;
} Inst;

extern CPU_state cpu;
int init_cpu(vaddr_t entry);

#endif
