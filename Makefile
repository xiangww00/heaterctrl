
UNAME := $(shell uname -s)

MAIN_VER = 1
MINOR_VER = 01
REL_VERSION = $(MAIN_VER).$(MINOR_VER)

SVN_REV = $(shell svn info 2> /dev/null | grep Revision | cut -d' ' -f2)
CHECK_CHG = $(notdir $(filter-out M,$(shell svn -q status )))

empty :=
SPACE :=$(empty) $(empty)

SVN_REV_FILE :=$(subst $(SPACE),.,$(SVN_REV))

ifeq ($(OS),Windows_NT)
TOOLS_PATH := /opt/gcc-arm-none-eabi-7-2018-q2-update/bin
else
ifeq ($(UNAME), Linux)
TOOLS_PATH := /opt/toolchains/gcc-arm-none-eabi-7-2018-q2-update/bin
endif
ifeq ($(UNAME),Darwin)
TOOLS_PATH := /Users/weiwen/tools/gcc-arm-none-eabi-7-2018-q2-update/bin
endif
endif

PREFIX := $(TOOLS_PATH)/arm-none-eabi-
CC := $(PREFIX)gcc
AS := $(PREFIX)as
AR := $(PREFIX)ar
LD := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy

SED := sed
MV := mv
RM := rm 
DD := dd


.SUFFIXES: .o .c .s 

TOP_DIR = .

APP = $(TOP_DIR)/app
LIB = $(TOP_DIR)/lib

CMSIS_DIR = $(LIB)/CMSIS
DRIVER = $(LIB)/Driver
START = $(CMSIS_DIR)/Source

output_dir = $(TOP_DIR)/build

INC        =  -I$(CMSIS_DIR)/Include  -I$(DRIVER)/inc -I$(APP) 

#########################################################################

APP_FLAGS = 

STARTUP_DEFS = -D__STARTUP_CLEAR_BSS -D__START=main
DFLAGS =  -DSTM32F030 -DUSE_STDPERIPH_DRIVER 

DFLAGS += $(APP_FLAGS)

ifneq ($(REL),yes)
DFLAGS += -DDEBUG
endif


#########################################################################

ver = $(APP)/ver.h

driver_src = \
            $(DRIVER)/src/stm32f0xx_rtc.c    \
            $(DRIVER)/src/stm32f0xx_flash.c   \


#           $(DRIVER)/src/stm32f0xx_gpio.c   \
            $(DRIVER)/src/stm32f0xx_misc.c   \
            $(DRIVER)/src/stm32f0xx_iwdg.c   \
            $(DRIVER)/src/stm32f0xx_exti.c   \
            $(DRIVER)/src/stm32f0xx_misc.c   \
            $(DRIVER)/src/stm32f0xx_usart.c  \
            $(DRIVER)/src/stm32f0xx_dma.c    \
            $(DRIVER)/src/stm32f0xx_tim.c    \
            $(DRIVER)/src/stm32f0xx_adc.c   \
            $(DRIVER)/src/stm32f0xx_rcc.c    \


startup_src =$(START)/startup_stm32f030.s

                
usr_src =   $(APP)/main.c              \
            $(APP)/retarget.c          \
            $(APP)/board.c             \
            $(APP)/uart.c              \
            $(APP)/rtc.c               \
            $(APP)/adc.c               \
            $(APP)/crc8.c              \
            $(APP)/comslave.c          \
            $(APP)/loop.c              \
            $(APP)/parameter.c         \
            $(APP)/stm32f0xx_it.c      \
            $(APP)/system_stm32f0xx.c  \
            
#            $(APP)/crc16.c             \
#            $(APP)/mbslave.c           \

###############################################################################
ldscript := stm32f030r8.ld
PACK_NAME = yl030

packed_file :=$(output_dir)/$(PACK_NAME).v$(REL_VERSION).$(SVN_REV_FILE)_$(shell date +%Y%m%d_%H%M%S).tar.gz


app_bin := $(output_dir)/yl030.bin
app_elf := $(output_dir)/yl030.out

memmap = $(output_dir)/yl030.map

CPPFLAGS = $(INC) $(DFLAGS) -ffunction-sections -fdata-sections --specs=nano.specs \
            -Wl,--gc-sections -fno-builtin  -Wall -Wextra  -Wno-format  -Os  -flto

