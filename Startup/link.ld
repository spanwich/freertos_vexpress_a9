SECTIONS
{
    . = 0x40000000;
    .text : { 
        *(.text*)
    }
    .rodata : { 
        *(.rodata*) 
    }
    .data : { 
        *(.data*) 
    }
    
    /* BSS section for uninitialized data including FreeRTOS heap */
    .bss : { 
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        __bss_end__ = .;
    }
    
    /* Heap section - ensure it's placed after BSS */
    .heap : {
        __heap_start__ = .;
        . = . + 0x20000;  /* 128KB heap space */
        __heap_end__ = .;
    }
    
    /* Stack section */
    .stack : {
        . = . + 0x1000;   /* 4KB stack */
        __stack_top__ = .;
    }
    
    /DISCARD/ : { *(.note*) *(.comment*) *(.ARM.attributes*) }
}