/*!
 *****************************************************************************
  @file:  ad7124_console_app.c

  @brief: Implementation for the menu functions that handle the AD7124

  @details:
 -----------------------------------------------------------------------------
Copyright (c) 2019 Analog Devices, Inc.  All rights reserved.


/* includes */
#include "FreeRTOSConfig.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "hardware/timer.h"

#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"


#include "ad7124.h"
#include "ad7124_regs.h"
#include "ad7124_support.h"
#include "ad7124_regs_configs.h"

#include "configuration.h"
#include "lcd1602a.h"
#include "ad7124_console_app.h"
#include "math.h"
#include "hardware/gpio.h"
#include "queue.h"

#define AD7124_CHANNEL_COUNT 16

#define SHOW_ALL_CHANNELS     false
#define SHOW_ENABLED_CHANNELS  true

#define DISPLAY_DATA_TABULAR    0
#define DISPLAY_DATA_STREAM     1
#define update_time_timer_ms 	20
#define pulse_gpio 21

#define epsilon 0.001
#define setting_amount 4

QueueHandle_t queue_settings;

/*
 * This is the 'live' AD7124 register map that is used by the driver
 * the other 'default' configs are used to populate this at init time
 */
static struct ad7124_st_reg ad7124_register_map[AD7124_REG_NO];

// Pointer to the struct representing the AD7124 device
static struct ad7124_dev * pAd7124_dev = NULL;

/* The index within the target task's array of task notifications
   to use. */
const UBaseType_t ad7124_xArrayIndex = 1;






/*!
 * @brief      Initialize the AD7124 device and the SPI port as required
 *
 * @details    This resets and then writes the default register map value to
 *  		   the device.
 */
uint32_t ad7124_app_initialize()
{
	memcpy(ad7124_register_map, ad7124_regs_config_a, sizeof(ad7124_register_map));	

	// Used to create the ad7124 device
    struct	ad7124_init_param sAd7124_init =
  	{  		
  		ad7124_register_map,
  		10000,				// Retry count for polling				
  	};
	sAd7124_init.ad7124_xArrayIndex = &ad7124_xArrayIndex;

  	return(ad7124_setup(&pAd7124_dev, sAd7124_init));
	
}

static void spiInit() {
    // Initialize SPI port.    
    spi_init(spi_default, 500 * 1000);

    spi_set_format(spi_default, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);

    gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);    
    gpio_set_function(PICO_DEFAULT_SPI_CSN_PIN, GPIO_FUNC_SPI);    
    gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);    
}

float newvalue_settings[setting_amount] = {0};
float oldvalue_settings[setting_amount] = {-1, -1, -1, -1}; //only to be updated when newvalue is farther away than epsilon

volatile bool onstate = false, prev_on_state = false;

void output_task(void * pvParameters) {
	while(true) {
		uint32_t ulNotificationValue = ulTaskNotifyTakeIndexed(0,
                                                   pdTRUE,
                                                   portMAX_DELAY);		

		static struct str_setting curr_settings_dequeued = {0};
		static uint32_t current_time = 0;	

		if( queue_settings != NULL) {
			xQueueReceive( queue_settings,&(curr_settings_dequeued),( TickType_t ) 0 );
		}

		if(current_time < curr_settings_dequeued.on_time) {
			//machine is on
			onstate = true;
			//calculate cycles for one pulse period:
			if(current_time % curr_settings_dequeued.periond == 0) {
				gpio_put(pulse_gpio, true);
			} else {
				gpio_put(pulse_gpio, false);
			}
			current_time ++;
		} 
		else if (current_time >= curr_settings_dequeued.on_time && current_time < (curr_settings_dequeued.on_time + curr_settings_dequeued.off_time - 1))
		{
			gpio_put(pulse_gpio, false);
			current_time ++;
			onstate = false;
		}
		else 
		{
			onstate = false;
			current_time = 0;
			gpio_put(pulse_gpio, false);
		} 
		
	}
}

