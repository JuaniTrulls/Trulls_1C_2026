/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "timer_mcu.h"
#include "gpio_mcu.h"
#include "lcditse0803.h"
#include "led.h"
#include "servo_sg90.h"  // Driver de servo de la cátedra

/*==================[macros and definitions]=================================*/
#define CONFIG_PERIOD_TIMER_SEG 1000000 // 1 segundo en microsegundos

// Pines asignados para el módulo físico del Encoder
#define ENCODER_SW      GPIO_1    
#define ENCODER_DT      GPIO_2
#define ENCODER_CLK     GPIO_3

// Configuración del Servomotor según tus conexiones físicas
#define SERVO_ACCIONADOR SERVO_0   // Canal PWM_0 interno
#define SERVO_PIN        GPIO_6    // Conectado al cable de señal (PWM) de tu servo :)
#define ANGULO_ABIERTO   0       // Ángulo para abrir la caja
#define ANGULO_CERRADO   90        // Ángulo para bloquear la caja

// Relación de sensibilidad del encoder
#define PASOS_POR_MINUTO 5

// Definiciones para las notificaciones del Encoder (Bitmask)
#define GIRO_HORARIO    (1 << 0)
#define GIRO_ANTIHORARIO (1 << 1)

/*==================[typedef]================================================*/
typedef enum {
    ESTADO_CONFIGURADO,
    ESTADO_BLOQUEADO
} estado_caja_t;

/*==================[internal data definition]===============================*/
estado_caja_t estado_actual = ESTADO_CONFIGURADO;

// Variables de lógica de tiempo
volatile uint16_t tiempo_seteado_min = 0;   // Minutos elegidos definitivos
volatile uint16_t tiempo_restante_seg = 0;  // Cuenta regresiva interna en segundos

TaskHandle_t control_task_handle = NULL;
TaskHandle_t display_task_handle = NULL;
TaskHandle_t encoder_task_handle = NULL; 

/*==================[internal functions declaration]=========================*/

/**
 * @brief Interrupción por hardware (ISR) para el pin CLK del Encoder.
 */
