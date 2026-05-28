/*! @mainpage Medidor de Distancia con Ultrasonido Usando Interrupciones y Timer
 *
 * @section genDesc Descripción General
 *
 * Este proyecto implementa un medidor de distancia utilizando el sensor ultrasónico HC-SR04.
 * El sistema permite visualizar la distancia medida en un display LCD y mediante LEDs.
 *
 * Funcionalidades:
 * - Medición de distancia cada 1 segundo.
 * - Visualización en display LCD (0 a 999 cm).
 * - Indicación con LEDs según rango de distancia.
 * - Control mediante botones:
 *   - TEC1: Inicia/detiene la medición.
 *   - TEC2: Activa/desactiva modo HOLD.
 *
 * @section ledsControl Control de LEDs
 *
 * | Distancia (cm)| LEDs                  |
 * |---------------|-----------------------|
 * | < 10          | Todos apagados        |
 * | 10 - 20       | LED_1                 |
 * | 20 - 30       | LED_1 y LED_2         |
 * | > 30          | LED_1, LED_2 y LED_3  |
 *
 * @section hardwareCon Hardware utilizado
 *
 * | Señal  | GPIO   |
 * |--------|--------|
 * | ECHO   | GPIO_3 |
 * | TRIGGER| GPIO_2 |
 *
 * @section osc Osciloscopio
 *
 * Se verificaron las señales del sensor:
 * - TRIGGER: pulso de ~10 µs
 * - ECHO: pulso proporcional a la distancia
 *
 * @section author Autor
 *
 * Juan Ignacio Trulls Schmidt
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
/*==================[macros and definitions]=================================*/
#define ECHO_PIN    GPIO_3
#define TRIGGER_PIN GPIO_2
#define CONFIG_PERIOD_US 1000000
/*==================[internal data definition]===============================*/
/** @brief Indica si la medición está activa */
bool medir = false;

/** @brief Indica si el sistema está en modo HOLD */
bool hold = false;

/** @brief Distancia medida en centímetros */
uint32_t distancia = 0;

/** @brief Manejador para la tarea */
TaskHandle_t manejador_de_mi_tarea = NULL;
/*==================[internal functions declaration]=========================*/

/**
 * @brief Actualiza el estado de los LEDs según la distancia medida.
 *
 * Esta función enciende o apaga los LEDs de acuerdo al rango de distancia:
 * - <10 cm: todos apagados
 * - 10-20 cm: LED_1
 * - 20-30 cm: LED_1 y LED_2
 * - >30 cm: LED_1, LED_2 y LED_3
 */

void actualizar_leds(void)
{
    LedOff(LED_1);
    LedOff(LED_2);
    LedOff(LED_3);

    if (distancia >= 10 && distancia < 20)
        LedOn(LED_1);

    else if (distancia >= 20 && distancia < 30)
    {
        LedOn(LED_1);
        LedOn(LED_2);
    }

    else if (distancia >= 30)
    {
        LedOn(LED_1);
        LedOn(LED_2);
        LedOn(LED_3);
    }
}

/**
*@brief Tarea que lleva a cabo la interrupcion de la tecla 1 para iniciar o detener la medicion */
static void int_tecla1_medir(void*args)
	{
	medir=!medir;
	}

/**
*@brief Tarea que lleva a cabo la interrupcion de la tecla 2 para congelar la medicion */
static void int_tecla2_hold(void*args)
{
	hold=!hold;
}
/**
 * @brief Tarea encargada de enviar una notificacion una vez cumplido el timepo del timer
 */
void FuncTimerA(void* param){
    vTaskNotifyGiveFromISR(manejador_de_mi_tarea, pdFALSE);
}

void leer_teclado_pc(void* param)
{
	uint8_t tecla;
	UartReadByte(UART_PC, &tecla);

	//Aca gemini me dijo que convierta a mayusculas las teclas por las dudas
	if(tecla == 'O' || tecla == 'o') medir = !medir;
	if(tecla == 'H' || tecla == 'h') hold = !hold;
}
/**
 * @brief Tarea principal del sistema.
 *
 * Esta tarea:
 * - Ejecuta el ciclo principal del sistema
 * - Controla el tiempo de muestreo (1 segundo)
 * - Realiza la medición de distancia
 * - Actualiza LEDs y display
 *
 * Funcionamiento:
 * - Si 'medir' es verdadero:
 *      - Incrementa contador de tiempo
 *      - Cada 1 segundo:
 *          - Lee distancia
 *          - Actualiza LEDs
 *          - Muestra valor (si no está en HOLD)
 * - Si 'medir' es falso:
 *      - Apaga LEDs y display
 *
 * @param pvParameter Parámetro no utilizado
 */
static void MainTask(void *pvParameter)
{  

    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (medir) {
            distancia = HcSr04ReadDistanceInCentimeters();
            actualizar_leds();

            if (!hold) {
                LcdItsE0803Write(distancia);

				UartSendString(UART_PC, (char*)UartItoa(distancia,10));
				UartSendString(UART_PC, " cm\r\n");
            }
        
        } else {
        LcdItsE0803Off();
        LedOff(LED_1); LedOff(LED_2); LedOff(LED_3);
        }
    }
}



/*==================[external functions definition]==========================*/
/**
 * @brief Función principal del sistema.
 *
 * Inicializa:
 * - LEDs
 * - Switches
 * - Display LCD
 * - Sensor ultrasónico
 * - Interruptor cuando se activa la tecla 1
 * - Interruptor cuando se activa la tecla 2
 * - El timer
 *
 * Luego crea la tarea del sistema:
 * - MainTask
 * 
 * Por ultimo
 * - comienza el conteo del timer
 */
void app_main(void)
{
    LedsInit();
    SwitchesInit();
	SwitchActivInt(SWITCH_1, int_tecla1_medir, NULL);
	SwitchActivInt(SWITCH_2, int_tecla2_hold, NULL);
	serial_config_t config_serial = {
		.port = UART_PC,
		.baud_rate = 9600,
		.func_p = leer_teclado_pc,
		.param_p = NULL
	};
	UartInit(&config_serial);
    timer_config_t timer_led_1 = {
        .timer = TIMER_A,
        .period = CONFIG_PERIOD_US,
        .func_p = FuncTimerA,
        .param_p = NULL
    };
    TimerInit(&timer_led_1);
    if (!LcdItsE0803Init())
    {
        printf("Error LCD\n");
    }

    if (!HcSr04Init(ECHO_PIN, TRIGGER_PIN))
    {
        printf("Error sensor\n");
    }

    xTaskCreate(&MainTask, "MainControl", 512, NULL, 5, &manejador_de_mi_tarea);

    TimerStart(timer_led_1.timer);
}