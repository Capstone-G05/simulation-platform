#!/bin/bash

set -e # exit on errors

# Define the list of directories to build
directories=("FrontEndSimulation" "input-board")

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Loop through the directories
for dir in "${directories[@]}"; do
  echo "Building $dir"

  cd "$SCRIPT_DIR/$dir"

  npm install
  npm run build

  # Check for the build output directory
  if [ -d "dist" ] || [ -d "build" ]; then
    echo "$dir build success"
  else
    echo "$dir build failed"
    exit 1
  fi
done
