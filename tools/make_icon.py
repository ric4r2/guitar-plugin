"""Makes the program icons from the logo: python tools/make_icon.py

  Assets/icon.png        the big icon (taskbar, desktop, Start): the chrome Joe Garcia logo on black amp tolex
  Assets/icon-small.png  the small icon (title bar, small lists): a chrome "JG" monogram, readable at 16-32 pixels

Needs Pillow. The monogram uses the Kunstler Script font that comes with Windows/Office; the results are committed, so
this only has to run again to change the look.
"""
import os
import random
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = os.path.join(ROOT, "Assets")
S = 1024  # drawn at this size, the build scales it down


def rounded_mask(size, radius, inset=0):
    m = Image.new("L", (size, size), 0)
    ImageDraw.Draw(m).rounded_rectangle((inset, inset, size - 1 - inset, size - 1 - inset), radius, fill=255)
    return m


def background():
    """Black tolex: dark with a fine grain and a soft light from the top, inside a chrome rim."""
    rnd = random.Random(7)
    base = Image.new("RGB", (S, S), (18, 18, 20))
    grain = Image.effect_noise((S, S), 22).convert("RGB")
    base = Image.blend(base, grain, 0.08)
    # soft top light
    glow = Image.new("L", (S, S), 0)
    ImageDraw.Draw(glow).ellipse((-S * 0.3, -S * 0.75, S * 1.3, S * 0.55), fill=70)
    glow = glow.filter(ImageFilter.GaussianBlur(S * 0.12))
    base = Image.composite(Image.new("RGB", (S, S), (70, 66, 62)), base, glow)

    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    # chrome rim: a vertical light-dark-light gradient, the tolex panel inside it
    rim = chrome_gradient(S)
    img.paste(rim, (0, 0), rounded_mask(S, int(S * 0.2)))
    inset = int(S * 0.028)
    img.paste(base, (0, 0), rounded_mask(S, int(S * 0.2) - inset, inset))
    # a thin dark line between rim and panel
    line = Image.new("L", (S, S), 0)
    ImageDraw.Draw(line).rounded_rectangle((inset, inset, S - 1 - inset, S - 1 - inset), int(S * 0.2) - inset,
                                           outline=255, width=max(2, S // 256))
    img.paste((0, 0, 0, 255), (0, 0), line)
    return img


def chrome_gradient(size):
    """Polished chrome: bright, dark band across the middle, bright again, with a warm tint like the logo."""
    stops = [(0.0, (250, 248, 244)), (0.35, (200, 196, 190)), (0.48, (70, 64, 60)), (0.55, (120, 112, 104)),
             (0.75, (235, 230, 222)), (1.0, (150, 144, 136))]
    col = Image.new("RGB", (1, size))
    for y in range(size):
        t = y / (size - 1)
        for (t0, c0), (t1, c1) in zip(stops, stops[1:]):
            if t0 <= t <= t1:
                k = (t - t0) / (t1 - t0)
                col.putpixel((0, y), tuple(int(a + (b - a) * k) for a, b in zip(c0, c1)))
                break
    return col.resize((size, size))


def shadow(alpha, offset, blur, strength=200):
    sh = ImageChops.offset(alpha, offset[0], offset[1]).filter(ImageFilter.GaussianBlur(blur))
    return sh.point(lambda v: v * strength // 255)


def big_icon():
    img = background()
    logo = Image.open(os.path.join(ASSETS, "joe-garcia-logo.png")).convert("RGBA")
    logo = logo.crop(logo.getbbox())
    box = int(S * 0.80)
    scale = min(box / logo.width, box / logo.height)
    logo = logo.resize((int(logo.width * scale), int(logo.height * scale)), Image.LANCZOS)
    x, y = (S - logo.width) // 2, (S - logo.height) // 2 + S // 64
    layer = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    layer.paste(logo, (x, y), logo)
    sh = shadow(layer.getchannel("A"), (0, S // 80), S // 90)
    img.paste((0, 0, 0, 255), (0, 0), sh)
    return Image.alpha_composite(img, layer)


def small_icon():
    img = background()
    font = ImageFont.truetype(os.path.join(os.environ.get("WINDIR", r"C:\Windows"), "Fonts", "KUNSTLER.TTF"), int(S * 0.78))
    text = Image.new("L", (S * 2, S * 2), 0)
    d = ImageDraw.Draw(text)
    d.text((S * 0.25, S * 0.3), "JG", font=font, fill=255)
    # thicken the hairlines so they survive at 16 pixels
    text = text.filter(ImageFilter.MaxFilter(51))
    text = text.crop(text.getbbox())
    box = int(S * 0.86)
    scale = min(box / text.width, box / text.height)
    text = text.resize((int(text.width * scale), int(text.height * scale)), Image.LANCZOS)
    mask = Image.new("L", (S, S), 0)
    mask.paste(text, ((S - text.width) // 2, (S - text.height) // 2))

    # dark edge around the letters, then the chrome face
    edge = mask.filter(ImageFilter.MaxFilter(25))
    img.paste((0, 0, 0, 255), (0, 0), shadow(edge, (0, S // 70), S // 80, 230))
    img.paste((25, 22, 20, 255), (0, 0), edge)
    img.paste(chrome_gradient(S), (0, 0), mask)
    # a bright bevel on the upper edge of the strokes
    bevel = ImageChops.subtract(mask, ImageChops.offset(mask, 0, S // 140)).filter(ImageFilter.GaussianBlur(2))
    img.paste((255, 255, 255, 255), (0, 0), bevel.point(lambda v: v * 3 // 4))
    return img


if __name__ == "__main__":
    big_icon().save(os.path.join(ASSETS, "icon.png"))
    small_icon().save(os.path.join(ASSETS, "icon-small.png"))
    print("wrote Assets/icon.png and Assets/icon-small.png")
