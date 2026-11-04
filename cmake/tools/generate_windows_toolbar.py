"""Regenerate checked-in Windows bitmaps from original artwork (Pillow required).

This is a development tool; normal Windows builds use the checked-in resources.
"""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
ICONS = ROOT / "cmd/xfe/icons"
OUTPUT = ROOT / "cmd/winfe/res/classic"
WINDOWS_SOURCES = {
    "subscribed": "719.bmp", "unsubscribed": "720.bmp",
    "message-flag": "721.bmp", "priority-header": "725.bmp",
}
SOURCES = {
    "post": "MMsgPst", "post-reply": "MMsgPstR", "post-both": "MMsgPstF",
    "mark-thread": "MMarkT", "mark-group": "MMarkA",
    "news-host": "HHost", "news-group": "HGroup",
    "quote": "CQuote", "address": "CAddress",
    "folder": "GFolder", "folder-open": "GFolder",
    "message-read": "HMsg", "message-unread": "HMsg",
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
    # The Windows 3.02 tick and header markers differ from the X11 GIFs.
    # Retain the extracted source bitmaps, replacing only their magenta mask.
    for name, source in WINDOWS_SOURCES.items():
        image = Image.open(OUTPUT / "source-302" / source).convert("RGB")
        for y in range(image.height):
            for x in range(image.width):
                if image.getpixel((x, y)) == (255, 0, 255):
                    image.putpixel((x, y), FACE)
        image.save(OUTPUT / (name + ".bmp"))
        disabled = image.copy()
        for y in range(image.height):
            for x in range(image.width):
                if image.getpixel((x, y)) != FACE:
                    disabled.putpixel((x, y), (128, 128, 128))
        disabled.save(OUTPUT / (name + "-disabled.bmp"))


if __name__ == "__main__":
    main()
