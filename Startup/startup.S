.section .text
.global _start
_start:
    @ Set up stack pointer for different modes
    cps #0x12          @ Switch to IRQ mode
    ldr sp, =irq_stack_top
    
    cps #0x13          @ Switch to SVC mode (supervisor)
    ldr sp, =stack_top
    
    @ Initialize BSS section (zero out uninitialized data including FreeRTOS heap)
    ldr r0, =__bss_start__
    ldr r1, =__bss_end__
    mov r2, #0
bss_clear_loop:
    cmp r0, r1
    strcc r2, [r0], #4
    bcc bss_clear_loop
    
    @ Call main function
    b main

.section .bss
.align 3
stack_base:
    .space 8192        @ 8KB stack
stack_top:

irq_stack_base:
    .space 4096        @ 4KB IRQ stack
irq_stack_top: