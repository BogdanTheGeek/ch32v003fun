/* Small example showing how to use the SWIO programming pin to
   do printf through the debug interface */

#include "ch32fun.h"
#include <stdio.h>

extern const uint32_t _manual_ram_vma_start;
extern const uint32_t _manual_ram_vma_end;
extern const uint32_t _flash_manual_ram_start;


__attribute__( ( section( ".manual_ram" ) ) ) void blinker( uint32_t delay_ms )
{
	funDigitalWrite( PC0, FUN_LOW );
	Delay_Ms( delay_ms );
	funDigitalWrite( PC0, FUN_HIGH );
	Delay_Ms( delay_ms );
}

void load_manual_ram_section( void )
{
	const size_t manual_ram_size = (size_t)( &_manual_ram_vma_end - &_manual_ram_vma_start );
	memcpy( (void *)&_manual_ram_vma_start, (void *)&_flash_manual_ram_start, manual_ram_size );
}


int main()
{
	SystemInit();

	Delay_Ms( 100 ); // Wait for debugger to connect

	// Enable GPIOs
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOC;

	funPinMode( PD0, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP );
	funPinMode( PC0, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP );

	// when you don't use the function, you can use the ram area as scratch
	uint8_t *scartch = (uint8_t *)&_manual_ram_vma_start;

	for ( size_t i = 0; i < 10; i++ )
	{
		scartch[i] = (uint8_t)i;
	}

	printf( "scratch data: " );
	for ( size_t i = 0; i < 10; i++ )
	{
		printf( "%d ", scartch[i] );
	}
	printf( "\n" );


	printf( "blinker @ %p\n", blinker );
	load_manual_ram_section();
	printf( "loaded manual ram section\n" );

	while ( 1 )
	{
		blinker( 500 );
		printf( "tick\n" );
	}
}
