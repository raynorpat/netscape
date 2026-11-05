# Original Windows Gold artwork

The native frontend now uses the English Windows Navigator Gold **3.02**
executable's bitmap strips and window icons, replacing the previous X11 GIF
conversions. The original EDT editor strips remain in `../gold/`.

Source installer: [g32e302.exe](https://netscape.retropc.se/3/g32e302.exe).
Installer SHA256: `d053540b580809af7d2afb1a770cd26960c3996328fea89f7e2d1641acb1f117`.
The vendor executable/installer is not included. Unchanged extracted resources
used for regeneration are retained in `source-gold/`; `sha256.json` records
their hashes. The executable's original resource IDs are retained there.

| Original BITMAP | Output |
| --- | --- |
| 2007, 20x18 cells | Navigator toolbar, including Gold's Edit pen |
| 750, 20x20 cells | Mail toolbar |
| 751, 20x20 cells | News toolbar |
| 755, 20x20 cells | Message composition toolbar |
| 1100 / 1000, first 48x48 / 30x30 frame | Large / small Netscape logo |
| 460 | Navigator/Mail status switch icons |
| 1239, 17x20 | Page/pen icon beside Location |
| 1204, three 24x12 cells | New/normal/unknown status-mail indicators |
| 703 | Message-list glyph (read state is also shown by row font weight) |
| 701 / 702 | Closed / open folder |
| 717 / 718 | News server / group |
| 719 / 720 / 721 / 725 | Subscription boxes / message marker / priority |

The generator crops cells without redrawing or recoloring artwork. Magenta
transparency pixels are normalized to the fixed #C0C0C0 bitmap key. The key is
independent of native Windows UI colors. Controls draw disabled glyphs using
native embossed masks; generated silhouette BMPs retain fallback resource IDs.
Original cell proportions are preserved when rendered.

Window icons in `../gold/` reconstruct the original GROUP_ICON directories
with unchanged RT_ICON payloads: Navigator 2, Mail 221, News 224, message
composition 225 and Bookmarks 231. The frontend loads both original small and
large images. Gold Editor continues to use original group 101.

Regenerate with `python cmake/tools/generate_windows_toolbar.py` (Pillow is a
development dependency). Add `--check` to verify hashes and byte-for-byte
reproducibility without modifying files. Normal builds use checked-in assets
and do not require Pillow. `windows_original_artwork` checks actual compiled
bitmap and icon resource payloads with Windows APIs, without executing the
application. Native UI tests also inspect rendered artwork and disabled icons.

The older standard 3.02 marker extracts in `source-302/` are retained for
provenance; the generator now uses the Gold resources above. Original notices
apply. No screenshot pixels or recreated drawings are used as artwork.
