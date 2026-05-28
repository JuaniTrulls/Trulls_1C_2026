/*! @mainpage Actividad 4 - Proyecto 2: Osciloscopio y Generador ECG
 *
 * @section genDesc General Description
 *
 * Esta aplicacion convierte al ESP32 en un sistema dual: un generador de se;ales
 * analogicas y un oasciloscopio digital en tiempo real utilizando FreeRTOS.
 * El sistema lee un arreglo de datos que representan una se;al de ECG digital y
 * la transfiere secuencialmente al conversor Digital-Anaalogico (DAC). Fisicamente, 
 * esta salida se puentea a la entrada del conversor Analogico-Digital *ADC) CH1.
 * Una tarea periodica toma las muestras del ADC y las transmite por el puerto serie 
 * (UART) hacia la PC utilizando un formato compatible con "Serial Plotter".
 *
 * @section hardConn Hardware Connection
 *
 * |    Peripheral  |   ESP32   	|
 * |:--------------:|:--------------|
 * |  Entrada ADC	| 	  CH1  		|
 * |:--------------:|:--------------|
 * |   Salida DAC	| 	  CH0 		|
 *
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 10/05/2026 | Document creation		                         |
 *
 * @author Juan Ignacio Trulls Schmidt
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "hc_sr04.h"
#include "lcditse0803.h"
#include "switch.h"
#include "timer_mcu.h"
#include "uart_mcu.h"
#include "analog_io_mcu.h"
/*==================[macros and definitions]=================================*/
/* La frecuencia de muestreo debe ser de 500Hz, por lo tanto el periodo es de 1/500 = 0.002s = 2 micros
*/
#define CONFIG_PERIOD_US_AD 2000

/* Generacion del DAC (Señal de ECG): se sugiere actualizar el DAC a mayor velocidad o igual
Voy a usar tambien 25000 microsegundos 
*/
#define  CONFIG_PERIOD_US_DA 25*1000

/* tamañpo del arrglo del ECG (conteo de los elementos provistos)
*/
#define TAMANIO_ECG 223

/*==================[internal data definition]===============================*/
/** @brief Variable para almacenar el valor leido del ADC */
uint16_t voltaje = 0;

/** @brief Manejador para la tarea principal */
TaskHandle_t main_task_handle = NULL;

/** @brief Manejador para la tarea DAC */
TaskHandle_t dac_task_handle = NULL;

/* Arreglo que nos dio la catedra ()*/
const uint8_t ECG[TAMANIO_ECG] = {
    17,17,17,17,17,17,17,17,17,17,17,18,18,18,17,17,17,17,17,17,17,18,18,18,18,18,18,18,17,17,16,16,16,16,17,17,18,18,18,17,17,17,17,
    18,18,19,21,22,24,25,26,27,28,29,31,32,33,34,34,35,37,38,37,34,29,24,19,15,14,15,16,17,17,17,16,15,14,13,13,13,13,13,13,13,12,12,
    10,6,2,3,15,43,88,145,199,237,252,242,211,167,117,70,35,16,14,22,32,38,37,32,27,24,24,26,27,28,28,27,28,28,30,31,31,31,32,33,34,36,
    38,39,40,41,42,43,45,47,49,51,53,55,57,60,62,65,68,71,75,79,83,87,92,97,101,106,111,116,121,125,129,133,136,138,139,140,140,139,137,
    133,129,123,117,109,101,92,84,77,70,64,58,52,47,42,39,36,34,31,30,28,27,26,25,25,25,25,25,25,25,25,24,24,24,24,25,25,25,25,25,25,25,
    24,24,24,24,24,24,24,24,23,23,22,22,21,21,21,20,20,20,20,20,19,19,18,18,18,19,19,19,19,18,17,17,18,18,18,18,18,18,18,18,17,17,17,17,
    17,17,17
};

/*==================[internal functions declaration]=========================*/

/**
 * @brief Tarea encargada de enviar una notificacion una vez cumplido el tiempo del timer.
 * Es la interrupcion del Timer A para el OSciloscopio (ADC)
 * @param param Parametro opcional no utilizado
 * @return Envia una notificacion de forma segura desde la ISR para desbloquera MainTask
 */
