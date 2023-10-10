#include <asf.h>
#include "conf_board.h"

#include "gfx_mono_ug_2832hsweg04.h"
#include "gfx_mono_text.h"
#include "sysfont.h"

#define TASK_OLED_STACK_SIZE (1024 * 6 / sizeof(portSTACK_TYPE))
#define TASK_OLED_STACK_PRIORITY (tskIDLE_PRIORITY)

#define TASK_ALARM_STACK_SIZE (1024 * 6 / sizeof(portSTACK_TYPE))
#define TASK_ALARM_STACK_PRIORITY (tskIDLE_PRIORITY)

/** IOS **/
#define LED_1_PIO PIOA
#define LED_1_PIO_ID ID_PIOA
#define LED_1_IDX 0
#define LED_1_IDX_MASK (1 << LED_1_IDX)

#define LED_2_PIO PIOC
#define LED_2_PIO_ID ID_PIOC
#define LED_2_IDX 30
#define LED_2_IDX_MASK (1 << LED_2_IDX)

#define LED_3_PIO PIOB
#define LED_3_PIO_ID ID_PIOB
#define LED_3_IDX 2
#define LED_3_IDX_MASK (1 << LED_3_IDX)

#define BUT_1_PIO PIOD
#define BUT_1_PIO_ID ID_PIOD
#define BUT_1_IDX 28
#define BUT_1_IDX_MASK (1u << BUT_1_IDX)

#define BUT_2_PIO PIOC
#define BUT_2_PIO_ID ID_PIOC
#define BUT_2_IDX 31
#define BUT_2_IDX_MASK (1u << BUT_2_IDX)

#define BUT_3_PIO PIOA
#define BUT_3_PIO_ID ID_PIOA
#define BUT_3_IDX 19
#define BUT_3_IDX_MASK (1u << BUT_3_IDX)

/** RTOS  */
#define TASK_OLED_STACK_SIZE                (1024*6/sizeof(portSTACK_TYPE))
#define TASK_OLED_STACK_PRIORITY            (tskIDLE_PRIORITY)

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,  signed char *pcTaskName);
extern void vApplicationIdleHook(void);
extern void vApplicationTickHook(void);
extern void vApplicationMallocFailedHook(void);
extern void xPortSysTickHandler(void);

/************************************************************************/
/* prototypes                                                           */
/************************************************************************/
void io_init(void);

typedef struct {
	int id;
	int status;
} btn;

/************************************************************************/
/* RTOS application funcs                                               */
/************************************************************************/

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask, signed char *pcTaskName) {
	printf("stack overflow %x %s\r\n", pxTask, (portCHAR *)pcTaskName);
	for (;;) {	}
}

extern void vApplicationIdleHook(void) { }

extern void vApplicationTickHook(void) { }

extern void vApplicationMallocFailedHook(void) {
	configASSERT( ( volatile void * ) NULL );
}


SemaphoreHandle_t xSemaphoreEventAlarm;
SemaphoreHandle_t xSemaphoreAfecAlarm;
QueueHandle_t xQueueEvent;
QueueHandle_t xQueueAdc;


#define AFEC_POT AFEC0
#define AFEC_POT_ID ID_AFEC0
#define AFEC_POT_CHANNEL 0 // Canal do pino PD30

/************************************************************************/
/* prototypes                                                           */
/************************************************************************/
void io_init(void);
void print_log(char *id, char *value);
static void config_AFEC_pot(Afec *afec, uint32_t afec_id, uint32_t afec_channel,
afec_callback_t callback);
static void RTT_init(float freqPrescale, uint32_t IrqNPulses, uint32_t rttIRQSource);

/************************************************************************/
/* handlers / callbacks                                                 */
/************************************************************************/


void print_log(char *id, char *value) {
	int sec = rtt_read_timer_value(RTT);
	printf("[%s] %d %s \r\n", id, sec, value);
}

void but1_callback(void) {
	 	btn b = {.id = 1, .status = 0};
	 	b.status = !pio_get(BUT_1_PIO, PIO_INPUT, BUT_1_IDX_MASK);
	 	xQueueSendFromISR(xQueueEvent, &b, 0);
}

void but2_callback(void) {
		btn b = {.id = 2, .status = 0};
		b.status = !pio_get(BUT_2_PIO, PIO_INPUT, BUT_2_IDX_MASK);
		xQueueSendFromISR(xQueueEvent, &b, 0);
}

void but3_callback(void) {
	 	btn b = {.id = 3, .status = 0};
	 	b.status = !pio_get(BUT_3_PIO, PIO_INPUT, BUT_3_IDX_MASK);
	 	 	xQueueSendFromISR(xQueueEvent, &b, 0);
}


static void AFEC_pot_Callback(void) {
	 	uint32_t ul_value;
	 	ul_value = afec_channel_get_value(AFEC_POT, AFEC_POT_CHANNEL);
	 	xQueueSendFromISR(xQueueAdc, &ul_value, 0);
}

/************************************************************************/
/* TASKS                                                                */
/************************************************************************/

