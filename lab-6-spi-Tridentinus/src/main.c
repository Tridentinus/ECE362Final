/**
  ******************************************************************************
  * @file    main.c
  * @author  Weili An, Niraj Menon
  * @date    Feb 3, 2024
  * @brief   ECE 362 Lab 6 Student template
  ******************************************************************************
*/

/*******************************************************************************/

// Fill out your username, otherwise your completion code will have the 
// wrong username!
const char* username = "seaman7";
volatile float current_temp;

# define DHT11_PORT GPIOA
/*******************************************************************************/ 
#include "stm32f0xx.h"
#include <ctype.h>

void set_char_msg(int, char);
void nano_wait(unsigned int);
void game(void);
void internal_clock();
void check_wiring();
void autotest();

//===========================================================================
// extern void print(const char str[]);
//===========================================================================
void enable_ports(void) {
    // Only enable port C for the keypad
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
    GPIOC->MODER &= ~0xffff;
    GPIOC->MODER |= 0x55 << (4*2);
    GPIOC->OTYPER &= ~0xff;
    GPIOC->OTYPER |= 0xf0;
    GPIOC->PUPDR &= ~0xff;
    GPIOC->PUPDR |= 0x55;
}
void configure_pb7(void) {
    // Enable the GPIOB peripheral
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;

    // Set the mode to output for PB7
    GPIOB->MODER &= ~(3 << (7 * 2)); // Clear mode bits
    GPIOB->MODER |= (1 << (7 * 2));  // Set mode to output (01)

    // Set the output type to push-pull for PB7
    GPIOB->OTYPER &= ~(1 << 7);

    // Set no pull-up or pull-down for PB7
    GPIOB->PUPDR &= ~(3 << (7 * 2));

    // Set the output speed to low for PB7
    GPIOB->OSPEEDR &= ~(3 << (7 * 2));

    // Set PB7 to low
}
void Heater_On(void) {
    GPIOB->BSRR = (1 << 7);
}
void Heater_Off(void) {
    GPIOB->BRR = (1 << 7);
}   
void setup_tim3(void) {
    // Enable clock for GPIOC
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;

    // Enable clock for TIM3
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    // Set PC8, PC9 to alternate function mode
    GPIOC->MODER &= ~0xF0000; // Clear mode bits for PC8-PC9
    GPIOC->MODER |= 0xA0000;  // Set mode bits to alternate function

    GPIOC->AFR[1] &= ~0x000000FF; // Clear alternate function bits for PC8-PC9

    // Set TIM3 prescaler to divide by 48000 (48MHz / 48000 = 1kHz)
    TIM3->PSC = 18;

    // Set TIM3 auto-reload register to 999 (1kHz / 1000 = 1Hz)
    TIM3->ARR = 99;

    // Set TIM3 CCMR2 to PWM mode 1 for channels 3 and 4
    TIM3->CCMR2 |= TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2;
    TIM3->CCMR2 |= TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2;

    // Enable the outputs for channels 3 and 4
    TIM3->CCER |= TIM_CCER_CC3E | TIM_CCER_CC4E;

    // Set initial duty cycle for channels 3 and 4
    TIM3->CCR3 = 0;
    TIM3->CCR4 = 0;

    // Enable Timer 3
    TIM3->CR1 |= TIM_CR1_CEN;
}
void set_fan_speed(int speed) {
    TIM3->CCR3 = speed;
}
uint8_t col; // the column being scanned

void drive_column(int);   // energize one of the column outputs
int  read_rows();         // read the four row inputs
void update_history(int col, int rows); // record the buttons of the driven column
char get_key_event(void); // wait for a button event (press or release)
char get_keypress(void);  // wait for only a button press event.
float getfloat(void);     // read a floating-point number from keypad
void show_keys(void);     // demonstrate get_key_event()

//===========================================================================
// Bit Bang SPI LED Array
//===========================================================================
int msg_index = 0;
uint16_t msg[8] = { 0x0000,0x0100,0x0200,0x0300,0x0400,0x0500,0x0600,0x0700 };
extern const char font[];

