CC=arm-none-eabi-gcc
CFLAGS=-mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=softfp -marm -nostdlib -ffreestanding -I../Source/include -I../Source/portable/GCC/ARM_CA9 -I../Source -O2
LDFLAGS=-T../Startup/link.ld -nostdlib -mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=softfp

FREERTOS_OBJS=../Source/tasks.o ../Source/queue.o ../Source/list.o ../Source/timers.o ../Source/event_groups.o ../Source/stream_buffer.o ../Source/portable/MemMang/heap_4.o
PORT_OBJS=../Source/portable/GCC/ARM_CA9/port.o ../Source/portable/GCC/ARM_CA9/portASM.o
OBJS=../Startup/startup.o ../Source/main.o $(FREERTOS_OBJS) $(PORT_OBJS)

all: freertos.elf

freertos.elf: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f *.o ../Source/*.o ../Source/portable/GCC/ARM_CA9/*.o ../Startup/*.o freertos.elf

../Startup/startup.o: ../Startup/startup.S
	$(CC) $(CFLAGS) -c -o $@ $<

../Source/main.o: ../Source/main.c
	$(CC) $(CFLAGS) -c -o $@ $<

../Source/%.o: ../Source/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

../Source/portable/GCC/ARM_CA9/%.o: ../Source/portable/GCC/ARM_CA9/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

../Source/portable/GCC/ARM_CA9/portASM.o: ../Source/portable/GCC/ARM_CA9/portASM.S
	$(CC) $(CFLAGS) -c -o $@ $<

../Source/portable/MemMang/%.o: ../Source/portable/MemMang/%.c
	$(CC) $(CFLAGS) -c -o $@ $<