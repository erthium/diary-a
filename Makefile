# Compiler
CXX = g++
CXXFLAGS = -std=c++17 -fPIC `pkg-config --cflags Qt6Widgets`
LDFLAGS = `pkg-config --libs Qt6Widgets` -lssl -lcrypto

# Source files
SRC = src/main.cpp
BUILD_DIR = build
OBJ = $(SRC:src/%.cpp=$(BUILD_DIR)/%.o)
STYLES = $(wildcard styles/*.qss)
DEFAULT_STYLE = lazygit.qss
ASSETS = assets
ICON = icon.png

# Binary output
BIN_DIR = bin
STABLE_DIR = stable
TARGET = diary.a
OUTPUT = $(BIN_DIR)/$(TARGET)
STABLE_OUTPUT = $(STABLE_DIR)/$(TARGET)

# Default user-specific data directories
USER_DATA_DIR = ~/.diary_a
USER_ENTRY_DIR = $(USER_DATA_DIR)/entries
STYLES_DIR = $(USER_DATA_DIR)/styles
PRIVATE_KEY=$(USER_DATA_DIR)/private_key.pem
PUBLIC_KEY=$(USER_DATA_DIR)/public_key.pem
ASSETS_DIR = $(USER_DATA_DIR)/assets

# System-wide data directory
CONFIG_FILE = /etc/diary.conf

# Desktop Application
DESKTOP_DIR=~/.local/share/applications
DESKTOP_FILE=$(DESKTOP_DIR)/$(TARGET).desktop
APP_NAME=Diary-A


# Default build rule
all: $(OUTPUT)
	./$(OUTPUT)


# Link the application
$(OUTPUT): $(OBJ)
	@mkdir -p $(BIN_DIR)
	$(CXX) -o $@ $^ $(LDFLAGS)


# Compile source files to build directory
$(BUILD_DIR)/%.o: src/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) -c -o $@ $< $(CXXFLAGS)


# Ship the binary to stable directory
ship: $(OUTPUT)
	@mkdir -p $(STABLE_DIR)
	@cp $(OUTPUT) $(STABLE_OUTPUT)
	@echo "Shipped $(OUTPUT) to $(STABLE_OUTPUT)"


stable: $(STABLE_OUTPUT)
	./$(STABLE_OUTPUT)


# Installation process with key & config generation
install: $(STABLE_OUTPUT)
	@echo "Creating user data directory..."
	@mkdir -p $(USER_DATA_DIR)
	@mkdir -p $(USER_ENTRY_DIR)
	@mkdir -p $(STYLES_DIR)
	@cp $(STYLES) $(STYLES_DIR)
	@mkdir -p $(ASSETS_DIR)
	@cp -r $(ASSETS)/* $(ASSETS_DIR)

	@echo "user_data_dir=$(USER_DATA_DIR)" | sudo tee $(CONFIG_FILE) > /dev/null
	@echo "entry_dir=$(USER_ENTRY_DIR)" | sudo tee -a $(CONFIG_FILE) > /dev/null
	@echo "styles_dir=$(STYLES_DIR)" | sudo tee -a $(CONFIG_FILE) > /dev/null
	@echo "style=$(DEFAULT_STYLE)" | sudo tee -a $(CONFIG_FILE) > /dev/null
	@echo "assets_dir=$(ASSETS_DIR)" | sudo tee -a $(CONFIG_FILE) > /dev/null
	@echo "icon=$(ASSETS_DIR)/$(ICON)" | sudo tee -a $(CONFIG_FILE) > /dev/null

	@echo "Enter a password to protect the private key:"
	@stty -echo
	@read -r PASSWORD; echo; stty echo; \
	openssl genpkey -algorithm RSA -out $(PRIVATE_KEY) -aes256 -pass pass:$$PASSWORD -pkeyopt rsa_keygen_bits:2048 && \
	openssl rsa -in $(PRIVATE_KEY) -pubout -out $(PUBLIC_KEY) -passin pass:$$PASSWORD

	@echo "private_key=$(PRIVATE_KEY)" | sudo tee -a $(CONFIG_FILE) > /dev/null
	@echo "public_key=$(PUBLIC_KEY)" | sudo tee -a $(CONFIG_FILE) > /dev/null

	@echo "Installing binary..."
	@sudo install -m 755 $(STABLE_OUTPUT) /usr/local/bin/$(TARGET)

	@echo "Installation complete. Keys stored in $(USER_DATA_DIR)"

	@read -p "Do you want to create a desktop shortcut? (y/N) " RESP; \
	if [ "$$RESP" = "y" ] || [ "$$RESP" = "Y" ]; then \
	  mkdir -p $(DESKTOP_DIR); \
	  echo "[Desktop Entry]" > $(DESKTOP_FILE); \
	  echo "Name=$(APP_NAME)" >> $(DESKTOP_FILE); \
	  echo "Exec=/usr/local/bin/$(TARGET)" >> $(DESKTOP_FILE); \
	  echo "Icon=$(ASSETS_DIR)/$(ICON)" >> $(DESKTOP_FILE); \
	  echo "Type=Application" >> $(DESKTOP_FILE); \
	  echo "Categories=Utility;" >> $(DESKTOP_FILE); \
	  echo "Terminal=false" >> $(DESKTOP_FILE); \
	  chmod +x $(DESKTOP_FILE); \
	  update-desktop-database $(DESKTOP_DIR); \
	  echo "Desktop shortcut created!"; \
	else \
	  echo "Skipping desktop shortcut."; \
	fi


# Without key and config generation, upgrade the binary & styles
upgrade: $(STABLE_OUTPUT)
	@echo "Upgrading..."
	@rm -f $(STYLES_DIR)/*.qss
	@cp $(STYLES) $(STYLES_DIR)
	@sudo rm -f /usr/local/bin/$(STABLE_OUTPUT)
	@sudo install -m 755 $(STABLE_OUTPUT) /usr/local/bin/$(TARGET)
	@echo "Upgrade complete."


uninstall:
	@echo "Uninstalling..."
	@sudo rm -f /usr/local/bin/$(TARGET)
	@sudo rm -f $(CONFIG_FILE)
	@rm -f $(DESKTOP_FILE)
	@rm -rf $(USER_DATA_DIR)
	@echo "Uninstallation complete."


# Clean build files
clean:
	rm -f $(OBJ) $(OUTPUT)
