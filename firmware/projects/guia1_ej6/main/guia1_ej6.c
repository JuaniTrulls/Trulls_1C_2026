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
 * @author Juan Ignacio Trulls Schmidt (juanitrulls@gmail.com)
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

int8_t  convertToBcdArray (uint32_t data, uint8_t digits, uint8_t * bcd_number){
	for(uint8_t i = 0; i < digits; i++){
		bcd_number[digits - i - 1] = data % 10;
		data = data/10;
	}
	return 0;
}

void mostrar_numero(uint32_t data, uint8_t digits, gpioConf_t* bcd, gpioConf_t* sel)
{
    uint8_t bcd_array[digits];

    // Convertir número a BCD
    convertToBcdArray(data, digits, bcd_array);

    while (1)
    {
        for (uint8_t i = 0; i < digits; i++)
        {
            // Apagar todos los dígitos
            for (uint8_t j = 0; j < digits; j++)
            {
                GPIOOff(sel[j].pin);
            }

            // Enviar valor al BCD
            cambia_estado(bcd_array[i], bcd);

            // Encender dígito actual
            GPIOOn(sel[i].pin);

            // Delay corto para multiplexado
            vTaskDelay(5 / portTICK_PERIOD_MS);
        }
    }
}

void app_main(void)
{
    // Vector BCD (ya lo tenés global, podrías usar ese)
    gpioConf_t gpio_bcd[4] = {
        {GPIO_20, GPIO_OUTPUT},
        {GPIO_21, GPIO_OUTPUT},
        {GPIO_22, GPIO_OUTPUT},
        {GPIO_23, GPIO_OUTPUT}
    };

    // Vector de selección de dígitos
    gpioConf_t sel_digits[3] = {
        {GPIO_19, GPIO_OUTPUT},
        {GPIO_18, GPIO_OUTPUT},
        {GPIO_9,  GPIO_OUTPUT}
    };

    // Inicializar pines BCD
    for (int i = 0; i < 4; i++)
        GPIOInit(gpio_bcd[i].pin, gpio_bcd[i].dir);

    // Inicializar pines de selección
    for (int i = 0; i < 3; i++)
        GPIOInit(sel_digits[i].pin, sel_digits[i].dir);

    // Mostrar número
    mostrar_numero(123, 3, gpio_bcd, sel_digits);
}

/*==================[end of file]============================================*/