//===========================================================================
// Configure PB12 (CS), PB13 (SCK), and PB15 (SDI) for outputs
//===========================================================================
void setup_bb(void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
   
    GPIOB->MODER &= ~(0x00ffffff);

    GPIOB->MODER |= (0x00000045 << 24);

    GPIOB->BSRR = (1 << (12));
    GPIOB->BSRR = (1 << (13 + 16));
    // GPIOB->BSRR = (1 << (15));
}

void small_delay(void) {
    nano_wait(50000);
}

//===========================================================================
// Set the MOSI bit, then set the clock high and low.
// Pause between doing these steps with small_delay().
//===========================================================================
void bb_write_bit(int val) {
    // CS (PB12)
    // SCK (PB13)
    // SDI (PB15)
   if (val) {
        GPIOB->BSRR = (1 << (15));
    } else {
        GPIOB->BSRR = (1 << (15 + 16));
    }

    small_delay();
   
    GPIOB->BSRR = (1 << (13));
   
    small_delay();

    GPIOB->BSRR = (1 << (13 + 16));
}

//===========================================================================
// Set CS (PB12) low,
// write 16 bits using bb_write_bit,
// then set CS high.
//===========================================================================
void bb_write_halfword(int halfword) {
    GPIOB->BSRR = (1 << (12 + 16));

    for (int i = 15; i >= 0; i--) {
        if (halfword >> i & 0x00000001) {
            bb_write_bit(1);
        } else {
            bb_write_bit(0);
        }
    }

    GPIOB->BSRR = (1 << (12));
}

//===========================================================================
// Continually bitbang the msg[] array.
//===========================================================================
void drive_bb(void) {
    for(;;)
        for(int d=0; d<8; d++) {
            bb_write_halfword(msg[d]);
            nano_wait(1000000); // wait 1 ms between digits
        }
}

uint8_t DHT11_Start (void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;

    DHT11_PORT->MODER &= ~(3 << (6 * 2));
    DHT11_PORT->MODER |= (1 << (6 * 2));

    DHT11_PORT->OTYPER |= (1 << 6);

    DHT11_PORT->PUPDR &= ~(3 << (6 * 2));

    DHT11_PORT->BRR = (1 << 6);

    nano_wait(18000000);

    DHT11_PORT->MODER &= ~(3 << (6 * 2));

}

uint8_t DHT22_Start (void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;

    DHT11_PORT->MODER &= ~(3 << (6 * 2));
    DHT11_PORT->MODER |= (1 << (6 * 2));

    DHT11_PORT->OTYPER |= (1 << 6);

    DHT11_PORT->PUPDR &= ~(3 << (6 * 2));

    DHT11_PORT->BRR = (1 << 6);

    nano_wait(12000000);

    DHT11_PORT->BSRR = (1 << 6);
    nano_wait(20000);

    DHT11_PORT->MODER &= ~(3 << (6 * 2));

}

uint8_t DHT11_Check_Response (void) {
    uint8_t Response = 0;
    nano_wait(40000);

    if (!(DHT11_PORT->IDR & (1 << 6))) {
        nano_wait(80000);
        if ((DHT11_PORT->IDR & (1 << 6))) {
            print("1");
            Response = 1;
        }
        else {
            print("-1");
            Response = -1;
        }
    }
    while(DHT11_PORT->IDR & (1<<6));
    return Response;   
}
uint8_t DHT11_Read(void) {
    uint8_t i, j;
    uint8_t result = 0;
    for (j = 0; j < 8; j++) {
        while (!(DHT11_PORT->IDR & (1<<6)));
        
        nano_wait(40000);
        
        if (!(DHT11_PORT->IDR & (1<<6))) {
            result &= ~(1 << (7 - j));  
        } else {
            result |= (1 << (7 - j));  
        }
        
        while (DHT11_PORT->IDR & (1<<6));
    }
    return result;
}

void init_tim15(void) {
    RCC->APB2ENR |= RCC_APB2ENR_TIM15EN;
    TIM15->PSC = (2400 - 1);
    TIM15->ARR = (20-1);
    TIM15->DIER |= TIM_DIER_UDE;
    TIM15->CR1 |= 0x00000001;
}