void lcd_task(void * pvParameters ) {
	lcd_init();	
	lcd_clear();		
	
	//set prop
	goto_pos(6,0);
	lcd_print("s");
	goto_pos(6,1);
	lcd_print("s");
	goto_pos(13,0);
	lcd_print("ms");
	goto_pos(13,1);
	lcd_print("V");

	while(true) {
		
		uint32_t ulNotificationValue = ulTaskNotifyTakeIndexed(0,
                                                   pdTRUE,
                                                   pdMS_TO_TICKS( 2000 ));
		
		static struct str_setting current_settings; 
		for(size_t setting = 0; setting < setting_amount; setting++) {
			if(fabsf(oldvalue_settings[setting] - newvalue_settings[setting]) > epsilon) {
				oldvalue_settings[setting] = newvalue_settings[setting];
				char stringbuf[6] = {0};
				switch (setting) {
					case 0:
						goto_pos(0, 0); //timer on			
						current_settings.on_time = (uint32_t)(newvalue_settings[setting]*250); //0ms < 2.5v < 12 500ms
						sprintf(stringbuf, "%5.2f", ((float)current_settings.on_time)*0.02);
						break;					
					case 1:
						goto_pos(0, 1); //timer off
						current_settings.off_time = (uint32_t)(newvalue_settings[setting]*250); //0ms < 2.5v < 12 500ms
						sprintf(stringbuf, "%5.2f", ((float)current_settings.off_time)*0.02);
						break;
					case 2:
						goto_pos(8, 0); //period
						uint32_t period_ms = (uint32_t)(39+pow(15.62, (newvalue_settings[setting]))); //20ms < 2.5v < 1000ms
						current_settings.periond = period_ms/20;
						sprintf(stringbuf, "%4d", current_settings.periond * 20);
						break;
					case 3:
						goto_pos(8, 1);
						sprintf(stringbuf, "%4.0f", newvalue_settings[setting]*800); //0 < 2.5v < 2000
						break;
				}				
				
				lcd_print(stringbuf); 
			}
		}
		//update on_state
		
		if(onstate != prev_on_state) {
			prev_on_state = onstate;
			goto_pos(15, 1);
			lcd_print(prev_on_state ? "+" : "-");
		}
		
		
		xQueueSend( queue_settings,( void * ) &current_settings, ( TickType_t ) 0 );
	}
}


TaskHandle_t ad7124_taskhandle, lcd1602a_taskhandle, output_taskhandle;

void ad7124_task(void * pvParameters ) {
	ad7124_app_initialize();	
	do_continuous_conversion(true);
}



void gpio_isr(uint gpio, uint32_t events) {  
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* At this point xTaskToNotify should not be NULL as
       a transmission was in progress. */
    configASSERT( ad7124_taskhandle != NULL );

    /* Notify the task that the transmission is complete. */
	vTaskNotifyGiveIndexedFromISR( ad7124_taskhandle,
                                   ad7124_xArrayIndex,
                                   &xHigherPriorityTaskWoken );

    /* There are no transmissions in progress, so no tasks
       to notify. */
    //ad7124_taskhandle = NULL;

    /* If xHigherPriorityTaskWoken is now set to pdTRUE then a
       context switch should be performed to ensure the interrupt
       returns directly to the highest priority task. The macro used
       for this purpose is dependent on the port in use and may be
       called portEND_SWITCHING_ISR(). */
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );	
}

void set_isr_ad7124_rdy() {
   	gpio_init(d_out_rdy_pin);
	gpio_set_irq_callback(&gpio_isr);
   	// gpio_set_irq_enabled(d_out_rdy_pin, GPIO_IRQ_EDGE_FALL, true);
	irq_set_enabled(IO_IRQ_BANK0, true);

   //gpio_set_irq_enabled_with_callback(d_out_rdy_pin, GPIO_IRQ_EDGE_FALL, true, &gpio_isr);
}




bool repeating_timer_callback(__unused struct repeating_timer *t) {
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    configASSERT( output_taskhandle != NULL );

	vTaskNotifyGiveIndexedFromISR( output_taskhandle,
                                   0,
                                   &xHigherPriorityTaskWoken );
    
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );	
}


