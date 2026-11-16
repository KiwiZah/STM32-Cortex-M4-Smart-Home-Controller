# Hardware and source notes

**Target:** ST STM32L476G Discovery board (ARM Cortex-M4), as documented in the recovered project configurations and startup assembly.

## What the final archived BSP firmware implements

These are the behaviors visible in the compiled `BSP.elf` mixed disassembly and its accompanying handwritten Assembly file:

| Peripheral | Connection / operation | Evidence |
|---|---|---|
| 4 x 4 keypad | PE11, PE10, PE12, PE14 row outputs; PE15, PA1, PA2, PA3 column inputs | `MX_Keypad_Init`, `Keypad_GetKey` |
| PIN | Exactly four characters verified through `asm_check_password`; `#` confirms, `*` clears | `Process_Key`, `password_check.s` |
| Segmented LCD | Shows ON, OFF, PASS, FAIL, CLR and PIN-entry feedback | `BSP_LCD_GLASS_*` calls |
| Red and green LEDs | Red when appliance OFF; green when appliance ON | `Display_Appliance_State` |
| Servo | PB7 / TIM4 CH2, 1000 us closed, 2000 us open | `MX_TIM4_PWM_SERVO_Init`, `Servo_SetPulse` |
| Lamp PWM | PB6 / TIM4 CH1, 0 when off, 12000 of 19999 ticks when on | `Set_Lamp_Brightness` |
| Clock | MSI PLL with 80 MHz system clock, TIM4 prescaler 79 and period 19999 | `SystemClock_Config`, `MX_TIM4_PWM_SERVO_Init` |
| Interrupt experiment | PA0 / EXTI0 toggles red and green LEDs in a **separate** test project | `firmware/blink-test/main.c` |

The completed hardware prototype reportedly functioned during the university demonstration, **according to the original student**. This repository reconstruction was not physically retested.

## Important distinction: proposal vs recovered build

The earlier proposal and slides present a broader **stepper-motor door lock plus ultrasonic smart fan** with mode switching. The **recovered final archived firmware** instead shows an implemented servo, keypad, LCD, PWM lamp and LEDs. A separate code artifact demonstrates a push-button EXTI interrupt. The archives do **not** establish that an ultrasonic fan subsystem or the described two-mode controller is in the final archived binary. We retain the proposal as historical context, not proof of those features.

## Source provenance

- `firmware/reconstructed/password_check.s` is the recovered original handwritten ARM Thumb Assembly file (as provided).
- `firmware/reconstructed/main.c` is newly written **source reconstruction**, derived from function names, instructions, source comments and constants in the historical compiled mixed disassembly. It is **not** an exact preserved copy of the original missing `BSP/Src/main.c`.
- The reconstruction uses STM32 HAL/BSP register symbols and will require the matching ST STM32CubeL4 Discovery example sources and headers in order to build. This package does **not** claim a successful rebuild of the reconstructed application.
- `firmware/blink-test/` contains the original source, startup assembly, linker scripts, and STM32CubeIDE project configuration for the independent interrupt/LED exercise.
- `firmware/reconstructed/main.c` intentionally improves PIN display privacy (masking) and avoids a permanently blocking keypad-release loop. These specific changes are **not** verified on the hardware.

## Safety / security

- `DEMO_PIN` defaults to a public illustrative four-digit value for a classroom demonstration only; it is **not** a secure access-control system.
- The external servo and lamp require proper current-rated power/driver circuitry; never connect a motor or load directly to an STM32 GPIO.
- The shared TIM4 50 Hz PWM period is appropriate for the servo, but can cause visible lamp flicker. Do not describe it as calibrated brightness control.
- Use your original PCB/wiring for the precise electronics setup; this recovery provides only pins inferred from the compiled code.
- Do not publish original executable files or disassembly without checking embedded paths, debug metadata and personal data.
