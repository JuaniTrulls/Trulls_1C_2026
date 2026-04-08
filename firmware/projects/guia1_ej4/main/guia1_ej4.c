/*! @mainpage Template
 *
 * @section genDesc General Description
 *
 * This section describes how the program works.
 *
 * <a href="https://drive.google.com/...">Operation Example</a>
 *
 * @section hardConn Hardware Connection
 *
 * |    Peripheral  |   ESP32   	|
 * |:--------------:|:--------------|
 * | 	PIN_X	 	| 	GPIO_X		|
 *
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 12/09/2023 | Document creation		                         |
 *
 * @author Juani Trulls
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
/*==================[macros and definitions]=================================*/

/*==================[internal data definition]===============================*/

/*==================[internal functions declaration]=========================*/

/*==================[external functions definition]==========================*/

int8_t  convertToBcdArray (uint32_t data, uint8_t digits, uint8_t * bcd_number){
	for(uint8_t i = 0; i < digits; i++){
		bcd_number[digits - i - 1] = data % 10;
		data = data/10;
	}
	return 0;
}


void app_main(void){
	uint32_t data = 123;
	uint8_t digits = 3;
	uint8_t bcd_number[digits];
	convertToBcdArray( data, digits, bcd_number);
	printf ("%d %d %d", bcd_number[0],bcd_number[1], bcd_number[2]);
}
/*==================[end of file]============================================*/