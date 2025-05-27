# Compiler
CXX = g++
CXXFLAGS = -std=c++17 -fPIC -I./include `pkg-config --cflags Qt6Widgets`
LDFLAGS = `pkg-config --libs Qt6Widgets` -lssl -lcrypto

# Colors
BLACK := \033[0;30m
RED := \033[0;31m
GREEN := \033[0;32m
YELLOW := \033[0;33m
BLUE := \033[0;34m
MAGENTA := \033[0;35m
CYAN := \033[0;36m
WHITE := \033[0;37m
BOLD := \033[1m
RESET := \033[0m

# Source files
SRC_DIR = src
BUILD_DIR = build
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJ = $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
STYLES = $(wildcard styles/*.qss)
DEFAULT_STYLE = lazygit.qss
ASSETS = assets
ICON = icon.png
VERSION_TXT = version.txt

# Version management
VERSION := $(shell cat $(VERSION_TXT) 2>/dev/null || echo "0.0.0")
VERSION_MAJOR := $(shell echo $(VERSION) | cut -d. -f1)
VERSION_MINOR := $(shell echo $(VERSION) | cut -d. -f2)
VERSION_PATCH := $(shell echo $(VERSION) | cut -d. -f3)

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

# Run the application
run: $(OUTPUT)
	@echo -e "$(GREEN)Running $(TARGET)...$(RESET)"
	./$(OUTPUT)

# Link the application
$(OUTPUT): $(OBJ)
	@mkdir -p $(BIN_DIR)
	@echo -e "$(CYAN)Linking $(TARGET)...$(RESET)"
	$(CXX) -o $@ $^ $(LDFLAGS)
	@echo -e "$(GREEN)Build complete!$(RESET)"

# Compile source files to build directory
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	@echo -e "$(CYAN)Compiling $<...$(RESET)"
	$(CXX) -c -o $@ $< $(CXXFLAGS)

# Ship the binary to stable directory
ship: $(OUTPUT)
	@mkdir -p $(STABLE_DIR)
	@cp $(OUTPUT) $(STABLE_OUTPUT)
	@echo -e "$(GREEN)Shipped $(OUTPUT) to $(STABLE_OUTPUT)$(RESET)"

# Run the stable version
stable: $(STABLE_OUTPUT)
	@echo -e "$(GREEN)Running stable version...$(RESET)"
	./$(STABLE_OUTPUT)

# Version management rules
version:
	@echo -e "$(BOLD)Current version: $(YELLOW)$(VERSION)$(RESET)"
	@echo -e "$(CYAN)Usage:$(RESET)"
	@echo -e "  $(GREEN)make version-major$(RESET)    # Increment major version (X.0.0)"
	@echo -e "  $(GREEN)make version-minor$(RESET)    # Increment minor version (0.X.0)"
	@echo -e "  $(GREEN)make version-patch$(RESET)    # Increment patch version (0.0.X)"

version-major:
	@echo "$$(($(VERSION_MAJOR) + 1)).0.0" > $(VERSION_TXT)
	@echo -e "$(GREEN)Version updated to $(YELLOW)$$(cat $(VERSION_TXT))$(RESET)"

version-minor:
	@echo "$(VERSION_MAJOR).$$(($(VERSION_MINOR) + 1)).0" > $(VERSION_TXT)
	@echo -e "$(GREEN)Version updated to $(YELLOW)$$(cat $(VERSION_TXT))$(RESET)"

version-patch:
	@echo "$(VERSION_MAJOR).$(VERSION_MINOR).$$(($(VERSION_PATCH) + 1))" > $(VERSION_TXT)
	@echo -e "$(GREEN)Version updated to $(YELLOW)$$(cat $(VERSION_TXT))$(RESET)"

# Check installed version
check-version:
	@if [ -f $(CONFIG_FILE) ]; then \
		INSTALLED_VERSION=$$(grep "^version=" $(CONFIG_FILE) | cut -d= -f2); \
		if [ -n "$$INSTALLED_VERSION" ]; then \
			echo -e "$(CYAN)Installed version: $(YELLOW)$$INSTALLED_VERSION$(RESET)"; \
			echo -e "$(CYAN)Current version: $(YELLOW)$(VERSION)$(RESET)"; \
			if [ "$$INSTALLED_VERSION" != "$(VERSION)" ]; then \
				echo -e "$(RED)Version mismatch! Consider upgrading.$(RESET)"; \
			else \
				echo -e "$(GREEN)Versions match.$(RESET)"; \
			fi; \
		else \
			echo -e "$(YELLOW)No version information found in config file.$(RESET)"; \
		fi; \
	else \
		echo -e "$(RED)Application not installed.$(RESET)"; \
	fi

# Installation process with key & config generation
install: $(STABLE_OUTPUT)
	@if [ -f $(CONFIG_FILE) ]; then \
		echo -e "$(RED)Warning: Application is already installed!$(RESET)"; \
		echo -e "$(RED)Reinstalling will delete all existing diary entries and configuration.$(RESET)"; \
		echo -e "$(YELLOW)Do you want to continue? (y/N) $(RESET)"; \
		read -p "" RESP; \
		if [ "$$RESP" != "y" ] && [ "$$RESP" != "Y" ]; then \
			echo -e "$(YELLOW)Installation cancelled.$(RESET)"; \
			exit 1; \
		fi; \
		echo -e "$(CYAN)Proceeding with reinstallation...$(RESET)"; \
	fi

	@echo -e "$(CYAN)Creating user data directory...$(RESET)"
	@mkdir -p $(USER_DATA_DIR)
	@mkdir -p $(USER_ENTRY_DIR)
	@mkdir -p $(STYLES_DIR)
	@cp $(STYLES) $(STYLES_DIR)
	@mkdir -p $(ASSETS_DIR)
	@cp -r $(ASSETS)/* $(ASSETS_DIR)

	@echo -e "$(CYAN)Creating configuration...$(RESET)"
	@echo "user_data_dir=$(USER_DATA_DIR)" | sudo tee $(CONFIG_FILE) > /dev/null
	@echo "entry_dir=$(USER_ENTRY_DIR)" | sudo tee -a $(CONFIG_FILE) > /dev/null
	@echo "styles_dir=$(STYLES_DIR)" | sudo tee -a $(CONFIG_FILE) > /dev/null
	@echo "style=$(DEFAULT_STYLE)" | sudo tee -a $(CONFIG_FILE) > /dev/null
	@echo "assets_dir=$(ASSETS_DIR)" | sudo tee -a $(CONFIG_FILE) > /dev/null
	@echo "icon=$(ASSETS_DIR)/$(ICON)" | sudo tee -a $(CONFIG_FILE) > /dev/null
	@echo "version=$(VERSION)" | sudo tee -a $(CONFIG_FILE) > /dev/null

	@echo -e "$(CYAN)Generating RSA key pair for hybrid encryption...$(RESET)"
	@echo -e "$(YELLOW)The RSA key will be used to encrypt AES keys, which in turn encrypt your diary entries.$(RESET)"
	@echo -e "$(GREEN)Enter a password to protect the private key:$(RESET)"
	@stty -echo
	@read -r PASSWORD; echo; stty echo; \
	openssl genpkey -algorithm RSA -out $(PRIVATE_KEY) -aes256 -pass pass:$$PASSWORD -pkeyopt rsa_keygen_bits:2048 && \
	openssl rsa -in $(PRIVATE_KEY) -pubout -out $(PUBLIC_KEY) -passin pass:$$PASSWORD

	@echo "private_key=$(PRIVATE_KEY)" | sudo tee -a $(CONFIG_FILE) > /dev/null
	@echo "public_key=$(PUBLIC_KEY)" | sudo tee -a $(CONFIG_FILE) > /dev/null

	@echo -e "$(CYAN)Installing binary...$(RESET)"
	@sudo install -m 755 $(STABLE_OUTPUT) /usr/local/bin/$(TARGET)

	@echo -e "$(GREEN)Installation complete!$(RESET)"
	@echo -e "$(CYAN)Keys stored in: $(YELLOW)$(USER_DATA_DIR)$(RESET)"
	@echo -e "$(CYAN)Note: Your diary entries are encrypted using AES-256-CBC, with the AES key being encrypted by RSA-2048.$(RESET)"
	@echo -e "$(CYAN)Installed version: $(YELLOW)$(VERSION)$(RESET)"

	@echo -e "$(GREEN)Do you want to create a desktop shortcut? (y/N) $(RESET)"
	@read -p "" RESP; \
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
	  echo -e "$(GREEN)Desktop shortcut created!$(RESET)"; \
	else \
	  echo -e "$(YELLOW)Skipping desktop shortcut.$(RESET)"; \
	fi

# Without key and config generation, upgrade the binary & styles
upgrade: $(STABLE_OUTPUT)
	@echo -e "$(CYAN)Upgrading...$(RESET)"
	@rm -f $(STYLES_DIR)/*.qss
	@cp $(STYLES) $(STYLES_DIR)
	@sudo rm -f /usr/local/bin/$(STABLE_OUTPUT)
	@sudo install -m 755 $(STABLE_OUTPUT) /usr/local/bin/$(TARGET)
	@echo "version=$(VERSION)" | sudo tee -a $(CONFIG_FILE) > /dev/null
	@echo -e "$(GREEN)Upgrade complete. New version: $(YELLOW)$(VERSION)$(RESET)"

# Uninstallation process
uninstall:
	@echo -e "$(CYAN)Uninstalling...$(RESET)"
	@sudo rm -f /usr/local/bin/$(TARGET)
	@sudo rm -f $(CONFIG_FILE)
	@rm -f $(DESKTOP_FILE)
	@rm -rf $(USER_DATA_DIR)
	@echo -e "$(GREEN)Uninstallation complete.$(RESET)"

# Clean build files
clean:
	@echo -e "$(CYAN)Cleaning build files...$(RESET)"
	rm -rf $(BUILD_DIR)/* $(BIN_DIR)/*
	@echo -e "$(GREEN)Clean complete!$(RESET)"

.PHONY: all clean
