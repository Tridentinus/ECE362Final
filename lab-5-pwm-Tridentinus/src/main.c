/**
  ******************************************************************************
  * @file    main.c
  * @author  Weili An, Niraj Menon
  * @date    Jan 31 2024
  * @brief   ECE 362 Lab 5 Student template
  ******************************************************************************
*/

/*******************************************************************************/

// Fill out your username, otherwise your completion code will have the 
// wrong username!
const char* username = "seaman7";

/*******************************************************************************/ 

#include "stm32f0xx.h"
#include <math.h>   // for M_PI

void nano_wait(int);

// 16-bits per digit.
// The most significant 8 bits are the digit number.
// The least significant 8 bits are the segments to illuminate.
uint16_t msg[8] = { 0x0000,0x0100,0x0200,0x0300,0x0400,0x0500,0x0600,0x0700 };
extern const char font[];
// Print an 8-character string on the 8 digits
void print(const char str[]);
// Print a floating-point value.
void printfloat(float f);

void autotest(void);

//============================================================================
// PWM Lab Functions
//============================================================================
void setup_tim3(void) {
    // Enable clock for GPIOC
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;

    // Enable clock for TIM3
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    // Set PC6, PC7, PC8, PC9 to alternate function mode
    GPIOC->MODER &= ~0xFF000; // Clear mode bits for PC6-PC9
    GPIOC->MODER |= 0xAA000;  // Set mode bits to alternate function

    GPIOC->AFR[0] &= ~0xFF000000;
    GPIOC->AFR[0] &= ~0x000000FF;



    // Set TIM3 prescaler to divide by 48000 (48MHz / 48000 = 1kHz)
    TIM3->PSC = 4799;

    // Set TIM3 auto-reload register to 999 (1kHz / 1000 = 1Hz)
    TIM3->ARR = 3;

    // Set TIM3 CCMR1 and CCMR2 to PWM mode 1
    TIM3->CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2;
    TIM3->CCMR1 |= TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2;
    TIM3->CCMR2 |= TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2;
    TIM3->CCMR2 |= TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2;

    // Enable the four outputs
    TIM3->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;

    // Set TIM3 CCR1, CCR2, CCR3, CCR4
    TIM3->CCR1 = 0;
    TIM3->CCR2 = 10;
    TIM3->CCR3 = 100;
    TIM3->CCR4 = 100;

    // Enable Timer 3
    TIM3->CR1 |= TIM_CR1_CEN;
}

void setup_tim1(void) {
    // Enable the clock for TIM1 and GPIOA peripherals
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;

    // Configure MODER for each of the four TIM1_CHx pins to alternate function mode
    GPIOA->MODER &= ~(GPIO_MODER_MODER8 | GPIO_MODER_MODER9 | GPIO_MODER_MODER10 | GPIO_MODER_MODER11);
    GPIOA->MODER |= (GPIO_MODER_MODER8_1 | GPIO_MODER_MODER9_1 | GPIO_MODER_MODER10_1 | GPIO_MODER_MODER11_1);

    // Set up GPIOA AFR to use the alternate function for each of the four TIM1_CHx pins
    GPIOA->AFR[1] &= ~(GPIO_AFRH_AFRH0 | GPIO_AFRH_AFRH1 | GPIO_AFRH_AFRH2 | GPIO_AFRH_AFRH3);
    GPIOA->AFR[1] |= (2 << GPIO_AFRH_AFRH0_Pos) | (2 << GPIO_AFRH_AFRH1_Pos) | (2 << GPIO_AFRH_AFRH2_Pos) | (2 << GPIO_AFRH_AFRH3_Pos);

    // Enable the MOE bit in the TIM1 BDTR register
    TIM1->BDTR |= TIM_BDTR_MOE;

    // Set the TIM1 prescaler to divide by 1
    TIM1->PSC = 1-1;

    // Set the TIM1 auto-reload register such that an update event occurs at 20 KHz
    //48*10^6/24*10^3 2 * 10^3
    TIM1->ARR = 2400 - 1;

    // Configure the timer for PWM mode
    // Set the OCxM bits for all 4 channels in both TIM1_CCMR1 and TIM1_CCMR2 registers to PWM mode 1
    TIM1->CCMR1 &= ~(TIM_CCMR1_OC1M | TIM_CCMR1_OC2M);
    TIM1->CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2;
    TIM1->CCMR2 &= ~(TIM_CCMR2_OC3M | TIM_CCMR2_OC4M);
    TIM1->CCMR2 |= TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2;

    // For channel 4 of Timer 1, enable the "output compare preload enable"
    
    TIM1->CCMR2 |= TIM_CCMR2_OC4PE;

    // Enable the four channel outputs in the TIM1 CCER register
    TIM1->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;

    // Enable Timer 1
    TIM1->CR1 |= TIM_CR1_CEN;
}

