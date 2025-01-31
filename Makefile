# Compiler and flags
CC = clang
CFLAGS = -Wall -Werror -O3 -std=c23 -fPIC
LDFLAGS = -shared

# GStreamer includes and libraries (using pkg-config)
GSTREAMER_CFLAGS = $(shell pkg-config --cflags gstreamer-1.0 gstreamer-video-1.0)
GSTREAMER_LIBS = $(shell pkg-config --libs gstreamer-1.0 gstreamer-video-1.0)

# Gifski includes and library
GIFSKI_INCLUDE = -I/opt/software/gifski
GIFSKI_LIB = /opt/software/gifski/target/release/libgifski.a

# Source file, object file, and shared library
SRC = gstgifskienc.c
OBJ = gstgifskienc.o
PLUGIN = gstgifskienc.so

# Installation directory
INSTALL_DIR = /usr/lib/x86_64-linux-gnu/gstreamer-1.0

# Default target: build the plugin
all: $(PLUGIN)

# Compile the source file to an object file
$(OBJ): $(SRC)
	$(CC) $(CFLAGS) $(GSTREAMER_CFLAGS) $(GIFSKI_INCLUDE) -c $< -o $@

# Link the object file and libraries to create the shared library
$(PLUGIN): $(OBJ)
	$(CC) $(LDFLAGS) $< -o $@ $(GSTREAMER_LIBS) $(GIFSKI_LIB) -lm

# Install target: copy the plugin to the installation directory
install: $(PLUGIN)
	sudo cp $(PLUGIN) $(INSTALL_DIR)/lib$(PLUGIN)

# Clean target: remove object file and shared library
clean:
	rm -f $(OBJ) $(PLUGIN)

# Phony targets
.PHONY: all install clean
