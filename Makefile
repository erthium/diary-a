# Compiler
CXX = g++
CXXFLAGS = -std=c++17 -fPIC `pkg-config --cflags Qt6Widgets`
LDFLAGS = `pkg-config --libs Qt6Widgets`

# Source files
SRC = src/main.cpp
OBJ = $(SRC:.cpp=.o)

# Output binary
TARGET = bin/diary.a

# Default build rule
all: $(TARGET)
	./$(TARGET)

# Link the application
$(TARGET): $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)

# Compile source files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Install (copies the binary to /usr/local/bin)
install: $(TARGET)
	sudo install -m 755 $(TARGET) /usr/local/bin/

# Uninstall
uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)

# Clean build files
clean:
	rm -f $(OBJ) $(TARGET)