struct repeating_timer timer;        


int main() {

	
	queue_settings = xQueueCreate(3, sizeof(xMessage) );
	if( queue_settings == NULL )
    {
        printf("queue cannot be created\n");
		vTaskDelay(portMAX_DELAY);
    }
    

	gpio_init(pulse_gpio);
    gpio_set_dir(pulse_gpio, GPIO_OUT);  
	add_repeating_timer_ms(update_time_timer_ms, repeating_timer_callback, NULL, &timer);          
	stdio_init_all();    	
	spiInit();	
	set_isr_ad7124_rdy();

	BaseType_t ad7124_task_returned, lcd1602_task_returned;
	
	lcd1602_task_returned = xTaskCreate(lcd_task, "lcd1602a", 128, ( void * ) 1, tskIDLE_PRIORITY, &lcd1602a_taskhandle );
	ad7124_task_returned = xTaskCreate(ad7124_task, "ad7124_task", 512, ( void * ) 1, tskIDLE_PRIORITY, &ad7124_taskhandle );
	ad7124_task_returned = xTaskCreate(output_task, "ad7124_task", 128, ( void * ) 1, tskIDLE_PRIORITY, &output_taskhandle );
	vTaskStartScheduler();
	
	return 0;
}

// Private Functions

/*!
 * @brief      reads and displays the status register on the AD7124
 *
 * @details
 */
static void read_status_register(void)
{
	if (ad7124_read_register(pAd7124_dev, &pAd7124_dev->regs[AD7124_Status]) < 0) {
	   printf("\r\nError Encountered reading Status register\r\n");
	} else {
	    uint32_t status_value = (uint32_t)pAd7124_dev->regs[AD7124_Status].value;
        printf("\r\nRead Status Register = 0x%02lx\r\n", status_value);
	}
}

static int32_t set_idle_mode() {
	int32_t error_code = 0;
	pAd7124_dev->regs[AD7124_ADC_Control].value &= ~(AD7124_ADC_CTRL_REG_MODE(0xf)); //clear mode bits	
	pAd7124_dev->regs[AD7124_ADC_Control].value |= AD7124_ADC_CTRL_REG_MODE(4); //idle mode
	if ( (error_code = ad7124_write_register(pAd7124_dev, pAd7124_dev->regs[AD7124_ADC_Control]) ) < 0) {
		printf("Error (%ld) setting AD7124 power mode to low.\r\n", error_code);		
		return error_code;
	} else {
		printf("idle mode activated\n");
	}
}


/*!
 * @brief      Continuously acquires samples in Continuous Conversion mode
 *
 * @details   The ADC is run in continuous mode, and all samples are acquired
 *            and assigned to the channel they come from. Escape key an be used
 *            to exit the loop
 */
