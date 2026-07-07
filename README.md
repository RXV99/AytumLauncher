# Vita Java ME Emulator

A native PS Vita homebrew application that runs Java ME (MIDP 2.0) MIDlet
files (.jar/.jad) directly on Vita hardware.

## Features

- **Native Vita execution** - No PSP/Adrenaline layer required
- **libvita2d graphics** - Java ME LCDUI rendered on Vita's 960×544 display
- **Input mapping** - Vita buttons mapped to Java ME keypad codes + touch support
- **Launcher UI** - Browse and select Java apps from `ux0:data/java/`
- **JAD support** - Parses .jad descriptors for app metadata
- **File I/O** - Java FileConnection mapped to Vita's ux0: filesystem
- **Networking** - Basic HTTP support via SceNet
- **VPK packaging** - Ready for installation via VitaShell

## Requirements

### Build dependencies
- [vitasdk](https://vitasdk.org/) (devkit for PS Vita homebrew)
- CMake 3.10+
- vita2d library (included in vitasdk)

### Runtime requirements
- PS Vita (firmware 3.60+ with Henkaku/Enso)
- `ux0:data/java/` directory for storing MIDlet files

## Building

### Quick build (Windows)
```batch
set VITASDK=C:\path\to\vitasdk
build.bat
```

### Manual build
```bash
mkdir build && cd build
cmake .. -DVITASDK=/path/to/vitasdk
make -j$(nproc)
make vita-java-me.vpk
```

The output VPK will be at `build/vita-java-me.vpk`.

## Installation

1. Copy `vita-java-me.vpk` to your Vita
2. Install using VitaShell
3. Place `.jar` and `.jad` files in `ux0:data/java/`
4. Launch the app from LiveArea

## Usage

### Controls

| Vita Button       | Java ME Key      | Function              |
|-------------------|------------------|-----------------------|
| D-Pad Up/Down     | KEY_UP/DOWN      | Navigate menu         |
| Cross             | KEY_FIRE         | Select/Launch app     |
| Circle            | KEY_SOFT1        | Refresh list          |
| Triangle          | KEY_SOFT2        | Back to launcher      |
| Square            | KEY_SOFT3        | Quit to LiveArea      |
| Start             | KEY_NUM5         | Game action           |
| Select            | KEY_NUM0         | Game action           |
| L Trigger         | KEY_NUM1         | Game action           |
| R Trigger         | KEY_NUM3         | Game action           |
| Front Touch       | Pointer events   | Touchscreen input     |

### Supported MIDP APIs

The emulator implements native stubs for the following Java ME APIs:

- **javax.microedition.midlet** - MIDlet lifecycle
- **javax.microedition.lcdui** - Display, Canvas, Graphics, Font, Image,
  List, Alert, TextBox, Command, Displayable
- **javax.microedition.io** - Connector, HttpConnection, SocketConnection,
  file Connection
- **java.lang** - String, StringBuffer, Integer, Long, Float, Boolean,
  Math, System, Object, Class, Thread, Runtime, Throwable
- **java.util** - Vector, Hashtable, Stack, Date, Random, Enumeration
- **java.io** - InputStream, OutputStream, DataInputStream, DataOutputStream,
  ByteArrayOutputStream

### File locations

- `ux0:data/java/` - Store .jar and .jad files here
- Each .jar is auto-detected on launch
- If a .jad file exists alongside the .jar, it is parsed for metadata

## Project Structure

```
vita-java-me/
├── CMakeLists.txt       # Build system
├── build.bat            # Windows build helper
├── README.md            # This file
├── src/
│   ├── main.c           # Entry point, launcher/MIDlet loop
│   ├── launcher.c/h     # Launcher UI
│   ├── jad_parser.c/h   # JAD descriptor parser
│   ├── jvm_core.c/h     # Java bytecode interpreter
│   ├── midp_api.c/h     # MIDP 2.0 API native stubs
│   ├── graphics.c/h     # LCDUI -> libvita2d graphics
│   ├── input.c/h        # Vita button -> Java key mapping
│   ├── fileio.c/h       # File I/O abstraction
│   └── network.c/h      # HTTP/networking abstraction
├── sce_sys/
│   ├── param.sfo        # App metadata
│   └── template.xml     # LiveArea template
└── external/            # For phoneME submodule (optional)
```

## Architecture

### JVM Core (`jvm_core.c`)
A lightweight stack-based Java bytecode interpreter that:
- Parses .class files (magic, constant pool, methods, fields)
- Executes ~60 bytecodes including: arithmetic, loads/stores,
  branches, method invocation, field access, object allocation
- Supports native method registration via `jvm_register_native()`
- 8 MB heap, 64 KB thread stack

### MIDP API Layer (`midp_api.c`)
~150 native method stubs covering the essential MIDP 2.0 API surface.
Each stub maps a Java method call to Vita hardware via the abstraction layer.

### Graphics Layer (`graphics.c`)
Converts LCDUI drawing calls to vita2d primitives:
- `drawLine` -> `vita2d_draw_line`
- `fillRect` -> `vita2d_draw_fill_rect`
- `drawString` -> `vita2d_font_draw_text`
- Full anchor point support (LEFT, RIGHT, HCENTER, TOP, BOTTOM, VCENTER)

### Input Layer (`input.c`)
- SCE_CTRL buttons mapped to Java ME keycodes (-1 to -8 for directional/softkeys,
  48-57 for numeric, 42 for star, 35 for pound)
- Touch screen mapped to pointer events
- Event queue with press/release/repeat detection

## Limitations

- **Bytecode interpreter is simplified**: ~60 opcodes implemented out of 202.
  Complex bytecodes (invokedynamic, multianewarray, tableswitch edge cases)
  may not work with all MIDlets.
- **No garbage collection**: Heap is allocated linearly without GC. Long-running
  MIDlets may exhaust the 8 MB heap.
- **No threading**: The JVM runs single-threaded. `Thread.start()` is a stub.
- **Compressed JAR entries**: Only stored (uncompressed) entries in .jar files
  are supported. DEFLATE compression requires zlib integration.
- **Networking is stubbed**: HTTP connections return 200 OK with empty bodies.
  Full socket I/O is not implemented.
- **No sound**: Audio MIDP APIs (Manager, Player) are not implemented.
- **Limited font support**: Uses vita2d default font only.
- **Class loading**: Only loads classes explicitly found in the JAR; no dynamic
  class loading or delegation to parent classloaders.

## Roadmap

- [ ] Add zlib support for compressed JAR entries
- [ ] Implement GC (mark-sweep)
- [ ] Full networking stack (HTTP GET/POST with real socket I/O)
- [ ] Sound API integration (vita2d audio or SDL_mixer)
- [ ] Save state / RMS (Record Management System)
- [ ] Touch screen calibration options
- [ ] PhoneME Feature integration for full CDC compatibility

## License

This project is MIT-licensed. See LICENSE for details.

## Credits

- Built with [vitasdk](https://vitasdk.org/)
- Graphics via [libvita2d](https://github.com/xerpi/libvita2d)
- Inspired by phoneME Feature project
