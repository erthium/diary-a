# Compiler
CXX = g++
CXXFLAGS = -std=c++17 -fPIC `pkg-config --cflags Qt6Widgets`
LDFLAGS = `pkg-config --libs Qt6Widgets` -lssl -lcrypto

# Source files
SRC = src/main.cpp
OBJ = $(SRC:.cpp=.o)

# Output binary
TARGET = diary.a

# Default user-specific data directories
USER_DATA_DIR = ~/.diary_a
USER_ENTRY_DIR = $(USER_DATA_DIR)/entries
PRIVATE_KEY=$(USER_DATA_DIR)/private_key.pem
PUBLIC_KEY=$(USER_DATA_DIR)/public_key.pem
# System-wide data directory
CONFIG_FILE = /etc/diary.conf


# Default build rule
all: $(TARGET)
	./$(TARGET)


# Link the application
$(TARGET): $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)


# Compile source files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@


# Installation process with key & config generation
install: $(TARGET)
	@echo "Creating user data directory..."
	@mkdir -p $(USER_DATA_DIR)
	@mkdir -p $(USER_ENTRY_DIR)
	@echo "user_data_dir=$(USER_DATA_DIR)" | sudo tee $(CONFIG_FILE) > /dev/null
	@echo "entry_dir=$(USER_ENTRY_DIR)" | sudo tee -a $(CONFIG_FILE) > /dev/null

	@echo "Enter a password to protect the private key:"
	@stty -echo
	@read -r PASSWORD; echo; stty echo; \
	openssl genpkey -algorithm RSA -out $(PRIVATE_KEY) -aes256 -pass pass:$$PASSWORD -pkeyopt rsa_keygen_bits:2048 && \
	openssl rsa -in $(PRIVATE_KEY) -pubout -out $(PUBLIC_KEY) -passin pass:$$PASSWORD

	@echo "private_key=$(PRIVATE_KEY)" | sudo tee -a $(CONFIG_FILE) > /dev/null
	@echo "public_key=$(PUBLIC_KEY)" | sudo tee -a $(CONFIG_FILE) > /dev/null

	@echo "Installing binary..."
	@sudo install -m 755 $(TARGET) /usr/local/bin/

	@echo "Installation complete. Keys stored in $(USER_DATA_DIR)"


# Without key and config generation, upgrade the binary
upgrade: $(TARGET)
	@echo "Upgrading..."
	@sudo install -m 755 $(TARGET) /usr/local/bin/
	@echo "Upgrade complete."


uninstall:
	@echo "Uninstalling..."
	@sudo rm -f /usr/local/bin/$(TARGET)
	@sudo rm -f $(CONFIG_FILE)
	@rm -rf $(USER_DATA_DIR)
	@echo "Uninstallation complete."


# Clean build files
clean:
	rm -f $(OBJ) $(TARGET)