static int32_t do_continuous_conversion()
{
	static uint32_t toZeroValue = 0;

	int32_t error_code;
	int32_t sample_data;
	uint32_t portvalues;
	

	
	//select continuous convertion mode, all zero
	pAd7124_dev->regs[AD7124_ADC_Control].value &= ~(AD7124_ADC_CTRL_REG_MODE(0xf));
	
	//select high power	| CONTINUOUS read
	pAd7124_dev->regs[AD7124_ADC_Control].value |= AD7124_ADC_CTRL_REG_POWER_MODE(0b11) ;//| AD7124_ADC_CTRL_REG_CONT_READ;		

	if ((error_code = ad7124_write_register(pAd7124_dev, pAd7124_dev->regs[AD7124_ADC_Control]) < 0)) {
		printf("Error (%ld) setting AD7124 Continuous conversion mode.\r\n", error_code);		
		return -1;
	}

	uint8_t channel_read = 0, array_channel =0;							
	// Continuously read the channels, and store sample values
    while (true) {  			
		/*
		*  this polls the status register READY/ bit to determine when conversion is done
		*  this also ensures the STATUS register value is up to date and contains the
		*  channel that was sampled as well.
		*  Generally, no need to read STATUS separately, but for faster sampling
		*  enabling the DATA_STATUS bit means that status is appended to ADC data read
		*  so the channel being sampled is read back (and updated) as part of the same frame
		*/		
		
		
		if ( (error_code = ad7124_wait_for_conv_ready(pAd7124_dev, 100)) < 0) {
			printf("Error/Timeout waiting for conversion ready %ld\r\n", error_code);
			continue;
		}
	
		
		channel_read = pAd7124_dev->regs[AD7124_Status].value & 0x0000000F;
		if(pAd7124_dev->regs[AD7124_Channel_0 + channel_read].value & AD7124_CH_MAP_REG_CH_ENABLE) {
				

			if ( (error_code = ad7124_read_data(pAd7124_dev, &sample_data)) < 0) {
				printf("Error reading ADC Data (%ld).\r\n", error_code);
				continue;
			}
			
			if (channel_read == 0) {	
								
				printf("\n%09d, ", to_ms_since_boot(get_absolute_time()) - toZeroValue);	
				array_channel = 0;										        					
								
			} else {				
				printf(", ");												
			}
						
			xTaskNotifyGive(lcd1602a_taskhandle);  
			newvalue_settings[array_channel] = ad7124_convert_sample_to_voltage(pAd7124_dev, channel_read, sample_data);
			array_channel++;
			//update new values to array
			//printf("%.8f\n", ad7124_convert_sample_to_voltage(pAd7124_dev, channel_read, sample_data) );			
		}
	}	
}



static int32_t set_zero_scale_calibration() {
	// 5 = system zero scale calibration
	int32_t error_code = 0;
	pAd7124_dev->regs[AD7124_ADC_Control].value &= ~(AD7124_ADC_CTRL_REG_MODE(0xf) | AD7124_ADC_CTRL_REG_POWER_MODE(0x3));
	pAd7124_dev->regs[AD7124_ADC_Control].value |= AD7124_ADC_CTRL_REG_MODE(0b0111) | AD7124_ADC_CTRL_REG_POWER_MODE(0x01);
	if ( (error_code = ad7124_write_register(pAd7124_dev, pAd7124_dev->regs[AD7124_ADC_Control]) ) < 0) {
		printf("Error (%ld) setting AD7124 ADC into zero scale calibration.\r\n", error_code);		
		return error_code;
	} else {
		printf("zero scale calibration completed\n");
	}
}

static int32_t set_full_scale_calibration() {
	// 6 = system full scale calibration
	int32_t error_code = 0;
	pAd7124_dev->regs[AD7124_ADC_Control].value &= ~(AD7124_ADC_CTRL_REG_MODE(0xf) | AD7124_ADC_CTRL_REG_POWER_MODE(0x3) );
	pAd7124_dev->regs[AD7124_ADC_Control].value |= AD7124_ADC_CTRL_REG_MODE(0b1000) | AD7124_ADC_CTRL_REG_POWER_MODE(0x01);
	if ( (error_code = ad7124_write_register(pAd7124_dev, pAd7124_dev->regs[AD7124_ADC_Control]) ) < 0) {
		printf("Error (%ld) setting AD7124 ADC into internal full scale calibration.\r\n", error_code);		
		return error_code;
	} else {
		printf("full scale calibration completed\n");
	}
}

static int32_t read_error() {
	int32_t error_code;
	error_code = ad7124_read_register( pAd7124_dev, &pAd7124_dev->regs[AD7124_Error]);
		if(error_code < 0) {
			printf("error reading errorcode (%ld)", error_code);
			return error_code;
		} else {
			printf("error reg: %i\n", pAd7124_dev->regs[AD7124_Error].value);
		}
}



