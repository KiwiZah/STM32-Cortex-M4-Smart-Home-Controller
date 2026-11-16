.syntax unified
.cpu cortex-m4
.thumb

.global asm_check_password
.type asm_check_password, %function

/*
 * int asm_check_password(char *input, char *password)
 *
 * R0 = input pointer
 * R1 = password pointer
 * return R0 = 1 if same, 0 if different
 *
 * Checks exactly 4 characters.
 */

asm_check_password:
    push {r4, lr}

    movs r2, #0          /* index = 0 */

check_loop:
    cmp r2, #4
    beq password_ok

    ldrb r3, [r0, r2]    /* input[index] */
    ldrb r4, [r1, r2]    /* password[index] */

    cmp r3, r4
    bne password_fail

    adds r2, r2, #1
    b check_loop

password_ok:
    movs r0, #1
    pop {r4, pc}

password_fail:
    movs r0, #0
    pop {r4, pc}
