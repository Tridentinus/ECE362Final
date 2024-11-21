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
const char* username = "akhondok";

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
// Configure GPIOC
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

//============================================================================
// Configure Timer 15 for an update rate of 1 kHz.
// Trigger the DMA channel on each update.
// Copy this from lab 4 or lab 5.
//============================================================================
void init_tim15(void) {
    RCC->APB2ENR |= RCC_APB2ENR_TIM15EN;
    TIM15->PSC = (2400 - 1);
    TIM15->ARR = (20-1);
    TIM15->DIER |= TIM_DIER_UDE;
    TIM15->CR1 |= 0x00000001;
}

//===========================================================================
// Configure timer 7 to invoke the update interrupt at 1kHz
// Copy from lab 4 or 5.
//===========================================================================
void init_tim7(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM7EN;
    TIM7->PSC = 48 - 1;
    TIM7->ARR = 1000 - 1;
    TIM7->DIER |= TIM_DIER_UIE;
    NVIC_EnableIRQ(TIM7_IRQn);
    TIM7->CR1 |= TIM_CR1_CEN;
}

//===========================================================================
// Copy the Timer 7 ISR from lab 5
//===========================================================================
// TODO To be copied
void TIM7_IRQHandler() {
    TIM7->SR &= ~TIM_SR_UIF;
    uint8_t rows = read_rows();
    update_history(col, rows);
    col = (col + 1) & 3;
    drive_column(col);
}

//===========================================================================
// Initialize the SPI2 peripheral.
//===========================================================================
void init_spi2(void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;

    GPIOB->MODER &= ~(0xcf000000);
    GPIOB->MODER |= (0x0000008a << 24);

    GPIOB->AFR[0] &= ~(0xf0ff << 16);

    // SPI2->CR1 &= ~(0x00000040);
    SPI2->CR1 &= ~(SPI_CR1_SPE);
   
    SPI2->CR1 |= (0x00000007 << 3);
   
    SPI2->CR2 = (0x0000000f << 8);

    SPI2->CR1 |= 0x00000004;

    SPI2->CR2 |= 0x00000004;
    SPI2->CR2 |= 0x00000008;

    SPI2->CR2 |= 0x00000002;
   
    SPI2->CR1 |= 0x00000040;
}

//===========================================================================
// Configure the SPI2 peripheral to trigger the DMA channel when the
// transmitter is empty.  Use the code from setup_dma from lab 5.
//===========================================================================
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

//===========================================================================
// Enable the DMA channel.
//===========================================================================
void spi2_enable_dma(void) {
    DMA1_Channel5->CCR |= 0x00000001;
}

//===========================================================================
// 4.4 SPI OLED Display
//===========================================================================
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

//===========================================================================
// This is the 34-entry buffer to be copied into SPI1.
// Each element is a 16-bit value that is either character data or a command.
// Element 0 is the command to set the cursor to the first position of line 1.
// The next 16 elements are 16 characters.
// Element 17 is the command to set the cursor to the first position of line 2.
//===========================================================================
uint16_t display[34] = {
        0x002, // Command to set the cursor at the first position line 1
        0x200+'E', 0x200+'C', 0x200+'E', 0x200+'3', 0x200+'6', + 0x200+'2', 0x200+' ', 0x200+'i',
        0x200+'s', 0x200+' ', 0x200+'t', 0x200+'h', + 0x200+'e', 0x200+' ', 0x200+' ', 0x200+' ',
        0x0c0, // Command to set the cursor at the first position line 2
        0x200+'c', 0x200+'l', 0x200+'a', 0x200+'s', 0x200+'s', + 0x200+' ', 0x200+'f', 0x200+'o',
        0x200+'r', 0x200+' ', 0x200+'y', 0x200+'o', + 0x200+'u', 0x200+'!', 0x200+' ', 0x200+' ',
};

//===========================================================================
// Configure the proper DMA channel to be triggered by SPI1_TX.
// Set the SPI1 peripheral to trigger a DMA when the transmitter is empty.
//===========================================================================
void spi1_setup_dma(void) {
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    DMA1_Channel3->CCR &= ~(0x1);
    DMA1_Channel3->CMAR = (uint32_t) &display;
    DMA1_Channel3->CPAR = (uint32_t) &SPI1->DR;
    DMA1_Channel3->CNDTR = 34;
    DMA1_Channel3->CCR |= DMA_CCR_DIR;
    DMA1_Channel3->CCR |= DMA_CCR_MINC;
    DMA1_Channel3->CCR |= 0x00000400;
    DMA1_Channel3->CCR |= 0x00000100;
    DMA1_Channel3->CCR |= 0x00000020;
}

//===========================================================================
// Enable the DMA channel triggered by SPI1_TX.
//===========================================================================
void spi1_enable_dma(void) {
    DMA1_Channel3->CCR |= 0x00000001;
}

// Helper function to clear OLED screen
void clear_oled(void) {
    spi1_display1("                "); // Clear line 1
    spi1_display2("                "); // Clear line 2
}

// Helper function to prompt for temperature
void prompt_temperature(void) {
    clear_oled();
    spi1_display1("Enter Temp:");
    spi1_display2("(+/- 3 deg F)");
}

// Helper function to display invalid input message
void display_invalid_input(void) {
    clear_oled();
    spi1_display1("Invalid Input!");
    nano_wait(1500000000); // Wait 2 seconds
}

// Helper function to dynamically display current input on OLED
// Modified helper function to dynamically display current input on OLED
void display_user_input(int temp, int is_negative) {
    char buffer[16];
    snprintf(buffer, 16, "%s%d", is_negative ? "-" : "", temp);
    spi1_display1("                "); // Clear the first line
    spi1_display2(buffer);              // Display the input on the second line
}

// Modified function to read and display user input live
int read_numeric_input(void) {
    char key;
    int temp = 0;
    int is_negative = 0;
    int is_first_key = 1; // Track if this is the first keypress

    while (1) {
        key = get_keypress(); // Wait for a key press event

        if (is_first_key) {
            clear_oled();     // Clear the entire display when typing starts
            is_first_key = 0; // Ensure this happens only once
        }

        if (key == '#') { // '#' for submit
            return is_negative ? -temp : temp;
        } else if (key == '*') { // '*' for reset input
            temp = 0;
            is_negative = 0;
            is_first_key = 1; // Reset to show the prompt again
            prompt_temperature();
        } else if (key == '-' && temp == 0) { // Negative sign only allowed at the start
            is_negative = 1;
        } else if (isdigit(key)) { // Check if key is a digit
            temp = temp * 10 + (key - '0'); // Update numeric value
        } else { // Invalid input
            return -9999; // Special code for invalid input
        }

        // Update OLED display with the current input
        display_user_input(temp, is_negative);
    }
}

//===========================================================================
// Main function
//===========================================================================
int main(void) {
    internal_clock();
    enable_ports(); // GPIO enable for keypad
    init_tim7();    // Timer for keypad scanning

    init_spi1();      // SPI initialization for OLED
    spi1_init_oled(); // Initialize OLED

    while (1) {
        prompt_temperature(); // Prompt user for input

        int temp = read_numeric_input();

        if (temp == -9999) { // Check for invalid input
            display_invalid_input();
        } else {
            // Display the entered temperature
            clear_oled();
            char buffer[16];
            print(buffer, 16, "Set Temp: %d C", temp);
            spi1_display1(buffer);
            nano_wait(3000000000); // Wait 3 seconds before re-prompting
        }
    }

    return 0;
}