static int32_t set_slow_filters(bool enable) {
	//set high filter for calibration
	int32_t error_code = 0;
	enum ad7124_registers reg_nr;	
	
	for(reg_nr = AD7124_Filter_0; (reg_nr < AD7124_Offset_0) && !(error_code < 0); reg_nr++) {
		if(enable) {									
			struct ad7124_st_reg reg;
			reg=pAd7124_dev->regs[reg_nr];
			reg.value = AD7124_FILT_REG_FS(1024) | AD7124_FILT_REG_REJ60 | AD7124_FILT_REG_POST_FILTER(0b110); //cannot use higher than 1024, don't know why...
			error_code = ad7124_write_register(pAd7124_dev, reg);			
			//printf("set slow filters: %d\n", reg.value);			
		} else {
			// struct ad7124_st_reg reg;
			// reg=pAd7124_dev->regs[reg_nr];
			// reg.value = 0;
			// error_code = ad7124_write_register(pAd7124_dev, reg); //zero all?
			error_code = ad7124_write_register(pAd7124_dev, pAd7124_dev->regs[reg_nr]); //original value
			//printf("set original filters: %d\n", pAd7124_dev->regs[reg_nr].value);			
		}						
		if (error_code < 0) break;		
	}			
}

static uint32_t switch_channel(bool enable, enum ad7124_registers channel) {
	
	if(enable) {
	 	pAd7124_dev->regs[channel].value |= AD7124_CH_MAP_REG_CH_ENABLE;
	} else {
		pAd7124_dev->regs[channel].value &= ~(AD7124_CH_MAP_REG_CH_ENABLE);
	}
	
	int32_t error_code = 0;
	error_code |= ad7124_write_register(pAd7124_dev, pAd7124_dev->regs[channel]);										
	printf("%s channel %i\n", enable ? "enabled" : "disabled", channel);			
	
	return error_code;
}

static int32_t do_fullscale_calibration() {	
	int32_t error_code = 0;	
	int32_t enabled_channels = 0;

	for (enum ad7124_registers i = AD7124_Offset_0; i < AD7124_Gain_0; i++)
	{
		//write zero in offset register of each configuration 
		pAd7124_dev->regs[i].value = 0x800000;
		if ( (error_code = ad7124_write_register(pAd7124_dev, pAd7124_dev->regs[i]) ) < 0) {
			printf("Error (%ld) writing offset for setup 0.\r\n", error_code);
			return error_code;			
		}
	}

	for (uint8_t i = 0; i < AD7124_CHANNEL_COUNT; i++) {		
		if (pAd7124_dev->regs[AD7124_Channel_0 + i].value & AD7124_CH_MAP_REG_CH_ENABLE) {
			enabled_channels += (1 << i);	
			error_code |= switch_channel(false, AD7124_Channel_0 + i);
		}	
	}	
	
	error_code |= set_slow_filters(true);

	//loop channels for calibration
	for (uint8_t i = 0; i < AD7124_CHANNEL_COUNT; i++) { 
		if(enabled_channels & (1<<i)) {
			//enable for calibration
			switch_channel(true, AD7124_Channel_0 +i);
						
			//full scale must be done before zero scale calibration
			// set_full_scale_calibration();
			// ad7124_wait_for_conv_ready(pAd7124_dev, 10000);
			
			set_zero_scale_calibration();											
			ad7124_wait_for_conv_ready(pAd7124_dev, 10000);
						
			switch_channel(false, AD7124_Channel_0 +i);
		}
	}

	error_code |= set_slow_filters(false);

	for (uint8_t i = 0; i < AD7124_CHANNEL_COUNT; i++) {  
		if(enabled_channels & (1<<i)) {
			error_code |= switch_channel(true, AD7124_Channel_0 + i);
		}
	}
	
	error_code = set_idle_mode();
	if(error_code < 0) {		
		printf("error fullscale calibraion");
	}
}


/*!
 * @brief      reads the ID register on the AD7124
 *
 * @details
 */
static int32_t menu_read_id(void)
{
	if (ad7124_read_register(pAd7124_dev, &pAd7124_dev->regs[AD7124_ID]) < 0) {
	   printf("\r\nError Encountered reading ID register\r\n");
	} else {
	   printf("\r\nRead ID Register = 0x%02lx\r\n",
			   (uint32_t)pAd7124_dev->regs[AD7124_ID].value );
	}
	return 0;
}