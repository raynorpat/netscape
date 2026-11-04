"""Regenerate checked-in Windows bitmaps from original GIF artwork (Pillow required).

This is a development tool; normal Windows builds use the checked-in resources.
"""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
ICONS = ROOT / "cmd/xfe/icons"
OUTPUT = ROOT / "cmd/winfe/res/classic"
SOURCES = {
    "quote": "CQuote", "address": "CAddress",
    "folder": "GFolder", "folder-open": "GFolder",
    "message-read": "HMsg", "message-unread": "HMsg", "message-flag": "HMarked",
    "back": "TBack", "forward": "TNext", "home": "THome", "edit": "ed_edit",
    "reload": "TReload", "images": "TLoadImages", "open": "TOpenUrl",
    "print": "TPrint", "find": "TFind", "stop": "TStop",
    "logo": "anim/meteors/AnimHuge00", "getmail": "MGetM", "compose": "MMsgNew",
    "reply": "MMsgRep", "replyall": "MMsgRepA", "forwardmsg": "MMsgFwd",
    "next": "MNextU", "previous": "MPrevU", "delete": "MDel",
    "mail": "DNewMail", "news": "DNews", "send": "CSendM", "attach": "CAttachM",
}
FACE = (192, 192, 192)


def load(source):
    image = Image.open(source).convert("RGBA")
    background = Image.new("RGBA", image.size, FACE + (255,))
    background.alpha_composite(image)
    return background.convert("RGB")


def main():
    OUTPUT.mkdir(exist_ok=True)
    for name, source in SOURCES.items():
        image = load(ICONS / (source + ".gif"))
        if name == "message-unread":
            # Original green unread marker over the original envelope artwork.
            marker = Image.open(ICONS / "HURead.gif").convert("RGBA")
            overlay = image.convert("RGBA")
            overlay.alpha_composite(marker, (max(0, image.width-marker.width), max(0, image.height-marker.height)))
            image = overlay.convert("RGB")
        image.save(OUTPUT / (name + ".bmp"))
        inactive = ICONS / (source + ".i.gif")
        if inactive.exists():
            disabled = load(inactive)
        else:
            disabled = image.copy()
            for y in range(image.height):
                for x in range(image.width):
                    if image.getpixel((x, y)) != FACE:
                        disabled.putpixel((x, y), (128, 128, 128))
        disabled.save(OUTPUT / (name + "-disabled.bmp"))


if __name__ == "__main__":
    main()
