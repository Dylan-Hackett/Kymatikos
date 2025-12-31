#!/bin/bash

set -e  # Exit on any error

echo "========================================"
echo "Claude Code Uninstall & Reinstall Script"
echo "========================================"
echo ""

# Step 1: Uninstall binary
echo "Step 1: Removing Claude Code binary..."
rm -f ~/.local/bin/claude
rm -rf ~/.claude-code
echo "✓ Binary removed"
echo ""

# Step 2: Remove all config/cache
echo "Step 2: Removing all configuration and cache files..."
rm -rf ~/.claude
rm -f ~/.claude.json
rm -rf ~/.config/claude-code
echo "✓ Config and cache removed"
echo ""

# Step 3: Remove project-specific settings
echo "Step 3: Removing project-specific settings from Kymatikos..."
cd /Users/dylanhackett/Documents/Kymatikos
rm -rf .claude
rm -f .mcp.json
echo "✓ Project settings removed"
echo ""

# Step 4: Reinstall
echo "Step 4: Reinstalling Claude Code..."
curl -fsSL https://claude.ai/install.sh | bash
echo "✓ Claude Code reinstalled"
echo ""

# Step 5: Verify installation
echo "Step 5: Verifying installation..."
echo ""
~/.local/bin/claude doctor || echo "Doctor check completed (may show auth warning - this is normal)"
echo ""

echo "========================================"
echo "Reinstallation complete!"
echo "========================================"
echo ""
echo "Next steps:"
echo "1. cd /Users/dylanhackett/Documents/Kymatikos"
echo "2. claude"
echo "3. Follow the authentication prompts"
echo ""
