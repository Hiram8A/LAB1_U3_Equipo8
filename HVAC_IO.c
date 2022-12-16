/****************************************
// FileName:        HVAC_IO.c
 // Dependencies:    HVAC.h
 // Processor:       MSP432
 // Board:           MSP432P401R
 // Program version: CCS V12.0.0 TI
 // Company:         TECNM - CAMPUS CHIHUAHUA
 // Description:     Funciones de control de iluminación a través de estados y objetos.
 // Authors:         Hiram Ochoa Sáenz
  *                  Manuel Alejandro Quiroz Gallegos
  *                  Luis Octavio Méndez Valles
 // Updated:         14/12/2022
  ****************************************/

#include "HVAC.h"

//Lecturas de potenciómetros y persianas
char state1[MAX_MSG_SIZE];
char state2[MAX_MSG_SIZE];
char state3[MAX_MSG_SIZE];
char state4[MAX_MSG_SIZE];
char state5[MAX_MSG_SIZE];

/* Definición de botones. */
#define SYS_ON      BSP_BUTTON1
#define LIGHTS_ON   BSP_BUTTON2
#define PERSIANA_1  BSP_BUTTON6
#define PERSIANA_2  BSP_BUTTON7

/* Definición de leds. */
#define OFF_LED     BSP_LED1
#define RED_RGB     BSP_LED2
#define ON_RGB      BSP_LED3
#define BLUE_RGB    BSP_LED4

/* Variables sobre las cuales se maneja el sistema. */
uint32_t DELAY_P1 = 0, DELAY_P2 = 0, ITERATIONS = 0, STATE = 0, i, j;//Retrasos
float Pot_1, Pot_2, Pot_3; //Potenciometros

//Banderas para la ejecución del sistema
bool event = FALSE; //Evento; impresión inmediata
bool sys_flag, sys_ask = TRUE, sys_turn, sys_off, stop;
bool light_flag = FALSE;
bool LIGHT_1, LIGHT_2, LIGHT_3;
bool Per_UD_1 = FALSE, Per_UD_2 = FALSE, ACT_STATE_P1 = FALSE, ACT_STATE_P2 = FALSE, PREV_STATE_P1, PREV_STATE_P2;


/* Archivos sobre los cuales se escribe toda la información */
FILE _PTR_ input_port = NULL, _PTR_ output_port = NULL;                  // Entradas y salidas.
FILE _PTR_ fd_adc = NULL, _PTR_ fd_ch_1 = NULL, _PTR_ fd_ch_2 = NULL, _PTR_ fd_ch_3 = NULL;    // ADC: ch_1 -> Pot1, ch_2 -> Pot2, ch_3 -> Pot3.
FILE _PTR_ fd_uart = NULL;                                               // Comunicación serial asíncrona.

// Estructuras iniciales.

const ADC_INIT_STRUCT adc_init =
{
    ADC_RESOLUTION_DEFAULT,                                                     // Resolución.
    ADC_CLKDiv8                                                                 // División de reloj.
};

const ADC_INIT_CHANNEL_STRUCT adc_ch_param =
{
    POT_1,                                                      // Fuente de lectura, 'ANx'.
    ADC_CHANNEL_MEASURE_LOOP, // Banderas de inicialización
    50000,                                                                       // Periodo en uS, base 1000.
    ADC_TRIGGER_1                                                                // Trigger lógico que puede activar este canal.
};

const ADC_INIT_CHANNEL_STRUCT adc_ch_param2 =
{
    POT_2,                                                                         // Fuente de lectura, 'ANx'.
    ADC_CHANNEL_MEASURE_LOOP,                                                    // Banderas de inicialización.
    50000,                                                                       // Periodo en uS, base 1000.
    ADC_TRIGGER_2                                                                // Trigger lógico que puede activar este canal.
};

const ADC_INIT_CHANNEL_STRUCT adc_ch_param3 =
{
    POT_3,                                                                         // Fuente de lectura, 'ANx'.
    ADC_CHANNEL_MEASURE_LOOP,                                                    // Banderas de inicialización.
    50000,                                                                       // Periodo en uS, base 1000.
    ADC_TRIGGER_3                                                               // Trigger lógico que puede activar este canal.
};

static uint_32 data[] =                                                          // Formato de las entradas.
{                                                                                // Se prefirió un solo formato.
     SYS_ON,
     LIGHTS_ON,
     PERSIANA_1,
     PERSIANA_2,
     GPIO_LIST_END
};

static const uint_32 OFF_SYS_LED[] =                                                    // Formato de los leds, uno por uno.
{
     OFF_LED,
     GPIO_LIST_END
};

static const uint_32 R_RGB[] =                                                   // Formato de los leds, uno por uno.
{
     RED_RGB,
     GPIO_LIST_END
};

