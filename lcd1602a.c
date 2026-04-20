#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "lcd1602a.h"
#include "FreeRTOS.h"
#include "task.h"


#define RS 4
#define E 5
// Pin values
#define HIGH 1
#define LOW 0
// LCD pin RS meaning
#define COMMAND 0
#define DATA 1


uint8_t no_chars = 16;
uint8_t no_lines = 2;
uint8_t LCDpins[8] = {5,4,3,2,8,6};
uint8_t cursor_status[2] = {0};

uint32_t LCDmask_c = 0 ; // with clock
uint32_t LCDmask = 0 ; //without clock
    
void uint_into_8bits(uint8_t *raw_bits, uint8_t one_byte) {  	
    for (size_t i = 0 ; i < 8 ; i++ ) {
        raw_bits[7-i] = one_byte % 2 ;
        one_byte = one_byte >> 1 ;
    }
};


void send_raw_data_one_cycle(uint8_t raw_bits[]) { // Array of Bit 7, Bit 6, Bit 5, Bit 4, RS
    uint32_t bit_value = pin_values_to_mask(raw_bits,5) ;
    gpio_put_masked(LCDmask, bit_value) ;
    gpio_put(LCDpins[E], HIGH) ;    
    vTaskDelay(pdMS_TO_TICKS(5));
    gpio_put(LCDpins[E], LOW) ; // gpio values on other pins are pushed at the HIGH->LOW change of the clock. 
    vTaskDelay(pdMS_TO_TICKS(5));
};
    
void send_full_byte(uint8_t rs, uint8_t databits[]) { // RS + array of Bit7, ... , Bit0
    // send upper nibble (MSN)
    uint8_t rawbits[5];
    rawbits[4] = rs ;
    for (size_t i = 0 ; i<4 ; i++) { rawbits[i]=databits[i];}
    send_raw_data_one_cycle(rawbits);
    // send lower nibble (LSN)
    for (size_t i = 0; i<4 ; i++) { rawbits[i]=databits[i+4];}
    send_raw_data_one_cycle(rawbits);
};

void lcd_clear() {
    uint8_t clear_display[8] = {0,0,0,0,0,0,0,1};
    send_full_byte(COMMAND, clear_display);
    // extra sleep due to equipment time needed to clear the display
    vTaskDelay(pdMS_TO_TICKS(10));
};

    
void cursor_off() {
    uint8_t no_cursor[8] = {0,0,0,0,1,1,0,0};
    send_full_byte(COMMAND, no_cursor);
    cursor_status[0] = 0;
    cursor_status[1] = 0;
};

void cursor_on() {
    uint8_t command_cursor[8] = {0,0,0,0,1,1,1,1};
    send_full_byte(COMMAND, command_cursor);
    cursor_status[0] = 1;
    cursor_status[1] = 1;
};	
    
void display_off() {
    uint8_t command_display[8] = {0,0,0,0,1,0,0,0};
    command_display[7] = cursor_status[1];
    command_display[6] = cursor_status[0];		
    send_full_byte(COMMAND, command_display);
};

void display_on() {
    uint8_t command_display[8] = {0,0,0,0,1,1,0,0};
    command_display[7] = cursor_status[1];
    command_display[6] = cursor_status[0];		
    send_full_byte(COMMAND, command_display);
};

void lcd_init() { // initialize the LCD

    uint8_t all_ones[6] = {1,1,1,1,1,1};
    uint8_t set_function_8[5] = {0,0,1,1,0};
    uint8_t set_function_4a[5] = {0,0,1,0,0};
    
    uint8_t set_function_4[8] = {0,0,1,0,0,0,0,0};
    uint8_t cursor_set[8] = {0,0,0,0,0,1,1,0};
    uint8_t display_prop_set[8] = {0,0,0,0,1,1,0,0};
    
    //set mask, initialize masked pins and set to LOW 
    LCDmask_c = pin_values_to_mask(all_ones,6);
    LCDmask = pin_values_to_mask(all_ones,5);
    gpio_init_mask(LCDmask_c);   			// init all LCDpins
    gpio_set_dir_out_masked(LCDmask_c);	// Set as output all LCDpins
    gpio_clr_mask(LCDmask_c);				// LOW on all LCD pins 
    
    //set LCD to 4-bit mode and 1 or 2 lines
    //by sending a series of Set Function commands to secure the state and set to 4 bits
    if (no_lines == 2) { set_function_4[4] = 1; };
    send_raw_data_one_cycle(set_function_8);
    send_raw_data_one_cycle(set_function_8);
    send_raw_data_one_cycle(set_function_8);
    send_raw_data_one_cycle(set_function_4a);
    
    //getting ready
    send_full_byte(COMMAND, set_function_4);
    send_full_byte(COMMAND, cursor_set);
    send_full_byte(COMMAND, display_prop_set);
    lcd_clear() ;    
        
    cursor_status[0] = 0;
    cursor_status[1] = 0;
};

void goto_pos(uint8_t pos_i, uint8_t line) {
    uint8_t eight_bits[8];
    uint8_t pos = (uint8_t)pos_i;
    switch (no_lines) {
        case 2: 
            pos = 64*line+ pos + 0b10000000; 
            break ;
        case 4: 	
            if (line == 0 || line == 2) {
                pos = 64*(line/2) + pos + 0b10000000;
            } else {
                pos = 64*((line-1)/2) + 20 + pos + 0b10000000;
            };
            break;
        default:
            pos = pos ;
    };
    uint_into_8bits(eight_bits,pos);
    send_full_byte(COMMAND,eight_bits);
};

void lcd_print(const char * str) {
    uint8_t eight_bits[8];
    size_t i = 0 ;
    while (str[i] != 0) {
        uint_into_8bits(eight_bits,(uint8_t)(str[i]));
        send_full_byte(DATA, eight_bits);
        ++i;
    }
};
    
void lcd_print_wrapped(const char * str) {
    uint8_t eight_bits[8];
    size_t i = 0 ;
    
    goto_pos(0,0);

    while (str[i] != 0) {
        uint_into_8bits(eight_bits,(uint8_t)(str[i]));
        send_full_byte(DATA, eight_bits);
        ++i;
        if (i%no_chars == 0) { goto_pos(0,i/no_chars); }
    }
};
            
uint32_t pin_values_to_mask(uint8_t raw_bits[], uint8_t length) {   // Array of Bit 7, Bit 6, Bit 5, Bit 4, RS(, clock)
    uint32_t result = 0 ;
    uint8_t pinArray[32] ;
    for (size_t i = 0 ; i < 32; i++) {pinArray[i] = 0;}
    for (size_t i = 0 ; i < length ; i++) {pinArray[LCDpins[i]]= raw_bits[i];}
    for (size_t i = 0 ; i < 32; i++) {
        result = result << 1 ;
        result += pinArray[31-i] ;
    }
    return result ;
};