void init_tim7(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM7EN;
    TIM7->PSC = 48 - 1;
    TIM7->ARR = 1000 - 1;
    TIM7->DIER |= TIM_DIER_UIE;
    NVIC_EnableIRQ(TIM7_IRQn);
    TIM7->CR1 |= TIM_CR1_CEN;
}


void TIM7_IRQHandler() {
    TIM7->SR &= ~TIM_SR_UIF;
    uint8_t rows = read_rows();
    update_history(col, rows);
    col = (col + 1) & 3;
    drive_column(col);
}


void init_spi2(void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;

    GPIOB->MODER &= ~(0xcf000000);
    GPIOB->MODER |= (0x0000008a << 24);

    GPIOB->AFR[0] &= ~(0xf0ff << 16);

    SPI2->CR1 &= ~(SPI_CR1_SPE);
   
    SPI2->CR1 |= (0x00000007 << 3);
   
    SPI2->CR2 = (0x0000000f << 8);

    SPI2->CR1 |= 0x00000004;

    SPI2->CR2 |= 0x00000004;
    SPI2->CR2 |= 0x00000008;

    SPI2->CR2 |= 0x00000002;
   
    SPI2->CR1 |= 0x00000040;
}


void spi2_setup_dma(void) { 
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    DMA1_Channel5->CMAR = (uint32_t) &msg;
    DMA1_Channel5->CPAR = (uint32_t) &SPI2->DR;
    DMA1_Channel5->CNDTR = 8;
    DMA1_Channel5->CCR |= DMA_CCR_DIR;
    DMA1_Channel5->CCR |= DMA_CCR_MINC;
    DMA1_Channel5->CCR |= 0x00000400;
    DMA1_Channel5->CCR |= 0x00000100;
    DMA1_Channel5->CCR |= 0x00000020;
    SPI2->CR2 |= 0x00000002;

}


void spi2_enable_dma(void) {
    DMA1_Channel5->CCR |= 0x00000001;
}

void init_spi1() {
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    GPIOA->MODER &= ~(0xc000cc00);
    GPIOA->MODER |= (0x80008800);

    GPIOA->AFR[0] &= ~(0x0f0f << 20);
    GPIOA->AFR[1] &= ~(0x000f << 28);

    SPI1->CR1 &= ~(0x00000040);

    SPI1->CR1 |= (0x00000007 << 3);

    SPI1->CR2 &= ~(0x00000006 << 8);
    SPI1->CR2 = (0x00000009 << 8);

    SPI1->CR1 |= 0x00000004;

    SPI1->CR2 |= 0x00000002;

    SPI1->CR2 |= 0x00000004;
    SPI1->CR2 |= 0x00000008;

    SPI1->CR1 |= 0x00000040;
}
void spi_cmd(unsigned int data) {
    while(!(SPI1->SR & (1 << 1))) {}
   
    SPI1->DR = data;
}
void spi_data(unsigned int data) {
    spi_cmd(data | 0x200);
}
void spi1_init_oled() {
    nano_wait(1000000);
    spi_cmd(0x38);
    spi_cmd(0x08);
    spi_cmd(0x01);

    nano_wait(2000000);
    spi_cmd(0x06);
    spi_cmd(0x02);
    spi_cmd(0x0c);
}
void spi1_display1(const char *string) {
    spi_cmd(0x02);

    int curr = 0;
    while (string[curr] != '\0') {
        spi_data(string[curr]);
        curr++;
    }

}
void spi1_display2(const char *string) {
    spi_cmd(0xc0);

    int curr = 0;
    while (string[curr] != '\0') {
        spi_data(string[curr]);
        curr++;
    }
}

uint16_t display[34] = {
        0x002, 
        0x200+'E', 0x200+'C', 0x200+'E', 0x200+'3', 0x200+'6', + 0x200+'2', 0x200+' ', 0x200+'i',
        0x200+'s', 0x200+' ', 0x200+'t', 0x200+'h', + 0x200+'e', 0x200+' ', 0x200+' ', 0x200+' ',
        0x0c0, 
        0x200+'c', 0x200+'l', 0x200+'a', 0x200+'s', 0x200+'s', + 0x200+' ', 0x200+'f', 0x200+'o',
        0x200+'r', 0x200+' ', 0x200+'y', 0x200+'o', + 0x200+'u', 0x200+'!', 0x200+' ', 0x200+' ',
};