const uint_32 ON_SYS_RGB[] =                                                         // Formato de los leds, uno por uno.
{
     ON_RGB,
     GPIO_LIST_END
};

static const uint_32 B_RGB[] =                                                   // Formato de los leds, uno por uno.
{
     BLUE_RGB,
     GPIO_LIST_END
};

/*FUNCTION******************************************************************************
*
* Function Name    : Sys_ON
* Returned Value   : None.
* Comments         :
*    Determina e imprime el estado del sistema.
*
*END***********************************************************************************/
void Sys_ON(void)
{
    if(!sys_ask) //Enciende el sistema
    {
        ioctl(output_port, GPIO_IOCTL_WRITE_LOG0, &OFF_SYS_LED);
        ioctl(output_port, GPIO_IOCTL_WRITE_LOG1, &ON_SYS_RGB);
        printf("\n Sistema: ENCENDIDO \n\r");
        sys_turn = FALSE;
        sys_flag = TRUE;
    }
    else //Pregunta si se presiona mientras esta en ejecución
    {
        stop = TRUE;
        printf("\n SI DESEA TERMINAR LA APLICACIÓN, ENTONCES VUELVA APRESIONAR EL SWITCH \n\r");
        for(i=0;i<500;i++)
        {
            if(sys_off) //Apaga el sistema
            {
                System_Inicio();
            }
            usleep(10000);
        }
        if(stop)
        {
            sys_turn = FALSE;
            sys_ask = FALSE;
            sys_off = TRUE;
        }

    }
}

/**********************************************************************************
 * Function: INT_SWI
 * Preconditions: Interrupción habilitada, registrada e inicialización de módulos.
 * Overview: Función que es llamada cuando se genera
 *           la interrupción del botón SW1, SW2, SW6 o SW7.
 * Input: None.
 * Output: None.
 **********************************************************************************/
void INT_SWI(void)
{
    Int_clear_gpio_flags(input_port);

    ioctl(input_port, GPIO_IOCTL_READ, &data);

    if((data[0] & GPIO_PIN_STATUS) == 0)        // Lectura de los pines, botón 1.1.
    {
        sys_turn = TRUE;
        sys_ask = (!sys_ask);
        sys_off = (!sys_off);
        event=TRUE;
    }

    if((data[1] & GPIO_PIN_STATUS) == 0)        // Lectura de los pines, botón 1.4.
    {
        light_flag = (!light_flag);
        event=TRUE;
    }

    if((data[2] & GPIO_PIN_STATUS) == NORMAL_STATE_EXTRA_BUTTONS)        // Lectura de los pines, botón 2.6.
    {
        Per_UD_1 = (!Per_UD_1);
        ACT_STATE_P1 = Per_UD_1;
        event=TRUE;
    }
    if((data[3] & GPIO_PIN_STATUS) == NORMAL_STATE_EXTRA_BUTTONS)        // Lectura de los pines, botón 2.7.
    {
        Per_UD_2 = (!Per_UD_2);
        ACT_STATE_P2 = Per_UD_2;
        event=TRUE;
    }

    return;
}

/*FUNCTION******************************************************************************
*
* Function Name    : HVAC_InicialiceIO
* Returned Value   : boolean; inicialización correcta.
* Comments         :
*    Abre los archivos e inicializa las configuraciones deseadas de entrada y salida GPIO.
*
*END***********************************************************************************/

boolean HVAC_InicialiceIO(void)
{
    // Estructuras iniciales de entradas y salidas.

    const uint_32 output_set[] =
    {
         OFF_LED   | GPIO_PIN_STATUS_0,
         RED_RGB   | GPIO_PIN_STATUS_0,
         ON_RGB    | GPIO_PIN_STATUS_0,
         BLUE_RGB  | GPIO_PIN_STATUS_0,
         GPIO_LIST_END
    };

    const uint_32 input_set[] =
    {
         SYS_ON,
         LIGHTS_ON,
         PERSIANA_1,
         PERSIANA_2,
         GPIO_LIST_END
    };

    // Iniciando GPIO.
    ////////////////////////////////////////////////////////////////////

    output_port =  fopen_f("gpio:write", (char_ptr) &output_set);
    input_port =   fopen_f("gpio:read", (char_ptr) &input_set);

    if (output_port) { ioctl(output_port, GPIO_IOCTL_WRITE_LOG0, NULL); }   // Inicialmente salidas apagadas.
    ioctl (input_port, GPIO_IOCTL_SET_IRQ_FUNCTION, INT_SWI);               // Declarando interrupción.

    return (input_port != NULL) && (output_port != NULL);
}

