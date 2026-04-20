#ifndef lcd1602a
#define lcd1602a

#define BLINK true
#define NO_BLINK false

uint32_t pin_values_to_mask(uint8_t raw_bits[], uint8_t length);

void uint_into_8bits(uint8_t *raw_bits, uint8_t one_byte);

void send_raw_data_one_cycle(uint8_t raw_bits[]);
    
void send_full_byte(uint8_t rs, uint8_t databits[]);

void lcd_clear();

void cursor_off();

void cursor_on();

void lcd_init() ;

void goto_pos(uint8_t pos, uint8_t line);

void lcd_print(const char * str);
    
void lcd_print_wrapped(const char * str);

void display_off() ;

void display_on() ;
            
#endif