int getrgb(void);

// Helper function for you
// Accept a byte in BCD format and convert it to decimal
uint8_t bcd2dec(uint8_t bcd) {
    // Lower digit
    uint8_t dec = bcd & 0xF;

    // Higher digit
    dec += 10 * (bcd >> 4);
    return dec;
}

void setrgb(int rgb) {
    uint8_t b = bcd2dec(rgb & 0xFF);
    uint8_t g = bcd2dec((rgb >> 8) & 0xFF);
    uint8_t r = bcd2dec((rgb >> 16) & 0xFF);

    // Assign values to TIM1->CCRx registers
    // Remember these are all percentages
    // Also, LEDs are on when the corresponding PWM output is low
    // so you might want to invert the numbers.
    TIM1->CCR1 = 2400 - (r * 2400 / 100);
    TIM1->CCR2 = 2400 - (g * 2400 / 100);
    TIM1->CCR3 = 2400 - (b * 2400 / 100);


}



//============================================================================
// Lab 4 code
// Add in your functions from previous lab
//============================================================================

// Part 3: Analog-to-digital conversion for a volume level.
int volume = 2400;

// Variables for boxcar averaging.
#define BCSIZE 32
int bcsum = 0;
int boxcar[BCSIZE];
int bcn = 0;

void dialer(void);

// Parameters for the wavetable size and expected synthesis rate.
#define N 1000
#define RATE 20000
short int wavetable[N];
int step0 = 0;
int offset0 = 0;
int step1 = 0;
int offset1 = 0;

//============================================================================
// enable_ports()
//============================================================================
void enable_ports(void) {
    //ENABLE GPIOB and GPIOC (without changing anything else)
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN | RCC_AHBENR_GPIOCEN;
    //config pb0-10 as output
    //clear first
    GPIOB->MODER &= ~0x3FF; //0x3FF is 0000 0011 1111 1111
    GPIOB->MODER |= 0x155555; //0x155555 is 0001 0101 0101 0101 0101 0101

    //pc0-3 as input with pull high and pc4-7 as output (open drain)
    GPIOC->MODER &= ~0xFFFF; //0xFFFF is 0000 0000 0000 0000
    GPIOC->PUPDR |= 0x55; //0x5555 is 0101 0101 0101 0101
    GPIOC->MODER |= 0x5500; //0x5500 is 0101 0101 0000 0000
    GPIOC->OTYPER |= 0xF0;  //0xF0 is 1111 0000
}


//============================================================================
// setup_dma() + enable_dma()
//============================================================================

void setup_dma(void) {
    //circular dma transfer
    //enbable rcc clock to dma
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    // turn off enable bit for channel first (used with tim15)
    DMA1_Channel5->CCR &= ~DMA_CCR_EN;

    // set CMAR to address of msg
    DMA1_Channel5->CMAR = (uint32_t)msg;

    // set CPAR to address of GPIOB->ODR
    DMA1_Channel5->CPAR = (uint32_t)&(GPIOB->ODR);

    // set CNDTR to 8   (the number of 16-bit transfers)
    DMA1_Channel5->CNDTR = 8;

    // set DIR for copying from memory to peripheral
    DMA1_Channel5->CCR |= DMA_CCR_DIR;

    // set MINC to increment memory address for each transfer
    DMA1_Channel5->CCR |= DMA_CCR_MINC;

    //set MSIZE to 16 bits
    DMA1_Channel5->CCR &= ~DMA_CCR_MSIZE;
    DMA1_Channel5->CCR |= DMA_CCR_MSIZE_0;

    //set PSIZE to 16 bits
    DMA1_Channel5->CCR &= ~DMA_CCR_PSIZE;
    DMA1_Channel5->CCR |= DMA_CCR_PSIZE_0; 

    //set channel to circular mode
    DMA1_Channel5->CCR |= DMA_CCR_CIRC;




}

void enable_dma(void) {
    //enable the channel
    DMA1_Channel5->CCR |= DMA_CCR_EN;
    
}