static void task_afec(void *pvParameters) {
	config_AFEC_pot(AFEC_POT, AFEC_POT_ID, AFEC_POT_CHANNEL, AFEC_pot_Callback);
	uint32_t ul_value;
    RTT_init(1, 0, 0); 
	char str[32];
	int cntTh = 0;

	for (;;) {
		if (xQueueReceive(xQueueAdc, &ul_value, 0)) {
			sprintf(str, "%d", ul_value);
			print_log("AFEC ", str);

			if (ul_value > 3000)
			cntTh++;
			else
			cntTh = 0;
		}

		if (cntTh == 5)
		xSemaphoreGive(xSemaphoreAfecAlarm);
	}
}


static void task_event(void *pvParameters) {
	io_init();
	int btnData[3];
	btn btnStatus;
	char str[32];

	for (int i = 0; i < 3; i++) {
		btnData[i] = 0;
	}

	for (;;) {
		if (xQueueReceive(xQueueEvent, &btnStatus, (TickType_t)200)) {
			sprintf(str, "%d:%d", btnStatus.id, btnStatus.status);
			print_log("EVENT", str);

			btnData[btnStatus.id - 1] = btnStatus.status;
		}

		int cnt = 0;
		for (int i = 0; i < 3; i++) {
			cnt = btnData[i] + cnt;
		}

		if (cnt >= 2)
		xSemaphoreGive(xSemaphoreEventAlarm);
	}
}

static void task_alarm(void *pvParameters) {

	int alarm_afec = 0;
	int alarm_event = 0;

	for (;;) {
		if (xSemaphoreTake(xSemaphoreEventAlarm, 0)) {
			print_log("ALARM", "EVENT");
			gfx_mono_draw_string("EVENT", 0, 0, &sysfont);
		}

		if (xSemaphoreTake(xSemaphoreAfecAlarm, 0)) {
			print_log("ALARM", "AFEC");
			gfx_mono_draw_string("AFEC", 0, 16, &sysfont);
		}

		if (alarm_event)
		pio_clear(LED_1_PIO, LED_1_IDX_MASK);
		if (alarm_afec)
		pio_clear(LED_2_PIO, LED_2_IDX_MASK);
		vTaskDelay(100);
		

		pio_set(LED_1_PIO, LED_1_IDX_MASK);
		pio_set(LED_2_PIO, LED_2_IDX_MASK);
		vTaskDelay(100);
	}
}

static void task_oled(void *pvParameters) {
	gfx_mono_ssd1306_init();

	for (;;)  {
		gfx_mono_draw_filled_circle(12,12, 4, GFX_PIXEL_XOR, GFX_QUADRANT0| GFX_QUADRANT1 | GFX_QUADRANT2 | GFX_QUADRANT3);
		vTaskDelay(200);
	}
}

/************************************************************************/
/* funcoes                                                              */
/************************************************************************/

static void configure_console(void) {
	const usart_serial_options_t uart_serial_options = {
		.baudrate = CONF_UART_BAUDRATE,
		.charlength = CONF_UART_CHAR_LENGTH,
		.paritytype = CONF_UART_PARITY,
		.stopbits = CONF_UART_STOP_BITS,
	};

	/* Configure console UART. */
	stdio_serial_init(CONF_UART, &uart_serial_options);

	/* Specify that stdout should not be buffered. */
	setbuf(stdout, NULL);
}

void io_init(void) {
	pmc_enable_periph_clk(LED_1_PIO_ID);
	pmc_enable_periph_clk(LED_2_PIO_ID);
	pmc_enable_periph_clk(LED_3_PIO_ID);
	pmc_enable_periph_clk(BUT_1_PIO_ID);
	pmc_enable_periph_clk(BUT_2_PIO_ID);
	pmc_enable_periph_clk(BUT_3_PIO_ID);

	pio_configure(LED_1_PIO, PIO_OUTPUT_0, LED_1_IDX_MASK, PIO_DEFAULT);
	pio_configure(LED_2_PIO, PIO_OUTPUT_0, LED_2_IDX_MASK, PIO_DEFAULT);
	pio_configure(LED_3_PIO, PIO_OUTPUT_0, LED_3_IDX_MASK, PIO_DEFAULT);

	pio_configure(BUT_1_PIO, PIO_INPUT, BUT_1_IDX_MASK,
	PIO_PULLUP | PIO_DEBOUNCE);
	pio_configure(BUT_2_PIO, PIO_INPUT, BUT_2_IDX_MASK,
	PIO_PULLUP | PIO_DEBOUNCE);
	pio_configure(BUT_3_PIO, PIO_INPUT, BUT_3_IDX_MASK,
	PIO_PULLUP | PIO_DEBOUNCE);

	pio_handler_set(BUT_1_PIO, BUT_1_PIO_ID, BUT_1_IDX_MASK, PIO_IT_EDGE,
	but1_callback);
	pio_handler_set(BUT_2_PIO, BUT_2_PIO_ID, BUT_2_IDX_MASK, PIO_IT_EDGE,
	but2_callback);
	pio_handler_set(BUT_3_PIO, BUT_3_PIO_ID, BUT_3_IDX_MASK, PIO_IT_EDGE,
	but3_callback);

	pio_enable_interrupt(BUT_1_PIO, BUT_1_IDX_MASK);
	pio_enable_interrupt(BUT_2_PIO, BUT_2_IDX_MASK);
	pio_enable_interrupt(BUT_3_PIO, BUT_3_IDX_MASK);

	pio_get_interrupt_status(BUT_1_PIO);
	pio_get_interrupt_status(BUT_2_PIO);
	pio_get_interrupt_status(BUT_3_PIO);

	NVIC_EnableIRQ(BUT_1_PIO_ID);
	NVIC_SetPriority(BUT_1_PIO_ID, 4);

	NVIC_EnableIRQ(BUT_2_PIO_ID);
	NVIC_SetPriority(BUT_2_PIO_ID, 4);

	NVIC_EnableIRQ(BUT_3_PIO_ID);
	NVIC_SetPriority(BUT_3_PIO_ID, 4);
}

