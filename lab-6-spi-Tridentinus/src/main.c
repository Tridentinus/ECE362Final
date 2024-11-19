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

/*******************************************************************************/ 
#include "stm32f0xx.h"
//include sprintf  
#include <stdio.h>
void set_char_msg(int, char);
void nano_wait(unsigned int);
void game(void);
void internal_clock();
void check_wiring();
void autotest();

#define DHT11_PORT GPIOA
#define DHT11_PIN GPIO_PIN_7
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
void setup_bb(void) 
{   
    // Enable the GPIOB peripheral
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;

    // Set the mode to output for PB12, PB13, and PB15
    GPIOB->MODER &= ~((3 << (12 * 2)) | (3 << (13 * 2)) | (3 << (15 * 2))); // Clear mode bits
    GPIOB->MODER |= (1 << (12 * 2)) | (1 << (13 * 2)) | (1 << (15 * 2));    // Set mode to output (01)

    // Set the output type to push-pull for PB12, PB13, and PB15
    GPIOB->OTYPER &= ~((1 << 12) | (1 << 13) | (1 << 15));

    // Initialize the pins: CS high, SCK low
    GPIOB->BSRR = (1 << 12);  // Set PB12 (CS) high
    GPIOB->BRR = (1 << 13);   // Reset PB13 (SCK) low
}

void small_delay(void) {
    nano_wait(50000);
}

//===========================================================================
// Set the MOSI bit, then set the clock high and low.
// Pause between doing these steps with small_delay().
//===========================================================================
void bb_write_bit(int out) {
    // Set SDI (PB15) to 0 or 1 based on out
    if (out) {
        GPIOB->BSRR = (1 << 15);  // Set PB15 high
    } else {
        GPIOB->BRR = (1 << 15);   // Reset PB15 low
    }
    small_delay();

    // Set SCK (PB13) to 1
    GPIOB->BSRR = (1 << 13);  // Set PB13 high
    small_delay();

    // Set SCK (PB13) to 0
    GPIOB->BRR = (1 << 13);   // Reset PB13 low
}

//===========================================================================
// Set CS (PB12) low,
// write 16 bits using bb_write_bit,
// then set CS high.
//===========================================================================
void bb_write_halfword(int message) {
    // Set CS (PB12) low
    GPIOB->BRR = (1 << 12);

    // Write each bit from bit 15 to bit 0
    for (int i = 15; i >= 0; i--) {
        bb_write_bit((message >> i) & 1);
    }

    // Set CS (PB12) high
    GPIOB->BSRR = (1 << 12);
}

//===========================================================================
// Configure PB7 to be output, push-pull, no pull-up or pull-down, low speed
//===========================================================================
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

    // Write a 1 to PB7
    GPIOB->BSRR = (1 << 7);
    //wreit a 0 to PB7
    // print("oo");
    // GPIOB->BRR = (1 << 7);
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

    //enable the clock to timer 15
    RCC->APB2ENR |= RCC_APB2ENR_TIM15EN;

    //trigger dma reuest at rate of 1 kHz
    TIM15->DIER |= TIM_DIER_UDE;

    TIM15->PSC = 479; // 48MHz / 480 = 100kHz
    TIM15->ARR = 99; // 100kHz / 100 = 1kHz
    //no need to enable the interrupt in the NVIC->ISER
    //enable the timer
    TIM15->CR1 |= TIM_CR1_CEN;



}


//===========================================================================
// Configure timer 7 to invoke the update interrupt at 1kHz
// Copy from lab 4 or 5.
//===========================================================================

void init_tim7(void) {

    //enable the clock to timer 7
    RCC->APB1ENR |= RCC_APB1ENR_TIM7EN;

    //get  48MHz down to 1kHz with a combo of PSC and ARR
    TIM7->PSC = 479; // 48MHz / 480 = 100kHz
    TIM7->ARR = 99; // 100kHz / 100 = 1kHz

    //enable the update interrupt
    TIM7->DIER |= TIM_DIER_UIE;

    //enable the interrupt in the NVIC->ISER
    NVIC->ISER[0] = 1 << TIM7_IRQn;

    //start the timer
    TIM7->CR1 |= TIM_CR1_CEN;

}



