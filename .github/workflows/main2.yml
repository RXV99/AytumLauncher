name: Build VPK

on:
  push:
    branches: [ main, master ]
  workflow_dispatch:

jobs:
  build:
    runs-on: ubuntu-latest
    container:
      image: vitasdk/vitasdk:latest

    steps:
      - uses: actions/checkout@v4

      - name: Configure
        run: cmake -S . -B build -DCMAKE_BUILD_TYPE=Release 2>&1

      - name: Build ELF
        run: cmake --build build -j$(nproc) VERBOSE=1 2>&1

      - name: Create VPK
        run: |
          set -e
          cd build
          vita-elf-create vita-java-me.elf vita-java-me.velf 2>&1
          vita-make-fself vita-java-me.velf eboot.bin 2>&1
          cp -r ../sce_sys .
          vita-pack-vpk -s sce_sys/param.sfo -b eboot.bin ../vita-java-me.vpk 2>&1

      - name: Upload VPK
        uses: actions/upload-artifact@v4
        with:
          name: vita-java-me.vpk
          path: vita-java-me.vpk

      - name: Upload debug ELF
        uses: actions/upload-artifact@v4
        with:
          name: vita-java-me.elf
          path: build/vita-java-me.elf
