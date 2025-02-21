#!/bin/bash

set -e # exit on errors

git pull
git submodule foreach git pull

# Parse command line arguments for HOST and PORT
while getopts "h:p:" opt; do
  case $opt in
    h) HOST=$OPTARG ;;
    p) PORT=$OPTARG ;;
    \?) echo "Usage: $0 [-h HOST] [-p PORT]" && exit 1 ;;
  esac
done

# Check if HOST and PORT are provided, otherwise set default values
HOST=${HOST:-localhost}    # Default to 'localhost' if not provided
PORT=${PORT:-8000}         # Default to 8000 if not provided

# Set environment variables for React and Vite
export REACT_APP_HOST=$HOST
export REACT_APP_PORT=$PORT
export VITE_HOST=$HOST
export VITE_PORT=$PORT

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

docker compose down
docker compose up --build
