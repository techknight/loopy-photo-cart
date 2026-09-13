# Loopy Photo Cart -- the cartridge program
#
# Builds build/loopy-photo-cart-template.bin: the photo viewer with no photos
# in it. The web app appends a photo pack at PACK_OFFSET (docs/PLAN.md).
#
# Layout follows Kasami's loopy-homebrew-template (zlib; see
# LICENSE-loopy-template.txt): boot/ is the SH-1 startup and vector table,
# include/ the hardware headers, platform/ the Casio Loopy backend, src/ the
# program.
#
# Build from Windows with scripts/build.ps1, or inside WSL with
# scripts/build.sh. Requires the Wonderful Toolchain and python3.

SRAMSIZE    = 8K
STACKSIZE   = 8K
# Where the photo pack starts, as an offset into the cartridge. Code, font and
# UI art must fit below it; the linker refuses a build that does not.
PACK_OFFSET = 0x40000

# Stamped into the template descriptor (src/descriptor.c); at most 16 chars.
BUILD_ID ?= dev

WONDERFUL_TOOLCHAIN ?= /opt/wonderful
TOOLBIN ?= $(WONDERFUL_TOOLCHAIN)/toolchain/gcc-sh-elf/bin/
PREFIX  ?= sh-elf-
CC  = $(TOOLBIN)$(PREFIX)gcc
OBJ = $(TOOLBIN)$(PREFIX)objcopy
PYTHON ?= python3

OBJDIR   = obj
BUILDDIR = build
NAME     = loopy-photo-cart-template
ELF      = $(BUILDDIR)/$(NAME).elf
ROM      = $(BUILDDIR)/$(NAME).bin
ROMSWP   = $(BUILDDIR)/$(NAME)-byteswapped.bin
LDSCRIPT = tools/loopy.ld

SRCS_S = $(wildcard boot/*.s)
SRCS_C = $(wildcard boot/*.c) $(wildcard platform/*.c) $(wildcard src/*.c)
OBJS   = $(patsubst %.s,$(OBJDIR)/%.o,$(SRCS_S)) \
         $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS_C))

CFLAGS  = -O2 -g -gdwarf-4
# -m1 selects the SH-1 ISA. -mrenesas is not optional: without it,
# __attribute__((interrupt_handler)) generates the wrong prologue, and the
# calling convention stops matching the BIOS.
CFLAGS += -m1 -mrenesas
CFLAGS += -MMD -MP
CFLAGS += -ffreestanding
CFLAGS += -falign-functions=4 -ffunction-sections -fdata-sections
CFLAGS += -fomit-frame-pointer -fno-asynchronous-unwind-tables -fno-unwind-tables
CFLAGS += -Wall -Wextra -Wno-unused-parameter
# The template's sys_setInterruptPriority() reaches IPRB-IPRE by offsetting
# from INTC_IPRA, which GCC cannot know are contiguous registers.
CFLAGS += -Wno-array-bounds
CFLAGS += -Iinclude -Iplatform -Isrc
CFLAGS += -DCASIO_LOOPY
CFLAGS += -DLPC_PACK_OFFSET=$(PACK_OFFSET)ul -DLPC_BUILD_ID=\"$(BUILD_ID)\"

LDFLAGS  = -nostartfiles -Wl,--gc-sections -Wl,--no-warn-rwx-segment
# A stray section silently vanishing is a very expensive bug on a machine with
# no MMU; make it a link error instead.
LDFLAGS += -Wl,--orphan-handling=error -Wl,--print-memory-usage
LDFLAGS += -Wl,--defsym=SRAMSIZE=$(SRAMSIZE)
LDFLAGS += -Wl,--defsym=STACKSIZE=$(STACKSIZE)
LDFLAGS += -Wl,--defsym=PACK_OFFSET=$(PACK_OFFSET)
LDFLAGS += -Wl,-T $(LDSCRIPT)

# picolibc, for the string and memory routines.
LIBS = -lc

.DEFAULT_GOAL := all
.PHONY: all clean force

all: $(ROM) $(ROMSWP)

# make can't see changes to variables: stamp the ones that change the output
# into a sentinel every object depends on. Its mtime only moves when they do.
FLAGSIG := BUILD_ID=$(BUILD_ID) PACK_OFFSET=$(PACK_OFFSET) SRAMSIZE=$(SRAMSIZE) STACKSIZE=$(STACKSIZE)
force:
$(OBJDIR)/.flags: force
	@mkdir -p $(OBJDIR)
	@printf '%s\n' '$(FLAGSIG)' | cmp -s - $@ 2>/dev/null || printf '%s\n' '$(FLAGSIG)' > $@

$(ELF): $(OBJS) $(LDSCRIPT)
	@mkdir -p $(BUILDDIR)
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

$(ROM): $(ELF)
	$(OBJ) -O binary $< $@
	$(PYTHON) tools/fixrom.py $@

# Byteswapped variant for tooling that expects 16-bit word-swapped images.
$(ROMSWP): $(ROM)
	dd if=$< of=$@ conv=swab status=none

$(OBJDIR)/%.o: %.s $(OBJDIR)/.flags
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.c $(OBJDIR)/.flags
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(BUILDDIR)

-include $(OBJS:.o=.d)
