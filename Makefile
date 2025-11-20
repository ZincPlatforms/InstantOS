ARCH := x86_64

CC := gcc
CXX := g++
NASM := nasm
LD := ld

FLAGS := -Wall -Wextra -nostdinc -ffreestanding -fno-stack-protector \
                -fno-stack-check -fno-lto -fno-PIC -ffunction-sections -fdata-sections

CFLAGS := -std=c11 -Ifreestanding/c $(FLAGS)
CXXFLAGS := -std=c++23 -fno-rtti -fno-exceptions -Ifreestanding/c/include -Ifreestanding/cxx/include $(FLAGS)
CPPFLAGS := -I$(CURDIR)/src \
            -I$(CURDIR)/outside/limine \
            -I$(CURDIR)/src/cstd/include \
            -isystem $(CURDIR)/src/cxxstd/include

ARCH_FLAGS := -m64 -march=x86-64 -mabi=sysv -mno-80387 -mno-mmx -mno-sse \
              -mno-sse2 -mno-red-zone -mcmodel=kernel

LDFLAGS := -nostdlib -static -z max-page-size=0x1000 --gc-sections
LD_ARCH := -m elf_x86_64


KERNEL_OUT := $(CURDIR)/build/instant
ISO_ROOT := $(CURDIR)/build/iso_root
ISO_OUT := $(CURDIR)/build/instant.iso

.PHONY: all clean dirs iso

all: dirs
	$(MAKE) -C outside/limine CC=gcc CXX=g++ NASM=nasm
	$(MAKE) -C src ROOTDIR=$(CURDIR)
	$(LD) $(LD_ARCH) $(LDFLAGS) -T linker.ld -o $(KERNEL_OUT) $(shell find src -name "*.cc.o") $(shell find src -name "*.c.o") $(shell find src -name "*.asm.o")

iso: all
	rm -rf $(ISO_ROOT)
	mkdir -p $(ISO_ROOT)
	cp -v $(KERNEL_OUT) $(ISO_ROOT)/
	mkdir -p $(ISO_ROOT)/boot
	cp -v $(CURDIR)/outside/limine/limine-bios.sys \
	      $(CURDIR)/outside/limine/limine-bios-cd.bin \
	      $(CURDIR)/outside/limine/limine-uefi-cd.bin $(ISO_ROOT)/boot/
	mkdir -p $(ISO_ROOT)/EFI/BOOT
	cp -v $(CURDIR)/outside/limine/BOOTX64.EFI $(ISO_ROOT)/EFI/BOOT/
	cp -v $(CURDIR)/outside/limine/BOOTIA32.EFI $(ISO_ROOT)/EFI/BOOT/
	cp -v limine.conf $(ISO_ROOT)/boot/
	xorriso -as mkisofs -b boot/limine-bios-cd.bin \
	        -no-emul-boot -boot-load-size 4 -boot-info-table \
	        --efi-boot boot/limine-uefi-cd.bin \
	        -efi-boot-part --efi-boot-image --protective-msdos-label \
	        $(ISO_ROOT) -o $(ISO_OUT)
	$(CURDIR)/outside/limine/limine bios-install $(ISO_OUT)

dirs:
	mkdir -p build

clean:
	$(MAKE) -C src clean
	rm -rf build
