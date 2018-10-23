/* @{{
 *   `inst_exec_end'
 *   `cpu.pc' for jmp instruction
 *   `instr'  for special table
 * @}}
 */

// checked delayslot: jal, jalr, 


#define make_entry()

#define make_exec_handler(name) name: make_exec_wrapper
#define make_exec_wrapper(...) __VA_ARGS__; goto eoe;
#define make_eoe() eoe:;

static const void *branch_table[8] = {
  /* 0x00 */    &&beq, &&bne, &&inv, &&inv,
  /* 0x04 */    &&blt, &&bge, &&bltu, &&bgeu,
};

static const void *load_table[8] = {
  /* 0x00 */    &&lb, &&lh, &&lw, &&inv,
  /* 0x04 */    &&lbu, &&lhu, &&inv, &&inv,
};

static const void *store_table[8] = {
  /* 0x00 */    &&sb, &&sh, &&sw, &&inv,
  /* 0x04 */    &&inv, &&inv, &&inv, &&inv,
};

static const void *alui_table_0[8] = {
  /* 0x00 */    &&addi, &&slli, &&slti, &&sltiu,
  /* 0x04 */    &&xori, &&srli, &&ori, &&andi,
};

static const void *alui_table_32[8] = {
  /* 0x00 */    &&inv, &&inv, &&inv, &&inv,
  /* 0x04 */    &&inv, &&srai, &&inv, &&inv,
};

static const void *alu_table_0[8] = {
  /* 0x00 */    &&add, &&sll, &&slt, &&sltu,
  /* 0x04 */    &&xor_, &&srl, &&or_, &&and_,
};

static const void *alu_table_1[8] = {
  /* 0x00 */    &&mul, &&mulh, &&mulhsu, &&mulhu,
  /* 0x04 */    &&div, &&divu, &&rem, &&remu,
};

static const void *alu_table_32[8] = {
  /* 0x00 */    &&sub, &&inv, &&inv, &&inv,
  /* 0x04 */    &&inv, &&sra, &&inv, &&inv,
};

static const void *csr_table[8] = {
  /* 0x00 */    &&ecall_ebreak, &&csrrw, &&csrrs, &&csrrc,
  /* 0x04 */    &&inv, &&csrrwi, &&csrrsi, &&csrrci,
};

static const void * opcode_table[32] = {
  /* 0x00 */    &&exec_load, &&inv, &&inv, &&fence,
  /* 0x04 */	&&exec_alui, &&auipc, &&inv, &&inv,
  /* 0x08 */	&&exec_store, &&inv, &&inv, &&inv,
  /* 0x0c */	&&exec_alu, &&lui, &&inv, &&inv,
  /* 0x10 */	&&inv, &&inv, &&inv, &&inv,
  /* 0x14 */	&&inv, &&inv, &&inv, &&inv,
  /* 0x18 */	&&exec_branch, &&inv, &&inv, &&jalr,
  /* 0x1c */	&&exec_csr, &&jal, &&inv, &&inv,
};

make_entry() {
  assert(inst.op & 0x3 == 0x3);
  goto *opcode_table[inst.op >> 2];
}

make_exec_handler(exec_branch) ({
  goto *branch_table[inst.func3];
});

make_exec_handler(exec_load) ({
  goto *load_table[inst.func3];
});

make_exec_handler(exec_store) ({
  goto *store_table[inst.func3];
});

make_exec_handler(exec_alui) ({
  switch(inst.func7) {
    case 0: goto *alui_table_0[inst.func3];
    case 1: goto *alui_table_1[inst.func3];
	default: goto inv;
  }
});

make_exec_handler(exec_alu) ({
  switch(inst.func7) {
    case 0: goto *alu_table_0[inst.func3];
    case 1: goto *alu_table_1[inst.func3];
    case 32: goto *alu_table_32[inst.func3];
	default: goto inv;
  }
});

make_exec_handler(exec_csr) ({
  goto *csr_table[inst.func3];
});

make_exec_handler(inv) ({
  // the pc corresponding to this inst
  // pc has been updated by instr_fetch
#ifdef ENABLE_EXCEPTION
  signal_exception(EXC_RI);
#else
  uint8_t *p = (uint8_t *)&inst;
  printf("invalid opcode(pc = 0x%08x): %02x %02x %02x %02x ...\n",
	  cpu.pc, p[0], p[1], p[2], p[3]);
  nemu_state = NEMU_END;
#endif
});


make_exec_handler(syscall) ({
  signal_exception(EXC_SYSCALL);
});

make_exec_handler(breakpoint) ({
  if(work_mode == MODE_GDB) {
    nemu_state = NEMU_STOP;
  } else {
    signal_exception(EXC_BP);
  }
});

