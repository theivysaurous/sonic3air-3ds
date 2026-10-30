#---------------------------------------------------------------------------------
# Sonic 3 A.I.R. for Nintendo 3DS (New 3DS only, software renderer)
#
#   make            Build sonic3air.3dsx
#   make NATIVIZED=0   Skip the huge "nativized" script code (much faster to compile, but the game runs slower)
#   make clean
#
# Needs devkitPro with devkitARM + libctru, and SDL2 built for the 3DS (see README.md).
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment: export DEVKITPRO=<path to>/devkitpro")
endif
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment: export DEVKITARM=<path to>/devkitARM")
endif

export PATH := $(DEVKITARM)/bin:$(DEVKITPRO)/tools/bin:$(PATH)

TARGET     := sonic3air
BASEDIR    := ../../../..
BUILDDIR   := obj
NATIVIZED  ?= 1
OPTLEVEL   ?= -O2

PORTLIBS   := $(DEVKITPRO)/portlibs/3ds
CTRULIB    := $(DEVKITPRO)/libctru

PREFIX     := arm-none-eabi-
CC         := $(PREFIX)gcc
CXX        := $(PREFIX)g++

APP_TITLE  := Sonic 3 A.I.R.
APP_DESC   := Unofficial experimental 3DS port
APP_AUTHOR := Eukaryot / community

#---------------------------------------------------------------------------------
# Compiler options
#---------------------------------------------------------------------------------
ARCH       := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

INCLUDES   := \
	-I$(BASEDIR)/Oxygen/lemonscript/source \
	-I$(BASEDIR)/Oxygen/oxygenengine/source \
	-I$(BASEDIR)/Oxygen/sonic3air/source \
	-I$(BASEDIR)/librmx/source \
	-I$(BASEDIR)/librmx/source/rmxbase \
	-I$(BASEDIR)/librmx/source/rmxmedia \
	-I$(BASEDIR)/framework/external/ogg-vorbis/libogg/include \
	-I$(BASEDIR)/framework/external/ogg-vorbis/libvorbis/include \
	-I$(BASEDIR)/framework/external/zlib/zlib \
	-I$(BASEDIR)/framework/external/zlib/zlib/contrib/minizip \
	-I$(BASEDIR)/framework/external/imgui \
	-I$(PORTLIBS)/include -I$(CTRULIB)/include

DEFINES    := -D__3DS__ -DARM -DENDUSER -DNDEBUG

COMMON     := -g $(OPTLEVEL) -mword-relocations -ffunction-sections -fdata-sections -w $(ARCH)
CFLAGS     := $(COMMON) $(DEFINES) $(INCLUDES)
CXXFLAGS   := $(CFLAGS) -std=gnu++17 -fexceptions
LDFLAGS    := -specs=3dsx.specs -g $(ARCH) -Wl,--gc-sections -Wl,-Map,$(TARGET).map
LIBPATHS   := -L$(PORTLIBS)/lib -L$(CTRULIB)/lib
# SDL2main provides the real "main" on 3DS (it calls SDL_main, which is what "main" gets renamed to via SDL_main.h)
LIBS       := -lSDL2main -lSDL2 -lctru -lm

#---------------------------------------------------------------------------------
# Sources
#---------------------------------------------------------------------------------
include sources.mk

NATIVIZED_SOURCE := Oxygen/sonic3air/source/sonic3air/_nativized/NativizedCode.cpp

# Third party code, built from the copies in "framework/external" so that no extra portlibs are needed
OGG_SOURCES := \
	framework/external/ogg-vorbis/libogg/src/bitwise.c \
	framework/external/ogg-vorbis/libogg/src/framing.c

VORBIS_SOURCES := $(addprefix framework/external/ogg-vorbis/libvorbis/lib/, \
	analysis.c bitrate.c block.c codebook.c envelope.c floor0.c floor1.c info.c lookup.c lpc.c lsp.c \
	mapping0.c mdct.c psy.c registry.c res0.c sharedbook.c smallft.c synthesis.c vorbisenc.c vorbisfile.c window.c)

ZLIB_SOURCES := $(addprefix framework/external/zlib/zlib/, \
	adler32.c compress.c crc32.c deflate.c infback.c inffast.c inflate.c inftrees.c trees.c uncompr.c zutil.c) \
	framework/external/zlib/zlib/contrib/minizip/ioapi.c \
	framework/external/zlib/zlib/contrib/minizip/unzip.c

CPP_SOURCES := $(ENGINE_SOURCES)
ifeq ($(NATIVIZED),1)
	CPP_SOURCES += $(NATIVIZED_SOURCE)
else
	CPP_SOURCES += stub/NativizedCodeStub.cpp
endif
C_SOURCES := $(OGG_SOURCES) $(VORBIS_SOURCES) $(ZLIB_SOURCES)

# Third party C code needs a few extra flags
THIRDPARTY_CFLAGS := -I$(BASEDIR)/framework/external/ogg-vorbis/libvorbis/lib -DNOUNCRYPT -DNOCRYPT -DHAVE_UNISTD_H=1

OBJECTS := $(addprefix $(BUILDDIR)/, $(addsuffix .o, $(basename $(CPP_SOURCES) $(C_SOURCES))))

#---------------------------------------------------------------------------------
.PHONY: all clean

all: $(TARGET).3dsx

$(TARGET).smdh: icon.png
	@echo "Creating $@"
	@smdhtool --create "$(APP_TITLE)" "$(APP_DESC)" "$(APP_AUTHOR)" icon.png $@

$(TARGET).3dsx: $(TARGET).elf $(TARGET).smdh
	@echo "Creating $@"
	@3dsxtool $(TARGET).elf $@ --smdh=$(TARGET).smdh

$(TARGET).elf: $(OBJECTS)
	@echo "Linking $@"
	@$(CXX) $(LDFLAGS) $(OBJECTS) $(LIBPATHS) $(LIBS) -o $@

# Stub used when building without the nativized script code
stub/NativizedCodeStub.cpp:
	@mkdir -p stub
	@printf '%s\n' '#include <lemon/program/Program.h>' '#include <lemon/runtime/provider/NativizedOpcodeProvider.h>' 'namespace lemon { void createNativizedCodeLookup(Nativizer::LookupDictionary& dict) {} }' > $@

$(BUILDDIR)/stub/%.o: stub/%.cpp
	@mkdir -p $(@D)
	@echo "Compiling $<"
	@$(CXX) -c $(CXXFLAGS) $< -o $@

$(BUILDDIR)/framework/%.o: $(BASEDIR)/framework/%.c
	@mkdir -p $(@D)
	@echo "Compiling $<"
	@$(CC) -c $(CFLAGS) $(THIRDPARTY_CFLAGS) $< -o $@

$(BUILDDIR)/%.o: $(BASEDIR)/%.cpp
	@mkdir -p $(@D)
	@echo "Compiling $<"
	@$(CXX) -c $(CXXFLAGS) $< -o $@

clean:
	@echo "Cleaning"
	@rm -rf $(BUILDDIR) stub $(TARGET).elf $(TARGET).3dsx $(TARGET).smdh $(TARGET).map
