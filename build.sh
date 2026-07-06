#!/bin/bash
set -e

if [ -z "$VITASDK" ]; then
    if [ -d "/usr/local/vitasdk" ]; then
        export VITASDK=/usr/local/vitasdk
    elif [ -d "$HOME/vitasdk" ]; then
        export VITASDK=$HOME/vitasdk
    else
        echo "Error: VITASDK not set. Install vitasdk or set VITASDK env var."
        exit 1
    fi
    echo "Using VITASDK=$VITASDK"
fi

export PATH=$VITASDK/bin:$PATH

echo "=== Configuring with CMake ==="
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DVITASDK="$VITASDK"

echo "=== Building ==="
cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo "=== Creating VPK ==="
make -C build vita-java-me.vpk 2>/dev/null || cmake --build build --target vita-java-me.vpk

echo "=== Success ==="
echo "VPK at: build/vita-java-me.vpk"
echo ""
echo "Install on Vita via VitaShell."
echo "Place .jar/.jad files in ux0:data/java/"