make_exec_handler(jr) ({
  cpu.pc = cpu.gpr[inst.rs];
});

make_exec_handler(add) ({
  cpu.gpr[inst.rd] = cpu.gpr[inst.rs1] + cpu.gpr[inst.rs2];
});

make_exec_handler(sub) ({
  cpu.gpr[inst.rd] = cpu.gpr[inst.rs1] - cpu.gpr[inst.rs2];
});

make_exec_handler(slt) ({
  cpu.gpr[inst.rd] = (int32_t)cpu.gpr[inst.rs1] < (int32_t)cpu.gpr[inst.rs2];
});

make_exec_handler(sltu) ({
  cpu.gpr[inst.rd] = cpu.gpr[inst.rs1] < cpu.gpr[inst.rs2];
});

make_exec_handler(sll) ({
  cpu.gpr[inst.rd] = cpu.gpr[inst.rs1] << (cpu.gpr[inst.rs2] & 0x1f);
});

make_exec_handler(srl) ({
  cpu.gpr[inst.rd] = cpu.gpr[inst.rs1] >> (cpu.gpr[inst.rs2] & 0x1f);
});

make_exec_handler(sra) ({
  cpu.gpr[inst.rd] = (int32_t)cpu.gpr[inst.rs1] >> (cpu.gpr[inst.rs2] & 0x1f);
});

make_exec_handler(sub) ({
  union { struct { uint32_t lo, hi; }; int64_t val; } ret;
  ret.val = (int64_t)(int32_t)cpu.gpr[inst.rs] -
			(int64_t)(int32_t)cpu.gpr[inst.rt];
  if((ret.hi & 0x1) != ((ret.lo >> 31) & 1)) {
#ifdef ENABLE_EXCEPTION
    signal_exception(EXC_OV);
#else
	CPUAssert(0, "sub overflow, %08x - %08x\n", cpu.gpr[inst.rs],
		cpu.gpr[inst.rt]);
#endif
  } else {
	cpu.gpr[inst.rd] = ret.lo;
  }
	  regs[inst.rs], regs[inst.rt]);
});

make_exec_handler(nor) ({
  cpu.gpr[inst.rd] = ~(cpu.gpr[inst.rs] | cpu.gpr[inst.rt]);
	  regs[inst.rs], regs[inst.rt]);
});

#undef R_SIMPLE

make_exec_handler(clz) ({
  if(cpu.gpr[inst.rs] == 0) {
	cpu.gpr[inst.rd] = 32;
  } else {
	cpu.gpr[inst.rd] = __builtin_clz(cpu.gpr[inst.rs]);
  }
});

make_exec_handler(mult) ({
  int64_t prod = (int64_t)(int32_t)cpu.gpr[inst.rs] * (int64_t)(int32_t)cpu.gpr[inst.rt];
  cpu.lo = (uint32_t)prod;
  cpu.hi = (uint32_t)(prod >> 32);
});

make_exec_handler(multu) ({
  uint64_t prod = (uint64_t)cpu.gpr[inst.rs] * (uint64_t)cpu.gpr[inst.rt];
  cpu.lo = (uint32_t)prod;
  cpu.hi = (uint32_t)(prod >> 32);
});

make_exec_handler(divide) ({
  cpu.lo = (int32_t)cpu.gpr[inst.rs] / (int32_t)cpu.gpr[inst.rt];
  cpu.hi = (int32_t)cpu.gpr[inst.rs] % (int32_t)cpu.gpr[inst.rt];
});

make_exec_handler(divu) ({
  cpu.lo = cpu.gpr[inst.rs] / cpu.gpr[inst.rt];
  cpu.hi = cpu.gpr[inst.rs] % cpu.gpr[inst.rt];
});


make_exec_handler(sll) ({
  cpu.gpr[inst.rd] = cpu.gpr[inst.rt] << inst.shamt;
});

make_exec_handler(sllv) ({
  cpu.gpr[inst.rd] = cpu.gpr[inst.rt] << (cpu.gpr[inst.rs] & 0x1f);
});

make_exec_handler(sra) ({
  cpu.gpr[inst.rd] = (int32_t)cpu.gpr[inst.rt] >> inst.shamt;
});

make_exec_handler(srav) ({
  cpu.gpr[inst.rd] = (int32_t)cpu.gpr[inst.rt] >> (cpu.gpr[inst.rs] & 0x1f);
});

make_exec_handler(srl) ({
  cpu.gpr[inst.rd] = cpu.gpr[inst.rt] >> inst.shamt;
});

make_exec_handler(srlv) ({
  cpu.gpr[inst.rd] = cpu.gpr[inst.rt] >> (cpu.gpr[inst.rs] & 0x1f);
});

