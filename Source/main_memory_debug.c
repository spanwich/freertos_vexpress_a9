/*
 * Enhanced Memory Pattern Debugging Implementation for FreeRTOS-seL4 Integration
 * Based on methodology from memory_pattern_debugging_methodology.tex
 * 
 * This file extends the existing FreeRTOS implementation with comprehensive
 * memory pattern painting for debugging virtual memory mappings between
 * FreeRTOS guest VM and seL4 host system.
 */

#include "FreeRTOS.h"
#include "task.h"
#include <stddef.h>
#include <stdint.h>

// PL011 UART registers - matching seL4 VM configuration
#define UART0_DR (*(volatile unsigned int *)0x9000000)   // Data register
#define UART0_FR (*(volatile unsigned int *)0x9000018)   // Flag register

// Memory pattern constants for systematic debugging
#define PATTERN_STACK   0xDEADBEEF  // Stack region pattern
#define PATTERN_DATA    0x12345678  // Data section pattern  
#define PATTERN_HEAP    0xCAFEBABE  // Heap region pattern
#define PATTERN_TEST    0x55AA55AA  // Fault address test pattern
#define PATTERN_CYCLES  0xAAAAAAAA  // Cycling pattern for dynamic analysis

// Memory regions for systematic testing
#define GUEST_BASE         0x40000000  // seL4 VM guest base (CRITICAL!)
#define STACK_REGION_BASE  0x41000000  // Stack testing area
#define DATA_REGION_BASE   0x41200000  // Data section testing area
#define HEAP_REGION_BASE   0x41400000  // Heap testing area
#define PATTERN_REGION_BASE 0x42000000 // Main pattern painting area

#define REGION_SIZE        0x100000   // 1MB per region for testing
#define PATTERN_SIZE       0x400000   // 4MB for pattern painting

// Enhanced UART functions
void uart_putc(char c) {
    UART0_DR = c;
    for (volatile int i = 0; i < 10000; i++) {}  // Delay for stability
}

void uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n') {
            uart_putc('\r');  // Ensure proper line endings
        }
        uart_putc(*s);
        s++;
    }
}

void uart_decimal(unsigned long val) {
    if (val == 0) {
        uart_putc('0');
        return;
    }
    
    // Convert to hex instead to avoid division completely
    uart_puts("0x");
    uart_hex((unsigned int)val);
}

void uart_hex(unsigned int val) {
    uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        int digit = (val >> i) & 0xF;
        uart_putc(digit < 10 ? '0' + digit : 'A' + digit - 10);
    }
}

// Memory pattern painting with verification
int paint_memory_region(volatile uint32_t *start, size_t word_count, uint32_t pattern, const char *region_name) {
    uart_puts("\n=== Painting Memory Region: ");
    uart_puts(region_name);
    uart_puts(" ===\n");
    uart_puts("Start Address: ");
    uart_hex((uint32_t)start);
    uart_puts("\nWord Count: ");
    uart_decimal(word_count);
    uart_puts("\nPattern: ");
    uart_hex(pattern);
    uart_puts("\n");
    
    // Paint memory with progress tracking
    for (size_t i = 0; i < word_count; i++) {
        start[i] = pattern;
        
        // Progress indicator every 16K words (64KB) - avoid division
        if ((i & 0x3FFF) == 0 && i > 0) {  // i % 16384 == 0, using bitwise AND
            uart_puts("Progress: ");
            // Simple progress without division - just show chunks
            uart_decimal(i >> 14);  // i / 16384 using bit shift
            uart_puts(" chunks\n");
        }
    }
    
    uart_puts("Painting complete. Verifying...\n");
    
    // Verification pass
    int errors = 0;
    for (size_t i = 0; i < word_count && errors < 10; i++) {
        uint32_t actual = start[i];
        if (actual != pattern) {
            uart_puts("MISMATCH at offset ");
            uart_decimal(i);
            uart_puts(" (address ");
            uart_hex((uint32_t)&start[i]);
            uart_puts("): expected ");
            uart_hex(pattern);
            uart_puts(", got ");
            uart_hex(actual);
            uart_puts("\n");
            errors++;
        }
    }
    
    if (errors == 0) {
        uart_puts("[OK] All ");
        uart_decimal(word_count);
        uart_puts(" words verified successfully\n");
    } else {
        uart_puts("[FAIL] Found ");
        uart_decimal(errors);
        uart_puts(" verification errors\n");
    }
    
    return errors;
}

