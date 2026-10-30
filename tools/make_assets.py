import math, os, struct, wave
from PIL import Image, ImageDraw, ImageFont

RED = (226, 35, 26)
DARK = (140, 14, 10)
WHITE = (255, 255, 255)
os.makedirs("assets", exist_ok=True)


def square(cx, cy, half, deg):
    a = math.radians(deg)
    c, s = math.cos(a), math.sin(a)
    pts = ((-half, -half), (half, -half), (half, half), (-half, half))
    return [(cx + x * c - y * s, cy + x * s + y * c) for x, y in pts]


def get_font(size):
    for p in ("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
              "/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf"):
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()


def make_icon():
    if os.path.exists("assets/icon.png"):
        return
    S = 384
    im = Image.new("RGB", (S, S), RED)
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, S - 1, S - 1], outline=DARK, width=24)
    d.polygon(square(S / 2, S / 2, 120, -18), fill=WHITE)
    d.polygon(square(S / 2, S / 2, 56, -18), fill=RED)
    im.resize((48, 48), Image.LANCZOS).save("assets/icon.png")


def make_banner():
    if os.path.exists("assets/banner.png"):
        return
    W, H = 512, 256
    im = Image.new("RGB", (W, H), RED)
    d = ImageDraw.Draw(im)
    for y in range(H):
        d.line([(0, y), (W, y)], fill=(226 - int(60 * y / H), 35 - int(20 * y / H), 26 - int(16 * y / H)))
    d.rectangle([0, 0, W - 1, H - 1], outline=DARK, width=8)
    d.polygon(square(112, 128, 84, -18), fill=WHITE)
    d.polygon(square(112, 128, 40, -18), fill=(200, 28, 22))
    f = get_font(56)
    d.text((226, 70), "ROBLOX", font=f, fill=(90, 8, 6))
    d.text((222, 66), "ROBLOX", font=f, fill=WHITE)
    f2 = get_font(40)
    d.text((228, 150), "3DS", font=f2, fill=(255, 220, 220))
    im.resize((256, 128), Image.LANCZOS).save("assets/banner.png")


def make_audio():
    if os.path.exists("assets/audio.wav"):
        return
    rate = 22050
    frames = []
    for freq, dur in ((660, 0.18), (880, 0.30)):
        n = int(rate * dur)
        for i in range(n):
            env = math.exp(-4.0 * i / n)
            frames.append(int(12000 * env * math.sin(2 * math.pi * freq * i / rate)))
    with wave.open("assets/audio.wav", "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b"".join(struct.pack("<h", s) for s in frames))


make_icon()
make_banner()
make_audio()
print("Assets OK")
