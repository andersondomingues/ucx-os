TARGET_LIST = \
	'arm/versatilepb' 'avr/atmega328p' 'avr/atmega2560' \
	'mips/hf-risc' 'riscv/hf-riscv' 'riscv/hf-riscv-e' \
	'riscv/hf-riscv-llvm' 'riscv/riscv32-qemu' 'riscv/riscv32-qemu-llvm' \
	'riscv/riscv64-qemu' 'riscv/riscv64-qemu-llvm'

NOC_X = $(shell echo $$(( $(NOC_DIM_X) - 1 )) )
NOC_Y = $(shell echo $$(( $(NOC_DIM_Y) - 1 )) )

NOC_X ?= 4
NOC_Y ?= 4

SERIAL_BAUD=57600
SERIAL_DEVICE=/dev/ttyUSB0

SRC_DIR = .

BUILD_DIR = $(SRC_DIR)/build
BUILD_APP_DIR = $(BUILD_DIR)/app
BUILD_HAL_DIR = $(BUILD_DIR)/hal
BUILD_KERNEL_DIR = $(BUILD_DIR)/kernel
BUILD_TARGET_DIR = $(BUILD_DIR)/target

-include $(BUILD_TARGET_DIR)/target.mak
-include $(SRC_DIR)/arch/$(ARCH)/arch.mak

INC_DIRS += -I $(SRC_DIR)/include

incl:
ifeq ('$(ARCH)', 'none')
	@echo "You must specify target architecture (ARCH=arch/target)."
	@echo "Supported targets are: $(TARGET_LIST)"
else
	@echo "ARCH = $(ARCH)" > $(BUILD_TARGET_DIR)/target.mak
endif


serial:
	stty ${SERIAL_BAUD} raw cs8 -parenb -crtscts clocal cread ignpar ignbrk -ixon -ixoff -ixany -brkint -icrnl -imaxbel -opost -onlcr -isig -icanon -iexten -echo -echoe -echok -echoctl -echoke -F ${SERIAL_DEVICE}

load: serial
	cat image.bin > $(SERIAL_DEVICE)

debug: serial
	cat ${SERIAL_DEVICE}

## RISC-V / Qemu
run_riscv32:
	echo "hit Ctrl+a x to quit"
#	qemu-system-riscv32 -machine virt -nographic -bios $(BUILD_TARGET_DIR)/image.bin -serial mon:stdio
	qemu-system-riscv32 -machine virt -bios none -kernel $(BUILD_TARGET_DIR)/image.elf -nographic

run_riscv64:
	echo "hit Ctrl+a x to quit"
	qemu-system-riscv64 -machine virt -bios none -kernel $(BUILD_TARGET_DIR)/image.elf -nographic
	
run_versatilepb:
	echo "hit Ctrl+a x to quit"
	qemu-system-arm -cpu arm1176 -m 128 -M versatilepb -serial stdio -kernel $(BUILD_TARGET_DIR)/image.elf