void FuncTimerMuestreo (void* param){
	vTaskNotifyGiveFromISR(main_task_handle, pdFALSE);
}

/**
 * @brief Tarea encargada de enviar una notifiacion una vez que se cumpla el tiempo del timer
 * Interrupcion del Timer B para el Generador (DAC)
 * @param param Parametro opcional no utilizado
 * @return envia una notificacion de forma segura desde la ISR para desbloquear DacTask
 */
void FuncTimerGeneracion(void* param){
	vTaskNotifyGiveFromISR(dac_task_handle, pdFALSE);
}


/** @brief Tarea que lee el ADC y envia el dato por la UART 
 * Es la tarea del "osciloscopio"
 * @param pvParameter Parametro opcional de FreeRTOS
 * @details Espera en estado bloqueado hasta recibir la notificacion del Timer A
 * Lee el canal CH1 y envia la cadena formateada por la UART como entero ASCII
*/
static void MainTask(void *pvParameter){
	while (true) {
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		AnalogInputReadSingle(CH1, &voltaje);

		UartSendString(UART_PC, ">voltaje:");
		UartSendString(UART_PC, (char*)UartItoa(voltaje, 10));
		UartSendString(UART_PC, "\r\n");
	}
}

/**
 * @brief Tarea del Generador: Recorre el arreglo de ECG y escribe en el DAC
 * @param pvParameter Parametro opcional de FreeRTOS
 * @details se activa mediante la notificacion periodica del Timer B. Recorre de manera
 * ciclica el arreglo global ECG enviando cada valor a la salida analogica
 */
static void DacTask(void *pvParameter){
	uint8_t indice = 0;
	while (true) {
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		AnalogOutputWrite(ECG[indice]);

		indice++;
		if (indice >= TAMANIO_ECG) {
			indice = 0;
		}
	}
}
/*==================[external functions definition]==========================*/
/**
 * @brief Funcion principal del ciclo de vida de la aplciacion de ESP-IDF
 * @details Inicializa los perifericos de hardware (UART, ADC, DAC), configura los 
 * Timers con sus respectivas interrupciones, crea las tareas y arranca los Timers.
 */
void app_main(void){
	/* Inicializacion de la comunicacion Serie (UART)*/
	serial_config_t config_serial = {
		.port = UART_PC,
		.baud_rate = 115200,
		.func_p = UART_NO_INT,
		.param_p = NULL
	};
	UartInit(&config_serial);

	/* Inicializacion del Conversor Analogico Digital (ADC) */
	analog_input_config_t config_adc = {
		.input = CH1,
		.mode = ADC_SINGLE,
		.func_p = NULL,
		.param_p = NULL,
		.sample_frec = 0
	};
	AnalogInputInit(&config_adc);

	/* Inicializacion del Conversor Digital-Analogico (DAC)*/
	AnalogOutputInit();

	/* Inicializacion del Timer A (Control de tiempo para el ADC)*/
	timer_config_t timer_ad = {
		.timer = TIMER_A,
		.period = CONFIG_PERIOD_US_AD,
		.func_p = FuncTimerMuestreo,
		.param_p = NULL
	};
	TimerInit(&timer_ad);

	/* Inicializacion del Timer B (control de tiempo para el DAC)*/
	timer_config_t timer_da = {
		.timer = TIMER_B,
		.period = CONFIG_PERIOD_US_DA,
		.func_p = FuncTimerGeneracion,
		.param_p = NULL,
	};
	TimerInit(&timer_da);

	/* Creacion de tareas en el planificador de FreeRTOS con prioridad alta (5)*/
	xTaskCreate(&MainTask, "Osciloscopio", 2048, NULL, 5, &main_task_handle);
	xTaskCreate(&DacTask, "GeneradorECG", 2048, NULL, 5, &dac_task_handle);

	/* Activacion por hardware de ambos temporizadores*/
	TimerStart(timer_ad.timer);
	TimerStart(timer_da.timer);
}
/*==================[end of file]============================================*/