CFLAGS = -mcpu=cortex-m0 -mthumb -march=armv6-m
LDFLAGS = -T $(ldscript) -Wl,-Map=$(memmap)  -mthumb \
          -mcpu=cortex-m0 --specs=nosys.specs --specs=nano.specs  -fno-builtin -flto
LDFLAGS += -nostartfiles $(STARTUP_DEFS)  #-nostdlib -nodefaultlibs -lgcc 

#CFLAGS += -mfloat-abi=soft 
#LDFLAGS += -mfloat-abi=soft  -lm 

##############################################################################

driver_objects := $(addprefix $(output_dir)/, $(driver_src:.c=.o))

app_objects := $(addprefix $(output_dir)/, $(startup_src:.s=.o))
app_objects += $(addprefix $(output_dir)/, $(usr_src:.c=.o))

app_assb := $(addprefix $(output_dir)/, $(usr_src:.c=.s))
app_assb += $(driver_objects:.o=.s) 

dependencies := $(addprefix $(output_dir)/, $(driver_src:.c=.d))
dependencies += $(addprefix $(output_dir)/, $(usr_src:.c=.d))

define make-depend
  $(CC) $(CPPFLAGS) -MM $1 2> /dev/null| \
  $(SED) -e 's,\($(notdir $2)\) *:,$(dir $2)\1: ,' > $3.tmp
  $(SED) -e 's/#.*//' \
      -e 's/^[^:]*: *//' \
      -e 's/ *\\$$//' \
      -e '/^$$/ d' \
      -e 's/$$/ :/' $3.tmp >> $3.tmp
  $(MV) $3.tmp $3
endef

.PHONY: all clean assb 

all: $(packed_file)
ifneq "$(strip $(CHECK_CHG))" ""	
	@echo Dirty Version Build 
else	
	@echo Clean Version Build 
endif


$(packed_file): $(app_bin) 
	@tar zcvf $(packed_file) $(app_bin) 
	@echo Generating $(notdir $@) 

assb: $(ver) $(app_assb)

$(app_elf): $(ver) $(app_objects) $(driver_objects) $(ldscript) 
	@echo Linking $(notdir $@)
	@$(LD)  -o $@ $(app_objects) $(driver_objects) $(LDFLAGS) 

$(app_bin): $(app_elf)
	@echo Creating Binary file $(notdir $@)  
	@$(OBJCOPY) -O binary $(app_elf) $(app_bin)
	@echo Done

$(ver): version.sh
	@echo Generating version file $(ver) 
	@./version.sh
	@echo "#define MAIN_VERSION $(MAIN_VER)$(MINOR_VER) " >>$(ver) 
	@echo "#define COMPILE_BY \"$(shell whoami)\" " >>$(ver) 
	@echo "#define COMPILE_AT \"$(shell hostname)\" " >>$(ver) 
	@echo "#define COMPILER \"$(shell $(CC) -v 2>&1 | tail -n 1)\" " >>$(ver) 
ifneq "$(strip $(CHECK_CHG))" ""	
	@echo "#define VERSION_CHECK \"dirty\" " >>$(ver) 
else	
	@echo "#define VERSION_CHECK \"clean\" " >>$(ver) 
endif

clean:
	-@$(RM) -rf $(output_dir) 2> /dev/null
	-@$(RM) -rf $(ver) 



$(output_dir)/%.o : %.c
	@echo $<
	@mkdir -p $(dir $@)
	@$(call make-depend,$<,$@,$(subst .o,.d,$@))
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(output_dir)/%.o : %.s
	@echo $<
	@mkdir -p $(dir $@)
	@$(AS) $(CFLAGS) $< -o $@

$(output_boot)/%.o : %.c
	@echo $<
	@mkdir -p $(dir $@)
	@$(call make-depend,$<,$@,$(subst .o,.d,$@))
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(output_boot)/%.o : %.s
	@echo $<
	@mkdir -p $(dir $@)
	@$(AS) $(CFLAGS) $< -o $@


$(output_dir)/%.s : %.c
	@echo $<
	@mkdir -p $(dir $@)
	@$(call make-depend,$<,$@,$(subst .o,.d,$@))
	@$(CC) $(CPPFLAGS) $(CFLAGS) -fno-lto -S $< -o $@

ifneq "$(MAKECMDGOALS)" "clean"
-include $(dependencies)
endif