void FuncInterruptEncoderCLK(void* param) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (estado_actual == ESTADO_CONFIGURADO) {
        bool dt_state = GPIORead(ENCODER_DT);

        if (dt_state == false) {
            xTaskNotifyFromISR(encoder_task_handle, GIRO_HORARIO, eSetBits, &xHigherPriorityTaskWoken);
        } else {
            xTaskNotifyFromISR(encoder_task_handle, GIRO_ANTIHORARIO, eSetBits, &xHigherPriorityTaskWoken);
        }

        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

/**
 * @brief ISR del Timer de hardware (Se ejecuta cada 1 segundo exacto).
 */
void FuncTimerCuentaRegresiva(void* param) {
    if (estado_actual == ESTADO_BLOQUEADO) {
        if (tiempo_restante_seg > 0) {
            tiempo_restante_seg--;
        } else {
            vTaskNotifyGiveFromISR(control_task_handle, NULL);
        }
    }
}

/**
 * @brief TAREA: Procesamiento asincrónico y filtrado del Encoder.
 */
static void vEncoderTask(void* pvParameter) {
    uint32_t notificacion_recibida;
    int8_t acumulador_pasos = 0;

    while (true) {
        xTaskNotifyWait(0x00, ULONG_MAX, &notificacion_recibida, portMAX_DELAY);

        if (estado_actual == ESTADO_CONFIGURADO) {
            if (notificacion_recibida & GIRO_HORARIO) {
                acumulador_pasos++;
                if (acumulador_pasos >= PASOS_POR_MINUTO) {
                    acumulador_pasos = 0; 
                    if (tiempo_seteado_min < 999) {
                        tiempo_seteado_min++; 
                    }
                }
            }
            else if (notificacion_recibida & GIRO_ANTIHORARIO) {
                acumulador_pasos--;
                if (acumulador_pasos <= -PASOS_POR_MINUTO) {
                    acumulador_pasos = 0; 
                    if (tiempo_seteado_min > 0) {
                        tiempo_seteado_min--; 
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}

/**
 * @brief Tarea de Control: Monitorea el pulsador del encoder, gestiona estados, LEDs y el SERVO.
 */
static void vControlTask(void* pvParameter) {
    GPIOInit(ENCODER_SW, GPIO_INPUT);
    GPIOInit(ENCODER_CLK, GPIO_INPUT);
    GPIOInit(ENCODER_DT, GPIO_INPUT);

    GPIOActivInt(ENCODER_CLK, FuncInterruptEncoderCLK, false, NULL);

    // Estado inicial: LEDs en configuración y el Servo arranca abierto
    LedOn(LED_1);
    LedOff(LED_2);
    ServoMove(SERVO_ACCIONADOR, ANGULO_ABIERTO); 

    while (true) {
        // --- MODO CONFIGURACIÓN ---
        if (estado_actual == ESTADO_CONFIGURADO) {
            if (GPIORead(ENCODER_SW) == 0) {
                vTaskDelay(pdMS_TO_TICKS(50)); 
                if (GPIORead(ENCODER_SW) == 0 && tiempo_seteado_min > 0) {
                    
                    tiempo_restante_seg = tiempo_seteado_min * 60;
                    estado_actual = ESTADO_BLOQUEADO;

                    // Conmutación de indicadores visuales
                    LedOff(LED_1);
                    LedOn(LED_2);

                    // ACCIÓN DEL SERVO: Gira a la posición de trabado en GPIO_6
                    ServoMove(SERVO_ACCIONADOR, ANGULO_CERRADO);
                }
            }
        }
        
        // --- MODO TEMPORIZADOR EN CURSO (BLOQUEADO E ININTERRUMPIBLE) ---
        if (estado_actual == ESTADO_BLOQUEADO) {
            if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)) > 0) {
                
                tiempo_seteado_min = 0;
                tiempo_restante_seg = 0;
                estado_actual = ESTADO_CONFIGURADO;

                // Restablece el estado visual de los LEDs
                LedOff(LED_2);
                LedOn(LED_1);

                // ACCIÓN DEL SERVO: Gira a la posición original abriendo la caja
                ServoMove(SERVO_ACCIONADOR, ANGULO_ABIERTO);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/**
 * @brief Tarea de Visualización: Se encarga de refrescar el display local.
 */
static void vDisplayTask(void *pvParameter) {
    LcdItsE0803Init(); 

    while (true) {
        if (estado_actual == ESTADO_CONFIGURADO) {
            LcdItsE0803Write(tiempo_seteado_min);
        }
        else if (estado_actual == ESTADO_BLOQUEADO) {
            uint16_t min_restantes = (tiempo_restante_seg + 59) / 60;
            LcdItsE0803Write(min_restantes);
        }
        vTaskDelay(pdMS_TO_TICKS(100)); 
    }
}

/*==================[external functions definition]==========================*/
void app_main(void) {
    LedsInit();

    // Inicialización del servomotor asociada al GPIO_6 por macro
    ServoInit(SERVO_ACCIONADOR, SERVO_PIN);

    timer_config_t timer_cuenta = {
        .timer = TIMER_A,
        .period = CONFIG_PERIOD_TIMER_SEG,
        .func_p = FuncTimerCuentaRegresiva,
        .param_p = NULL
    };
    TimerInit(&timer_cuenta);

    xTaskCreate(&vEncoderTask, "ProcesarEncoder", 2048, NULL, 6, &encoder_task_handle); 
    xTaskCreate(&vControlTask, "ControlCaja", 2048, NULL, 5, &control_task_handle);
    xTaskCreate(&vDisplayTask, "Display", 2048, NULL, 4, &display_task_handle);

    TimerStart(timer_cuenta.timer);
}
/*==================[end of file]============================================*/