# Zed Text Editor Makefile

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g $(shell pkg-config --cflags freetype2)
INCLUDES = -Isrc -Ivendor/freetype/include
LIBS = -lX11 -lGL -lGLEW -lpthread -lm $(shell pkg-config --libs freetype2)

# FreeType will be added when vendored
# FREETYPE_OBJS = ...

SRC_DIR = src
BUILD_DIR = build
TARGET = zed

# Source files
SRCS = $(SRC_DIR)/main.cpp

# Object files
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Default target
all: $(BUILD_DIR) $(TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Unity build (fast iteration)
unity: $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TARGET) $(SRC_DIR)/main.cpp $(LIBS)

# Tests
test:
	@echo "Building and running tests..."
	@$(MAKE) -C tests all
	@echo ""
	@echo "Running legacy rope tests..."
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(BUILD_DIR)/rope_test tests/rope_test.cpp $(LIBS)
	@./$(BUILD_DIR)/rope_test

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# Run the editor
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean test run unity