//============================================================================
// init_tim15()
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


//=============================================================================
// Part 2: Debounced keypad scanning.
//=============================================================================

uint8_t col; // the column being scanned

void drive_column(int);   // energize one of the column outputs
int  read_rows();         // read the four row inputs
void update_history(int col, int rows); // record the buttons of the driven column
char get_key_event(void); // wait for a button event (press or release)
char get_keypress(void);  // wait for only a button press event.
float getfloat(void);     // read a floating-point number from keypad
void show_keys(void);     // demonstrate get_key_event()

//============================================================================
// The Timer 7 ISR
//============================================================================
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


//============================================================================
// init_tim7()
//============================================================================
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


//============================================================================
// setup_adc()
//============================================================================
void setup_adc(void) {
    //clock to gpioa
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;

    //set PA1 (ADC_IN1) to analog mode
    GPIOA->MODER |= GPIO_MODER_MODER1;

    //enable the clock to the ADC
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    //turn on 14Mhz (HSI14) clock
    RCC->CR2 |= RCC_CR2_HSI14ON;
    //check the HSI14RDY bit in CR2
    while((RCC->CR2 & RCC_CR2_HSI14RDY) == 0);

    //activate the ADC
    ADC1->CR |= ADC_CR_ADEN;

    //wait for the ADC to be ready
    while((ADC1->ISR & ADC_ISR_ADRDY) == 0);

    //select the channel in CHSELR
    ADC1->CHSELR = ADC_CHSELR_CHSEL1;
}



//============================================================================
// Timer 2 ISR
//============================================================================
// Write the Timer 2 ISR here.  Be sure to give it the right name.

void TIM2_IRQHandler() {
    //clear the update interrupt flag
    TIM2->SR &= ~TIM_SR_UIF;

    //start the conversion
    ADC1->CR |= ADC_CR_ADSTART;

    //wait for the conversion to complete
    while((ADC1->ISR & ADC_ISR_EOC) == 0);

    //read the value
    int value = ADC1->DR;

   bcsum -= boxcar[bcn];
    bcsum += boxcar[bcn] = ADC1->DR;
    bcn += 1;
    if (bcn >= BCSIZE)
        bcn = 0;
    volume = bcsum / BCSIZE;
}

//============================================================================
// init_tim2()
//============================================================================
void init_tim2(void) {
    //enable the clock to timer 2
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    //get 48MHz down to 1kHz with a combo of PSC and ARR
    TIM2->PSC = 47999; // 48MHz / 48000 = 1kHz
    TIM2->ARR = 99; // 1kHz / 100 = 10Hz

    //enable the update interrupt
    TIM2->DIER |= TIM_DIER_UIE;

    //enable the interrupt in the NVIC->ISER
    NVIC->ISER[0] = 1 << TIM2_IRQn;

    //start the timer
    TIM2->CR1 |= TIM_CR1_CEN;

    NVIC_SetPriority(TIM2_IRQn, 3);
    
}




//============================================================================
// setup_dac()
//============================================================================

void setup_dac(void) {
    //eable clock to GPIOA (reenable)
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;

    //set PA4 to analog mode (for DAC_OUT1)
    GPIOA->MODER |= GPIO_MODER_MODER4;

    //enable the clock to the DAC (RCC)
    RCC->APB1ENR |= RCC_APB1ENR_DACEN;

    //select a tim6 TRGO event to trigger the DAC (use tsel)
    DAC->CR &= ~DAC_CR_TSEL1;

    //emable the trigger
    DAC->CR |= DAC_CR_TEN1;

    //enable the DAC
    DAC->CR |= DAC_CR_EN1;
    
}
//============================================================================
// Timer 6 ISR
//============================================================================
// Write the Timer 6 ISR here.  Be sure to give it the right name.
void TIM6_DAC_IRQHandler() {
    // Acknowledge the interrupt here first!
    TIM6->SR &= ~TIM_SR_UIF;

    // Increment offset0 by step0
    offset0 += step0;
    // Increment offset1 by step1
    offset1 += step1;

    // If offset0 is >= (N << 16)
    if (offset0 >= (N << 16)) {
        // Decrement offset0 by (N << 16)
        offset0 -= (N << 16);
    }

    // If offset1 is >= (N << 16)
    if (offset1 >= (N << 16)) {
        // Decrement offset1 by (N << 16)
        offset1 -= (N << 16);
    }

    // int samp = sum of wavetable[offset0>>16] and wavetable[offset1>>16]
    int samp = wavetable[offset0 >> 16] + wavetable[offset1 >> 16];

    // Multiply samp by volume
    samp *= volume;

    // Shift samp right by 17 bits to ensure it's in the right format for `DAC_DHR12R1`
    // samp >>= 17;
    samp >>= 18;
    // Increment samp by 2048
    // samp += 2048;
    samp += 1200;
    // Copy samp to DAC->DHR12R1
    // DAC->DHR12R1 = samp;
    TIM1->CCR4 = samp;
}


