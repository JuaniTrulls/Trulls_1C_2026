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
 * @author Juani Trulls (juan.trulls@ingenieria.uner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "switch.h"
/*==================[macros and definitions]=================================*/

/*==================[internal data definition]===============================*/
struct leds
{
	uint8_t mode;       //ON, OFF, TOGGLE
	uint8_t n_led;        //indica el número de led a controlar
	uint8_t n_ciclos;     //indica la cantidad de ciclos de encendido/apagado
	uint16_t periodo;     //indica el tiempo de cada ciclo
} my_leds; 

enum{ON, OFF, TOGGLE};
#define CONFIG_BLINK_PERIOD 1000

/*==================[internal functions declaration]=========================*/
void controlar_leds (struct leds  *juani_leds)
{
	switch (juani_leds->mode)
	{
		case ON:
			LedOn(juani_leds->n_led);
			break;

		case OFF:
			LedOff(juani_leds->n_led);
			break;

		case TOGGLE:
			uint8_t i=0;
			while (i < 2*juani_leds->n_ciclos) {
				LedToggle(juani_leds->n_led);
				i++;
				vTaskDelay(juani_leds->periodo / portTICK_PERIOD_MS);
			}
			break;	

	}
	
}

void app_main(void)
{
    // Inicializar LEDs (MUY importante)
    LedsInit();

    // Configurar estructura
    my_leds.mode = TOGGLE;
    my_leds.n_led = LED_3;
    my_leds.n_ciclos = 10;
    my_leds.periodo = CONFIG_BLINK_PERIOD;

    // Llamar a tu función
    controlar_leds(&my_leds);

	vTaskDelay(2000);

	// Configurar estructura
    my_leds.mode = ON;
    my_leds.n_led = LED_1;
    my_leds.n_ciclos = 10;
    my_leds.periodo = CONFIG_BLINK_PERIOD;

    // Llamar a tu función
    controlar_leds(&my_leds);

	vTaskDelay(2000);

	// Configurar estructura
    my_leds.mode = ON;
    my_leds.n_led = LED_2;
    my_leds.n_ciclos = 10;
    my_leds.periodo = CONFIG_BLINK_PERIOD;

	controlar_leds(&my_leds);


}
/*==================[external functions definition]==========================*/

/*==================[end of file]============================================*/