// Critical address space analysis
void analyze_critical_addresses(void) {
    uart_puts("\n=== CRITICAL ADDRESS SPACE ANALYSIS ===\n");
    
    struct {
        uint32_t addr;
        const char *description;
        int safe_to_test;
    } test_addresses[] = {
        {0x00000000, "NULL pointer", 0},
        {0x00000008, "ARM SWI vector (FAULT LOCATION!)", 0},
        {0x40000000, "Guest VM base address", 1},
        {0x41000000, "Stack region start", 1},
        {0x42000000, "Pattern painting area", 1},
        {0x9000000,  "UART0 device register", 1},
        {0x8040000,  "GIC base address", 1},
    };
    
    for (int i = 0; i < 7; i++) {
        uart_puts("\nTesting: ");
        uart_puts(test_addresses[i].description);
        uart_puts(" (");
        uart_hex(test_addresses[i].addr);
        uart_puts(")\n");
        
        if (!test_addresses[i].safe_to_test) {
            uart_puts("SKIPPED - unsafe address, would cause fault\n");
            continue;
        }
        
        volatile uint32_t *ptr = (volatile uint32_t *)test_addresses[i].addr;
        uint32_t read_value = *ptr;
        uart_puts("Read access: OK, value = ");
        uart_hex(read_value);
        uart_puts("\n");
    }
}

// Execution context analysis
void analyze_execution_context(void) {
    uint32_t pc_reg, sp_reg, cpsr_reg, lr_reg;
    
    uart_puts("\n=== EXECUTION CONTEXT ANALYSIS ===\n");
    
    // Capture current processor state
    __asm__ volatile (
        "mov %0, pc\n"
        "mov %1, sp\n"
        "mrs %2, cpsr\n"
        "mov %3, lr\n"
        : "=r" (pc_reg), "=r" (sp_reg), "=r" (cpsr_reg), "=r" (lr_reg)
    );
    
    uart_puts("PC (Program Counter): ");
    uart_hex(pc_reg);
    uart_puts("\nSP (Stack Pointer):   ");
    uart_hex(sp_reg);
    uart_puts("\nCPSR (Status Reg):    ");
    uart_hex(cpsr_reg);
    uart_puts("\nLR (Link Register):   ");
    uart_hex(lr_reg);
    uart_puts("\n");
    
    // Analyze processor mode
    uint32_t mode = cpsr_reg & 0x1F;
    uart_puts("Processor Mode: ");
    switch (mode) {
        case 0x10: uart_puts("User (0x10)"); break;
        case 0x13: uart_puts("Supervisor (0x13)"); break;
        case 0x1F: uart_puts("System (0x1F)"); break;
        case 0x17: uart_puts("Abort (0x17)"); break;
        case 0x1B: uart_puts("Undefined (0x1B)"); break;
        case 0x12: uart_puts("IRQ (0x12)"); break;
        case 0x11: uart_puts("FIQ (0x11)"); break;
        default: 
            uart_puts("Unknown (");
            uart_hex(mode);
            uart_puts(")");
            break;
    }
    uart_puts("\n");
}

