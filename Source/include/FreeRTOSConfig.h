#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#define configUSE_PREEMPTION			1
#define configUSE_IDLE_HOOK				0
#define configUSE_TICK_HOOK				0
#define configCPU_CLOCK_HZ				( ( unsigned long ) 1000000000 )
#define configTICK_RATE_HZ				( ( TickType_t ) 1000 )
#define configMAX_PRIORITIES			( 10 )
#define configMINIMAL_STACK_SIZE		( ( unsigned short ) 128 )
#define configTOTAL_HEAP_SIZE			( ( size_t ) ( 65 * 1024 ) )
#define configMAX_TASK_NAME_LEN			( 10 )
#define configUSE_TRACE_FACILITY		1
#define configUSE_16_BIT_TICKS			0
#define configIDLE_SHOULD_YIELD			1
#define configUSE_MUTEXES				1
#define configUSE_RECURSIVE_MUTEXES		1
#define configUSE_COUNTING_SEMAPHORES	1
#define configUSE_ALTERNATIVE_API		0
#define configCHECK_FOR_STACK_OVERFLOW	0
#define configUSE_APPLICATION_TASK_TAG	0
#define configQUEUE_REGISTRY_SIZE		8

#define configUSE_CO_ROUTINES 			0
#define configMAX_CO_ROUTINE_PRIORITIES ( 2 )

#define configUSE_TIMERS				1
#define configTIMER_TASK_PRIORITY		( 2 )
#define configTIMER_QUEUE_LENGTH		5
#define configTIMER_TASK_STACK_DEPTH	( configMINIMAL_STACK_SIZE * 2 )

#define INCLUDE_vTaskPrioritySet		1
#define INCLUDE_uxTaskPriorityGet		1
#define INCLUDE_vTaskDelete				1
#define INCLUDE_vTaskCleanUpResources	1
#define INCLUDE_vTaskSuspend			1
#define INCLUDE_vTaskDelayUntil			1
#define INCLUDE_vTaskDelay				1

#define configASSERT( x ) if( ( x ) == 0 ) vAssertCalled( __LINE__, __FILE__ )

/* Port specific definitions. */
#define configUNIQUE_INTERRUPT_PRIORITIES		256
#define configUSE_PORT_OPTIMISED_TASK_SELECTION	1
#define configUSE_TICKLESS_IDLE					0
#define portTICK_TYPE_IS_ATOMIC					1

/* seL4 VM Virtual GIC CPU Interface address */
#define configINTERRUPT_CONTROLLER_BASE_ADDRESS	0x08040000
#define configINTERRUPT_CONTROLLER_CPU_INTERFACE_OFFSET	0x0
#define configMAX_API_CALL_INTERRUPT_PRIORITY	200

/* Minimal FPU support */
#define configUSE_TASK_FPU_SUPPORT	1

/* vexpress-a9 timer */
#define configSETUP_TICK_INTERRUPT()	vSetupTickInterrupt()
#define configCLEAR_TICK_INTERRUPT()	/* Not needed */

void vSetupTickInterrupt(void);

extern void vAssertCalled( unsigned long ulLine, const char * const pcFileName );

#endif /* FREERTOS_CONFIG_H */