make_exec_handler(movn) ({
  if (cpu.gpr[inst.rt] != 0)
	cpu.gpr[inst.rd] = cpu.gpr[inst.rs];
});

make_exec_handler(movz) ({
  if (cpu.gpr[inst.rt] == 0)
	cpu.gpr[inst.rd] = cpu.gpr[inst.rs];
});

make_exec_handler(mfhi) ({
  cpu.gpr[inst.rd] = cpu.hi;
});

make_exec_handler(mthi) ({
  cpu.hi = cpu.gpr[inst.rs];
});

make_exec_handler(mflo) ({
  cpu.gpr[inst.rd] = cpu.lo;
});

make_exec_handler(mtlo) ({
  cpu.lo = cpu.gpr[inst.rs];
});

make_exec_handler(jalr) ({
  cpu.gpr[inst.rd] = cpu.pc + 4;
  cpu.pc = cpu.gpr[inst.rs];
});


make_exec_handler(lui) ({
  cpu.gpr[inst.rd] = inst.U.imm_31_12 << 12;
});

make_exec_handler(auipc) ({
  cpu.gpr[inst.rd] = cpu.pc + (inst.U.imm_31_12 << 12);
});

make_exec_handler(addi) ({
  cpu.gpr[inst.rd] = cpu.gpr[inst.rs1] + inst.I.simm_11_0;
});

make_exec_handler(slli) ({
  cpu.gpr[inst.rd] = cpu.gpr[inst.rs1] << inst.I.shamt;
});

make_exec_handler(srli) ({
  cpu.gpr[inst.rd] = cpu.gpr[inst.rs1] >> inst.I.shamt;
});

make_exec_handler(srai) ({
  cpu.gpr[inst.rd] = (int32_t)cpu.gpr[inst.rs1] >> inst.I.shamt;
});

make_exec_handler(andi) ({
  cpu.gpr[inst.rd] = cpu.gpr[inst.rs1] & inst.I.simm_11_0;
});

make_exec_handler(ori) ({
  cpu.gpr[inst.rd] = cpu.gpr[inst.rs1] | inst.I.simm_11_0;
});

make_exec_handler(xori) ({
  cpu.gpr[inst.rd] = cpu.gpr[inst.rs1] ^ inst.I.simm_11_0;
});

make_exec_handler(sltiu) ({
  cpu.gpr[inst.rd] = cpu.gpr[inst.rs1] < inst.I.imm_11_0;
});

make_exec_handler(slti) ({
  cpu.gpr[inst.rd] = (int32_t)cpu.gpr[inst.rs1] < inst.I.simm_11_0;
});


#ifdef ENABLE_EXCEPTION

#define CHECK_ALIGNED_ADDR_AdEL(align, addr) \
  if(((addr) & (align - 1)) != 0) { \
	cpu.cp0.badvaddr = addr; \
	signal_exception(EXC_AdEL); \
	goto eoe; \
  }

#define CHECK_ALIGNED_ADDR_AdES(align, addr) \
  if(((addr) & (align - 1)) != 0) { \
	cpu.cp0.badvaddr = addr; \
	signal_exception(EXC_AdES); \
	goto eoe; \
  }

#else

#define CHECK_ALIGNED_ADDR(align, addr) \
  CPUAssert(((addr) & (align - 1)) == 0, "address(0x%08x) is unaligned, pc=%08x\n", (addr), cpu.pc)

#define CHECK_ALIGNED_ADDR_AdEL CHECK_ALIGNED_ADDR
#define CHECK_ALIGNED_ADDR_AdES CHECK_ALIGNED_ADDR

#endif

make_exec_handler(swl) ({
  uint32_t waddr = cpu.gpr[inst.rs] + inst.simm;
  int idx = waddr & 0x3;
  int len = idx + 1;
  uint32_t wdata = cpu.gpr[inst.rt] >> ((3 - idx) * 8);

  store_mem((waddr >> 2) << 2, len, wdata);
});

make_exec_handler(swr) ({
  uint32_t waddr = cpu.gpr[inst.rs] + inst.simm;
  int len = 4 - (waddr & 0x3);
  uint32_t wdata = cpu.gpr[inst.rt];

  store_mem(waddr, len, wdata);
});

/* S-type */
#define STORE_SIMM ( \
  (inst.S.imm_4_0  << 0)  | \
  (inst.S.imm_11_5 << 5)  | \
  (inst.S.simm_11 << 12) )

make_exec_handler(sw) ({
  CHECK_ALIGNED_ADDR_AdES(4, cpu.gpr[inst.rs1] + STORE_SIMM);
  store_mem(cpu.gpr[inst.rs1] + STORE_SIMM, 4, cpu.gpr[inst.rs2]);
});

