import struct, zipfile, os, sys, subprocess, shutil

def make_sfo(path):
    entries = [
        ("TITLE_ID", "JAVAME00001"),
        ("TITLE", "Java ME Emulator"),
        ("APP_VER", "01.00"),
        ("VERSION", "01.00"),
        ("CATEGORY", "gde"),
        ("PARENTAL_LEVEL", "1"),
        ("BOOTABLE", "1"),
        ("ATTRIBUTE", "0"),
    ]
    key_table = b""
    data_table = b""
    index = b""
    for key, val in entries:
        kb = key.encode("ascii") + b"\x00"
        vb = val.encode("utf-8") + b"\x00"
        aligned = (len(vb) + 3) & ~3
        key_off = len(key_table)
        data_off = len(data_table)
        key_table += kb
        data_table += vb
        while len(data_table) % 4:
            data_table += b"\x00"
        index += struct.pack("<HHIII", key_off, 0x0204, len(vb), aligned, data_off)
    num = len(entries)
    key_ofs = 20 + num * 16
    data_ofs = key_ofs + len(key_table)
    hdr = struct.pack("<IIIII", 0x00505346, 0x0101, key_ofs, data_ofs, num)
    with open(path, "wb") as f:
        f.write(hdr + index + key_table + data_table)

os.chdir("build")
print("=== Converting ELF ===")
subprocess.run(["vita-elf-create", "vita-java-me.elf", "vita-java-me.velf"], check=True)
print("=== Creating EBOOT ===")
subprocess.run(["vita-make-fself", "vita-java-me.velf", "eboot.bin"], check=True)

print("=== Copying sce_sys ===")
if os.path.exists("sce_sys"):
    shutil.rmtree("sce_sys")
shutil.copytree("../sce_sys", "sce_sys")

print("=== Creating binary SFO ===")
make_sfo("sce_sys/param.sfo")

print("=== Creating VPK ===")
os.chdir("..")
with zipfile.ZipFile("vita-java-me.vpk", "w", zipfile.ZIP_DEFLATED) as zf:
    zf.write("build/eboot.bin", "eboot.bin")
    for root, dirs, files in os.walk("build/sce_sys"):
        for fn in files:
            path = os.path.join(root, fn)
            arcname = os.path.relpath(path, "build/")
            zf.write(path, arcname)

print("=== VPK created ===")
size = os.path.getsize("vita-java-me.vpk")
print(f"Size: {size} bytes ({size/1024:.1f} KB)")