void spi1_setup_dma(void) {
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    DMA1_Channel3->CCR &= ~DMA_CCR_EN;

    DMA1_Channel3->CMAR = (uint32_t)&display;

    DMA1_Channel3->CPAR = (uint32_t)&(SPI1->DR);

    DMA1_Channel3->CNDTR = 34;
    DMA1_Channel3->CCR |= DMA_CCR_DIR;
    DMA1_Channel3->CCR |= DMA_CCR_MINC;


    TIM6->PSC = 23999;   
    TIM6->ARR = 999;    

    TIM6->DIER |= TIM_DIER_UIE;

    NVIC->ISER[0] = 1 << TIM6_DAC_IRQn;

    DMA1_Channel3->CCR &= ~DMA_CCR_PSIZE;
    DMA1_Channel3->CCR |= DMA_CCR_PSIZE_0; 

    DMA1_Channel3->CCR |= DMA_CCR_CIRC;

    SPI1->CR2 |= SPI_CR2_TXDMAEN;
}

void spi1_enable_dma(void) {
    DMA1_Channel3->CCR |= 0x00000001;
}

void clear_oled(void) {
    spi1_display1("                ");
    spi1_display2("                ");
}

void prompt_temperature(void) {
    clear_oled();
    spi1_display1("Enter Temp:");
    spi1_display2("(+/- .5 F)");
}

void display_invalid_input(void) {
    clear_oled();
    spi1_display1("Invalid Input!");
    nano_wait(1500000000); 
}

void display_user_input(int temp, int is_negative) {
    char buffer[16];
    snprintf(buffer, 16, "%s%d", is_negative ? "-" : "", temp);
    spi1_display1("                "); 
    spi1_display2(buffer);              
}

int read_numeric_input(void) {
    char key;
    int temp = 0;
    int is_negative = 0;
    int is_first_key = 1;

    while (1) {
        key = get_keypress(); 

        if (is_first_key) {
            clear_oled();     
            is_first_key = 0; 
        }

        if (key == '#') { 
            return is_negative ? -temp : temp;
        } else if (key == '*') { 
            temp = 0;
            is_negative = 0;
            is_first_key = 1;
            prompt_temperature();
        } else if (key == '-' && temp == 0) { 
            is_negative = 1;
        } else if (isdigit(key)) { 
            temp = temp * 10 + (key - '0'); 
        } else { 
            return -9999;        
        }

        display_user_input(temp, is_negative);
    }
}
volatile struct {
    uint8_t temp_byte1;
    uint8_t temp_byte2;
    uint8_t rh_byte1;
    uint8_t rh_byte2;
    uint8_t valid;      
    uint8_t state;     
    uint32_t timestamp;
} dht11_data = {0};

volatile uint8_t timer_counter = 0;

