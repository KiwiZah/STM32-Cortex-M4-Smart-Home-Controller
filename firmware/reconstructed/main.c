/*
 * STM32L476G-DISCO smart-home prototype: reconstructed application main.c.
 *
 * This is NEWLY WRITTEN recovery source, not the exact historical C file.
 * Behavior and pin mapping were recovered from BSP.elf mixed disassembly;
 * the original source is missing from the supplied archives.
 *
 * Requires the official STM32CubeL4 Discovery BSP example with HAL and BSP
 * headers in the include path; see README.md for the integration steps.
 *
 * Proof of concept only: the example PIN is public, there is no persistent
 * credential storage, lockout policy, tamper resistance, or certified lock.
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "stm32l4xx_hal.h"
#include "stm32l476g_discovery.h"
#include "stm32l476g_discovery_glass_lcd.h"

#ifndef DEMO_PIN
#define DEMO_PIN "1234"
#endif
_Static_assert(sizeof(DEMO_PIN) == 5, "DEMO_PIN must contain exactly four ASCII characters");

extern int asm_check_password(const char *input, const char *password);

#define KEYPAD_ROWS 4u
#define KEYPAD_COLS 4u
#define TIM4_PWM_PERIOD 19999u
#define SERVO_CLOSED_US 1000u
#define SERVO_OPEN_US 2000u
#define LAMP_ON_DUTY 12000u

/* Project's original wiring as recovered from compiled application. */
static const uint8_t row_pins[KEYPAD_ROWS] = {11u, 10u, 12u, 14u}; /* PE11, PE10, PE12, PE14 */
static const char keymap[KEYPAD_ROWS][KEYPAD_COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'},
};

static char entered_pin[5] = {0};
static uint8_t input_index = 0;
static bool appliance_on = false;

static void SystemClock_Config(void);
static void SystemHardwareInit(void);
static void MX_TIM4_PWM_SERVO_Init(void);
static void MX_Keypad_Init(void);
static void Keypad_AllRowsLow(void);
static void Keypad_SetRowHigh(uint8_t row);
static bool Keypad_ReadColumn(uint8_t column);
static char Keypad_GetKey(void);
static void Clear_Input(void);
static void Process_Key(char key);
static void Display_Appliance_State(void);
static void Set_Lamp_Brightness(uint16_t duty);
static void Servo_SetPulse(uint16_t pulse_us);
static void Servo_Close(void);
static void Servo_Open(void);
static void Toggle_Appliance(void);
static void Error_Handler(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    SystemHardwareInit();
    MX_TIM4_PWM_SERVO_Init();
    MX_Keypad_Init();

    appliance_on = false;
    Servo_Close();
    Display_Appliance_State();

    for (;;) {
        const char key = Keypad_GetKey();
        if (key != '\0') {
            Process_Key(key);
        }
    }
}

static void SystemHardwareInit(void)
{
    /* The original integrated build did not enable the joystick EXTI callback:
       it conflicted with keypad pins. The separate BlinkTest illustrates EXTI. */
    BSP_LED_Init(LED_RED);
    BSP_LED_Init(LED_GREEN);
    BSP_LED_Off(LED_GREEN);
    BSP_LED_On(LED_RED);
    (void)BSP_LCD_GLASS_Init();
}

static void Display_Appliance_State(void)
{
    BSP_LCD_GLASS_Clear();
    if (appliance_on) {
        BSP_LED_On(LED_GREEN);
        BSP_LED_Off(LED_RED);
        BSP_LCD_GLASS_DisplayString((uint8_t *)"ON");
        Set_Lamp_Brightness(LAMP_ON_DUTY);
    } else {
        BSP_LED_Off(LED_GREEN);
        BSP_LED_On(LED_RED);
        BSP_LCD_GLASS_DisplayString((uint8_t *)"OFF");
        Set_Lamp_Brightness(0);
    }
}

static void Toggle_Appliance(void)
{
    appliance_on = !appliance_on;
    Display_Appliance_State();
    if (appliance_on) {
        Servo_Open();
    } else {
        Servo_Close();
    }
}

