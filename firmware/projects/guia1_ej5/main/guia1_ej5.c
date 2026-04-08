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
 * @author Juaneko
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gpio_mcu.h"
/*==================[macros and definitions]=================================*/

/*==================[internal data definition]===============================*/
typedef struct
{
	gpio_t pin;			/*!< GPIO pin number */
	io_t dir;			/*!< GPIO direction '0' IN;  '1' OUT*/
} gpioConf_t;


gpioConf_t gpio_vector[4] = {
    {GPIO_20, GPIO_OUTPUT}, // b0
    {GPIO_21, GPIO_OUTPUT}, // b1
    {GPIO_22, GPIO_OUTPUT}, // b2
    {GPIO_23, GPIO_OUTPUT}  // b3
};


/*==================[internal functions declaration]=========================*/

void cambia_estado (uint8_t entero, gpioConf_t* bcd){
	for (int i = 0; i < 4; i++){
		uint8_t bit = (entero >> i) & 0x01;
		if (bit)
		{
			GPIOOn(bcd[i].pin);
		}
		else
		{
			GPIOOff(bcd[i].pin);
		}
	}
}
/*==================[external functions definition]==========================*/


void app_main(void){
	gpioConf_t gpio_vector[4] = {
        {GPIO_20, GPIO_OUTPUT},
        {GPIO_21, GPIO_OUTPUT},
        {GPIO_22, GPIO_OUTPUT},
        {GPIO_23, GPIO_OUTPUT}
    };
	for(uint8_t i = 0; i < 4; i++)
		GPIOInit(gpio_vector[i].pin,gpio_vector[i].dir);

	uint8_t numero = 5;

	cambia_estado(numero, gpio_vector);
}
/*==================[end of file]============================================*/