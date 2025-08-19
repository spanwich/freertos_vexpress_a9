/*
 * Minimal Memory Pattern Debugging - No FreeRTOS Dependencies
 * Direct hardware memory pattern painting for seL4/QEMU analysis
 */

#include <stddef.h>
#include <stdint.h>

// PL011 UART registers
#define UART0_DR (*(volatile unsigned int *)0x9000000)
#define UART0_FR (*(volatile unsigned int *)0x9000018)

// Memory pattern constants
#define PATTERN_STACK   0xDEADBEEF
#define PATTERN_DATA    0x12345678
#define PATTERN_HEAP    0xCAFEBABE
#define PATTERN_TEST    0x55AA55AA

// Memory regions for testing
#define GUEST_BASE         0x40000000
#define STACK_REGION_BASE  0x41000000
#define DATA_REGION_BASE   0x41200000
#define HEAP_REGION_BASE   0x41400000
#define PATTERN_REGION_BASE 0x42000000

#define REGION_SIZE        0x100000   // 1MB per region
#define PATTERN_SIZE       0x400000   // 4MB for pattern painting

void uart_putc(char c) {
    UART0_DR = c;
    for (volatile int i = 0; i < 1000; i++) {}
}

void uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s);
        s++;
    }
}

void uart_hex(unsigned int val) {
    uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        int digit = (val >> i) & 0xF;
        uart_putc(digit < 10 ? '0' + digit : 'A' + digit - 10);
    }
}

int paint_memory_region(volatile uint32_t *start, size_t word_count, uint32_t pattern, const char *region_name) {
    uart_puts("\n=== Painting Memory Region: ");
    uart_puts(region_name);
    uart_puts(" ===\n");
    uart_puts("Start Address: ");
    uart_hex((uint32_t)start);
    uart_puts("\nWord Count: ");
    uart_hex(word_count);
    uart_puts("\nPattern: ");
    uart_hex(pattern);
    uart_puts("\n");
    
    // Paint memory
    for (size_t i = 0; i < word_count; i++) {
        start[i] = pattern;
        
        if ((i & 0x3FFF) == 0 && i > 0) {
            uart_puts("Progress: ");
            uart_hex(i >> 14);
            uart_puts(" chunks\n");
        }
    }
    
    uart_puts("Painting complete. Verifying...\n");
    
    // Verification
    int errors = 0;
    for (size_t i = 0; i < word_count && errors < 10; i++) {
        uint32_t actual = start[i];
        if (actual != pattern) {
            uart_puts("MISMATCH at offset ");
            uart_hex(i);
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
        uart_puts("[OK] All words verified successfully\n");
    } else {
        uart_puts("[FAIL] Found ");
        uart_hex(errors);
        uart_puts(" verification errors\n");
    }
    
    return errors;
}

void analyze_execution_context(void) {
    uint32_t pc_reg, sp_reg, cpsr_reg, lr_reg;
    
    uart_puts("\n=== EXECUTION CONTEXT ANALYSIS ===\n");
    
    __asm__ volatile (
        "mov %0, pc\n"
        "mov %1, sp\n"
        "mrs %2, cpsr\n"
        "mov %3, lr\n"
        : "=r" (pc_reg), "=r" (sp_reg), "=r" (cpsr_reg), "=r" (lr_reg)
    );
    
    uart_puts("PC: "); uart_hex(pc_reg); uart_puts("\n");
    uart_puts("SP: "); uart_hex(sp_reg); uart_puts("\n");
    uart_puts("CPSR: "); uart_hex(cpsr_reg); uart_puts("\n");
    uart_puts("LR: "); uart_hex(lr_reg); uart_puts("\n");
    
    uint32_t mode = cpsr_reg & 0x1F;
    uart_puts("Processor Mode: ");
    switch (mode) {
        case 0x10: uart_puts("User"); break;
        case 0x13: uart_puts("Supervisor"); break;
        case 0x1F: uart_puts("System"); break;
        default: uart_puts("Other"); break;
    }
    uart_puts("\n");
}

int main(void) {
    uart_puts("\n========================================\n");
    uart_puts("  MINIMAL MEMORY PATTERN DEBUGGING\n");
    uart_puts("  PhD Research - Direct Hardware Access\n");
    uart_puts("========================================\n");
    
    analyze_execution_context();
    
    const char *region_names[] = {"Stack", "Data", "Heap", "Pattern"};
    volatile uint32_t *region_bases[] = {
        (volatile uint32_t *)STACK_REGION_BASE,
        (volatile uint32_t *)DATA_REGION_BASE,
        (volatile uint32_t *)HEAP_REGION_BASE,
        (volatile uint32_t *)PATTERN_REGION_BASE
    };
    uint32_t patterns[] = {PATTERN_STACK, PATTERN_DATA, PATTERN_HEAP, PATTERN_TEST};
    
    for (int cycle = 0; cycle < 3; cycle++) {
        uart_puts("\n=== MEMORY PATTERN CYCLE ");
        uart_hex(cycle);
        uart_puts(" ===\n");
        
        for (int i = 0; i < 4; i++) {
            size_t words = (i == 3) ? (PATTERN_SIZE >> 2) : (REGION_SIZE >> 2);
            int errors = paint_memory_region(
                region_bases[i], 
                words,
                patterns[i] ^ (cycle << 24),  // Vary pattern by cycle
                region_names[i]
            );
            
            if (errors > 0) {
                uart_puts("WARNING: Memory errors in ");
                uart_puts(region_names[i]);
                uart_puts(" region!\n");
            }
        }
        
        uart_puts("\n=== READY FOR QEMU MEMORY DUMP ===\n");
        uart_puts("QEMU monitor commands:\n");
        uart_puts("  (qemu) x/32wx 0x41000000  # Stack region\n");
        uart_puts("  (qemu) x/32wx 0x41200000  # Data region\n");
        uart_puts("  (qemu) x/32wx 0x41400000  # Heap region\n");
        uart_puts("  (qemu) x/32wx 0x42000000  # Pattern region\n");
        
        uart_puts("\nWaiting for memory analysis...\n");
        for (volatile int i = 0; i < 100000000; i++) {}  // Long delay
    }
    
    uart_puts("\n=== MEMORY PATTERN DEBUGGING COMPLETE ===\n");
    uart_puts("All memory regions have been painted and verified.\n");
    uart_puts("System will halt.\n");
    
    while (1) {
        uart_puts(".");
        for (volatile int i = 0; i < 50000000; i++) {}
    }
}