## kernel
ucx.pack: incl hal.pack libs.pack kernel.pack devices.pack 
	cp *.o $(SRC_DIR)/build/kernel
	$(AR) $(ARFLAGS) $(BUILD_TARGET_DIR)/libucxos.a $(BUILD_KERNEL_DIR)/*.o
	echo null > ucx.pack

kernel.pack: pipe.o semaphore.o ucx.o main.o
	@echo null > kernel.pack

main.o: $(SRC_DIR)/init/main.c
	$(CC) $(CFLAGS) $(SRC_DIR)/init/main.c
ucx.o: $(SRC_DIR)/kernel/ucx.c
	$(CC) $(CFLAGS) $(SRC_DIR)/kernel/ucx.c
semaphore.o: $(SRC_DIR)/kernel/semaphore.c
	$(CC) $(CFLAGS) $(SRC_DIR)/kernel/semaphore.c
pipe.o: $(SRC_DIR)/kernel/pipe.c
	$(CC) $(CFLAGS) $(SRC_DIR)/kernel/pipe.c

libs.pack: libc.o dump.o malloc.o list.o queue.o noc.o
	@echo null > libs.pack

queue.o: $(SRC_DIR)/lib/queue.c
	$(CC) $(CFLAGS) $(SRC_DIR)/lib/queue.c
list.o: $(SRC_DIR)/lib/list.c
	$(CC) $(CFLAGS) $(SRC_DIR)/lib/list.c
malloc.o: $(SRC_DIR)/lib/malloc.c
	$(CC) $(CFLAGS) $(SRC_DIR)/lib/malloc.c
dump.o: $(SRC_DIR)/lib/dump.c
	$(CC) $(CFLAGS) $(SRC_DIR)/lib/dump.c
libc.o: $(SRC_DIR)/lib/libc.c
	$(CC) $(CFLAGS) $(SRC_DIR)/lib/libc.c
noc.o: $(SRC_DIR)/lib/noc.c
	$(CC) $(CFLAGS) $(SRC_DIR)/lib/noc.c -DNOC_Y=$(NOC_Y) -DNOC_X=$(NOC_X)
		
devices.pack: ddma.o
	@echo null > devices.pack

ddma.o: $(SRC_DIR)/device/ddma.c
	$(CC) $(CFLAGS) $(SRC_DIR)/device/ddma.c

## kernel + application link
link:
ifeq ('$(ARCH)', 'avr/atmega32')
	$(LD) $(LDFLAGS) -o $(BUILD_TARGET_DIR)/image.elf $(BUILD_APP_DIR)/*.o -L$(BUILD_TARGET_DIR) -lucxos
else ifeq ('$(ARCH)', 'avr/atmega328p')
	$(LD) $(LDFLAGS) -o $(BUILD_TARGET_DIR)/image.elf $(BUILD_APP_DIR)/*.o -L$(BUILD_TARGET_DIR) -lucxos
else ifeq ('$(ARCH)', 'avr/atmega2560')
	$(LD) $(LDFLAGS) -o $(BUILD_TARGET_DIR)/image.elf $(BUILD_APP_DIR)/*.o -L$(BUILD_TARGET_DIR) -lucxos
else 
	$(LD) $(LDFLAGS) -T$(LDSCRIPT) -Map $(BUILD_TARGET_DIR)/image.map -o $(BUILD_TARGET_DIR)/image.elf $(BUILD_APP_DIR)/*.o -L$(BUILD_TARGET_DIR) -lucxos
endif
	$(DUMP) --disassemble --reloc $(BUILD_TARGET_DIR)/image.elf > $(BUILD_TARGET_DIR)/image.lst
	$(DUMP) -h $(BUILD_TARGET_DIR)/image.elf > $(BUILD_TARGET_DIR)/image.sec
	$(DUMP) -s $(BUILD_TARGET_DIR)/image.elf > $(BUILD_TARGET_DIR)/image.cnt
	$(OBJ) -O binary $(BUILD_TARGET_DIR)/image.elf $(BUILD_TARGET_DIR)/image.bin
	$(OBJ) -R .eeprom -O ihex $(BUILD_TARGET_DIR)/image.elf $(BUILD_TARGET_DIR)/image.hex
	$(SIZE) $(BUILD_TARGET_DIR)/image.elf
	hexdump -v -e '4/1 "%02x" "\n"' $(BUILD_TARGET_DIR)/image.bin > $(BUILD_TARGET_DIR)/code.txt

## applications
delay: rebuild
	$(CC) $(CFLAGS) -o $(BUILD_APP_DIR)/delay.o app/delay.c
	@$(MAKE) --no-print-directory link

echo: rebuild
	$(CC) $(CFLAGS) -o $(BUILD_APP_DIR)/echo.o app/echo.c
	@$(MAKE) --no-print-directory link

hello: incl ucx.pack 
	$(CC) $(CFLAGS) -o $(BUILD_APP_DIR)/hello.o app/hello.c
	@$(MAKE) --no-print-directory link

hello_p: rebuild
	$(CC) $(CFLAGS) -o $(BUILD_APP_DIR)/hello_preempt.o app/hello_preempt.c
	@$(MAKE) --no-print-directory link

mutex: rebuild
	$(CC) $(CFLAGS) -o $(BUILD_APP_DIR)/mutex.o app/mutex.c
	@$(MAKE) --no-print-directory link
	
pipes: rebuild
	$(CC) $(CFLAGS) -o $(BUILD_APP_DIR)/pipes.o app/pipes.c
	@$(MAKE) --no-print-directory link

pipes_s: rebuild
	$(CC) $(CFLAGS) -o $(BUILD_APP_DIR)/pipes_small.o app/pipes_small.c
	@$(MAKE) --no-print-directory link

pipes_struct: rebuild
	$(CC) $(CFLAGS) -o $(BUILD_APP_DIR)/pipes_struct.o app/pipes_struct.c
	@$(MAKE) --no-print-directory link

prodcons: rebuild
	$(CC) $(CFLAGS) -o $(BUILD_APP_DIR)/prodcons.o app/prodcons.c
	@$(MAKE) --no-print-directory link

progress: rebuild
	$(CC) $(CFLAGS) -o $(BUILD_APP_DIR)/progress.o app/progress.c
	@$(MAKE) --no-print-directory link
	
suspend: rebuild
	$(CC) $(CFLAGS) -o $(BUILD_APP_DIR)/suspend.o app/suspend.c
	@$(MAKE) --no-print-directory link

test64: rebuild
	$(CC) $(CFLAGS) -o $(BUILD_APP_DIR)/test64.o app/test64.c
	@$(MAKE) --no-print-directory link

test_fixed: rebuild
	$(CC) $(CFLAGS) -o $(BUILD_APP_DIR)/test_fixed.o app/test_fixed.c
	@$(MAKE) --no-print-directory link

timer: rebuild
	$(CC) $(CFLAGS) -o $(BUILD_APP_DIR)/timer.o app/timer.c
	@$(MAKE) --no-print-directory link

# clean and rebuild rules
rebuild:
	find '$(BUILD_APP_DIR)' -type f -name '*.o' -delete

clean:
	find '$(BUILD_APP_DIR)' '$(BUILD_KERNEL_DIR)' -type f -name '*.o' -delete
	find '$(BUILD_TARGET_DIR)' -type f -name '*.o' -delete -o -name '*~' -delete \
		-o -name 'image.*' -delete -o -name 'code.*' -delete
	find '$(SRC_DIR)' -type f -name '*.pack' -delete 
	find '$(SRC_DIR)' -type f -name '*.o' -delete 

veryclean: clean
	echo "ARCH = none" > $(BUILD_TARGET_DIR)/target.mak
	find '$(BUILD_TARGET_DIR)' -type f -name '*.a' -delete
	rm -rf img/build*

NOC_X_SEQ = $(shell seq 0 $(NOC_X))
NOC_Y_SEQ = $(shell seq 0 $(NOC_Y))

imgs.pack:
	for x in $(NOC_X_SEQ); do \
		for y in $(NOC_Y_SEQ); do \
			echo "==================================================="; \
			echo "Generating software image for PE XY=("$$x","$$y")"; \
			echo "==================================================="; \
			make hello ARCH=riscv/hf-riscv-e --no-print-directory NOC_X=$$x NOC_Y=$$y ; \
			mkdir -p img/build-$$x-$$y ; \
			cp -r ./build/target/* img/build-$$x-$$y ; \
			rm -rf noc.o ; \
		done ; \
	done
	@$(MAKE) clean