/*FUNCTION******************************************************************************
*
* Function Name    : HVAC_InicialiceADC
* Returned Value   : boolean; inicialización correcta.
* Comments         :
*    Abre los archivos e inicializa las configuraciones deseadas para
*    el módulo general ADC y dos de sus canales; uno para la temperatura, otro para
*    el heartbeat.
*
*END***********************************************************************************/
boolean HVAC_InicialiceADC (void)
{

    // Iniciando ADC y canales.
    ////////////////////////////////////////////////////////////////////

    fd_adc   = fopen_f("adc:",  (const char*) &adc_init);               // Módulo.
    fd_ch_1 =  fopen_f("adc:1", (const char*) &adc_ch_param);           // Canal uno, arranca al instante.
    fd_ch_2 =  fopen_f("adc:2", (const char*) &adc_ch_param2);          // Canal dos.
    fd_ch_3 =  fopen_f("adc:3", (const char*) &adc_ch_param3);          // Canal tres.

    return (fd_adc != NULL) && (fd_ch_2 != NULL) && (fd_ch_1 != NULL) && (fd_ch_3 != NULL);  // Valida que se crearon los archivos.
}

/*FUNCTION******************************************************************************
*
* Function Name    : HVAC_InicialiceUART
* Returned Value   : boolean; inicialización correcta.
* Comments         :
*    Abre los archivos e inicializa las configuraciones deseadas para
*    configurar el modulo UART (comunicación asíncrona).
*
*END***********************************************************************************/
boolean HVAC_InicialiceUART (void)
{
    // Estructura inicial de comunicación serie.
    const UART_INIT_STRUCT uart_init =
    {
        /* Selected port */        1,
        /* Selected pins */        {1,2},
        /* Clk source */           SM_CLK,
        /* Baud rate */            BR_115200,

        /* Usa paridad */          NO_PARITY,
        /* Bits protocolo  */      EIGHT_BITS,
        /* Sobremuestreo */        OVERSAMPLE,
        /* Bits de stop */         ONE_STOP_BIT,
        /* Dirección TX */         LSB_FIRST,

        /* Int char's \b */        NO_INTERRUPTION,
        /* Int char's erróneos */  NO_INTERRUPTION
    };

    // Inicialización de archivo.
    fd_uart = fopen_f("uart:", (const char*) &uart_init);

    return (fd_uart != NULL); // Valida que se crearon los archivos.
}

/*FUNCTION******************************************************************************
*
* Function Name    : System_Inicio
* Returned Value   : None.
* Comments         :
*    Imprime e indica el estado inicial del sistema (apagado).
*
*END***********************************************************************************/
void System_Inicio (void){
    ioctl(output_port, GPIO_IOCTL_WRITE_LOG1, &OFF_SYS_LED);
    ioctl(output_port, GPIO_IOCTL_WRITE_LOG0, &R_RGB);
    ioctl(output_port, GPIO_IOCTL_WRITE_LOG0, &ON_SYS_RGB);
    ioctl(output_port, GPIO_IOCTL_WRITE_LOG0, &B_RGB);
    printf("\n SISTEMA: APAGADO \n\r");
    sys_turn = FALSE;
    sys_ask = TRUE;
    sys_off = FALSE;
    sys_flag = FALSE;
    stop = FALSE;
}

/*FUNCTION******************************************************************************
*
* Function Name    : HVAC_Actualizar
* Returned Value   : None.
* Comments         :
*    Función que actualiza la lectura de los potenciometros, asi como las luces.
*
*END***********************************************************************************/
void HVAC_Actualizar(void)
{
   _mqx_int val1 = 0, val2 = 0, val3 = 0;
   boolean flag = TRUE;
   static boolean bandera_inicial = 0;

   if(bandera_inicial == 0)
   {
       // Entrando por primera vez, empieza a correr los canales de los potenciometros.
       ioctl (fd_ch_1, IOCTL_ADC_RUN_CHANNEL, NULL);
       ioctl (fd_ch_2, IOCTL_ADC_RUN_CHANNEL, NULL);
       ioctl (fd_ch_3, IOCTL_ADC_RUN_CHANNEL, NULL);
       bandera_inicial = 1;
   }

   //Valor se guarda en val1, flag nos dice si fue exitoso.
   flag =  (fd_adc && fread_f(fd_ch_1, &val1, sizeof(val1))) ? 1 : 0;

   if(flag != TRUE)
   {
       printf("Error al leer archivo. Cierre del programa\r\n");
       exit(1);
   }

   //Valor se guarda en val2, flag nos dice si fue exitoso.
   flag =  (fd_adc && fread_f(fd_ch_2, &val2, sizeof(val2))) ? 1 : 0;

   if(flag != TRUE)
   {
       printf("Error al leer archivo. Cierre del programa\r\n");
       exit(1);
   }

   //Valor se guarda en val3, flag nos dice si fue exitoso.
   flag =  (fd_adc && fread_f(fd_ch_3, &val3, sizeof(val3))) ? 1 : 0;

   if(flag != TRUE)
   {
       printf("Error al leer archivo. Cierre del programa\r\n");
       exit(1);
   }

   //Conversión de los valores a maximo 10 luxes
   Pot_1 = (val1/ 16383.0*10);
   Pot_2 = (val2/ 16383.0*10);
   Pot_3 = (val3/ 16383.0*10);

   if(light_flag == TRUE)
   { // Enciende o apaga las luces
       LIGHT_1 = TRUE;
       LIGHT_2 = TRUE;
       LIGHT_3 = TRUE;
   } else {
       LIGHT_1 = FALSE;
       LIGHT_2 = FALSE;
       LIGHT_3 = FALSE;
   }

   //Cambia el estado del sistema (ON, OFF, PAUSE)
   if(sys_turn)
       Sys_ON();

    return;
}