static void Set_Lamp_Brightness(uint16_t duty)
{
    if (duty > TIM4_PWM_PERIOD) duty = TIM4_PWM_PERIOD;
    TIM4->CCR1 = duty;                 /* TIM4_CH1: PB6, smart lamp */
}

static void Servo_SetPulse(uint16_t pulse_us)
{
    if (pulse_us < 500u) pulse_us = 500u;
    if (pulse_us > 2500u) pulse_us = 2500u;
    TIM4->CCR2 = pulse_us;             /* TIM4_CH2: PB7, servo */
}

static void Servo_Close(void)
{
    Servo_SetPulse(SERVO_CLOSED_US);
    HAL_Delay(500u);
}

static void Servo_Open(void)
{
    Servo_SetPulse(SERVO_OPEN_US);
    HAL_Delay(500u);
}

static void MX_TIM4_PWM_SERVO_Init(void)
{
    /* Original 80 MHz MCU clock / (79 + 1) -> 1 MHz timer counter.
       20,000 ticks per period -> 50 Hz, shared by lamp and servo. */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();

    GPIOB->MODER &= ~((3u << (6u * 2u)) | (3u << (7u * 2u)));
    GPIOB->MODER |=  ((2u << (6u * 2u)) | (2u << (7u * 2u)));
    GPIOB->AFR[0] &= ~((0xFu << (6u * 4u)) | (0xFu << (7u * 4u)));
    GPIOB->AFR[0] |=  ((2u << (6u * 4u)) | (2u << (7u * 4u)));

    TIM4->PSC = 79u;
    TIM4->ARR = TIM4_PWM_PERIOD;
    TIM4->CCR1 = 0u;
    TIM4->CCR2 = SERVO_CLOSED_US;

    /* PWM mode 1, OC1PE/OC2PE preload, active-high channels. */
    TIM4->CCMR1 &= ~((7u << 4u) | (1u << 3u) | (7u << 12u) | (1u << 11u));
    TIM4->CCMR1 |=  ((6u << 4u) | (1u << 3u) | (6u << 12u) | (1u << 11u));
    TIM4->CCER &= ~((1u << 1u) | (1u << 5u));
    TIM4->CCER |=  ((1u << 0u) | (1u << 4u));
    TIM4->CR1 |= TIM_CR1_ARPE;
    TIM4->EGR = TIM_EGR_UG;
    TIM4->CR1 |= TIM_CR1_CEN;
}

static void Keypad_AllRowsLow(void)
{
    for (unsigned row = 0u; row < KEYPAD_ROWS; ++row) {
        GPIOE->ODR &= ~(1u << row_pins[row]);
    }
}

static void Keypad_SetRowHigh(uint8_t row)
{
    Keypad_AllRowsLow();
    if (row < KEYPAD_ROWS) GPIOE->ODR |= 1u << row_pins[row];
}

static void MX_Keypad_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    for (unsigned row = 0u; row < KEYPAD_ROWS; ++row) {
        const uint32_t pin = row_pins[row];
        GPIOE->MODER = (GPIOE->MODER & ~(3u << (pin * 2u))) | (1u << (pin * 2u));
        GPIOE->OTYPER &= ~(1u << pin);
        GPIOE->PUPDR &= ~(3u << (pin * 2u));
    }
    const uint32_t pe_column = 15u;
    GPIOE->MODER &= ~(3u << (pe_column * 2u));
    GPIOE->PUPDR = (GPIOE->PUPDR & ~(3u << (pe_column * 2u))) |
                   (2u << (pe_column * 2u));
    for (uint32_t pin = 1u; pin <= 3u; ++pin) {
        GPIOA->MODER &= ~(3u << (pin * 2u));
        GPIOA->PUPDR = (GPIOA->PUPDR & ~(3u << (pin * 2u))) | (2u << (pin * 2u));
    }
    Keypad_AllRowsLow();
}

static bool Keypad_ReadColumn(uint8_t column)
{
    switch (column) {
        case 0: return (GPIOE->IDR & (1u << 15u)) != 0u; /* PE15 */
        case 1: return (GPIOA->IDR & (1u << 1u)) != 0u;  /* PA1  */
        case 2: return (GPIOA->IDR & (1u << 2u)) != 0u;  /* PA2  */
        case 3: return (GPIOA->IDR & (1u << 3u)) != 0u;  /* PA3  */
        default: return false;
    }
}

