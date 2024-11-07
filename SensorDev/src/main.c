/**
	******************************************************************************
	* @file    main.c
	* @author  Ac6
	* @version V1.0
	* @date    01-December-2013
	* @brief   Default main function.
	******************************************************************************
*/

#include "stm32f0xx.h"

void check_wiring(void);
void grader(void);

// nano wait function is already defined in either hwready or grader
void nano_wait(unsigned int n) {
		asm(    "        mov r0,%0\n"
						"repeat: sub r0,#83\n"
						"        bgt repeat\n" : : "r"(n) : "r0", "cc");
}

/*******************************************************
 *
 * Only type below this section.
 *
*******************************************************/

// STEP 1
// Implement init_tim1.
void init_tim1() {
	// Enable GPIOA clock
	RCC->AHBENR |= RCC_AHBENR_GPIOAEN;

	// Set PA8, PA9, PA10 to alternate function mode (AF2)
	GPIOA->MODER &= ~(GPIO_MODER_MODER8 | GPIO_MODER_MODER9 | GPIO_MODER_MODER10);
	GPIOA->MODER |= GPIO_MODER_MODER8_1 | GPIO_MODER_MODER9_1 | GPIO_MODER_MODER10_1;
	GPIOA->AFR[1] &= ~(0x00000FFF);
	GPIOA->AFR[1] |= 0x00000222;

	// Enable TIM1 clock
	RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;

	// Set TIM1 to upcounting mode
	TIM1->CR1 &= ~TIM_CR1_DIR;

	// Set prescaler to 47 (PSC = 47)
	TIM1->PSC = 47;

	// Set auto-reload register to 999 (ARR = 999)
	TIM1->ARR = 999;

	// Configure TIM1 channels for PWM mode 1
	TIM1->CCMR1 &= ~(TIM_CCMR1_OC1M | TIM_CCMR1_OC2M);
	TIM1->CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2;
	TIM1->CCMR2 &= ~TIM_CCMR2_OC3M;
	TIM1->CCMR2 |= TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2;

	// Enable output for TIM1 channels
	TIM1->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E;

	// Enable main output
	TIM1->BDTR |= TIM_BDTR_MOE;

	// Initialize CCR registers
	TIM1->CCR1 = 999;
	TIM1->CCR2 = 999;
	TIM1->CCR3 = 0;

	// Enable TIM1 counter
	TIM1->CR1 |= TIM_CR1_CEN;
}

// STEP 2
// Implement init_exti.
void init_exti() {
	// Enable clocks for GPIOA, GPIOB, and SYSCFG
	RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN;
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;


	// // Configure PA0 as input with pull-down resistor
	GPIOA->MODER &= ~GPIO_MODER_MODER0; // Input mode
	GPIOA->PUPDR &= ~GPIO_PUPDR_PUPDR0;
	GPIOA->PUPDR |= GPIO_PUPDR_PUPDR0_1; // Pull-down

	// Configure PB2 as input with pull-down resistor
	GPIOB->MODER &= ~GPIO_MODER_MODER2; // Input mode
	GPIOB->PUPDR &= ~GPIO_PUPDR_PUPDR2;
	GPIOB->PUPDR |= GPIO_PUPDR_PUPDR2_1; // Pull-down

	// Connect PA0 to EXTI0
	SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI0;
	SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI0_PA;

	// Connect PB2 to EXTI2
	SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI2;
	SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI2_PB;

	// Set EXTI0 to rising edge trigger
	EXTI->RTSR |= EXTI_RTSR_TR0;

	// Set EXTI2 to rising edge trigger
	EXTI->RTSR |= EXTI_RTSR_TR2;

	// Enable EXTI0 and EXTI2 interrupts
	EXTI->IMR |= EXTI_IMR_MR0 | EXTI_IMR_MR2;

	

	// Enable EXTI0 and EXTI2 interrupts in NVIC
	NVIC_EnableIRQ(EXTI0_1_IRQn);
	NVIC_EnableIRQ(EXTI2_3_IRQn);
}

uint32_t WAIT_TIME = 1000000;

// STEP 3
// Write your exception handler for PA0.
void EXTI0_1_IRQHandler(void) {
	EXTI->PR |= EXTI_PR_PR0; // Clear pending interrupt for PA0
	WAIT_TIME = WAIT_TIME + 50000;
}

// STEP 4
// Write your exception handler for PB2.
void EXTI2_3_IRQHandler(void) {
	EXTI->PR |= EXTI_PR_PR2; // Clear pending interrupt for PB2
	// If WAIT_TIME is greater than 50000, decrement it by 50000
	WAIT_TIME = (WAIT_TIME > 50000) ? WAIT_TIME - 50000 : WAIT_TIME;

}

//code so i can toggle gpioc pin 14 on and off to control a heating element

void init_gpio(void) {
	// Enable GPIOC clock
	RCC->AHBENR |= RCC_AHBENR_GPIOCEN;

	// Set PC13 to output mode
	GPIOC->MODER &= ~GPIO_MODER_MODER13;
	GPIOC->MODER |= GPIO_MODER_MODER13_0;
}

// Function to toggle GPIOC pin 13 on and off
void toggle_gpio(void) {
	// Toggle PC13
	GPIOC->ODR ^= GPIO_ODR_13;
}


int main(void) {
	init_gpio();

	for(;;) {
		toggle_gpio(); // Turn on heating element
		nano_wait(10000000000); // Wait for 30 seconds
		toggle_gpio(); // Turn off heating element
		nano_wait(500000000); // Wait for 5 seconds
	}


}


/***********************************************************
 *
 * Only change the check_wiring call below this section.
 *
***********************************************************/

// int main(void)
// {
// 	// comment out after hardware readiness check
// 		// and make sure hwready.o is included in platformio.ini
// 				internal_clock();
// 	// check_wiring();

// 	/////////////////////////////////////////////////////////

// 	init_tim1();
// 	init_exti();
// 	// DO NOT MOVE OR CHANGE THE grader FUNCTION CALL.
// 		// Make sure grader.o REPLACES hwready.o in platformio.ini to use this.
// 	grader();
// 	int state = 0;
// 	uint32_t* r1 = (uint32_t*) &TIM1->CCR3;
// 	uint32_t* r2 = (uint32_t*) &TIM1->CCR2;
// 	uint32_t* r3 = (uint32_t*) &TIM1->CCR1;
// 	int ARR = TIM1->ARR;
// 		for(;;) {
// 			if (state == 0) {
// 				*r1 += 1;
// 				state = (*r1 == (ARR / 8)) ? 1 : 0;
// 			}
// 			else if (state == 1) {
// 				*r1 = (*r1 >= (ARR-1)) ? ARR : (*r1 + 1);
// 				*r2 -= 1;
// 				if (*r2 == 0) {
// 					uint32_t* tmp = r1;
// 					r1 = r2;
// 					r2 = r3;
// 					r3 = tmp;
// 					state = 0;
// 				}
// 			}
// 			nano_wait(WAIT_TIME);
// 		}
// }