make_exec_handler(sh) ({
  CHECK_ALIGNED_ADDR_AdES(2, cpu.gpr[inst.rs1] + STORE_SIMM);
  store_mem(cpu.gpr[inst.rs1] + STORE_SIMM, 2, cpu.gpr[inst.rs2]);
});

make_exec_handler(sb) ({
  CHECK_ALIGNED_ADDR_AdES(1, cpu.gpr[inst.rs1] + STORE_SIMM);
  store_mem(cpu.gpr[inst.rs1] + STORE_SIMM, 1, cpu.gpr[inst.rs2]);
});

#undef STORE_SIMM

make_exec_handler(lw) ({
  CHECK_ALIGNED_ADDR_AdEL(4, cpu.gpr[inst.rs1] + inst.I.simm_11_0);
  cpu.gpr[inst.rd] = load_mem(cpu.gpr[inst.rs1] + inst.I.simm_11_0, 4);
});

make_exec_handler(lb) ({
  CHECK_ALIGNED_ADDR_AdEL(1, cpu.gpr[inst.rs1] + inst.I.simm_11_0);
  cpu.gpr[inst.rd] = (int32_t)(int8_t)load_mem(cpu.gpr[inst.rs1] + inst.I.simm_11_0, 1);
});

make_exec_handler(lbu) ({
  CHECK_ALIGNED_ADDR_AdEL(1, cpu.gpr[inst.rs1] + inst.I.simm_11_0);
  cpu.gpr[inst.rd] = load_mem(cpu.gpr[inst.rs1] + inst.I.simm_11_0, 1);
});

make_exec_handler(lh) ({
  CHECK_ALIGNED_ADDR_AdEL(2, cpu.gpr[inst.rs1] + inst.I.simm_11_0);
  cpu.gpr[inst.rd] = (int32_t)(int16_t)load_mem(cpu.gpr[inst.rs1] + inst.I.simm_11_0, 2);
});

make_exec_handler(lhu) ({
  CHECK_ALIGNED_ADDR_AdEL(2, cpu.gpr[inst.rs1] + inst.I.simm_11_0);
  cpu.gpr[inst.rd] = load_mem(cpu.gpr[inst.rs1] + inst.I.simm_11_0, 2);
});

/* B-type */
#define BRANCH_SIMM ( \
  (inst.B.imm_4_1  << 1)  | \
  (inst.B.imm_10_5 << 5)  | \
  (inst.B.imm_11   << 11) | \
  (inst.B.simm_12  << 12) )

make_exec_handler(beq) ({
  if (cpu.gpr[inst.rs1] == cpu.gpr[inst.rs2])
	cpu.pc += BRANCH_SIMM << 1;
});

make_exec_handler(bne) ({
  if (cpu.gpr[inst.rs1] == cpu.gpr[inst.rs2])
	cpu.pc += BRANCH_SIMM << 1;
});

make_exec_handler(blt) ({
  if ((int32_t)cpu.gpr[inst.rs1] < (int32_t)cpu.gpr[inst.rs2])
	cpu.pc += BRANCH_SIMM << 1;
});

make_exec_handler(bge) ({
  if ((int32_t)cpu.gpr[inst.rs1] >= (int32_t)cpu.gpr[inst.rs2])
	cpu.pc += BRANCH_SIMM << 1;
});

make_exec_handler(bltu) ({
  if (cpu.gpr[inst.rs1] < cpu.gpr[inst.rs2])
	cpu.pc += BRANCH_SIMM << 1;
});

make_exec_handler(bgeu) ({
  if (cpu.gpr[inst.rs1] >= cpu.gpr[inst.rs2])
	cpu.pc += BRANCH_SIMM << 1;
});

make_exec_handler(fence) ({
  assert(0 && "fence not implemented");
});

make_exec_handler(ecall_ebreak) ({
  assert(0 && "ecall_ebreak not implemented");
});

#undef BRANCH_SIMM

make_exec_handler(jal) ({
  cpu.gpr[inst.rd] = cpu.pc + 4;
  cpu.pc += (inst.U.simm << 2);
});

make_exec_handler(j) ({
  cpu.pc = (cpu.pc & 0xf0000000) | (inst.addr << 2);
});

make_exec_handler(cheat) ({
  // the meaning of cheat is a set of API to check consistence
  switch(cpu.gpr[0]) {
  /*
    case NEMU_SAVE_GPR:
	case NEMU_SAVE_MEM:
	case NEMU_SAVE_CPR0:
	case NEMU_CHECK_GPR:
	case NEMU_CHECK_MEM:
	case NEMU_CHECK_CPR0:
	*/
	default: break;
  }
});

make_eoe() { } // end of execution
