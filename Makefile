CC = gcc
CFLAGS = -Wall -Wextra -pthread -std=c99
INCLUDES = -Iinclude
LIBS = -lpthread

# Windows-specific libraries
ifeq ($(OS),Windows_NT)
    LIBS += -lws2_32
endif

# Source files
SRCDIR = src
COMPDIR = $(SRCDIR)/components
INCDIR = include
SOURCES = $(COMPDIR)/platform.c \
          $(COMPDIR)/http_parser.c \
          $(COMPDIR)/thread_pool.c \
          $(COMPDIR)/connection_pool.c \
          $(COMPDIR)/cache.c \
          $(COMPDIR)/proxy_server.c \
          $(SRCDIR)/proxy_server.c

# Object files
OBJDIR = obj
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Target executable
TARGET = proxy_server

# Default target
all: $(TARGET)

# Create object directory
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Build object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build the main executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBS) -o $(TARGET)
	@echo "Modular proxy server built successfully!"
	@echo "Usage: ./$(TARGET) [port]"

# Build original monolithic version for comparison
original: $(SRCDIR)/proxy_server_with_cache.c
	$(CC) $(CFLAGS) $(INCLUDES) $< $(LIBS) -o proxy_server_original
	@echo "Original proxy server built successfully!"

# Clean build files
clean:
	rm -rf $(OBJDIR)
	rm -f $(TARGET) proxy_server_original

# Install dependencies (placeholder)
install-deps:
	@echo "Installing dependencies..."
	@echo "Make sure you have gcc and pthread libraries installed"

# Run tests
test: $(TARGET)
	@echo "Running modular proxy server tests..."
	cd tests && ./run_all_tests.ps1

# Performance comparison between modular and original
compare: $(TARGET) original
	@echo "Both versions built. You can now compare performance:"
	@echo "  Modular version: ./$(TARGET) 8080"
	@echo "  Original version: ./proxy_server_original 8080"

# Debug build
debug: CFLAGS += -g -DDEBUG -O0
debug: $(TARGET)
	@echo "Debug version built with symbols"

# Release build  
release: CFLAGS += -O3 -DNDEBUG
release: $(TARGET)
	@echo "Release version built with optimizations"

# Help
help:
	@echo "Available targets:"
	@echo "  all        - Build modular proxy server (default)"
	@echo "  original   - Build original monolithic version"
	@echo "  clean      - Remove build files"
	@echo "  test       - Run test suite"
	@echo "  compare    - Build both versions for comparison"
	@echo "  debug      - Build with debug symbols"
	@echo "  release    - Build optimized release version"
	@echo "  help       - Show this help"

.PHONY: all clean install-deps test compare debug release help original
