.section .text
.global _start
_start:
    @ Install our vector table using VBAR
    ldr r0, =_freertos_vector_table
    mcr p15, 0, r0, c12, c0, 0  @ Set VBAR (Vector Base Address Register)
    
    @ Set up stack pointer for different modes
    cps #0x12          @ Switch to IRQ mode
    ldr sp, =irq_stack_top
    
    cps #0x13          @ Switch to SVC mode (supervisor)
    ldr sp, =stack_top
    
    @ Call main function
    b main

@ Exception handlers
undefined_handler:
    b undefined_handler

prefetch_abort_handler:
    b prefetch_abort_handler

data_abort_handler:
    b data_abort_handler

fiq_handler:
    b fiq_handler

.section .bss
.align 3
stack_base:
    .space 8192        @ 8KB stack
stack_top:

irq_stack_base:
    .space 4096        @ 4KB IRQ stack
irq_stack_top: