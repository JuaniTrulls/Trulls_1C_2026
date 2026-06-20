/*! @mainpage Proyecto Final Electronica Programable 1er Cuatrimestre 2026:
 * "FocusBox: dispositivo de bloque de distracciones y fomentador de productividad"
 *
 * @section genDesc General Description
 *
 * Este programa controla una caja de seguridad temporizada utilizando FreeRTOS.
 * Permite al usuario setear un tiempo en minutos girando el encoder rotativo ( en 
 * sentido antihorario para incrementar y horario para decrementar).
 * Al pulsar el switch integrado del encoder, el sistema pasa a un estado de bloqueo, en
 * este momento el servomotor, solidario a la tapa de la caja, se mueve a la posicion de 
 * cierre (90 grados) y comienza la cuenta regresiva. Una cez finalizado el tiempo, el sevomotor
 * regresa automaticamente a la posicion de apertura (0 grados) abriendo la caja
 *
 * 
 *
 * @section hardConn Hardware Connection
 *
 * |    Encoder     |    ESP32   	|
 * |:--------------:|:--------------|
 * | 	   Vcc  	| 	 3.3 V  	|
 * | 	Encoder SD	| 	 GPIO_1  	|
 * | 	Encoder DT	| 	 GPIO_2  	|
 * | 	Encoder CLK	| 	 GPIO_3  	|
 * | 	   GND   	| 	  GND    	|
 * 
 * |   Servo Motor  |   ESP32   	|
 * |:--------------:|:--------------|
 * | 	   Vcc	    | 	   5V    	|
 * | 	Señal PWM	| 	 GPIO_6  	|
 * | 	   GND     	| 	   GND  	|
 * 
 * |   Display      |   ESP-32   	|
 * |:--------------:|:-------------:|
 * | 	Vcc 	    |	5V      	|
 * | 	BCD1		| 	GPIO_20		|
 * | 	BCD2	 	| 	GPIO_21		|
 * | 	BCD3	 	| 	GPIO_22		|
 * | 	BCD4	 	| 	GPIO_23		|
 * | 	SEL1	 	| 	GPIO_19		|
 * | 	SEL2	 	| 	GPIO_18		|
 * | 	SEL3	 	| 	GPIO_9		|
 * | 	Gnd 	    | 	GND     	|
 *
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 03/06/2026 | Inicio del Proyecto	                         |
 * | 17/06/2026 | Filmacion del funcionamiento                   |
 * | 23/06/2026 | Presentacion Final                             |
 *
 * @author Trulls Schmidt Juan Ignacio
 *
 */

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
#include "servo_sg90.h"  

/*==================[macros and definitions]=================================*/
/** @brief Periodo edl temporizador configurado a 1 segundo expresado en microsegundos */
#define CONFIG_PERIOD_TIMER_SEG 1000000 // 1 segundo en microsegundos

// Pines asignados para el módulo físico del Encoder
/** @brief GPIO asignado al switch integrado del encoder rotativo */
#define ENCODER_SW      GPIO_1    
/** @brief GPIO asignado al canal complementario de direccion (Data) del encoder */
#define ENCODER_DT      GPIO_2
/** @brief GPIO asignado al canal del reloj (CLK) deque genera las interrupciones del encoder */
#define ENCODER_CLK     GPIO_3

// Configuración del Servomotor según tus conexiones físicas
/** @brief Canal PWM asignado por hardware para el control del sevomotor. */
#define SERVO_ACCIONADOR SERVO_0   // Canal PWM_0 interno
/** @brief GPIO asignado para la transmision de modulacion por ancho de pulsos al servo*/
#define SERVO_PIN        GPIO_6    // Conectado al cable de señal (PWM) del servo :) (este en la placa esta indicado como SDA)
/** @brief Angulo en grados correspondiente al estado de caja abierta */
#define ANGULO_ABIERTO   0       // Ángulo para abrir la caja
/** @brief Angulo en grados correspondiente al estado de caja cerrada */
#define ANGULO_CERRADO   90        // Ángulo para bloquear la caja

/** @brief Cantidad de cambios mecanicos necesarios para incrementar o decrementar 1 minuto
 * Ya que de otra forma la sensibilidad del encoder es muy alta y no se logra un buen seteo temporal
 */
#define PASOS_POR_MINUTO 5

