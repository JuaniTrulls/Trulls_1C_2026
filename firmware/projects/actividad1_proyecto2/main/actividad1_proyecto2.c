/*! @mainpage Medidor de Distancia con Ultrasonido
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
 * | Distancia (cm) | LEDs |
 * |---------------|------|
 * | < 10          | Todos apagados |
 * | 10 - 20       | LED_1 |
 * | 20 - 30       | LED_1 y LED_2 |
 * | > 30          | LED_1, LED_2 y LED_3 |
 *
 * @section hardwareCon Hardware utilizado
 *
 * | Señal   | GPIO |
 * |--------|------|
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
/*==================[macros and definitions]=================================*/
#define ECHO_PIN    GPIO_3
#define TRIGGER_PIN GPIO_2
#define CONFIG_PERIOD 1000
#define TASK_DELAY 10
/*==================[internal data definition]===============================*/
/** @brief Indica si la medición está activa */
bool medir = false;

/** @brief Indica si el sistema está en modo HOLD */
bool hold = false;

/** @brief Distancia medida en centímetros */
uint32_t distancia = 0;
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
 * @brief Tarea encargada de la lectura de los botones.
 *
 * Esta tarea:
 * - Lee el estado de los switches
 * - Alterna la variable 'medir' con TEC1
 * - Alterna la variable 'hold' con TEC2
 * - Implementa un antirrebote por software
 *
 * @param pvParameter Parámetro no utilizado
 */
static void ButtonsTask(void *pvParameter)
{
    while (true)
    {
        int8_t teclas = SwitchesRead();


        if (teclas & SWITCH_1)
        {
            medir = !medir;
            vTaskDelay(200 / portTICK_PERIOD_MS);
        }

        if (teclas & SWITCH_2)
        {
            hold = !hold;
            vTaskDelay(200 / portTICK_PERIOD_MS);
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
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
    uint16_t contador_ms = 0;

    while (true) {
        if (medir) {
            contador_ms = contador_ms + TASK_DELAY;

            if (contador_ms >= CONFIG_PERIOD) {
                contador_ms = 0;

                distancia = HcSr04ReadDistanceInCentimeters();

                actualizar_leds();

                if (!hold) {
                    LcdItsE0803Write(distancia);
                }
            }
        } else {
        contador_ms = 0;
        LcdItsE0803Off();
        LedOff(LED_1); LedOff(LED_2); LedOff(LED_3);
        }
        vTaskDelay(TASK_DELAY / portTICK_PERIOD_MS);
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
 *
 * Luego crea las tareas del sistema:
 * - ButtonsTask
 * - MainTask
 */
void app_main(void)
{
    LedsInit();
    SwitchesInit();

    if (!LcdItsE0803Init())
    {
        printf("Error LCD\n");
    }

    if (!HcSr04Init(ECHO_PIN, TRIGGER_PIN))
    {
        printf("Error sensor\n");
    }

    xTaskCreate(&ButtonsTask, "Buttons", 512, NULL, 5, NULL);
    xTaskCreate(&MainTask, "MainControl", 512, NULL, 5, NULL);
}