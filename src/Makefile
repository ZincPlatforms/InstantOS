ROOTDIR ?= ..

ARCH := x86_64

CC := gcc
CXX := g++
NASM := nasm

FLAGS := -Wall -Wextra -nostdinc -ffreestanding -fno-stack-protector \
                -fno-stack-check -fno-lto -fno-PIC -ffunction-sections -fdata-sections

CFLAGS := -std=c11 $(FLAGS)
CXXFLAGS := -std=c++23 -fno-rtti -fno-exceptions $(FLAGS)

CPPFLAGS := -I$(ROOTDIR)/src \
            -I$(ROOTDIR)/outside/limine \
            -I$(ROOTDIR)/src/cstd/include \
            -isystem $(ROOTDIR)/src/cxxstd/include

ARCH_FLAGS := -m64 -march=x86-64 -mabi=sysv -mno-80387 -mno-mmx -mno-sse \
              -mno-sse2 -mno-red-zone -mcmodel=kernel

CFLAGS += $(ARCH_FLAGS)
CXXFLAGS += $(ARCH_FLAGS)

NASMFLAGS := -felf64 -g -F dwarf -Wall

C_SRC := $(shell find . -name "*.c")
CPP_SRC := $(shell find . -name "*.cpp")
ASM_SRC := $(shell find . -name "*.asm")

OBJ := $(C_SRC:.c=.o) $(CPP_SRC:.cpp=.o) $(ASM_SRC:.asm=.o)

.PHONY: all clean

all: $(OBJ)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

%.o: %.asm
	$(NASM) $(NASMFLAGS) $< -o $@

clean:
	rm -f $(OBJ)
