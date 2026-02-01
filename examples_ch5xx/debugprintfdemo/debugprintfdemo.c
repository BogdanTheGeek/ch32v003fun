/* Small example showing how to use the SWIO programming pin to
   do printf through the debug interface */

#include "ch32fun.h"
#include <stdio.h>

#define tselect "0x7A0"
#define tdata1 "0x7A1" // MRW Debug trigger data register 1
#define tdata2 "0x7A2" // MRW Debug trigger data register 2
#define tinfo "0x7A4" // MRO Trigger information register

volatile uint32_t bigbuffer[64] = { 0 };

void EnableBreakPoint( uint32_t addr )
{
	volatile uint32_t value = 0 << 12 | // ACTION
	                          1 << 6 | // M
	                          1 << 3 | // U
	                          1 << 2 | // EXEC
	                          1 << 1 | // STORE
	                          1 << 0; // LOAD

	asm volatile( ADD_ARCH_ZICSR "csrw " tselect ", 0" );
	asm volatile( ADD_ARCH_ZICSR "csrw " tdata1 ", %0" ::"r"( value ) );
	asm volatile( ADD_ARCH_ZICSR "csrw " tdata2 ", %0" : : "r"( addr ) );
}


#if 1
void Break_Point_Handler( void ) __attribute__( ( interrupt ) );
void Break_Point_Handler( void )
{
	printf( "BREAKPOINT!!!!!\n" );
	EnableBreakPoint( 0 );
}
#endif


int main()
{
	SystemInit();

	const volatile uint32_t *brk = &bigbuffer[16];

	funGpioInitAll(); // no-op on ch5xx

	// (void)WaitForDebuggerToAttach( 1000 );
	Delay_Ms( 1000 );
	printf( "Starting\n" );
	Delay_Ms( 1000 );

	EnableBreakPoint( (uint32_t)brk );

	int i = 0;
	while ( 1 )
	{
		volatile uint32_t *p = &bigbuffer[i];
		printf( "val[%d] = 0x%08X\r\n", i++, *p );

		Delay_Ms( 100 );

		if ( i >= 32 )
		{
			i = 0;
			EnableBreakPoint( (uint32_t)brk );
		}
	}
}