//===========================================================================
// Copy the Timer 7 ISR from lab 5
//===========================================================================
// TODO To be copied

void TIM7_IRQHandler() {
    //check the rows and columns and update the history of each key for debouncing

    //clear the update interrupt flag (acknowledge the interrupt)
    TIM7->SR &= ~TIM_SR_UIF;

    //read the row values
    int rows = read_rows();
    update_history(col, rows);
    col = (col + 1) & 3;
    drive_column(col);
}


//===========================================================================
// Initialize the SPI2 peripheral.
//===========================================================================
void init_spi2(void) {
    // Enable the GPIOB peripheral pis
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    // Set the mode to alternate function for PB12, PB13, and PB15
    GPIOB->MODER &= ~((3 << (12 * 2)) | (3 << (13 * 2)) | (3 << (15 * 2))); // Clear mode bits

    // Set the mode to alternate function for PB12, PB13, and PB15
    GPIOB->MODER |= (2 << (12 * 2)) | (2 << (13 * 2)) | (2 << (15 * 2));    // Set mode to alternate function (10)

    // Set the alternate function to AF0 for PB12, PB13, and PB15
    GPIOB->AFR[1] &= ~((0xF << (4 * 4)) | (0xF << (5 * 4)) | (0xF << (7 * 4))); // Clear alternate function bits

    // Set the alternate function to AF0 for PB12, PB13, and PB15
    GPIOB->AFR[1] |= (0 << (4 * 4)) | (0 << (5 * 4)) | (0 << (7 * 4)); // Set alternate function to AF0

    //emable the SPI2 peripheral
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    // Ensure that the CR1_SPE bit is clear first
    SPI2->CR1 &= ~SPI_CR1_SPE;

    // Set the baud rate as low as possible (maximum divisor for BR)
    SPI2->CR1 |= SPI_CR1_BR; 

    // Configure the interface for a 16-bit word size
    SPI2->CR2 = (SPI2->CR2 & ~SPI_CR2_DS) | (0xF << SPI_CR2_DS_Pos);

    // Configure the SPI channel to be in "master configuration"
    SPI2->CR1 |= SPI_CR1_MSTR;

    // Set the SS Output enable bit and enable NSSP
    SPI2->CR2 |= SPI_CR2_SSOE | SPI_CR2_NSSP;

    // Set the TXDMAEN bit to enable DMA transfers on transmit buffer empty
    SPI2->CR2 |= SPI_CR2_TXDMAEN;

    // Enable the SPI channel
    SPI2->CR1 |= SPI_CR1_SPE;
}

//===========================================================================
// Configure the SPI2 peripheral to trigger the DMA channel when the
// transmitter is empty.  Use the code from setup_dma from lab 5.
//===========================================================================
void spi2_setup_dma(void) {

    // Circular DMA transfer
    // Enable RCC clock to DMA
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    // Turn off enable bit for channel first (used with TIM15)
    DMA1_Channel5->CCR &= ~DMA_CCR_EN;

    // Set CMAR to address of msg
    DMA1_Channel5->CMAR = (uint32_t)msg;

    // Set CPAR to address of SPI2->DR
    DMA1_Channel5->CPAR = (uint32_t)&(SPI2->DR);

    // Set CNDTR to 8 (the number of 16-bit transfers)
    DMA1_Channel5->CNDTR = 8;

    // Set DIR for copying from memory to peripheral
    DMA1_Channel5->CCR |= DMA_CCR_DIR;

    // Set MINC to increment memory address for each transfer
    DMA1_Channel5->CCR |= DMA_CCR_MINC;

    // Set MSIZE to 16 bits
    DMA1_Channel5->CCR &= ~DMA_CCR_MSIZE;
    DMA1_Channel5->CCR |= DMA_CCR_MSIZE_0;

    // Set PSIZE to 16 bits
    DMA1_Channel5->CCR &= ~DMA_CCR_PSIZE;
    DMA1_Channel5->CCR |= DMA_CCR_PSIZE_0; 

    // Set channel to circular mode
    DMA1_Channel5->CCR |= DMA_CCR_CIRC;

    // Enable the SPI2 bit that generates a DMA request whenever the TXE flag is set
    SPI2->CR2 |= SPI_CR2_TXDMAEN;
    
}

