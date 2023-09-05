
#include "pico/stdlib.h"

#define ADR_INPUTS_START 2
#define DATA_GPIO_START (ADR_INPUTS_START + 10)
#define DATA_GPIO_END (DATA_GPIO_START + 4) 
#define WE_INPUT 22

uint8_t ram[1 << 10] = {};
uint32_t addr_mask = 0; 
uint32_t data_mask = 0; 
uint8_t gpio = 0; 
uint32_t t = 0;
bool written = false; 

const uint LED_PIN = PICO_DEFAULT_LED_PIN;
  
int main() {

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);  

    for (gpio = ADR_INPUTS_START; gpio < DATA_GPIO_START; gpio++) {
        addr_mask |= (1 << gpio);
        gpio_init(gpio);
        gpio_set_function(gpio, GPIO_FUNC_SIO); 
        gpio_set_dir(gpio, GPIO_IN);
        gpio_set_inover(gpio, GPIO_OVERRIDE_NORMAL);
    }

    for (gpio = DATA_GPIO_START; gpio < DATA_GPIO_END; gpio++) {
        data_mask |= (1 << gpio);
        gpio_init(gpio);
        gpio_set_function(gpio, GPIO_FUNC_SIO); 
        gpio_set_dir(gpio, GPIO_OUT);
        gpio_set_outover(gpio, GPIO_OVERRIDE_NORMAL);
    }

    gpio_init(WE_INPUT);
    gpio_set_function(WE_INPUT, GPIO_FUNC_SIO); 
    gpio_set_dir(WE_INPUT, GPIO_IN);
    gpio_pull_up(WE_INPUT); 
    //gpio_set_inover(WE_INPUT, GPIO_OVERRIDE_PULLUP);

    uint32_t adr = 0;
    uint64_t val = 0;
    bool we = false; 
    bool last_we = false; 
    bool first = true; 

    for (adr = 0; adr < (1 << 10); adr++) {
        //ram[adr] = 0xffffffff;
        ram[adr] = 0; 
    } 

    while (true) {  
    
        adr = (gpio_get_all() & addr_mask) >> ADR_INPUTS_START ;
        //adr = gpio_get_all() & addr_mask;
        we = gpio_get(WE_INPUT); 
        
        if (we != last_we || first) {
            first = false; 
            written = false; 
            last_we = we; 
            if (we) {
                // READ SRAM - SET ALL DATA LINES TO OUTPUT (1 IN MASK = OUTPUT)
                gpio_set_dir_masked(data_mask, data_mask); 
            } else {
                gpio_set_dir_masked(data_mask, 0);
                t = time_us_64(); 
            }

            // sleep_us(5);
 
        }   
 
        if ( we) { 
            // READ SRAM 
            val = ram[adr]; 
            gpio_put_masked(data_mask, ~ (val << DATA_GPIO_START)); 
            gpio_put(LED_PIN, 0);
        } else {
            // WRITE SRAM 
            if (! written && ( time_us_64() - t ) > 5) {
                val = gpio_get(12) | ( gpio_get(13) <<  1) | (gpio_get(14) << 2) | (gpio_get(15) << 3); 
                ram[adr] = ~ val; 
                gpio_put(LED_PIN, 1);
                written = true; 
            }
        }
    }

}
 