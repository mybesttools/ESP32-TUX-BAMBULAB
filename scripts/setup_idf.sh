#!/bin/bash
#
# ESP-IDF Setup Script for ESP32-TUX-BAMBULAB
# This script installs ESP-IDF v5.1 (compatible with this project)
#

set -e

echo "=================================================="
echo "ESP-IDF Installation for ESP32-TUX-BAMBULAB"
echo "=================================================="
echo ""

# Check if ESP-IDF is already installed
if [ -d "$HOME/esp-idf" ]; then
    echo "ESP-IDF already exists at $HOME/esp-idf"
    read -p "Do you want to reinstall? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Setup cancelled."
        exit 0
    fi
    rm -rf "$HOME/esp-idf"
fi

# Check prerequisites
echo "Checking prerequisites..."

# Check for git
if ! command -v git &> /dev/null; then
    echo "ERROR: git is not installed. Please install git first."
    echo "  brew install git"
    exit 1
fi

# Check for Python 3
if ! command -v python3 &> /dev/null; then
    echo "ERROR: python3 is not installed. Please install Python 3.8 or later."
    echo "  brew install python@3.10"
    exit 1
fi

PYTHON_VERSION=$(python3 --version | awk '{print $2}')
echo "✓ Python $PYTHON_VERSION found"

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo "Installing CMake..."
    brew install cmake
fi
echo "✓ CMake found"

# Check for Ninja
if ! command -v ninja &> /dev/null; then
    echo "Installing Ninja build system..."
    brew install ninja
fi
echo "✓ Ninja found"

# Install ESP-IDF
echo ""
echo "Cloning ESP-IDF v5.1..."
cd "$HOME"
git clone -b v5.1 --recursive https://github.com/espressif/esp-idf.git

# Install ESP-IDF tools
echo ""
echo "Installing ESP-IDF tools..."
cd esp-idf
./install.sh esp32,esp32s3

# Create source script
echo ""
echo "Creating activation script..."
cat > "$HOME/esp-idf/activate.sh" << 'EOF'
#!/bin/bash
# Source this file to activate ESP-IDF
. $HOME/esp-idf/export.sh
EOF
chmod +x "$HOME/esp-idf/activate.sh"

# Add to shell profile (optional)
SHELL_RC=""
if [ -f "$HOME/.zshrc" ]; then
    SHELL_RC="$HOME/.zshrc"
elif [ -f "$HOME/.bashrc" ]; then
    SHELL_RC="$HOME/.bashrc"
fi

if [ -n "$SHELL_RC" ]; then
    echo ""
    read -p "Add ESP-IDF activation to $SHELL_RC? (y/N) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "" >> "$SHELL_RC"
        echo "# ESP-IDF Setup (added by setup_idf.sh)" >> "$SHELL_RC"
        echo "alias get_idf='. \$HOME/esp-idf/export.sh'" >> "$SHELL_RC"
        echo "Added 'get_idf' alias to $SHELL_RC"
        echo "Run 'get_idf' to activate ESP-IDF in any terminal"
    fi
fi

echo ""
echo "=================================================="
echo "✓ ESP-IDF Installation Complete!"
echo "=================================================="
echo ""
echo "To use ESP-IDF:"
echo "  1. Activate it in current shell:"
echo "     . ~/esp-idf/export.sh"
echo ""
echo "  2. Or use the alias (if added to shell profile):"
echo "     get_idf"
echo ""
echo "  3. Test the installation:"
echo "     idf.py --version"
echo ""
echo "To build ESP32-TUX-BAMBULAB:"
echo "  cd $(pwd)"
echo "  . ~/esp-idf/export.sh"
echo "  idf.py build"
echo ""
