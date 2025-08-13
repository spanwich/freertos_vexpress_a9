#include "FreeRTOS.h"
#include "task.h"
#include <stddef.h>

// PL011 UART registers - matching the original vm_freertos example
#define UART0_DR (*(volatile unsigned int *)0x9000000)   // Data register
#define UART0_FR (*(volatile unsigned int *)0x9000018)   // Flag register

void uart_putc(char c) {
    // Better implementation with delay like original example
    UART0_DR = c;
    for (volatile int i = 0; i < 10000; i++) {}  // Simple delay
}

void uart_puts(const char *s) {
    while (*s) {
        uart_putc(*s);
        s++;
    }
}

void uart_decimal(unsigned long val) {
    if (val == 0) {
        uart_putc('0');
        return;
    }
    
    char buffer[12]; // enough for 32-bit number
    int i = 0;
    while (val > 0) {
        buffer[i++] = '0' + (val % 10);
        val /= 10;
    }
    
    // Print in reverse order
    while (i > 0) {
        uart_putc(buffer[--i]);
    }
}

void uart_hex(unsigned int val) {
    int i;
    for (i = 28; i >= 0; i -= 4) {
        int digit = (val >> i) & 0xF;
        uart_putc(digit < 10 ? '0' + digit : 'A' + digit - 10);
    }
}

// Delay function from original example
void delay(volatile unsigned int count) {
    while (count--) {
        for (volatile int i = 0; i < 100000; i++) {}  // Reduced for faster testing
    }
}

// Message printing functions from original example
void print_freertos_starting(void) {
    uart_putc('F'); uart_putc('r'); uart_putc('e'); uart_putc('e');
    uart_putc('R'); uart_putc('T'); uart_putc('O'); uart_putc('S');
    uart_putc(' '); uart_putc('s'); uart_putc('t'); uart_putc('a');
    uart_putc('r'); uart_putc('t'); uart_putc('i'); uart_putc('n');
    uart_putc('g'); uart_putc('.'); uart_putc('.'); uart_putc('.');
    uart_putc('\n');
}

void print_hello_message(void) {
    uart_putc('H'); uart_putc('e'); uart_putc('l'); uart_putc('l');
    uart_putc('o'); uart_putc(' '); uart_putc('f'); uart_putc('r');
    uart_putc('o'); uart_putc('m'); uart_putc(' '); uart_putc('F');
    uart_putc('r'); uart_putc('e'); uart_putc('e'); uart_putc('R');
    uart_putc('T'); uart_putc('O'); uart_putc('S'); uart_putc('!');
    uart_putc('\n');
}

// Simple printf replacement
int printf(const char *format, ...) {
    uart_puts(format);
    return 0;
}

// Standard C library functions
void *memcpy(void *dest, const void *src, size_t n) {
    char *d = (char *)dest;
    const char *s = (const char *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

void *memset(void *s, int c, size_t n) {
    char *p = (char *)s;
    for (size_t i = 0; i < n; i++) {
        p[i] = c;
    }
    return s;
}

void vAssertCalled(unsigned long ulLine, const char * const pcFileName) {
    uart_puts("\r\n=== DETAILED ASSERT FAILURE DEBUG ===\r\n");
    uart_puts("ASSERT FAILED at line: ");
    uart_decimal(ulLine);
    uart_puts("\r\n");
    uart_puts("File: ");
    uart_puts(pcFileName);
    uart_puts("\r\n");
    
    // Provide specific debugging for common assertion locations
    // Simple check for "port.c" in filename
    int isPortC = 0;
    const char *p = pcFileName;
    while (*p) {
        if (*p == 'p' && *(p+1) == 'o' && *(p+2) == 'r' && *(p+3) == 't' && *(p+4) == '.') {
            isPortC = 1;
            break;
        }
        p++;
    }
    
    if (isPortC) {
        uart_puts("\r\n--- PORT.C ASSERTION ANALYSIS ---\r\n");
        if (ulLine >= 410 && ulLine <= 420) {
            uart_puts("CPU Mode assertion - checking APSR register\r\n");
        } else if (ulLine >= 430 && ulLine <= 450) {
            uart_puts("GIC Binary Point Register assertion\r\n");
        } else if (ulLine >= 470 && ulLine <= 480) {
            uart_puts("Critical nesting assertion\r\n");
        } else if (ulLine >= 490 && ulLine <= 500) {
            uart_puts("Interrupt nesting assertion\r\n");
        } else {
            uart_puts("Other port.c assertion at line ");
            uart_decimal(ulLine);
            uart_puts("\r\n");
        }
    }
    
    uart_puts("\r\nSystem will halt here for debugging.\r\n");
    uart_puts("=====================================\r\n");
    for (;;);
}

void vSetupTickInterrupt(void) {
    // Simple stub - in a real system this would configure the timer
    // For now just placeholder
    uart_puts("vSetupTickInterrupt called - timer stub\r\n");
}

void vPLCMain(void *pvParameters) {
    unsigned int counter = 0;
    
    for (;;) {
        print_hello_message();
        
        // Print a counter to show task is running
        uart_puts("PLC Task Counter: ");
        // Simple counter display (0-9)
        uart_putc('0' + (counter % 10));
        uart_puts("\r\n");
        
        counter++;
        vTaskDelay(pdMS_TO_TICKS(2000));  // 2 second delay
    }
}

// Demo task similar to original example
void vDemoTask(void *pvParameters) {
    for (;;) {
        uart_puts("Demo task: FreeRTOS on seL4 microkernel!\r\n");
        vTaskDelay(pdMS_TO_TICKS(3000));  // 3 second delay
    }
}

int main(void) {
    uart_puts("=== MAIN() ENTRY POINT ===\r\n");
    print_freertos_starting();
    
    uart_puts("=== ABOUT TO INITIALIZE FREERTOS ===\r\n");
    uart_puts("Initializing FreeRTOS on seL4...\r\n");
    uart_puts("Creating tasks...\r\n");
    
    uart_puts("vPLCMain function address: 0x");
    uart_hex((unsigned int)vPLCMain);
    uart_puts("\r\n");
    uart_puts("vDemoTask function address: 0x");
    uart_hex((unsigned int)vDemoTask);
    uart_puts("\r\n");
    
    // Create multiple tasks like a real system
    BaseType_t result1 = xTaskCreate(vPLCMain, "PLC", configMINIMAL_STACK_SIZE * 2, NULL, 2, NULL);
    uart_puts("PLC task creation result: ");
    uart_decimal(result1);
    uart_puts("\r\n");
    
    BaseType_t result2 = xTaskCreate(vDemoTask, "Demo", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    uart_puts("Demo task creation result: ");
    uart_decimal(result2);
    uart_puts("\r\n");
    
    uart_puts("Starting FreeRTOS scheduler...\r\n");
    uart_puts("Tasks will begin running momentarily...\r\n");
    
    vTaskStartScheduler();
    
    // This should never be reached
    uart_puts("CRITICAL ERROR: Scheduler returned unexpectedly!\r\n");
    while (1) {
        delay(1000);
        uart_puts("System halted.\r\n");
    };
}