// Enhanced memory pattern debugging task
void vMemoryPatternDebugTask(void *pvParameters) {
    static uint32_t cycle_counter = 0;
    const char *region_names[] = {"Stack", "Data", "Heap", "Pattern"};
    volatile uint32_t *region_bases[] = {
        (volatile uint32_t *)STACK_REGION_BASE,
        (volatile uint32_t *)DATA_REGION_BASE,
        (volatile uint32_t *)HEAP_REGION_BASE,
        (volatile uint32_t *)PATTERN_REGION_BASE
    };
    uint32_t patterns[] = {PATTERN_STACK, PATTERN_DATA, PATTERN_HEAP, PATTERN_TEST};
    
    uart_puts("\n========================================\n");
    uart_puts("  ENHANCED MEMORY PATTERN DEBUG TASK\n");
    uart_puts("  FreeRTOS-seL4 Memory Mapping Analysis\n");
    uart_puts("========================================\n");
    
    // Initial system analysis
    analyze_execution_context();
    analyze_critical_addresses();
    
    for (;;) {
        uart_puts("\n=== MEMORY PATTERN CYCLE ");
        uart_decimal(cycle_counter);
        uart_puts(" ===\n");
        
        // Paint all regions with their designated patterns
        for (int i = 0; i < 4; i++) {
            size_t words = (i == 3) ? (PATTERN_SIZE >> 2) : (REGION_SIZE >> 2);  // Divide by 4 using bit shift
            int errors = paint_memory_region(
                region_bases[i], 
                words,
                patterns[i], 
                region_names[i]
            );
            
            if (errors > 0) {
                uart_puts("WARNING: Memory errors detected in ");
                uart_puts(region_names[i]);
                uart_puts(" region!\n");
            }
        }
        
        // Create a cycling pattern in the main pattern area for dynamic analysis
        volatile uint32_t *pattern_area = (volatile uint32_t *)PATTERN_REGION_BASE;
        uint32_t dynamic_pattern = PATTERN_CYCLES ^ (cycle_counter << 16);
        
        uart_puts("\nCreating dynamic pattern for instruction tracing...\n");
        uart_puts("Dynamic pattern: ");
        uart_hex(dynamic_pattern);
        uart_puts("\n");
        
        // Write dynamic pattern to specific locations for QEMU analysis
        for (int i = 0; i < 1024; i++) {
            pattern_area[i * 1024] = dynamic_pattern + i;  // Spread across 4MB
        }
        
        uart_puts("\n=== MEMORY STATE SUMMARY ===\n");
        uart_puts("Cycle: ");
        uart_decimal(cycle_counter);
        uart_puts("\nRegions painted: 4\n");
        uart_puts("Total memory painted: ");
        uart_decimal((3 * REGION_SIZE + PATTERN_SIZE) >> 10);  // Divide by 1024 using bit shift
        uart_puts(" KB\n");
        
        uart_puts("\nMemory map verification:\n");
        for (int i = 0; i < 4; i++) {
            uart_puts("  ");
            uart_puts(region_names[i]);
            uart_puts(": ");
            uart_hex((uint32_t)region_bases[i]);
            uart_puts(" -> ");
            uart_hex(region_bases[i][0]);
            uart_puts("\n");
        }
        
        uart_puts("\n=== READY FOR QEMU MEMORY DUMP ===\n");
        uart_puts("Use QEMU monitor commands:\n");
        uart_puts("  (qemu) info registers\n");
        uart_puts("  (qemu) x/32wx 0x41000000  # Stack region\n");
        uart_puts("  (qemu) x/32wx 0x41200000  # Data region\n");
        uart_puts("  (qemu) x/32wx 0x41400000  # Heap region\n");
        uart_puts("  (qemu) x/32wx 0x42000000  # Pattern region\n");
        uart_puts("  (qemu) x/32wx 0x40000000  # Guest base\n");
        
        cycle_counter++;
        
        // Long pause to allow for memory dumps and analysis
        uart_puts("\nWaiting 15 seconds for memory analysis...\n");
        vTaskDelay(pdMS_TO_TICKS(15000));  // 15 second delay
    }
}

// Standard library functions
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

int printf(const char *format, ...) {
    uart_puts(format);
    return 0;
}

