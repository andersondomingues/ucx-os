# this is stuff specific to this architecture
ARCH_DIR = $(SRC_DIR)/arch/$(ARCH)
INC_DIRS  = -I $(ARCH_DIR)

F_CLK=25000000

#remove unreferenced functions
CFLAGS_STRIP = -fdata-sections -ffunction-sections
LDFLAGS_STRIP = --gc-sections

# this is stuff used everywhere - compiler and flags should be declared (ASFLAGS, CFLAGS, LDFLAGS, LD_SCRIPT, CC, AS, LD, DUMP, READ, OBJ and SIZE).
ASFLAGS = -march=rv32e -mabi=ilp32e #-fPIC
CFLAGS = -Wall -march=rv32e -mabi=ilp32e -O2 -c -mstrict-align -ffreestanding -nostdlib -ffixed-a5 $(INC_DIRS) -DCPU_SPEED=${F_CLK} -DLITTLE_ENDIAN $(CFLAGS_STRIP) #-mrvc -fPIC -DDEBUG_PORT
ARFLAGS = r

LDFLAGS = -melf32lriscv $(LDFLAGS_STRIP)
LDSCRIPT = $(ARCH_DIR)/hf-risc.ld

CC = riscv64-elf-gcc
AS = riscv64-elf-as
LD = riscv64-elf-ld
DUMP = riscv64-elf-objdump -Mno-aliases
READ = riscv64-elf-readelf
OBJ = riscv64-elf-objcopy
SIZE = riscv64-elf-size
AR = riscv64-elf-ar

HAL_SRC = $(ARCH_DIR)/hal.c \
		$(ARCH_DIR)/interrupt.c \
		$(ARCH_DIR)/../../common/muldiv.c

hal.pack: crt0.o $(HAL_SRC)
	$(CC) $(CFLAGS) $(HAL_SRC)
	echo null > hal.pack

crt0.o: 
	$(AS) $(ASFLAGS) -o crt0.o $(ARCH_DIR)/crt0.s
