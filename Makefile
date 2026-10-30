.RECIPEPREFIX := >

ifeq ($(strip $(DEVKITARM)),)
$(error "DEVKITARM non defini")
endif
ifeq ($(strip $(DEVKITPRO)),)
$(error "DEVKITPRO non defini")
endif

TARGET     := roblox3ds
APP_TITLE  := Roblox 3DS
APP_DESC   := Jeu 3D de fan : bombe, roquette, epee
APP_AUTHOR := Moi

CC    := $(DEVKITARM)/bin/arm-none-eabi-gcc
CTRU  := $(DEVKITPRO)/libctru
TOOLS := $(DEVKITPRO)/tools/bin

ARCH    := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS  := -g -Wall -O2 -mword-relocations -ffunction-sections -fomit-frame-pointer $(ARCH) -I$(CTRU)/include -D__3DS__
LDFLAGS := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(TARGET).map -Wl,--gc-sections
LIBS    := -L$(CTRU)/lib -lctru -lm

SRC := $(wildcard source/*.c)
OBJ := $(patsubst source/%.c,build/%.o,$(SRC))

.PHONY: all clean cia

all: $(TARGET).3dsx

build/%.o: source/%.c
> @mkdir -p build
> $(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(TARGET).elf: $(OBJ)
> $(CC) $(LDFLAGS) $(OBJ) $(LIBS) -o $@

$(TARGET).smdh: assets/icon.png
> $(TOOLS)/smdhtool --create "$(APP_TITLE)" "$(APP_DESC)" "$(APP_AUTHOR)" assets/icon.png $@

$(TARGET).3dsx: $(TARGET).elf $(TARGET).smdh
> $(TOOLS)/3dsxtool $< $@ --smdh=$(TARGET).smdh

cia: $(TARGET).elf
> bash tools/make_cia.sh

clean:
> rm -rf build $(TARGET).elf $(TARGET).3dsx $(TARGET).smdh $(TARGET).cia $(TARGET).map banner.bnr icon.icn

-include $(OBJ:.o=.d)
