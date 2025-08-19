/*
 * Simple heap implementation without scheduler dependencies
 * for use during early initialization before scheduler starts
 */

#include "FreeRTOS.h"

/* Simple heap - just a static buffer */
static uint8_t ucHeap[configTOTAL_HEAP_SIZE];
static size_t xNextFreeByte = 0;

void * pvPortMalloc( size_t xWantedSize )
{
    void * pvReturn = NULL;
    
    /* Simple alignment */
    if (xWantedSize & 3) {
        xWantedSize = (xWantedSize + 4) & ~3;
    }
    
    /* Check if we have enough space */
    if (xNextFreeByte + xWantedSize <= configTOTAL_HEAP_SIZE) {
        pvReturn = &ucHeap[xNextFreeByte];
        xNextFreeByte += xWantedSize;
    }
    
    return pvReturn;
}

void vPortFree( void * pv )
{
    /* Memory cannot be freed using this simple scheme */
    (void) pv;
}

void vPortInitialiseBlocks( void )
{
    xNextFreeByte = 0;
}

size_t xPortGetFreeHeapSize( void )
{
    return (configTOTAL_HEAP_SIZE - xNextFreeByte);
}

size_t xPortGetMinimumEverFreeHeapSize( void )
{
    return (configTOTAL_HEAP_SIZE - xNextFreeByte);
}