void init_tim6(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;


    TIM6->PSC = 23999;   
    TIM6->ARR = 999;    

    TIM6->DIER |= TIM_DIER_UIE;

    NVIC->ISER[0] = 1 << TIM6_DAC_IRQn;

    TIM6->CR1 |= TIM_CR1_CEN;
}
volatile uint8_t target = 0;
void TIM6_DAC_IRQHandler(void) {
    TIM6->SR &= ~TIM_SR_UIF;

    timer_counter++;

    if (timer_counter >= 4) {
        timer_counter = 0;  

        DHT22_Start();
        uint8_t presence = DHT11_Check_Response();
       
        if (presence == 1) {
            uint8_t rh1 = DHT11_Read();
            uint8_t rh2 = DHT11_Read();
            uint8_t t1 = DHT11_Read();
            uint8_t t2 = DHT11_Read();
            uint8_t checksum = DHT11_Read();
         
                dht11_data.temp_byte1 = t1;
                dht11_data.temp_byte2 = t2;
                dht11_data.rh_byte1 = rh1;
                dht11_data.rh_byte2 = rh2;
                dht11_data.valid = 1;
                
               

                uint16_t temp = (t1 << 8) | t2;
                uint16_t rh = (rh1 << 8) | rh2;
                float temp_f = (float)temp / 10;
                float rh_f = (float)rh / 10;

                int temp_int = (int)temp_f;
                int temp_dec = (int)((temp_f - temp_int) * 10);
                int rh_int = (int)rh_f;
                int rh_dec = (int)((rh_f - rh_int) * 10);

                temp_f = temp_f * 9 / 5 + 32;
                current_temp = temp_f;

                temp_int = (int)temp_f;
                temp_dec = (int)((temp_f - temp_int) * 10);
                

                char temp_str[20];
                snprintf(temp_str, sizeof(temp_str), "%d%dF%d%d", temp_int, temp_dec, rh_int, rh_dec);
                print(temp_str);

                if (temp_f >= 100) {
                    msg[2] |= 0x80;
                } 
                else {
                    msg[1] |= 0x80;
                }
                if (rh_f >= 100) {
                    msg[6] |= 0x80;
                } 
                else if (rh_f >= 10) {
                    msg[5] |= 0x80;
                }
                else {
                    msg[4] |= 0x80;
                }
        } else {
            print("Sensor Error\n");
        }
    }
}

volatile float diff = 0;
char buffer[16] = {0};
char buffer2[16] = {0};
void adjust_HVAC(float diff, int temp) {
   if (diff > .5) {
        clear_oled();

        snprintf(buffer, 16, "Heating to %d F", temp);
        int diffi = (int)diff;
        int diffd = (int)((diff - diffi) * 10);

        snprintf(buffer2, 16, "delta: %d.%d F", diffi, diffd);

        spi1_display1(buffer);
        spi1_display2(buffer2);
        
        set_fan_speed(0);
        Heater_On();

   }
   else if (diff < -.5) {
        clear_oled();
        snprintf(buffer, 16, "Cooling to %d F", temp);
        int diffi = (int)diff;
        int diffd = (int)((diff - diffi) * 10);


        diffi = diffi * -1;
        diffd = diffd * -1;
        snprintf(buffer2, 16, "delta: %d.%d F", diffi, diffd);
        spi1_display1(buffer);
        spi1_display2(buffer2);
        Heater_Off();
        set_fan_speed(100);
   }
   else {
        clear_oled();
        spi1_display1("Target reached!");
        int diffi = (int)diff;
        int diffd = (int)((diff - diffi) * 10);

        snprintf(buffer2, 16, "delta: %d.%d F", diffi, diffd);
        spi1_display2(buffer2);

        if (temp > 74) {
            set_fan_speed(0);
            Heater_On();
        }
        else if (temp < 74) {
            set_fan_speed(50);
            Heater_Off();
        }
        else {
            set_fan_speed(0);
            Heater_Off();
        }
   }
}



uint8_t Presence = 0;
uint8_t Checksum = 0;
uint8_t Rh_byte1 = 0;
uint8_t Rh_byte2 = 0;
uint8_t Temp_byte1 = 0;
uint8_t Temp_byte2 = 0;
int main(void) {
    internal_clock();
    enable_ports(); 

    configure_pb7(); 
    Heater_On();    
    init_spi1();      
    spi1_init_oled(); 
    setup_tim3();
    init_tim6();




#define SPI_LEDS
#if defined(SPI_LEDS)
    init_spi2();
    spi2_setup_dma();
    spi2_enable_dma();
    init_tim15();
#endif

    set_fan_speed(100);
    configure_pb7();
    prompt_temperature(); 

    int temp = read_numeric_input();

    if (temp == -9999) { 
            display_invalid_input();
    }
    if (temp > 100 || temp < 0) {
        display_invalid_input();
    }
    char buffer[16];
    char buffer2[16];
    while (1) {
        diff =(float) (temp - current_temp);
        adjust_HVAC(diff, temp);
        nano_wait(100000000); 
    }

    return 0;
}
