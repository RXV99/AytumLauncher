import struct, zipfile, os, sys, subprocess, shutil

os.chdir("build")
print("=== Converting ELF ===")
subprocess.run(["vita-elf-create", "vita-java-me", "vita-java-me.velf"], check=True)
print("=== Creating EBOOT ===")
subprocess.run(["vita-make-fself", "vita-java-me.velf", "eboot.bin"], check=True)

print("=== Copying sce_sys ===")
if os.path.exists("sce_sys"):
    shutil.rmtree("sce_sys")
shutil.copytree("../sce_sys", "sce_sys")

# Vita-pack-vpk expects the param.sfo text file.
# Remove any stale binary SFO from previous runs.
print("=== Creating VPK via vita-pack-vpk ===")
result = subprocess.run(
    ["vita-pack-vpk", "-s", "sce_sys/param.sfo", "-b", "eboot.bin", "../vita-java-me.vpk"],
    capture_output=True, text=True
)
print(result.stdout)
print(result.stderr)
if result.returncode != 0:
    print("vita-pack-vpk failed, falling back to manual VPK creation")
    os.chdir("..")
    with zipfile.ZipFile("vita-java-me.vpk", "w", zipfile.ZIP_DEFLATED) as zf:
        zf.write("build/eboot.bin", "eboot.bin")
        for root, dirs, files in os.walk("build/sce_sys"):
            for fn in files:
                path = os.path.join(root, fn)
                arcname = os.path.relpath(path, "build/")
                zf.write(path, arcname)

os.chdir("..")
print("=== VPK created ===")
size = os.path.getsize("vita-java-me.vpk")
print(f"Size: {size} bytes ({size/1024:.1f} KB)")
