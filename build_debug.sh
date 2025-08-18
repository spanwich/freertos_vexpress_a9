#!/bin/bash
# Build script for FreeRTOS memory debugging binary
# Based on PhD research methodology for memory pattern debugging

set -e  # Exit on any error

echo "=========================================="
echo "  FreeRTOS Memory Debug Build Script"
echo "  PhD Research - Secure Virtualization"
echo "=========================================="

# Configuration
BUILD_DIR="/home/konton-otome/phd/freertos_vexpress_a9/Build"
SOURCE_DIR="/home/konton-otome/phd/freertos_vexpress_a9/Source"
OUTPUT_DIR="/home/konton-otome/phd/camkes-vm-examples/projects/vm-examples/apps/Arm/vm_freertos/qemu-arm-virt"

# Build type (normal or debug)
BUILD_TYPE=${1:-debug}

echo "Build type: $BUILD_TYPE"
echo "Build directory: $BUILD_DIR"
echo "Output directory: $OUTPUT_DIR"

cd "$BUILD_DIR"

# Clean previous build
echo "Cleaning previous build..."
make clean

# Select main source file based on build type
if [ "$BUILD_TYPE" = "debug" ]; then
    MAIN_SOURCE="$SOURCE_DIR/main_memory_debug.c"
    OUTPUT_PREFIX="freertos_debug"
    echo "Using debug main: $MAIN_SOURCE"
else
    MAIN_SOURCE="$SOURCE_DIR/main.c"
    OUTPUT_PREFIX="freertos"
    echo "Using normal main: $MAIN_SOURCE"
fi

# Verify main source exists
if [ ! -f "$MAIN_SOURCE" ]; then
    echo "ERROR: Main source file not found: $MAIN_SOURCE"
    exit 1
fi

# Compile main source
echo "Compiling main source..."
arm-none-eabi-gcc -mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=softfp -marm -nostdlib -ffreestanding \
    -I../Source/include -I../Source/portable/GCC/ARM_CA9 -I../Source -O2 \
    -c -o ../Source/main_temp.o "$MAIN_SOURCE"

# Compile other required objects if not present
echo "Compiling FreeRTOS components..."

# Startup
if [ ! -f ../Startup/startup.o ]; then
    arm-none-eabi-gcc -mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=softfp -marm -nostdlib -ffreestanding \
        -I../Source/include -I../Source/portable/GCC/ARM_CA9 -I../Source -O2 \
        -c -o ../Startup/startup.o ../Startup/startup.S
fi

# FreeRTOS core
for source in tasks.c queue.c list.c timers.c event_groups.c stream_buffer.c; do
    obj_file="../Source/${source%.c}.o"
    if [ ! -f "$obj_file" ]; then
        echo "  Compiling $source..."
        arm-none-eabi-gcc -mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=softfp -marm -nostdlib -ffreestanding \
            -I../Source/include -I../Source/portable/GCC/ARM_CA9 -I../Source -O2 \
            -c -o "$obj_file" "../Source/$source"
    fi
done

# FreeRTOS port
for source in port.c portASM.S; do
    obj_file="../Source/portable/GCC/ARM_CA9/${source%.*}.o"
    if [ ! -f "$obj_file" ]; then
        echo "  Compiling port $source..."
        arm-none-eabi-gcc -mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=softfp -marm -nostdlib -ffreestanding \
            -I../Source/include -I../Source/portable/GCC/ARM_CA9 -I../Source -O2 \
            -c -o "$obj_file" "../Source/portable/GCC/ARM_CA9/$source"
    fi
done

# Memory management
obj_file="../Source/portable/MemMang/heap_4.o"
if [ ! -f "$obj_file" ]; then
    echo "  Compiling heap_4.c..."
    arm-none-eabi-gcc -mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=softfp -marm -nostdlib -ffreestanding \
        -I../Source/include -I../Source/portable/GCC/ARM_CA9 -I../Source -O2 \
        -c -o "$obj_file" "../Source/portable/MemMang/heap_4.c"
fi

# Link everything together
echo "Linking $OUTPUT_PREFIX.elf..."
arm-none-eabi-gcc -mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=softfp -T../Startup/link.ld -nostdlib \
    -o "${OUTPUT_PREFIX}.elf" \
    ../Startup/startup.o \
    ../Source/main_temp.o \
    ../Source/tasks.o \
    ../Source/queue.o \
    ../Source/list.o \
    ../Source/timers.o \
    ../Source/event_groups.o \
    ../Source/stream_buffer.o \
    ../Source/portable/MemMang/heap_4.o \
    ../Source/portable/GCC/ARM_CA9/port.o \
    ../Source/portable/GCC/ARM_CA9/portASM.o

# Convert to binary format for seL4 VM
echo "Converting to binary format..."
arm-none-eabi-objcopy -O binary "${OUTPUT_PREFIX}.elf" "${OUTPUT_PREFIX}_image.bin"

# Copy to seL4 VM directory
echo "Copying to seL4 VM directory..."
cp "${OUTPUT_PREFIX}_image.bin" "$OUTPUT_DIR/"

# Clean up temporary object
rm -f ../Source/main_temp.o

# Display build information
echo ""
echo "Build completed successfully!"
echo ""
echo "Files generated:"
echo "  ELF: $BUILD_DIR/${OUTPUT_PREFIX}.elf"
echo "  Binary: $BUILD_DIR/${OUTPUT_PREFIX}_image.bin"
echo "  Deployed: $OUTPUT_DIR/${OUTPUT_PREFIX}_image.bin"
echo ""

# Show binary size and information
if command -v arm-none-eabi-size >/dev/null 2>&1; then
    echo "Binary size information:"
    arm-none-eabi-size "${OUTPUT_PREFIX}.elf"
    echo ""
fi

if command -v arm-none-eabi-objdump >/dev/null 2>&1; then
    echo "Section headers:"
    arm-none-eabi-objdump -h "${OUTPUT_PREFIX}.elf" | head -20
    echo ""
    
    echo "Entry point and key symbols:"
    arm-none-eabi-objdump -t "${OUTPUT_PREFIX}.elf" | grep -E "(main|_start|vMemoryPattern)" | head -10
    echo ""
fi

echo "Next steps:"
echo "1. Update seL4 CMakeLists.txt to use ${OUTPUT_PREFIX}_image.bin"
echo "2. Build seL4 VM with: cd camkes-vm-examples/build && ninja"
echo "3. Run with QEMU monitor: ./simulate with -monitor tcp:127.0.0.1:55555,server,nowait"
echo "4. Use memory analyzer: python3 qemu_memory_analyzer.py"
echo ""
echo "For instruction tracing, add to QEMU: -d exec,cpu -D trace.log"