static char Keypad_GetKey(void)
{
    for (uint8_t row = 0u; row < KEYPAD_ROWS; ++row) {
        Keypad_SetRowHigh(row);
        for (volatile uint32_t settling = 0; settling < 3000u; ++settling) { }
        for (uint8_t col = 0u; col < KEYPAD_COLS; ++col) {
            if (Keypad_ReadColumn(col)) {
                HAL_Delay(40u);
                if (!Keypad_ReadColumn(col)) continue;
                const char result = keymap[row][col];
                /* A stuck key should not hang the controller indefinitely. */
                const uint32_t start = HAL_GetTick();
                while (Keypad_ReadColumn(col) &&
                       (uint32_t)(HAL_GetTick() - start) < 3000u) {
                    HAL_Delay(10u);
                }
                Keypad_AllRowsLow();
                HAL_Delay(120u);
                return result;
            }
        }
    }
    Keypad_AllRowsLow();
    return '\0';
}

static void Clear_Input(void)
{
    input_index = 0u;
    memset(entered_pin, 0, sizeof(entered_pin));
}

static void Display_Masked_Input(void)
{
    /* Improvement over the recovered firmware: do not reveal entered PIN. */
    char masked[5] = {0};
    for (uint8_t i = 0u; i < input_index; ++i) masked[i] = '-';
    BSP_LCD_GLASS_Clear();
    BSP_LCD_GLASS_DisplayString((uint8_t *)masked);
}

static void Process_Key(char key)
{
    if (key == '*') {
        Clear_Input();
        BSP_LCD_GLASS_Clear();
        BSP_LCD_GLASS_DisplayString((uint8_t *)"CLR");
        HAL_Delay(500u);
        Display_Appliance_State();
        return;
    }
    if (key == '#') {
        /* Assembly compares exactly four ASCII characters. */
        if (input_index == 4u && asm_check_password(entered_pin, DEMO_PIN)) {
            BSP_LCD_GLASS_Clear();
            BSP_LCD_GLASS_DisplayString((uint8_t *)"PASS");
            HAL_Delay(700u);
            Toggle_Appliance();
        } else {
            BSP_LCD_GLASS_Clear();
            BSP_LCD_GLASS_DisplayString((uint8_t *)"FAIL");
            HAL_Delay(1000u);
            Display_Appliance_State();
        }
        Clear_Input();
        return;
    }
    if (key >= '0' && key <= '9' && input_index < 4u) {
        entered_pin[input_index++] = key;
        entered_pin[input_index] = '\0';
        Display_Masked_Input();
    } else if (input_index >= 4u) {
        BSP_LCD_GLASS_Clear();
        BSP_LCD_GLASS_DisplayString((uint8_t *)"FULL");
        HAL_Delay(500u);
        Display_Masked_Input();
    }
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef oscillator = {0};
    RCC_ClkInitTypeDef clocks = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK) Error_Handler();
    __HAL_RCC_PWR_CLK_DISABLE();

    oscillator.OscillatorType = RCC_OSCILLATORTYPE_MSI;
    oscillator.MSIState = RCC_MSI_ON;
    oscillator.MSIClockRange = RCC_MSIRANGE_7;
    oscillator.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
    oscillator.PLL.PLLState = RCC_PLL_ON;
    oscillator.PLL.PLLSource = RCC_PLLSOURCE_MSI;
    oscillator.PLL.PLLM = 1;
    oscillator.PLL.PLLN = 20;
    oscillator.PLL.PLLP = 7;
    oscillator.PLL.PLLQ = 4;
    oscillator.PLL.PLLR = 2;
    if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) Error_Handler();

    clocks.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                       RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clocks.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clocks.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clocks.APB1CLKDivider = RCC_HCLK_DIV1;
    clocks.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clocks, FLASH_LATENCY_4) != HAL_OK) Error_Handler();
}

/* Matches the recovered compiled integrated application: keypad scanning
   is used, and the separate joystick interrupt is not initialized here. */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    (void)GPIO_Pin;
}

static void Error_Handler(void)
{
    __disable_irq();
    for (;;) { }
}