//============================================================================
// init_tim6()
//============================================================================

void init_tim6(void) {
    // Enable the clock to timer 6
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;

    // Get 48MHz down to RATE with a combo of PSC and ARR
    TIM6->PSC = (480000/ RATE) - 1; // 48MHz / RATE
    TIM6->ARR = 100- 1; // 1 tick per RATE

    // Enable the update interrupt
    TIM6->DIER |= TIM_DIER_UIE;

    // Enable the interrupt in the NVIC->ISER
    NVIC->ISER[0] |= 1 << TIM6_DAC_IRQn;

    // Start the timer with MMS = 010 (TRGO on update event) TRGO triggers the DAC ()
    //TIM6->CR2 |= TIM_CR2_MMS_1;

    // Enable the timer
    TIM6->CR1 |= TIM_CR1_CEN;
}

//===========================================================================
// init_wavetable()
// Write the pattern for a complete cycle of a sine wave into the
// wavetable[] array.
//===========================================================================
void init_wavetable(void) {
    for(int i=0; i < N; i++)
        wavetable[i] = 32767 * sin(2 * M_PI * i / N);
}



//============================================================================
// set_freq()
//============================================================================
void set_freq(int chan, float f) {
    if (chan == 0) {
        if (f == 0.0) {
            step0 = 0;
            offset0 = 0;
        } else
            step0 = (f * N / RATE) * (1<<16);
    }
    if (chan == 1) {
        if (f == 0.0) {
            step1 = 0;
            offset1 = 0;
        } else
            step1 = (f * N / RATE) * (1<<16);
    }
}



//============================================================================
// All the things you need to test your subroutines.
//============================================================================
int main(void) {
    internal_clock();

    // Uncomment autotest to get the confirmation code.
    // autotest();

    // Demonstrate part 1
#define TEST_TIMER3
#ifdef TEST_TIMER3 

    setup_tim3();
    for(;;) { } 
#endif

    // Initialize the display to something interesting to get started.
    msg[0] |= font['E'];
    msg[1] |= font['C'];
    msg[2] |= font['E'];
    msg[3] |= font[' '];
    msg[4] |= font['3'];
    msg[5] |= font['6'];
    msg[6] |= font['2'];
    msg[7] |= font[' '];

    enable_ports();
    setup_dma();
    enable_dma();
    init_tim15();
    init_tim7();
    setup_adc();
    init_tim2();
    init_wavetable();
    init_tim6();

    setup_tim1();

    // demonstrate part 2
// #define TEST_TIM1
#ifdef TEST_TIM1
    for(;;) {
        // Breathe in...
        for(float x=1; x<2400; x *= 1.1) {
            TIM1->CCR1 = TIM1->CCR2 = TIM1->CCR3 = 2400-x;
            nano_wait(100000000);
        }
        // ...and out...
        for(float x=2400; x>=1; x /= 1.1) {
            TIM1->CCR1 = TIM1->CCR2 = TIM1->CCR3 = 2400-x;
            nano_wait(100000000);
        }
        // ...and start over.
    }
#endif

    // demonstrate part 3
#define MIX_TONES
#ifdef MIX_TONES
    set_freq(0, 1000);
    for(;;) {
        char key = get_keypress();
        if (key == 'A')
            set_freq(0,getfloat());
        if (key == 'B')
            set_freq(1,getfloat());
    }
#endif

    // demonstrate part 4
// #define TEST_SETRGB
#ifdef TEST_SETRGB
    for(;;) {
        char key = get_keypress();
        if (key == 'A')
            set_freq(0,getfloat());
        if (key == 'B')
            set_freq(1,getfloat());
        if (key == 'D')
            setrgb(getrgb());
    }
#endif

    // Have fun.
    dialer();
}
