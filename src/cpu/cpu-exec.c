#include <sys/time.h>
#include <setjmp.h>
#include <assert.h>

#include "nemu.h"
#include "device.h"
#include "monitor.h"
#include "memory.h"


CPU_state cpu;

void signal_exception(int code);

const char *regs[32] = {
  "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
  "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
  "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
  "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"
};

#define UNLIKELY(cond) __builtin_expect(!!(cond), 0)
#define LIKELY(cond) __builtin_expect(!!(cond), 1)

#define MAX_INSTR_TO_PRINT 10

nemu_state_t nemu_state = NEMU_STOP;

static uint64_t nemu_start_time = 0;

char asm_buf[80], *asm_buf_p;

// 1s = 10^3 ms = 10^6 us
static uint64_t get_current_time() { // in us
  struct timeval t;
  gettimeofday(&t, NULL);
  return t.tv_sec * 1000000 + t.tv_usec - nemu_start_time;
}

int dsprintf(char *buf, const char *fmt, ...) {
  int len = 0;
#if 0
  va_list ap;
  va_start(ap, fmt);
  len = vprintf(fmt, ap);
  va_end(ap);
  printf("\n");
#endif
  return len;
}


void print_registers(uint32_t instr) {
  static unsigned int ninstr = 0;
  // print registers to stderr, so that will not mixed with uart output
  eprintf("$pc:    0x%08x    $hi:    0x%08x    $lo:    0x%08x\n", cpu.pc, cpu.hi, cpu.lo);
  eprintf("$ninstr: 0x%08x                     $instr: 0x%08x\n", ninstr, instr);
  eprintf("$0 :0x%08x  $at:0x%08x  $v0:0x%08x  $v1:0x%08x\n", cpu.gpr[0], cpu.gpr[1], cpu.gpr[2], cpu.gpr[3]);
  eprintf("$a0:0x%08x  $a1:0x%08x  $a2:0x%08x  $a3:0x%08x\n", cpu.gpr[4], cpu.gpr[5], cpu.gpr[6], cpu.gpr[7]);
  eprintf("$t0:0x%08x  $t1:0x%08x  $t2:0x%08x  $t3:0x%08x\n", cpu.gpr[8], cpu.gpr[9], cpu.gpr[10], cpu.gpr[11]);
  eprintf("$t4:0x%08x  $t5:0x%08x  $t6:0x%08x  $t7:0x%08x\n", cpu.gpr[12], cpu.gpr[13], cpu.gpr[14], cpu.gpr[15]);
  eprintf("$s0:0x%08x  $s1:0x%08x  $s2:0x%08x  $s3:0x%08x\n", cpu.gpr[16], cpu.gpr[17], cpu.gpr[18], cpu.gpr[19]);
  eprintf("$s4:0x%08x  $s5:0x%08x  $s6:0x%08x  $s7:0x%08x\n", cpu.gpr[20], cpu.gpr[21], cpu.gpr[22], cpu.gpr[23]);
  eprintf("$t8:0x%08x  $t9:0x%08x  $k0:0x%08x  $k1:0x%08x\n", cpu.gpr[24], cpu.gpr[25], cpu.gpr[26], cpu.gpr[27]);
  eprintf("$gp:0x%08x  $sp:0x%08x  $fp:0x%08x  $ra:0x%08x\n", cpu.gpr[28], cpu.gpr[29], cpu.gpr[30], cpu.gpr[31]);
  ninstr++;
}

int init_cpu(vaddr_t entry) {
  nemu_start_time = get_current_time();
  cpu.pc = entry;
  return 0;
}

static inline uint32_t read_handler(uint8_t *p, int len) {
  switch(len) {
	case 1: return p[0];
	case 2: return (p[1] << 8) | p[0];
	case 3: return (p[2] << 16) | (p[1] << 8) | p[0];
	case 4: return (p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0];
	default: CPUAssert(0, "invalid len %d\n", len); break;
  }
}

static inline void write_handler(uint8_t *p, uint32_t len, uint32_t data) {
  switch(len) {
	case 1: p[0] = data; return;
	case 2: p[0] = data & 0xFF;
			p[1] = (data >> 8) & 0xFF;
			return;
	case 3: p[0] = data & 0xFF;
			p[1] = (data >> 8) & 0xFF;
			p[2] = (data >> 16) & 0xFF;
			return;
	case 4: p[0] = data & 0xFF;
			p[1] = (data >> 8) & 0xFF;
			p[2] = (data >> 16) & 0xFF;
			p[3] = (data >> 24) & 0xFF;
			return;
	default: CPUAssert(0, "invalid len %d\n", len); break;
  }
}

static inline uint32_t load_mem(vaddr_t addr, int len) {
#ifdef DEBUG
  CPUAssert(addr >= 0x1000, "preventive check failed, try to load memory from addr %08x\n", addr);
#endif
  uint32_t pa = prot_addr(addr);
  if(LIKELY(DDR_BASE <= pa && pa < DDR_BASE + DDR_SIZE)) {
	// Assert(0, "addr:%08x, pa:%08x\n", addr, pa);
    addr = pa - DDR_BASE;
	return read_handler(&ddr[addr], len);
  } else if(BRAM_BASE <= pa && pa < BRAM_BASE + BRAM_SIZE) {
    addr = pa - BRAM_BASE;
	return read_handler(&bram[addr], len);
  }
  return vaddr_read(addr, len);
}

static inline void store_mem(vaddr_t addr, int len, uint32_t data) {
#ifdef DEBUG
  CPUAssert(addr >= 0x1000, "preventive check failed, try to store memory to addr %08x\n", addr);
#endif

  uint32_t pa = prot_addr(addr);
  if(LIKELY(DDR_BASE <= pa && pa < DDR_BASE + DDR_SIZE)) {
    addr = pa - DDR_BASE;
	write_handler(&ddr[addr], len, data);
  } else {
    vaddr_write(addr, len, data);
  }
}

static inline uint32_t instr_fetch(vaddr_t addr) {
  return load_mem(addr, 4);
}

void signal_exception(int code) {
  assert(0 && "exception is not implemented");
}


/* Simulate how the CPU works. */
void cpu_exec(uint64_t n) {
  if (nemu_state == NEMU_END) {
    printf("Program execution has ended. To restart the program, exit NEMU and run again.\n");
    return;
  }

  nemu_state = NEMU_RUNNING;

  for (; n > 0; n --) {
#ifdef DEBUG
	instr_enqueue_pc(cpu.pc);
#endif

#if 0
    asm_buf_p = asm_buf;
    asm_buf_p += dsprintf(asm_buf_p, "%8x:    ", cpu.pc);
#endif

	assert((cpu.pc & 0x3) == 0);

    Inst inst;
	inst.val = instr_fetch(cpu.pc);

#ifdef DEBUG
	instr_enqueue_instr(inst.val);
#endif

    asm_buf_p += dsprintf(asm_buf_p, "%08x    ", inst.val);

#include "exec-handlers.h"

#ifdef DEBUG
	// if(0x60000000 <= cpu.pc && cpu.pc < 0x80000000)
	// eprintf("%08x: %08x\n", cpu.pc, inst.val);
    if(work_mode == MODE_LOG) print_registers(inst.val);
#endif

	cpu.pc += 4;

    if (nemu_state != NEMU_RUNNING) { return; }
  }

  if (nemu_state == NEMU_RUNNING) { nemu_state = NEMU_STOP; }
}