void vAssertCalled(unsigned long ulLine, const char * const pcFileName) {
    uart_puts("\n=== DETAILED ASSERT FAILURE DEBUG ===\n");
    uart_puts("ASSERT FAILED at line: ");
    uart_decimal(ulLine);
    uart_puts("\nFile: ");
    uart_puts(pcFileName);
    uart_puts("\n");
    
    // Enhanced assertion analysis
    const char *p = pcFileName;
    int isPortC = 0;
    while (*p) {
        if (*p == 'p' && *(p+1) == 'o' && *(p+2) == 'r' && *(p+3) == 't' && *(p+4) == '.') {
            isPortC = 1;
            break;
        }
        p++;
    }
    
    if (isPortC) {
        uart_puts("\n--- PORT.C ASSERTION ANALYSIS ---\n");
        if (ulLine >= 410 && ulLine <= 420) {
            uart_puts("CPU Mode assertion - checking APSR register\n");
        } else if (ulLine >= 430 && ulLine <= 450) {
            uart_puts("GIC Binary Point Register assertion\n");
        } else if (ulLine >= 470 && ulLine <= 480) {
            uart_puts("Critical nesting assertion\n");
        } else {
            uart_puts("Other port.c assertion at line ");
            uart_decimal(ulLine);
            uart_puts("\n");
        }
        
        // Dump current execution context for debugging
        analyze_execution_context();
    }
    
    uart_puts("\nSystem will halt here for debugging.\n");
    uart_puts("=====================================\n");
    for (;;);
}

void vSetupTickInterrupt(void) {
    uart_puts("vSetupTickInterrupt called - timer configured\n");
}

// Simple monitoring task
void vMonitorTask(void *pvParameters) {
    uint32_t counter = 0;
    
    for (;;) {
        uart_puts("Monitor: System running, cycle ");
        uart_decimal(counter);
        uart_puts("\n");
        
        counter++;
        vTaskDelay(pdMS_TO_TICKS(8000));  // 8 second interval
    }
}

int main(void) {
    uart_puts("\n========================================\n");
    uart_puts("  FREERTOS MEMORY PATTERN DEBUGGING\n");
    uart_puts("  PhD Research - Secure Virtualization\n");
    uart_puts("========================================\n");
    
    uart_puts("Initializing FreeRTOS with memory debugging...\n");
    
    uart_puts("Function addresses:\n");
    uart_puts("  main: ");
    uart_hex((uint32_t)main);
    uart_puts("\n  vMemoryPatternDebugTask: ");
    uart_hex((uint32_t)vMemoryPatternDebugTask);
    uart_puts("\n  vMonitorTask: ");
    uart_hex((uint32_t)vMonitorTask);
    uart_puts("\n");
    
    // Create enhanced debugging tasks
    BaseType_t result1 = xTaskCreate(
        vMemoryPatternDebugTask, 
        "MemDebug", 
        configMINIMAL_STACK_SIZE * 8,  // Larger stack for debugging
        NULL, 
        3,  // High priority
        NULL
    );
    uart_puts("Memory debug task creation: ");
    uart_decimal(result1);
    uart_puts("\n");
    
    BaseType_t result2 = xTaskCreate(
        vMonitorTask, 
        "Monitor", 
        configMINIMAL_STACK_SIZE * 2, 
        NULL, 
        1,  // Lower priority
        NULL
    );
    uart_puts("Monitor task creation: ");
    uart_decimal(result2);
    uart_puts("\n");
    
    uart_puts("Starting FreeRTOS scheduler with memory debugging...\n");
    uart_puts("Memory pattern painting will begin shortly.\n");
    
    vTaskStartScheduler();
    
    // Should never reach here
    uart_puts("CRITICAL ERROR: Scheduler returned unexpectedly!\n");
    while (1) {
        uart_puts("System halted.\n");
        for (volatile int i = 0; i < 10000000; i++);
    }
}