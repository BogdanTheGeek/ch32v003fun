// Could be defined here, or in the processor defines.
#include "ch32fun.h"
#include <stdint.h>
#include <stdio.h>

#define INSTR_10(x) \
   x \
   x \
   x \
   x \
   x \
   x \
   x \
   x \
   x \
   x

#define INSTR_100(x) \
   INSTR_10(x) \
   INSTR_10(x)

// Helper to read the cycle counter (standard RISC-V way)
// If your hardware uses a memory-mapped "SysTick", replace this with:
// uint32_t count = *(volatile uint32_t*)SYSTICK_ADDRESS;
static inline uint32_t get_cycles()
{
	return SysTick->CNT;
}

void print_result( const char *name, uint32_t total_cycles, uint32_t overhead )
{
	if ( total_cycles < overhead )
	{
		printf( "%s | Error: Timing too low\n", name );
		return;
	}

	printf( "%s | %u cycles\n", name, total_cycles);
}

// void run_benchmark() __attribute__( ( section( ".srodata" ) ) ) __attribute__( ( used ) );
void run_benchmark()
{
	uint32_t start, end, overhead;

	// --- 1. Baseline Overhead (The "Empty" Loop) ---
	// We include a NOP here to match the structure of the test loops.
	start = get_cycles();
	end = get_cycles();
	overhead = end - start;

	printf( "Instruction Benchmark (RV32EC)\n" );
	printf( "Iterations: %d | Overhead: %u cycles\n", 20, overhead );
	printf( "-------------------------------------------\n" );

	// --- 2. ADDI (Addition Immediate) ---
	start = get_cycles();
	INSTR_100({
		asm volatile( "addi x1, x1, 1" );
	})
	end = get_cycles();
	print_result( "ADDI", end - start, overhead );

	// --- 3. SLLI (Shift Left Logical Immediate) ---
	start = get_cycles();
	INSTR_100({
		asm volatile( "slli x1, x1, 2" );
	})
	end = get_cycles();
	print_result( "SLLI", end - start, overhead );

	// --- 4. LW (Load Word) - Assuming data is in Cache/TCM ---
	int val = 0xAA;
	int *ptr = &val;
	start = get_cycles();
	INSTR_100({
		asm volatile( "lw x1, 0(%0)" : : "r"( ptr ) : "x1" );
	})
	end = get_cycles();
	print_result( "LW (TCM)", end - start, overhead );

	// --- 5. C.ADDI (Compressed 16-bit Add) ---
	start = get_cycles();
	INSTR_100({
		asm volatile( "c.addi x1, 1" );
	})
	end = get_cycles();
	print_result( "C.ADDI", end - start, overhead );

	// --- 6. C.LI (Compressed Load Immediate) ---
	start = get_cycles();
	INSTR_100({
		asm volatile( "c.li x1, 10" );
	})
	end = get_cycles();
	print_result( "C.LI", end - start, overhead );

	// --- 7. XOR (Logic) ---
	start = get_cycles();
	INSTR_100({
		asm volatile( "xor x1, x1, x2" );
	})
	end = get_cycles();
	print_result( "XOR", end - start, overhead );

	// --- 1. Baseline (2 NOPs) ---
	// Every test loop below will have exactly 2 instruction slots.
	start = get_cycles();
	INSTR_100({
		asm volatile( "nop; nop" );
	})
	end = get_cycles();
	overhead = end - start;

	printf( "RV32EC Pipeline Analysis (Integer Scale)\n" );
	printf( "-------------------------------------------\n" );

	// --- 2. Load-Use Hazard ---
	// We load a value into x1 and immediately try to use x1 in the next cycle.
	// Most 5-stage pipelines will stall for 1 cycle here.
	start = get_cycles();
	INSTR_100({
		asm volatile( "lw x1, 0(%0) \n\t"
					  "addi x1, x1, 1"
			:
			: "r"( ptr )
			: "x1" );
	})
	end = get_cycles();
	print_result( "Load-Use Stall", end - start, overhead );

	// --- 3. Branch Not Taken ---
	// x1 is 1, so 'beq x1, x0' is false. Usually 1 cycle (no flush).
	int x1_val = 1;
	start = get_cycles();
	INSTR_100({
		asm volatile( "beq %0, x0, 1f \n\t"
					  "nop             \n\t"
					  "1:"
			:
			: "r"( x1_val ) );
	})
	end = get_cycles();
	print_result( "Branch Not Taken", end - start, overhead );

	// --- 4. Branch Taken (Forward) ---
	// x0 is always 0, so 'beq x0, x0' is true.
	// This forces a pipeline flush on most small RISC-V cores.
	start = get_cycles();
	INSTR_100({
		asm volatile( "beq x0, x0, 2f \n\t"
					  "nop             \n\t"
					  "2:" );
	})
	end = get_cycles();
	print_result( "Branch Taken", end - start, overhead );

}

int main()
{
	SystemInit();

	// Reset any pre-existing configuration
	SysTick->CTLR = 0x0000;
	
	SysTick->CTLR |= SYSTICK_CTLR_STE   |  // Enable Counter
	                 SYSTICK_CTLR_STCLK ;  // Set Clock Source to HCLK/1
	

	Delay_Ms( 500 );

	printf( "Starting function in RAM test\r\n" );

	run_benchmark();
}
