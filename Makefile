GDK ?= /tmp/SGDK
export PATH := /workspace/mdtools/root/usr/bin:/workspace/mdtools/jdk-21.0.13+9-jre/bin:$(PATH)
export LD_LIBRARY_PATH := /workspace/mdtools/root/usr/lib/x86_64-linux-gnu:$(LD_LIBRARY_PATH)
P := m68k-linux-gnu-
CC := $(P)gcc-14
OBJCPY := $(P)objcopy
OUT := out
SRC_C := $(filter-out src/boot/rom_head.c,$(wildcard src/*.c) $(wildcard src/*/*.c))
RES := $(wildcard res/*.res)
OBJS := $(addprefix $(OUT)/,$(RES:.res=.o) $(patsubst src/%.c,src/%.o,$(filter src/%.c,$(SRC_C))))
FLAGS := -I. -Iinc -Ires -Isrc -I$(OUT)/res -isystem$(GDK)/inc -isystem$(GDK)/res -DSGDK_GCC -m68000 -Wall -Wextra -Wno-shift-negative-value -Wno-main -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable -fno-builtin -ffunction-sections -fdata-sections -fms-extensions -O2 -fomit-frame-pointer
AFLAGS := -x assembler-with-cpp -Wa,--register-prefix-optional,--bitwise-or $(FLAGS)

.PHONY: all clean
all: $(OUT)/rom.bin

$(OUT)/rom.bin: $(OUT)/rom.out
	$(OBJCPY) -O binary $< $@
	java -jar $(GDK)/bin/sizebnd.jar $@ -sizealign 131072 -checksum

$(OUT)/rom.out: $(OUT)/sega.o $(OBJS)
	$(CC) -m68000 -n -T $(GDK)/md.ld -nostdlib $^ /workspace/mdtools/lib/libmd_gcc14.a $(GDK)/lib/libgcc.a -o $@ -Wl,--gc-sections -Wl,--build-id=none

$(OUT)/sega.o: $(OUT)/rom_head.bin
	cp $(GDK)/src/boot/sega.s $(OUT)/sega.s
	$(CC) $(AFLAGS) -c $(OUT)/sega.s -o $@

$(OUT)/rom_head.bin: src/boot/rom_head.c
	mkdir -p $(OUT)/src $(OUT)/res
	$(CC) $(FLAGS) -c $< -o $(OUT)/rom_head.o
	$(OBJCPY) -O binary $(OUT)/rom_head.o $@

$(OUT)/res/%.o: res/%.res
	mkdir -p $(OUT)/res
	java -jar $(GDK)/bin/rescomp.jar $< $(OUT)/res/$*.s
	cp -f $(OUT)/res/$*.h res/$*.h
	$(CC) $(AFLAGS) -c $(OUT)/res/$*.s -o $@

$(OUT)/src/%.o: src/%.c src/*.h $(OUT)/res/resources.o
	mkdir -p $(OUT)/src
	$(CC) $(FLAGS) -c $< -o $@

clean:
	rm -rf out