static void config_AFEC_pot(Afec *afec, uint32_t afec_id, uint32_t afec_channel,
afec_callback_t callback) {
	/*************************************
	* Ativa e configura AFEC
	*************************************/
	/* Ativa AFEC - 0 */
	afec_enable(afec);

	/* struct de configuracao do AFEC */
	struct afec_config afec_cfg;

	/* Carrega parametros padrao */
	afec_get_config_defaults(&afec_cfg);

	/* Configura AFEC */
	afec_init(afec, &afec_cfg);

	/* Configura trigger por software */
	afec_set_trigger(afec, AFEC_TRIG_SW);

	/*** Configuracao específica do canal AFEC ***/
	struct afec_ch_config afec_ch_cfg;
	afec_ch_get_config_defaults(&afec_ch_cfg);
	afec_ch_cfg.gain = AFEC_GAINVALUE_0;
	afec_ch_set_config(afec, afec_channel, &afec_ch_cfg);

	/*
	* Calibracao:
	* Because the internal ADC offset is 0x200, it should cancel it and shift
	down to 0.
	*/
	afec_channel_set_analog_offset(afec, afec_channel, 0x200);

	/***  Configura sensor de temperatura ***/
	struct afec_temp_sensor_config afec_temp_sensor_cfg;

	afec_temp_sensor_get_config_defaults(&afec_temp_sensor_cfg);
	afec_temp_sensor_set_config(afec, &afec_temp_sensor_cfg);

	/* configura IRQ */
	afec_set_callback(afec, afec_channel, callback, 1);
	NVIC_SetPriority(afec_id, 4);
	NVIC_EnableIRQ(afec_id);
}

static void RTT_init(float freqPrescale, uint32_t IrqNPulses, uint32_t rttIRQSource) {

	uint16_t pllPreScale = (int) (((float) 32768) / freqPrescale);
	
	rtt_sel_source(RTT, false);
	rtt_init(RTT, pllPreScale);
	
	if (rttIRQSource & RTT_MR_ALMIEN) {
		uint32_t ul_previous_time;
		ul_previous_time = rtt_read_timer_value(RTT);
		while (ul_previous_time == rtt_read_timer_value(RTT));
		rtt_write_alarm_time(RTT, IrqNPulses+ul_previous_time);
	}

	/* config NVIC */
	NVIC_DisableIRQ(RTT_IRQn);
	NVIC_ClearPendingIRQ(RTT_IRQn);
	NVIC_SetPriority(RTT_IRQn, 4);
	NVIC_EnableIRQ(RTT_IRQn);

	/* Enable RTT interrupt */
	if (rttIRQSource & (RTT_MR_RTTINCIEN | RTT_MR_ALMIEN))
	rtt_enable_interrupt(RTT, rttIRQSource);
	else
	rtt_disable_interrupt(RTT, RTT_MR_RTTINCIEN | RTT_MR_ALMIEN);
	
}

/************************************************************************/
/* main                                                                 */
/************************************************************************/


int main(void) {
	/* Initialize the SAM system */
	sysclk_init();
	board_init();
	configure_console();
	io_init();
	
		xSemaphoreEventAlarm = xSemaphoreCreateBinary();
		xSemaphoreAfecAlarm = xSemaphoreCreateBinary();
		xQueueEvent = xQueueCreate(32, sizeof(btn));
		xQueueAdc = xQueueCreate(32, sizeof(int));


if (xTaskCreate(task_alarm, "alarm", TASK_ALARM_STACK_SIZE, NULL,
TASK_ALARM_STACK_PRIORITY, NULL) != pdPASS) {
	printf("Failed to create oled task\r\n");
}

	if (xTaskCreate(task_oled, "oled", TASK_OLED_STACK_SIZE, NULL, TASK_OLED_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create oled task\r\n");
	}
	
	if (xTaskCreate(task_event, "event", TASK_ALARM_STACK_SIZE, NULL,
	TASK_ALARM_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create oled task\r\n");
	}
	
	if (xTaskCreate(task_afec, "afec", TASK_ALARM_STACK_SIZE, NULL,
	TASK_ALARM_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create oled task\r\n");
	}


	vTaskStartScheduler();
	while(1){}

	return 0;
}
