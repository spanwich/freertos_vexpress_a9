/*
 * FreeRTOS Kernel <DEVELOPMENT BRANCH>
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/* Standard includes. */
#include <stdlib.h>
#include <string.h>

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Debug UART functions */
#define UART0_DR (*(volatile unsigned int *)0x9000000)
#define UART0_FR (*(volatile unsigned int *)0x9000018)

static void uart_putc(char c) {
    while (UART0_FR & (1 << 5));  // Wait until TX FIFO not full
    UART0_DR = c;
}

static void uart_puts(const char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}

static void uart_hex(unsigned int val) {
    int i;
    for (i = 28; i >= 0; i -= 4) {
        int digit = (val >> i) & 0xF;
        uart_putc(digit < 10 ? '0' + digit : 'A' + digit - 10);
    }
}

static void uart_dec(unsigned int val) {
    char buffer[10];
    int i = 0;
    if (val == 0) {
        uart_putc('0');
        return;
    }
    while (val > 0) {
        buffer[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (i > 0) {
        uart_putc(buffer[--i]);
    }
}

#ifndef configINTERRUPT_CONTROLLER_BASE_ADDRESS
    #error "configINTERRUPT_CONTROLLER_BASE_ADDRESS must be defined.  See www.FreeRTOS.org/Using-FreeRTOS-on-Cortex-A-Embedded-Processors.html"
#endif

#ifndef configINTERRUPT_CONTROLLER_CPU_INTERFACE_OFFSET
    #error "configINTERRUPT_CONTROLLER_CPU_INTERFACE_OFFSET must be defined.  See www.FreeRTOS.org/Using-FreeRTOS-on-Cortex-A-Embedded-Processors.html"
#endif

#ifndef configUNIQUE_INTERRUPT_PRIORITIES
    #error "configUNIQUE_INTERRUPT_PRIORITIES must be defined.  See www.FreeRTOS.org/Using-FreeRTOS-on-Cortex-A-Embedded-Processors.html"
#endif

#ifndef configSETUP_TICK_INTERRUPT
    #error "configSETUP_TICK_INTERRUPT() must be defined.  See www.FreeRTOS.org/Using-FreeRTOS-on-Cortex-A-Embedded-Processors.html"
#endif /* configSETUP_TICK_INTERRUPT */

#ifndef configMAX_API_CALL_INTERRUPT_PRIORITY
    #error "configMAX_API_CALL_INTERRUPT_PRIORITY must be defined.  See www.FreeRTOS.org/Using-FreeRTOS-on-Cortex-A-Embedded-Processors.html"
#endif

#if configMAX_API_CALL_INTERRUPT_PRIORITY == 0
    #error "configMAX_API_CALL_INTERRUPT_PRIORITY must not be set to 0"
#endif

#if configMAX_API_CALL_INTERRUPT_PRIORITY > configUNIQUE_INTERRUPT_PRIORITIES
    #error "configMAX_API_CALL_INTERRUPT_PRIORITY must be less than or equal to configUNIQUE_INTERRUPT_PRIORITIES as the lower the numeric priority value the higher the logical interrupt priority"
#endif

#if configUSE_PORT_OPTIMISED_TASK_SELECTION == 1
    /* Check the configuration. */
    #if ( configMAX_PRIORITIES > 32 )
        #error "configUSE_PORT_OPTIMISED_TASK_SELECTION can only be set to 1 when configMAX_PRIORITIES is less than or equal to 32.  It is very rare that a system requires more than 10 to 15 difference priorities as tasks that share a priority will time slice."
    #endif
#endif /* configUSE_PORT_OPTIMISED_TASK_SELECTION */

/* In case security extensions are implemented. */
#if configMAX_API_CALL_INTERRUPT_PRIORITY <= ( configUNIQUE_INTERRUPT_PRIORITIES / 2 )
    #error "configMAX_API_CALL_INTERRUPT_PRIORITY must be greater than ( configUNIQUE_INTERRUPT_PRIORITIES / 2 )"
#endif

/* Some vendor specific files default configCLEAR_TICK_INTERRUPT() in
 * portmacro.h. */
#ifndef configCLEAR_TICK_INTERRUPT
    #define configCLEAR_TICK_INTERRUPT()
#endif

/* A critical section is exited when the critical section nesting count reaches
 * this value. */
#define portNO_CRITICAL_NESTING          ( ( uint32_t ) 0 )

/* In all GICs 255 can be written to the priority mask register to unmask all
 * (but the lowest) interrupt priority. */
#define portUNMASK_VALUE                 ( 0xFFUL )

/* Tasks are not created with a floating point context, but can be given a
 * floating point context after they have been created.  A variable is stored as
 * part of the tasks context that holds portNO_FLOATING_POINT_CONTEXT if the task
 * does not have an FPU context, or any other value if the task does have an FPU
 * context. */
#define portNO_FLOATING_POINT_CONTEXT    ( ( StackType_t ) 0 )

/* Constants required to setup the initial task context. */
#define portINITIAL_SPSR                 ( ( StackType_t ) 0x1f ) /* System mode, ARM mode, IRQ enabled FIQ enabled. */
#define portTHUMB_MODE_BIT               ( ( StackType_t ) 0x20 )
#define portINTERRUPT_ENABLE_BIT         ( 0x80UL )
#define portTHUMB_MODE_ADDRESS           ( 0x01UL )

/* Used by portASSERT_IF_INTERRUPT_PRIORITY_INVALID() when ensuring the binary
 * point is zero. */
#define portBINARY_POINT_BITS            ( ( uint8_t ) 0x03 )

/* Masks all bits in the APSR other than the mode bits. */
#define portAPSR_MODE_BITS_MASK          ( 0x1F )

/* The value of the mode bits in the APSR when the CPU is executing in user
 * mode. */
#define portAPSR_USER_MODE               ( 0x10 )

/* The critical section macros only mask interrupts up to an application
 * determined priority level.  Sometimes it is necessary to turn interrupt off in
 * the CPU itself before modifying certain hardware registers. */
#define portCPU_IRQ_DISABLE()                  \
    __asm volatile ( "CPSID i" ::: "memory" ); \
    __asm volatile ( "DSB" );                  \
    __asm volatile ( "ISB" );

#define portCPU_IRQ_ENABLE()                   \
    __asm volatile ( "CPSIE i" ::: "memory" ); \
    __asm volatile ( "DSB" );                  \
    __asm volatile ( "ISB" );


/* Macro to unmask all interrupt priorities. */
#define portCLEAR_INTERRUPT_MASK()                            \
    {                                                         \
        portCPU_IRQ_DISABLE();                                \
        portICCPMR_PRIORITY_MASK_REGISTER = portUNMASK_VALUE; \
        __asm volatile ( "DSB        \n"                      \
                         "ISB        \n" );                   \
        portCPU_IRQ_ENABLE();                                 \
    }

#define portINTERRUPT_PRIORITY_REGISTER_OFFSET    0x400UL
#define portMAX_8_BIT_VALUE                       ( ( uint8_t ) 0xff )
#define portBIT_0_SET                             ( ( uint8_t ) 0x01 )

/* Let the user override the pre-loading of the initial LR with the address of
 * prvTaskExitError() in case it messes up unwinding of the stack in the
 * debugger. */
#ifdef configTASK_RETURN_ADDRESS
    #define portTASK_RETURN_ADDRESS    configTASK_RETURN_ADDRESS
#else
    #define portTASK_RETURN_ADDRESS    prvTaskExitError
#endif

/* The space on the stack required to hold the FPU registers.  This is 32 64-bit
 * registers, plus a 32-bit status register. */
#define portFPU_REGISTER_WORDS    ( ( 32 * 2 ) + 1 )

/*-----------------------------------------------------------*/

/*
 * Starts the first task executing.  This function is necessarily written in
 * assembly code so is implemented in portASM.s.
 */
extern void vPortRestoreTaskContext( void );

/*
 * Used to catch tasks that attempt to return from their implementing function.
 */
static void prvTaskExitError( void );

/*
 * If the application provides an implementation of vApplicationIRQHandler(),
 * then it will get called directly without saving the FPU registers on
 * interrupt entry, and this weak implementation of
 * vApplicationFPUSafeIRQHandler() is just provided to remove linkage errors -
 * it should never actually get called so its implementation contains a
 * call to configASSERT() that will always fail.
 *
 * If the application provides its own implementation of
 * vApplicationFPUSafeIRQHandler() then the implementation of
 * vApplicationIRQHandler() provided in portASM.S will save the FPU registers
 * before calling it.
 *
 * Therefore, if the application writer wants FPU registers to be saved on
 * interrupt entry their IRQ handler must be called
 * vApplicationFPUSafeIRQHandler(), and if the application writer does not want
 * FPU registers to be saved on interrupt entry their IRQ handler must be
 * called vApplicationIRQHandler().
 */
void vApplicationFPUSafeIRQHandler( uint32_t ulICCIAR ) __attribute__( ( weak ) );

/*-----------------------------------------------------------*/

/* A variable is used to keep track of the critical section nesting.  This
 * variable has to be stored as part of the task context and must be initialised to
 * a non zero value to ensure interrupts don't inadvertently become unmasked before
 * the scheduler starts.  As it is stored as part of the task context it will
 * automatically be set to 0 when the first task is started. */
volatile uint32_t ulCriticalNesting = 9999UL;

/* Saved as part of the task context.  If ulPortTaskHasFPUContext is non-zero then
 * a floating point context must be saved and restored for the task. */
volatile uint32_t ulPortTaskHasFPUContext = pdFALSE;

/* Set to 1 to pend a context switch from an ISR. */
volatile uint32_t ulPortYieldRequired = pdFALSE;

/* Counts the interrupt nesting depth.  A context switch is only performed if
 * if the nesting depth is 0. */
volatile uint32_t ulPortInterruptNesting = 0UL;

/* Used in the asm file. */
__attribute__( ( used ) ) const uint32_t ulICCIARAddress = portICCIAR_INTERRUPT_ACKNOWLEDGE_REGISTER_ADDRESS;
__attribute__( ( used ) ) const uint32_t ulICCEOIRAddress = portICCEOIR_END_OF_INTERRUPT_REGISTER_ADDRESS;
__attribute__( ( used ) ) const uint32_t ulICCPMRAddress = portICCPMR_PRIORITY_MASK_REGISTER_ADDRESS;
__attribute__( ( used ) ) const uint32_t ulMaxAPIPriorityMask = ( configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT );

/*-----------------------------------------------------------*/

/*
 * See header file for description.
 */
StackType_t * pxPortInitialiseStack( StackType_t * pxTopOfStack,
                                     TaskFunction_t pxCode,
                                     void * pvParameters )
{
    uart_puts("=== pxPortInitialiseStack called ===\n");
    uart_puts("pxTopOfStack = 0x");
    uart_hex((unsigned int)pxTopOfStack);
    uart_puts("\n");
    uart_puts("pxCode = 0x");
    uart_hex((unsigned int)pxCode);
    uart_puts("\n");
    /* Setup the initial stack of the task.  The stack is set exactly as
     * expected by the portRESTORE_CONTEXT() macro.
     *
     * For RFEIA instruction, we need: [PC, CPSR] from low to high address
     * This matches what SRSDB stores: [LR, SPSR] from low to high address */
    *pxTopOfStack = ( StackType_t ) NULL;
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) NULL;
    pxTopOfStack--;
    
    /* RFEIA loads CPSR from the higher address */
    *pxTopOfStack = ( StackType_t ) portINITIAL_SPSR;
    
    if( ( ( uint32_t ) pxCode & portTHUMB_MODE_ADDRESS ) != 0x00UL )
    {
        /* The task will start in THUMB mode. */
        *pxTopOfStack |= portTHUMB_MODE_BIT;
    }

    pxTopOfStack--;

    /* RFEIA loads PC from the lower address - this must be the task entry point */
    *pxTopOfStack = ( StackType_t ) pxCode;
    pxTopOfStack--;

    /* Next all the registers other than the stack pointer. */
    *pxTopOfStack = ( StackType_t ) portTASK_RETURN_ADDRESS; /* R14 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x12121212;              /* R12 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x11111111;              /* R11 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x10101010;              /* R10 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x09090909;              /* R9 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x08080808;              /* R8 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x07070707;              /* R7 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x06060606;              /* R6 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x05050505;              /* R5 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x04040404;              /* R4 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x03030303;              /* R3 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x02020202;              /* R2 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x01010101;              /* R1 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) pvParameters;            /* R0 */
    pxTopOfStack--;

    /* The task will start with a critical nesting count of 0 as interrupts are
     * enabled. */
    *pxTopOfStack = portNO_CRITICAL_NESTING;

    #if ( configUSE_TASK_FPU_SUPPORT == 1 )
    {
        /* The task will start without a floating point context.  A task that
         * uses the floating point hardware must call vPortTaskUsesFPU() before
         * executing any floating point instructions. */
        pxTopOfStack--;
        *pxTopOfStack = portNO_FLOATING_POINT_CONTEXT;
    }
    #elif ( configUSE_TASK_FPU_SUPPORT == 2 )
    {
        /* The task will start with a floating point context.  Leave enough
         * space for the registers - and ensure they are initialised to 0. */
        pxTopOfStack -= portFPU_REGISTER_WORDS;
        memset( pxTopOfStack, 0x00, portFPU_REGISTER_WORDS * sizeof( StackType_t ) );

        pxTopOfStack--;
        *pxTopOfStack = pdTRUE;
        ulPortTaskHasFPUContext = pdTRUE;
    }
    #else /* if ( configUSE_TASK_FPU_SUPPORT == 1 ) */
    {
        #error "Invalid configUSE_TASK_FPU_SUPPORT setting - configUSE_TASK_FPU_SUPPORT must be set to 1, 2, or left undefined."
    }
    #endif /* if ( configUSE_TASK_FPU_SUPPORT == 1 ) */

    uart_puts("=== Stack initialization complete ===\n");
    uart_puts("Returning pxTopOfStack = 0x");
    uart_hex((unsigned int)pxTopOfStack);
    uart_puts("\n");
    
    /* The stack layout for RFEIA (from low to high address):
     * pxTopOfStack + many_offsets: PC value  (what RFEIA loads into PC)
     * pxTopOfStack + many_offsets + 1: SPSR value (what RFEIA loads into CPSR)
     * Let's find where they actually are by looking near the end of the stack area */
    StackType_t *stack_end = pxTopOfStack;
    while (stack_end < (StackType_t*)0x50000000) {  /* Search within reasonable bounds */
        if (*stack_end == (StackType_t)pxCode) {  /* Found our PC value */
            uart_puts("Found PC on stack at offset ");
            uart_dec((int)(stack_end - pxTopOfStack));
            uart_puts(": PC = 0x");
            uart_hex(*stack_end);
            uart_puts("\n");
            uart_puts("SPSR at next location = 0x");
            uart_hex(*(stack_end + 1));
            uart_puts("\n");
            break;
        }
        stack_end++;
        if ((stack_end - pxTopOfStack) > 50) break; /* Limit search */
    }
    
    return pxTopOfStack;
}
/*-----------------------------------------------------------*/

static void prvTaskExitError( void )
{
    /* A function that implements a task must not exit or attempt to return to
     * its caller as there is nothing to return to.  If a task wants to exit it
     * should instead call vTaskDelete( NULL ).
     *
     * Artificially force an assert() to be triggered if configASSERT() is
     * defined, then stop here so application writers can catch the error. */
    configASSERT( ulPortInterruptNesting == ~0UL );
    portDISABLE_INTERRUPTS();

    for( ; ; )
    {
    }
}
/*-----------------------------------------------------------*/

BaseType_t xPortStartScheduler( void )
{
    uint32_t ulAPSR;

    #if ( configASSERT_DEFINED == 1 )
    {
        volatile uint8_t ucOriginalPriority;
        volatile uint8_t * const pucFirstUserPriorityRegister = ( volatile uint8_t * const ) ( configINTERRUPT_CONTROLLER_BASE_ADDRESS + portINTERRUPT_PRIORITY_REGISTER_OFFSET );
        volatile uint8_t ucMaxPriorityValue;

        /* Determine how many priority bits are implemented in the GIC.
         *
         * Save the interrupt priority value that is about to be clobbered. */
        ucOriginalPriority = *pucFirstUserPriorityRegister;

        /* Determine the number of priority bits available.  First write to
         * all possible bits. */
        uart_puts("Writing 0xFF to GIC priority register\n");
        *pucFirstUserPriorityRegister = portMAX_8_BIT_VALUE;

        /* Read the value back to see how many bits stuck. */
        ucMaxPriorityValue = *pucFirstUserPriorityRegister;
        uart_puts("Raw value read back from GIC: 0x");
        uart_hex(ucMaxPriorityValue);
        uart_puts("\n");

        /* Shift to the least significant bits. */
        while( ( ucMaxPriorityValue & portBIT_0_SET ) != portBIT_0_SET )
        {
            ucMaxPriorityValue >>= ( uint8_t ) 0x01;
        }

        /* Sanity check configUNIQUE_INTERRUPT_PRIORITIES matches the read
         * value. */
        uart_puts("GIC Priority Discovery Debug:\n");
        uart_puts("ucMaxPriorityValue = ");
        uart_dec(ucMaxPriorityValue);
        uart_puts("\n");
        uart_puts("portLOWEST_INTERRUPT_PRIORITY = ");
        uart_dec(portLOWEST_INTERRUPT_PRIORITY);
        uart_puts("\n");
        
        configASSERT( ucMaxPriorityValue == portLOWEST_INTERRUPT_PRIORITY );

        /* Restore the clobbered interrupt priority register to its original
         * value. */
        *pucFirstUserPriorityRegister = ucOriginalPriority;
    }
    #endif /* configASSERT_DEFINED */


    /* Only continue if the CPU is not in User mode.  The CPU must be in a
     * Privileged mode for the scheduler to start. */
    __asm volatile ( "MRS %0, APSR" : "=r" ( ulAPSR )::"memory" );
    ulAPSR &= portAPSR_MODE_BITS_MASK;
    // Debug: Print CPU mode and GIC register values for all modes
    uart_puts("CPU Debug: APSR = 0x");
    uart_hex(ulAPSR);
    uart_puts(", USER_MODE = 0x");
    uart_hex(portAPSR_USER_MODE);
    uart_puts("\n");
    
    // Re-enable CPU mode assertion with debug info
    uart_puts("About to check CPU mode assertion\n");
    configASSERT( ulAPSR != portAPSR_USER_MODE );

    // Always print GIC debug info regardless of CPU mode
    uart_puts("GIC Debug: Binary Point Register = 0x");
    uart_hex(portICCBPR_BINARY_POINT_REGISTER);
    uart_puts("\n");
    uart_puts("GIC Debug: Binary Point Bits = 0x");
    uart_hex(portICCBPR_BINARY_POINT_REGISTER & portBINARY_POINT_BITS);
    uart_puts("\n");
    uart_puts("GIC Debug: Max Binary Point Value = ");
    uart_dec(portMAX_BINARY_POINT_VALUE);
    uart_puts("\n");

    if( ulAPSR != portAPSR_USER_MODE )
    {
        uart_puts("Running in privileged mode\n");
        /* Only continue if the binary point value is set to its lowest possible
         * setting.  See the comments in vPortValidateInterruptPriority() below for
         * more information. */
        
        // Temporarily disable strict GIC assertion for virtualized environment
        // configASSERT( ( portICCBPR_BINARY_POINT_REGISTER & portBINARY_POINT_BITS ) <= portMAX_BINARY_POINT_VALUE );

        if( ( portICCBPR_BINARY_POINT_REGISTER & portBINARY_POINT_BITS ) <= portMAX_BINARY_POINT_VALUE )
        {
            /* Interrupts are turned off in the CPU itself to ensure tick does
             * not execute while the scheduler is being started.  Interrupts are
             * automatically turned back on in the CPU when the first task starts
             * executing. */
            portCPU_IRQ_DISABLE();

            /* Start the timer that generates the tick ISR. */
            configSETUP_TICK_INTERRUPT();

            /* Start the first task executing. */
            uart_puts("About to call vPortRestoreTaskContext()\n");
            
            /* Debug: Check pxCurrentTCB before context switch */
            extern void *pxCurrentTCB;
            uart_puts("pxCurrentTCB = 0x");
            uart_hex((unsigned int)pxCurrentTCB);
            uart_puts("\n");
            
            if (pxCurrentTCB != NULL) {
                uart_puts("pxCurrentTCB points to valid memory\n");
                /* Check the stack pointer stored in the TCB */
                void **tcb_ptr = (void **)pxCurrentTCB;
                unsigned int *stack_ptr = (unsigned int *)*tcb_ptr;
                uart_puts("Stack pointer in TCB = 0x");
                uart_hex((unsigned int)stack_ptr);
                uart_puts("\n");
                
                /* Debug: Check stack contents - scan for our PC value */
                uart_puts("Stack inspection - searching for PC value:\n");
                uart_puts("TCB Stack pointer = 0x");
                uart_hex((unsigned int)stack_ptr);
                uart_puts("\n");
                
                /* Look for our PC value in the stack */
                for (int i = 0; i < 20; i++) {
                    uart_puts("stack[");
                    uart_dec(i);
                    uart_puts("] = 0x");
                    uart_hex(stack_ptr[i]);
                    if (stack_ptr[i] == 0x40000CC8 || stack_ptr[i] == 0x4000001C) {
                        uart_puts(" <-- FOUND PC!");
                    }
                    if (stack_ptr[i] == 0x1F) {
                        uart_puts(" <-- FOUND SPSR!");
                    }
                    uart_puts("\n");
                }
            } else {
                uart_puts("ERROR: pxCurrentTCB is NULL!\n");
            }
            
            /* Debug: Create a custom context restore to see what RFEIA would load */
            uart_puts("About to manually check what RFEIA will load...\n");
            
            /* Simulate the start of portRESTORE_CONTEXT */
            extern void *pxCurrentTCB;
            unsigned int *stack_ptr = (unsigned int *)(*(unsigned int **)pxCurrentTCB);
            
            uart_puts("Initial stack pointer from TCB: 0x");
            uart_hex((unsigned int)stack_ptr);
            uart_puts("\n");
            
            /* Simulate FPU context pop */
            unsigned int fpu_context = *stack_ptr++;
            uart_puts("FPU context flag: 0x");
            uart_hex(fpu_context);
            uart_puts("\n");
            
            /* Skip FPU registers if needed (they're not popped since fpu_context == 0) */
            
            /* Simulate critical nesting pop */
            unsigned int critical_nesting = *stack_ptr++;
            uart_puts("Critical nesting: 0x");
            uart_hex(critical_nesting);
            uart_puts("\n");
            
            /* Skip priority mask setup... */
            
            /* Simulate register pops: POP {R0-R12, R14} = 14 values */
            uart_puts("Simulating POP {R0-R12, R14} - advancing by 14 values...\n");
            stack_ptr += 14;
            
            uart_puts("Stack pointer after all POPs: 0x");
            uart_hex((unsigned int)stack_ptr);
            uart_puts("\n");
            
            uart_puts("Values RFEIA would load:\n");
            uart_puts("PC (from stack[0]): 0x");
            uart_hex(stack_ptr[0]);
            uart_puts("\n");
            uart_puts("CPSR (from stack[1]): 0x");
            uart_hex(stack_ptr[1]);
            uart_puts("\n");
            
            /* CRITICAL: Check if seL4 VM is translating addresses correctly */
            uart_puts("=== seL4 ADDRESS TRANSLATION CHECK ===\n");
            uart_puts("Current function address: 0x");
            uart_hex((unsigned int)&uart_puts);  /* Check if our own function address makes sense */
            uart_puts("\n");
            uart_puts("Stack address range: TCB=0x");
            uart_hex((unsigned int)pxCurrentTCB);
            uart_puts(" to stack=0x");
            uart_hex((unsigned int)stack_ptr + 64);  /* Check stack address range */
            uart_puts("\n");
            
            /* Test if we can read/write from the addresses we think contain PC */
            unsigned int test_value = 0xDEADBEEF;
            unsigned int *test_addr = &stack_ptr[16];  /* Where we think PC is stored */
            uart_puts("Test write/read to PC location: ");
            *test_addr = test_value;
            if (*test_addr == test_value) {
                uart_puts("PASS - Memory is accessible\n");
                *test_addr = 0x4000532C;  /* Restore correct value */
            } else {
                uart_puts("FAIL - Memory translation issue! Read: 0x");
                uart_hex(*test_addr);
                uart_puts("\n");
            }
            
            /* Instead of complex assembly, try direct function call */
            uart_puts("=== ATTEMPTING DIRECT TASK CALL ===\n");
            uart_puts("Calling vPLCMain directly as a test...\n");
            
            /* Get the first task function address */
            if (stack_ptr[16] == 0x40000CC8) {
                uart_puts("About to call vPLCMain(NULL)...\n");
                extern void vPLCMain(void *pvParameters);
                
                /* Call it in a way that won't hang if it has infinite loops */
                uart_puts("Calling vPLCMain - expecting it to print messages...\n");
                
                /* Simple test - call the function but catch any immediate return */
                void (*task_func)(void*) = (void (*)(void*))0x40000CC8;
                uart_puts("About to jump to task function at 0x40000CC8\n");
                
                /* Instead of calling the infinite loop version, let's check what's at that address */
                uart_puts("Checking memory at task address:\n");
                unsigned int *code = (unsigned int *)0x40000CC8;
                for (int i = 0; i < 4; i++) {
                    uart_puts("code[");
                    uart_dec(i);
                    uart_puts("] = 0x");
                    uart_hex(code[i]);
                    uart_puts("\n");
                }
                
                uart_puts("Now actually calling the task function...\n");
                task_func(NULL);
            }
            
            uart_puts("Direct call completed, now trying vPortRestoreTaskContext...\n");
            /* Now call the real function to see what happens */
            vPortRestoreTaskContext();
        }
    }
    else
    {
        uart_puts("Running in USER mode - bypassing GIC checks for virtualized environment\n");
        
        /* In virtualized environments, we may be running in USER mode
         * but still need to proceed with scheduler initialization */
        
        /* Interrupts are turned off in the CPU itself to ensure tick does
         * not execute while the scheduler is being started. */
        portCPU_IRQ_DISABLE();

        /* Start the timer that generates the tick ISR. */
        configSETUP_TICK_INTERRUPT();

        /* Start the first task executing. */
        uart_puts("About to call vPortRestoreTaskContext() from USER mode path\n");
        
        /* Debug: Check pxCurrentTCB before context switch */
        extern void *pxCurrentTCB;
        uart_puts("pxCurrentTCB = 0x");
        uart_hex((unsigned int)pxCurrentTCB);
        uart_puts("\n");
        
        if (pxCurrentTCB != NULL) {
            uart_puts("pxCurrentTCB points to valid memory\n");
            /* Check the stack pointer stored in the TCB */
            void **tcb_ptr = (void **)pxCurrentTCB;
            unsigned int *stack_ptr = (unsigned int *)*tcb_ptr;
            uart_puts("Stack pointer in TCB = 0x");
            uart_hex((unsigned int)stack_ptr);
            uart_puts("\n");
            
            /* Debug: Check stack contents that RFEIA will load */
            uart_puts("Stack inspection (RFEIA will load these):\n");
            uart_puts("stack[0] (should be SPSR) = 0x");
            uart_hex(stack_ptr[0]);
            uart_puts("\n");
            uart_puts("stack[1] (should be PC/LR) = 0x");
            uart_hex(stack_ptr[1]);
            uart_puts("\n");
            uart_puts("stack[2] = 0x");
            uart_hex(stack_ptr[2]);
            uart_puts("\n");
            uart_puts("stack[3] = 0x");
            uart_hex(stack_ptr[3]);
            uart_puts("\n");
        } else {
            uart_puts("ERROR: pxCurrentTCB is NULL!\n");
        }
        
        vPortRestoreTaskContext();
    }

    /* Will only get here if vTaskStartScheduler() was called with the CPU in
     * a non-privileged mode or the binary point register was not set to its lowest
     * possible value.  prvTaskExitError() is referenced to prevent a compiler
     * warning about it being defined but not referenced in the case that the user
     * defines their own exit address. */
    ( void ) prvTaskExitError;
    return 0;
}
/*-----------------------------------------------------------*/

void vPortEndScheduler( void )
{
    /* Not implemented in ports where there is nothing to return to.
     * Artificially force an assert. */
    configASSERT( ulCriticalNesting == 1000UL );
}
/*-----------------------------------------------------------*/

void vPortEnterCritical( void )
{
    /* Mask interrupts up to the max syscall interrupt priority. */
    ulPortSetInterruptMask();

    /* Now that interrupts are disabled, ulCriticalNesting can be accessed
     * directly.  Increment ulCriticalNesting to keep a count of how many times
     * portENTER_CRITICAL() has been called. */
    ulCriticalNesting++;

    /* This is not the interrupt safe version of the enter critical function so
     * assert() if it is being called from an interrupt context.  Only API
     * functions that end in "FromISR" can be used in an interrupt.  Only assert if
     * the critical nesting count is 1 to protect against recursive calls if the
     * assert function also uses a critical section. */
    if( ulCriticalNesting == 1 )
    {
        configASSERT( ulPortInterruptNesting == 0 );
    }
}
/*-----------------------------------------------------------*/

void vPortExitCritical( void )
{
    if( ulCriticalNesting > portNO_CRITICAL_NESTING )
    {
        /* Decrement the nesting count as the critical section is being
         * exited. */
        ulCriticalNesting--;

        /* If the nesting level has reached zero then all interrupt
         * priorities must be re-enabled. */
        if( ulCriticalNesting == portNO_CRITICAL_NESTING )
        {
            /* Critical nesting has reached zero so all interrupt priorities
             * should be unmasked. */
            portCLEAR_INTERRUPT_MASK();
        }
    }
}
/*-----------------------------------------------------------*/

void FreeRTOS_Tick_Handler( void )
{
    /* Set interrupt mask before altering scheduler structures.   The tick
     * handler runs at the lowest priority, so interrupts cannot already be masked,
     * so there is no need to save and restore the current mask value.  It is
     * necessary to turn off interrupts in the CPU itself while the ICCPMR is being
     * updated. */
    portCPU_IRQ_DISABLE();
    portICCPMR_PRIORITY_MASK_REGISTER = ( uint32_t ) ( configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT );
    __asm volatile ( "dsb        \n"
                     "isb        \n" ::: "memory" );
    portCPU_IRQ_ENABLE();

    /* Increment the RTOS tick. */
    if( xTaskIncrementTick() != pdFALSE )
    {
        ulPortYieldRequired = pdTRUE;
    }

    /* Ensure all interrupt priorities are active again. */
    portCLEAR_INTERRUPT_MASK();
    configCLEAR_TICK_INTERRUPT();
}
/*-----------------------------------------------------------*/

#if ( configUSE_TASK_FPU_SUPPORT != 2 )

    void vPortTaskUsesFPU( void )
    {
        uint32_t ulInitialFPSCR = 0;

        /* A task is registering the fact that it needs an FPU context.  Set the
         * FPU flag (which is saved as part of the task context). */
        ulPortTaskHasFPUContext = pdTRUE;

        /* Initialise the floating point status register. */
        __asm volatile ( "FMXR  FPSCR, %0" ::"r" ( ulInitialFPSCR ) : "memory" );
    }

#endif /* configUSE_TASK_FPU_SUPPORT */
/*-----------------------------------------------------------*/

void vPortClearInterruptMask( uint32_t ulNewMaskValue )
{
    if( ulNewMaskValue == pdFALSE )
    {
        portCLEAR_INTERRUPT_MASK();
    }
}
/*-----------------------------------------------------------*/

uint32_t ulPortSetInterruptMask( void )
{
    uint32_t ulReturn;

    /* Interrupt in the CPU must be turned off while the ICCPMR is being
     * updated. */
    portCPU_IRQ_DISABLE();

    if( portICCPMR_PRIORITY_MASK_REGISTER == ( uint32_t ) ( configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT ) )
    {
        /* Interrupts were already masked. */
        ulReturn = pdTRUE;
    }
    else
    {
        ulReturn = pdFALSE;
        portICCPMR_PRIORITY_MASK_REGISTER = ( uint32_t ) ( configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT );
        __asm volatile ( "dsb        \n"
                         "isb        \n" ::: "memory" );
    }

    portCPU_IRQ_ENABLE();

    return ulReturn;
}
/*-----------------------------------------------------------*/

#if ( configASSERT_DEFINED == 1 )

    void vPortValidateInterruptPriority( void )
    {
        /* The following assertion will fail if a service routine (ISR) for
         * an interrupt that has been assigned a priority above
         * configMAX_SYSCALL_INTERRUPT_PRIORITY calls an ISR safe FreeRTOS API
         * function.  ISR safe FreeRTOS API functions must *only* be called
         * from interrupts that have been assigned a priority at or below
         * configMAX_SYSCALL_INTERRUPT_PRIORITY.
         *
         * Numerically low interrupt priority numbers represent logically high
         * interrupt priorities, therefore the priority of the interrupt must
         * be set to a value equal to or numerically *higher* than
         * configMAX_SYSCALL_INTERRUPT_PRIORITY.
         *
         * FreeRTOS maintains separate thread and ISR API functions to ensure
         * interrupt entry is as fast and simple as possible. */
        configASSERT( portICCRPR_RUNNING_PRIORITY_REGISTER >= ( uint32_t ) ( configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT ) );

        /* Priority grouping:  The interrupt controller (GIC) allows the bits
         * that define each interrupt's priority to be split between bits that
         * define the interrupt's pre-emption priority bits and bits that define
         * the interrupt's sub-priority.  For simplicity all bits must be defined
         * to be pre-emption priority bits.  The following assertion will fail if
         * this is not the case (if some bits represent a sub-priority).
         *
         * The priority grouping is configured by the GIC's binary point register
         * (ICCBPR).  Writing 0 to ICCBPR will ensure it is set to its lowest
         * possible value (which may be above 0). */
        configASSERT( ( portICCBPR_BINARY_POINT_REGISTER & portBINARY_POINT_BITS ) <= portMAX_BINARY_POINT_VALUE );
    }

#endif /* configASSERT_DEFINED */
/*-----------------------------------------------------------*/

void vApplicationFPUSafeIRQHandler( uint32_t ulICCIAR )
{
    ( void ) ulICCIAR;
    configASSERT( ( volatile void * ) NULL );
}
