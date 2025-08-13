.section .text
.global _start
_start:
    @ Set up stack pointer for different modes
    cps #0x12          @ Switch to IRQ mode
    ldr sp, =irq_stack_top
    
    cps #0x13          @ Switch to SVC mode (supervisor)
    ldr sp, =stack_top
    
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