/** @brief Mascara de bits que representa un desplazamiento en sentido de las agujas del reloj */
#define GIRO_HORARIO    (1 << 0)
/** @brief Mascara de bits que representa un desplazamiento en sentido antihorario */
#define GIRO_ANTIHORARIO (1 << 1)

/*==================[typedef]================================================*/
/** @brief Enum que define los estados operativos posibles de la maquina de estados de la caja */
typedef enum {
    ESTADO_CONFIGURADO, /**< Modo de reposo incial donde el usuario incrementa/decrementa el tiempo deseado */
    ESTADO_BLOQUEADO /**< Modo ininterrumpible donde el conteo esta activo y el servo bloquea el acceso */
} estado_caja_t;

/*==================[internal data definition]===============================*/
/** @brief Variable global que almacena el estado actual de la maquina de estados. */
estado_caja_t estado_actual = ESTADO_CONFIGURADO;

/** @brief Variable volatil que almacena los minutos definidos por el usario mediante eel encoder */
volatile uint16_t tiempo_seteado_min = 0;   // Minutos elegidos definitivos
/** @brief Variable voltil que gestiona los segundos remanentes de la cuenta regresiva activa */
volatile uint16_t tiempo_restante_seg = 0;  // Cuenta regresiva interna en segundos

/** @brief Descriptor de FreeRTOS asignado a la tarea que monitoriza el comportamieento principal de la caja */
TaskHandle_t control_task_handle = NULL;
/** @brief Descriptor de FreeRTOS asignado a la tarea encargada de refrescar el display */
TaskHandle_t display_task_handle = NULL;
/** @brief Descriptor de FreeRTOR asignado a la tarea asincrona deee calculo y filtrado de muescas del encoder */
TaskHandle_t encoder_task_handle = NULL; 

/*==================[internal functions declaration]=========================*/

/**
 * @brief Servicio de Interrupcion por Hardware (ISR) asociado al flanco de bajada del pin CLK
 * @details Realiza una lectura veloz del estado del pin DT complementario para discriminar el sentido
 * del giro instantaneo, notificando por bitmask a la tarea encargada del filtrado sin demorar el procesador.
 * @param param Parametro de usuario opcional 
 * @return void
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
 * @brief Servicio de Interrupcion (ISR) asociado al Timer A (Se ejecuta cada 1 segundo exacto).
 * @details se ejecuta de forma exavta a una cadencia periodica de 1 segundo. Si el dispositivo
 * se eencuentra en el estado bloqueo, decrementa la base de tiempo y envia una notificacion a la tarea
 * de control principal cuando el conteo llega a cero.
 * @param para Parametro de usuario opcional (recibe NULL)
 * @return void
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
 * @brief Tarea encargada del procesamiento asincrono del encoder rotativo
 * @details Permanece bloqueada en un estado de bajo consumo esperando se;ales de la ISR. Consume los bits
 * de direccion y acumula pasos fisicos. Al alcanzar el umbtal establecido por la macro, impacta de forma
 * segura sobre la variable global de minutos. Incluye retardo para absorber los rebotes mecanicos residuales.
 * @param pvParameter Parametro de configuracion inicial preovisto por FreeRTOS (recibe NULL)
 * @return void
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
 * @brief Tarea principal de control, toma de decisiones y maquina de estados.
 * @details Inicializa todos los perifericos de entrada digitales del encoder, configura sus respectivas interrupciones
 * y gestiona las salidas fisicas *LEDs incorporados y Servomotor PWM). Controla las transiciones criticas al presionar
 * el swich del encoder y procesa de forma ininterrumpible el desblouque final 
 * @param pvParameter Parametro de configuracion inicial provisto por FreeRTOS
 * @return void
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
 * @brief Tarea interfaz de usuario encargada del control del Display de 3 digitos.
 * @details Se ejecuta de forma periodica cada 100ms. Lee las variables del dominio de tiempo segun 
 * el estado perativo activo de la caja de seguridad y las escribe en la pantalla en formato decimal.
 * @param pvParameter Parametro de configuracion inicial provisto por FreeRTOS 
 * @return void
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
/**
 * @brief funcion principal de inicio del firmware (Punto de entrada general).
 * @details Inicializa las funciones de bajo nivel de la placa base (LEDs integrados), define las configuracionees
 * del temporizador peridoico de hardware del sistema y las asocia a sus respectivas rutinas ISR. Registra e inicia
 * el planificador (Scheduler) de las tareas de FreeRTOS con prioridades balanceadas y arranca el conteo fisico.
 * @return void
 */
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