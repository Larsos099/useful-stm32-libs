import cairosvg
from PIL import Image
import io
import math

W, H = 33, 33
filename = "info"
# Rasterize SVG
png = cairosvg.svg2png(
    url=f"{filename}.svg",
    output_width=W,
    output_height=H
)

img = Image.open(io.BytesIO(png)).convert("L")

bitmap = []

# threshold to 1-bit
for y in range(H):
    row = []
    for x in range(W):
        row.append(1 if img.getpixel((x, y)) < 128 else 0)
    bitmap.append(row)

# pack bits (MSB first)
packed = []

for y in range(H):
    byte = 0
    bit_count = 0

    for x in range(W):
        byte <<= 1
        if bitmap[y][x]:
            byte |= 1

        bit_count += 1

        if bit_count == 8:
            packed.append(byte)
            byte = 0
            bit_count = 0

    # pad remaining bits in row (11 bits → 3 left)
    if bit_count != 0:
        byte <<= (8 - bit_count)
        packed.append(byte)

# Emit C array
print(f"static const uint8_t icon_{filename}[{len(packed)}] = {{")
for i in range(0, len(packed), 12):
    line = ", ".join(f"0x{b:02X}" for b in packed[i:i+12])
    print(f"    {line},")
print("};")