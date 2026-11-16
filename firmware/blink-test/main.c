#include <stdint.h>

/*
 * Smart Home Controller - External Interrupt Fixed Version
 * Board: STM32L476G-DISCO
 *
 * User/Wake-up button = PA0
 * LD4 red LED         = PB2
 * LD5 green LED       = PE8
 *
 * Function:
 * - Short button press also works.
 * - Each press toggles appliance ON/OFF.
 * - Green LED = appliance ON
 * - Red LED   = appliance OFF
 */

/* RCC registers */
#define RCC_BASE        0x40021000UL
#define RCC_AHB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x4C))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x60))

/* GPIOA for button PA0 */
#define GPIOA_BASE      0x48000000UL
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_PUPDR     (*(volatile uint32_t *)(GPIOA_BASE + 0x0C))
#define GPIOA_IDR       (*(volatile uint32_t *)(GPIOA_BASE + 0x10))

/* GPIOB for red LED PB2 */
#define GPIOB_BASE      0x48000400UL
#define GPIOB_MODER     (*(volatile uint32_t *)(GPIOB_BASE + 0x00))
#define GPIOB_ODR       (*(volatile uint32_t *)(GPIOB_BASE + 0x14))

/* GPIOE for green LED PE8 */
#define GPIOE_BASE      0x48001000UL
#define GPIOE_MODER     (*(volatile uint32_t *)(GPIOE_BASE + 0x00))
#define GPIOE_ODR       (*(volatile uint32_t *)(GPIOE_BASE + 0x14))

/* SYSCFG for EXTI */
#define SYSCFG_BASE     0x40010000UL
#define SYSCFG_EXTICR1  (*(volatile uint32_t *)(SYSCFG_BASE + 0x08))

/* EXTI registers */
#define EXTI_BASE       0x40010400UL
#define EXTI_IMR1       (*(volatile uint32_t *)(EXTI_BASE + 0x00))
#define EXTI_RTSR1      (*(volatile uint32_t *)(EXTI_BASE + 0x08))
#define EXTI_FTSR1      (*(volatile uint32_t *)(EXTI_BASE + 0x0C))
#define EXTI_PR1        (*(volatile uint32_t *)(EXTI_BASE + 0x14))

/* NVIC */
#define NVIC_ISER0      (*(volatile uint32_t *)0xE000E100UL)
#define EXTI0_IRQn      6

volatile uint8_t appliance_on = 0;
volatile uint8_t button_event = 0;

/* Small delay for debounce */
void delay(void)
{
    for (volatile uint32_t i = 0; i < 80000; i++)
    {
        /* wait */
    }
}

/* Interrupt handler for PA0 button */
void EXTI0_IRQHandler(void)
{
    if (EXTI_PR1 & (1 << 0))
    {
        /* Clear interrupt flag */
        EXTI_PR1 = (1 << 0);

        /* Temporarily disable EXTI0 to avoid button bounce */
        EXTI_IMR1 &= ~(1 << 0);

        /* Tell main loop that button was pressed */
        button_event = 1;
    }
}

int main(void)
{
    /* Enable clocks */
    RCC_AHB2ENR |= (1 << 0);   // GPIOA clock
    RCC_AHB2ENR |= (1 << 1);   // GPIOB clock
    RCC_AHB2ENR |= (1 << 4);   // GPIOE clock
    RCC_APB2ENR |= (1 << 0);   // SYSCFG clock

    /* PA0 input */
    GPIOA_MODER &= ~(3 << (0 * 2));

    /* PA0 pull-down */
    GPIOA_PUPDR &= ~(3 << (0 * 2));
    GPIOA_PUPDR |=  (2 << (0 * 2));

    /* PB2 output: red LED */
    GPIOB_MODER &= ~(3 << (2 * 2));
    GPIOB_MODER |=  (1 << (2 * 2));

    /* PE8 output: green LED */
    GPIOE_MODER &= ~(3 << (8 * 2));
    GPIOE_MODER |=  (1 << (8 * 2));

    /* Connect EXTI0 to PA0 */
    SYSCFG_EXTICR1 &= ~(0xF << 0);

    /* Configure EXTI0 */
    EXTI_IMR1  |=  (1 << 0);   // Enable EXTI0 interrupt line
    EXTI_RTSR1 |=  (1 << 0);   // Rising edge: button press
    EXTI_FTSR1 &= ~(1 << 0);   // Falling edge disabled

    /* Enable EXTI0 in NVIC */
    NVIC_ISER0 |= (1 << EXTI0_IRQn);

    while (1)
    {
        if (button_event)
        {
            button_event = 0;

            /* Toggle appliance immediately */
            appliance_on = !appliance_on;

            /* Wait until button is released */
            while (GPIOA_IDR & (1 << 0))
            {
                /* wait */
            }

            /* Small debounce delay after release */
            delay();

            /* Clear any extra pending interrupt caused by bouncing */
            EXTI_PR1 = (1 << 0);

            /* Re-enable EXTI0 */
            EXTI_IMR1 |= (1 << 0);
        }

        /* Update LEDs */
        if (appliance_on)
        {
            GPIOE_ODR |=  (1 << 8);   // green ON
            GPIOB_ODR &= ~(1 << 2);   // red OFF
        }
        else
        {
            GPIOE_ODR &= ~(1 << 8);   // green OFF
            GPIOB_ODR |=  (1 << 2);   // red ON
        }
    }
}
