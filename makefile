# === Projekt-Einstellungen ===
TARGET    = demo
BUILD_DIR = build

#Set to 1 when using ADE
USE_ADE = 1

ifeq ($(USE_ADE), 0)
    CXX = m68k-amigaos-g++
    CC	= m68k-amigaos-gcc
else
    CXX = g++
    CC  = gcc
endif

# === Flags ===
# -I. erlaubt das Inkludieren aus dem Projekt-Root
ifeq ($(USE_ADE), 0)
    COMMON_FLAGS = -O3 -Wall -m68040 -m68881 -ffast-math -fomit-frame-pointer -noixemul -fno-rtti -fno-exceptions -s -I.
else
    COMMON_FLAGS = -O3 -Wall -m68040 -m68881 -ffast-math -fomit-frame-pointer -noixemul -fno-rtti -fno-exceptions -DOLD_GCC -s -I.
endif

# C++ spezifisch
CXXFLAGS     = $(COMMON_FLAGS) -std=c++11

# C spezifisch (inkl. Vorbis Big Endian Fix) und Linker Flags
ifeq ($(USE_ADE), 0)
    CFLAGS  = $(COMMON_FLAGS) -DSTB_VORBIS_BIG_ENDIAN -DDR_MP3_IMPLEMENTATION -DDR_FLAC_BIG_ENDIAN
    LDFLAGS = -noixemul -Wl,-static -Wl,--gc-sections
else
    CFLAGS  = $(COMMON_FLAGS) -DSTB_VORBIS_BIG_ENDIAN -DDR_MP3_IMPLEMENTATION -DDR_FLAC_BIG_ENDIAN 
    LDFLAGS = -noixemul -Wl,-static -fvtable-gc -fdata-sections -ffunction-section
endif
LIBS         = -lamiga

# === Source-Scanning ===
# Findet alle .cpp und .c Dateien in allen Unterordnern
SRC_CPP := $(shell find . -name "*.cpp")
SRC_C   := $(shell find . -name "*.c")

# Generiert die Pfade für die Objekt-Dateien im build/ Verzeichnis
OBJ = $(SRC_CPP:%.cpp=$(BUILD_DIR)/%.o) $(SRC_C:%.c=$(BUILD_DIR)/%.o)

# === Haupt-Regeln ===
all: $(TARGET)

# Linken der fertigen Datei
$(TARGET): $(OBJ)
	@echo ">>> Linking $(TARGET)..."
	$(CXX) $(OBJ) -o $@ $(LDFLAGS) $(LIBS)
	@echo "Done."

# Regel für C++ Dateien (.cpp)
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "Compiling C++: $<"
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $@

# Regel für C Dateien (.c)
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "Compiling C:   $<"
	$(CC) $(CFLAGS) -MMD -c $< -o $@

# Einbinden der Header-Abhängigkeiten
-include $(OBJ:.o=.d)

# Aufräumen
clean:
	@echo "Cleaning up..."
	rm -rf $(BUILD_DIR) $(TARGET) *.d

# Hilfs-Funktion zum Debuggen des Scanners
list:
	@echo "Gefundene Quellen:"
	@echo "CPP: $(SRC_CPP)"
	@echo "C:   $(SRC_C)"
	@echo "Objekte: $(OBJ)"

.PHONY: all clean list