void LIGHTS(void)
{
    switch (LIGHT_1)  // Almacena los lumenes de la luz 1 en la cadena state1 de estar encendido, si no, OFF
    {
        case TRUE: sprintf(state1,"LUZ_1= %.0f ", Pot_1);break;
        case FALSE: sprintf(state1,"LUZ_1= OFF ");break;
    }
    switch (LIGHT_2)  // Almacena los lumenes de la luz 2 en la cadena state2 de estar encendido, si no, OFF
    {
        case TRUE: sprintf(state2,"LUZ_2= %.0f ", Pot_2);break;
        case FALSE: sprintf(state2,"LUZ_2= OFF, ");break;
    }
    switch (LIGHT_3)  // Almacena los lumenes de la luz 3 en la cadena state3 de estar encendido, si no, OFF
    {
        case TRUE: sprintf(state3,"LUZ_3= %.0f ", Pot_3);break;
        case FALSE: sprintf(state3,"LUZ_3= OFF, ");break;
    }
}

void PERSIANAS(void)
{
    if(PREV_STATE_P1 != ACT_STATE_P1) //Si ocurre un evento en la persiana 1, activa un contador que funciona como retardo
        DELAY_P1++;

    if(DELAY_P1>0 && DELAY_P1<10) //Almacena el estado cambiante de la persiana 1 durante 5 segundos
    {
        if(Per_UD_1)
            sprintf(state4,"P1: UP ");
        else
            sprintf(state4,"P1: DOWN ");
    }
    else if (Per_UD_1) //Almacena el estado final de la persiana 1
    {
        PREV_STATE_P1 = Per_UD_1;
        DELAY_P1=0;
        sprintf(state4,"P1: OPEN ");
    }
    else
    {
        PREV_STATE_P1 = Per_UD_1;
        DELAY_P1=0;
        sprintf(state4,"P1: CLOSED ");
    }

    if(PREV_STATE_P2 != ACT_STATE_P2) //Si ocurre un evento en la persiana 2, activa un contador que funciona como retardo
        DELAY_P2++;

    if(DELAY_P2>0 && DELAY_P2<10) //Almacena el estado cambiante de la persiana 2 durante 5 segundos
    {
        if(Per_UD_2)
            sprintf(state5,"P2: UP \r\n");
        else
            sprintf(state5,"P2: DOWN \r\n");
    }
    else if (Per_UD_2) //Almacena el estado final de la persiana 2
    {
        PREV_STATE_P2 = Per_UD_2;
        DELAY_P2=0;
        sprintf(state5,"P2: OPEN \r\n");
    }
    else
    {
        PREV_STATE_P2 = Per_UD_2;
        DELAY_P2=0;
        sprintf(state5,"P2: CLOSED \r\n");
    }
}

/*FUNCTION******************************************************************************
*
* Function Name    : HVAC_PrintState
* Returned Value   : None.
* Comments         :
*    Imprime via UART la situación actual del sistema en términos de estados de las luces,
*    lumenes y estados de las persianas.
*    Imprime cada cierto número de iteraciones y justo despues de recibir un cambio
*    en las entradas, produciéndose un inicio en las iteraciones.
*END***********************************************************************************/
void HVAC_PrintState(void)
{
    ITERATIONS++;
    if((ITERATIONS > ITERATIONS_TO_PRINT || event) && sys_flag) // Entra si se cumplen las iteraciones necesarias o
    {                                                           // si ocurre un evento, mientras el sistema esté encendido
        ITERATIONS = 0;
        event = FALSE;
        LIGHTS();
        PERSIANAS();

        //Imprime los estados resultantes
        printf(state1);
        printf(state2);
        printf(state3);
        printf(state4);
        printf(state5);
        usleep(500000);
    }
}