//===========================================================================
// Enable the DMA channel.
//===========================================================================
void spi2_enable_dma(void) {
    DMA1_Channel5->CCR |= DMA_CCR_EN;
}

//===========================================================================
// 4.4 SPI OLED Display
//===========================================================================
void init_spi1() {
    // Enable the GPIOA peripheral
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;

    // Set the mode to alternate function for PA15, PA5, and PA7
    GPIOA->MODER &= ~((3 << (15 * 2)) | (3 << (5 * 2)) | (3 << (7 * 2))); // Clear mode bits
    GPIOA->MODER |= (2 << (15 * 2)) | (2 << (5 * 2)) | (2 << (7 * 2));    // Set mode to alternate function (10)

    // Set the alternate function to AF0 for PA15, PA5, and PA7
    GPIOA->AFR[1] &= ~((0xF << (15 - 8) * 4)); // Clear alternate function bits for PA15
    GPIOA->AFR[0] &= ~((0xF << (5 * 4)) | (0xF << (7 * 4))); // Clear alternate function bits for PA5 and PA7
    // GPIOA->AFR[1] |= (0 << (15 - 8) * 4); // Set alternate function to AF0 for PA15
    // GPIOA->AFR[0] |= (0 << (5 * 4)) | (0 << (7 * 4)); // Set alternate function to AF0 for PA5 and PA7

    // Enable the SPI1 peripheral
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    // Ensure that the CR1_SPE bit is clear first
    SPI1->CR1 &= ~SPI_CR1_SPE;

    // Set the baud rate as low as possible (maximum divisor for BR)
    SPI1->CR1 |= SPI_CR1_BR | SPI_CR1_MSTR;

    // SPI1->CR1 |= SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE;

    // Configure the interface for a 10-bit word size
    // DS[3:0] = 1001

    //cofigure the nSS for pa15


    SPI1->CR2 = SPI_CR2_SSOE | SPI_CR2_NSSP | SPI_CR2_DS_3 | SPI_CR2_DS_0;

    SPI1->CR2 |= SPI_CR2_TXDMAEN;

    // Enable the SPI channel
    SPI1->CR1 |= SPI_CR1_SPE;
}
void spi_cmd(unsigned int data) {
    //until SPI1 is empty
    while((SPI1->SR & SPI_SR_TXE) == 0);

    //set the data to the data register
    SPI1->DR = data;
}
void spi_data(unsigned int data) {
    //call spi_cmd with the data or 0x200 | data

    spi_cmd(0x200 | data);

}
void spi1_init_oled() {
    //wait 1ms with nano_wait
    nano_wait(1000000);

    spi_cmd(0x38); //function set
    spi_cmd(0x08); //display off
    spi_cmd(0x01); //clear display

    nano_wait(2000000); //wait 1ms

    spi_cmd(0x06); //entry mode set
    spi_cmd(0x02); //return home    
    spi_cmd(0x0c); //display on
}
void spi1_display1(const char *string) {
    //move the cursor to home position

    spi_cmd(0x02);

    // Loop through the string until you reach a null character
    for(int i = 0; i < 16 && string[i] != '\0'; i++) {
        // Send the character to the display
        spi_data(string[i]);
    }
}
void spi1_display2(const char *string) {
      //move the cursor to home position

    spi_cmd(0xc0);

    // Loop through the string until you reach a null character
    for(int i = 0; i < 16 && string[i] != '\0'; i++) {
        // Send the character to the display
        spi_data(string[i]);
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
    // Circular DMA transfer
    // Enable RCC clock to DMA
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    // Turn off enable bit for channel first
    DMA1_Channel3->CCR &= ~DMA_CCR_EN;

    // Set CMAR to address of display
    DMA1_Channel3->CMAR = (uint32_t)display;

    // Set CPAR to address of SPI1->DR
    DMA1_Channel3->CPAR = (uint32_t)&(SPI1->DR);

    // Set CNDTR to 34 (the number of 16-bit transfers)
    DMA1_Channel3->CNDTR = 34;

    // Set DIR for copying from memory to peripheral
    DMA1_Channel3->CCR |= DMA_CCR_DIR;

    // Set MINC to increment memory address for each transfer
    DMA1_Channel3->CCR |= DMA_CCR_MINC;

    // Set MSIZE to 16 bits
    DMA1_Channel3->CCR &= ~DMA_CCR_MSIZE;
    DMA1_Channel3->CCR |= DMA_CCR_MSIZE_0;

    // Set PSIZE to 16 bits
    DMA1_Channel3->CCR &= ~DMA_CCR_PSIZE;
    DMA1_Channel3->CCR |= DMA_CCR_PSIZE_0; 

    // Set channel to circular mode
    DMA1_Channel3->CCR |= DMA_CCR_CIRC;

    // Enable the SPI1 bit that generates a DMA request whenever the TXE flag is set
    SPI1->CR2 |= SPI_CR2_TXDMAEN;
}


//===========================================================================
// Enable the DMA channel triggered by SPI1_TX.
//===========================================================================

void spi1_enable_dma(void) {
    DMA1_Channel3->CCR |= DMA_CCR_EN;
}

//===========================================================================
// Main function
//===========================================================================
uint8_t Presence = 0;
uint8_t Checksum = 0;
int main(void) {
    internal_clock();
    msg[0] |= font['E'];
    msg[1] |= font['C'];
    msg[2] |= font['E'];
    msg[3] |= font[' '];
    msg[4] |= font['3'];
    msg[5] |= font['6'];
    msg[6] |= font['2'];
    msg[7] |= font[' '];

    // print("score, 100\n");
   
    // GPIO enable
    enable_ports();
    // setup keyboard
    init_tim7();
    init_tim1();

    // print("hello");
    // LED array Bit Bang
// #define BIT_BANG
#if defined(BIT_BANG)
    setup_bb();
    drive_bb();
#endif

    // Direct SPI peripheral to drive LED display
#define SPI_LEDS
#if defined(SPI_LEDS)
    init_spi2();
    spi2_setup_dma();
    spi2_enable_dma();
    init_tim15();
    // show_keys();
#endif

// print("o");
    // SPI OLED direct drive
// char tstr[20] = {0};
    // print("o       ");
    configure_pb7();



// }

    // LED array SPI
// #define SPI_LEDS_DMA
#if defined(SPI_LEDS_DMA)
    init_spi2();
    spi2_setup_dma();
    spi2_enable_dma();
    show_keys();
#endif

    // SPI OLED direct drive
// #define SPI_OLED
#if defined(SPI_OLED)
    init_spi1();
    spi1_init_oled();
    spi1_display1("Hello again,");
    spi1_display2(username);
#endif

    // SPI
// #define SPI_OLED_DMA
#if defined(SPI_OLED_DMA)
    init_spi1();
    spi1_init_oled();
    spi1_setup_dma();
    spi1_enable_dma();
#endif
    // __HAL_TIM_SET_COUNTER(&htim1, 0);  // Reset counter
    // while (1) {
    //     uint16_t cnt = __HAL_TIM_GET_COUNTER(&htim1);
    //     char tstr[20] = {0};
    //     snprintf(tstr, sizeof(tstr), "%d", cnt);
    //     print(tstr);
    //     nano_wait(1000000000);  // Wait for 1 ms
    // }

    // Uncomment when you are ready to generate a code.
    // autotest();

    // Game on!  The goal is to score 100 points.
    // game();
}
