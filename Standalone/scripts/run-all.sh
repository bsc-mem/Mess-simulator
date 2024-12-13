#!/bin/bash

# Clear the terminal
clear

# Check if the script is being run from the correct directory
current_folder=$(basename "$PWD")

if [ "$current_folder" != "Standalone" ]; then
    # Go one directory up
    cd ..
    current_folder=$(basename "$PWD")
    # Check again if it is "Standalone"
    if [ "$current_folder" != "Standalone" ]; then
        echo "Please run this script from the 'Standalone' folder."
        exit 1
    fi
fi

# Compile the standalone Mess simulator
make

echo "Running all scripts in the scripts directory..."
echo "-----------------------------------------------"

# Iterate over all .sh scripts in the scripts directory
for script in ./scripts/*.sh; do
    # Skip this script itself to avoid recursion
    if [[ "$script" == "./scripts/run-all.sh" ]]; then
        continue
    fi

    # Execute the script
    echo "Executing: $script"
    bash "$script"
    echo "-----------------------------------------------"
done

echo "All scripts executed."