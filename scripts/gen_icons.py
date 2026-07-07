import struct, zlib, os

def make_png(w, h, r, g, b):
    def chunk(t, d):
        c = t + d
        return struct.pack('>I', len(d)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)
    raw = b''
    for y in range(h):
        raw += b'\x00' + bytes([r, g, b]) * w
    return (b'\x89PNG\r\n\x1a\n' +
            chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(raw)) +
            chunk(b'IEND', b''))

os.makedirs('sce_sys/livearea/contents', exist_ok=True)
with open('sce_sys/icon0.png', 'wb') as f:
    f.write(make_png(128, 128, 64, 80, 160))
with open('sce_sys/livearea/contents/bg.png', 'wb') as f:
    f.write(make_png(840, 500, 50, 50, 80))
with open('sce_sys/livearea/contents/startup.png', 'wb') as f:
    f.write(make_png(280, 158, 64, 80, 160))
