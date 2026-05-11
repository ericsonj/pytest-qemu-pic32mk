## Local functions
define logger-compile
	@printf "%6s\t%-30s\n" $(1) $(2)
endef

.DEFAULT_GOAL := all

CSRC  =
ASSRC =
INCS  =
COMPILER_FLAGS =
SLIBS_OBJECTS =
SLIBS_NAMES =
SRC_DIRS =

include pymake/vars.mk
include pymake/srcs.mk

ASSRC_s   = $(filter %.s,$(ASSRC))
ASSRC_S   = $(filter %.S,$(ASSRC))
ASSRC_asm = $(filter %.asm,$(ASSRC))

OBJECTS = $(CSRC:%.c=$(PROJECT_OUT)/%.o) \
          $(ASSRC_s:%.s=$(PROJECT_OUT)/%.o) \
          $(ASSRC_S:%.S=$(PROJECT_OUT)/%.o) \
          $(ASSRC_asm:%.asm=$(PROJECT_OUT)/%.o)

include pymake/targets.mk

%.o : CFLAGS = $(COMPILER_FLAGS)

$(PROJECT_OUT)/%.o: %.c
	$(call logger-compile,"CC",$<)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCS) -o $@ -c $<

$(PROJECT_OUT)/%.o: %.S
	$(call logger-compile,"AS",$<)
	@mkdir -p $(dir $@)
	@mkdir -p $(dir $(PROJECT_OUT)/$<.pp)
	@sed -e 's/\r$$//' -e 's/,[ ]*code,[ ]*keep/,"ax"/g' -e 's/,[ ]*code$$/,"ax"/g' $< > $(PROJECT_OUT)/$<.pp
	$(CC) $(CFLAGS) $(INCS) -x assembler-with-cpp -o $@ -c $(PROJECT_OUT)/$<.pp

$(PROJECT_OUT)/%.o: %.s
	$(call logger-compile,"AS",$<)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCS) -o $@ -c $<

all: $(TARGETS)

clean: clean_targets
	@echo 'CLEAN'
	rm -rf $(PROJECT_OUT)

# =============================================================================
# QEMU targets
# =============================================================================
QEMU      := ../../build/qemu-system-mipsel
QEMU_ARGS  = -M pic32mk -bios $(TARGET_BIN) \
             -serial stdio -nographic -monitor none \
             -d unimp,guest_errors

.PHONY: run debug

run: $(TARGET_BIN)
	$(call logger-compile,"RUN",$(TARGET_BIN))
	$(QEMU) $(QEMU_ARGS)

# Starts QEMU halted, waiting for GDB on localhost:1234.
# Connect with: mipsel-linux-gnu-gdb -ex "target remote :1234" Release/pic32mk_rtos_demo.elf
debug: $(TARGET_BIN)
	$(call logger-compile,"DBG","waiting for GDB on :1234")
	$(QEMU) $(QEMU_ARGS) -s -S
