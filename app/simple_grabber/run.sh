#!/bin/bash

# Percorso all'SDK RPLidar (adatta questo al tuo caso)
RPLIDAR_SDK_DIR="/Users/enzochianello/Documents/rplidar_sdk"

# Percorso per gli header (.h/.hpp)
RPLIDAR_INCLUDE_DIR="$RPLIDAR_SDK_DIR/sdk/include"

# Percorso al source code
RPLIDAR_SRC_DIR="$RPLIDAR_SDK_DIR/sdk/src"

# Percorso per le librerie (.a/.so/.dylib)
# Scegli l'architettura corretta (linux/x64, mac/x64, windows/x64, ecc.)
RPLIDAR_LIB_DIR="$RPLIDAR_SDK_DIR/output/Darwin/Release/"
# OPPURE se sei su macOS:
# RPLIDAR_LIB_DIR = $(RPLIDAR_SDK_DIR)/lib/mac/x64 

# Compila il tuo file C++
g++ $1 -o my_rplidar_app \
    -I$RPLIDAR_INCLUDE_DIR \
    -I$RPLIDAR_SRC_DIR \
    -L$RPLIDAR_LIB_DIR \
    -lsl_lidar_sdk \
    -lpthread \
    -lstdc++ \
    -lm \
    -lzmq