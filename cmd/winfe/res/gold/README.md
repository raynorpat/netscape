# Gold editor artwork

The BMP files are 20 by 18 pixel cells cropped, without redrawing, from the English Windows Netscape Navigator Gold 3.02 RT_BITMAP resources 2001 (file/edit), 2004 (character), and 2005 (paragraph). The resource extraction used `out/research/navigator302/gold-resources/.rsrc/BITMAP` as its local input. File names retain the original resource ID and zero-based cell index.

Source installer: https://netscape.retropc.se/3/g32e302.exe . Installer SHA256: `d053540b580809af7d2afb1a770cd26960c3996328fea89f7e2d1641acb1f117`. The vendor executable is not included. The editor menus are transcribed separately from RT_MENU 101; the browser Gold menu is transcribed from RT_MENU 2. Resource SHA256 values are recorded in each menu transcription.

## Window icon

`editor.ico` mechanically reconstructs original RT_GROUP_ICON 101 with the unchanged image payloads from RT_ICON 29 (32 by 32, 4-bit) and RT_ICON 30 (16 by 16, 4-bit). The pen over the Navigator horizon matches the original Gold editor title-bar icon; the browser group 2 has the horizon without the pen. The extraction tool wrapped each RT_ICON in a single-image ICO; those wrappers were removed, and the original group entries were written as the combined ICO directory. No icon pixels were redrawn.

Original RT_GROUP_ICON 101 SHA256: `1fdd6e659a323aa21a89db1d5fb12604361c5959949ce56fa65f08fd93e2efb7`. Reconstructed `editor.ico` SHA256: `7fecd437e8d3e296b0e621a48e1f201440fd4d0799d51f138b492e6b62963361`.

Navigator, Mail, News, message composition and Bookmarks now also use original
Gold groups 2, 221, 224, 225 and 231, respectively. Their unchanged RT_ICON
payloads and original group directories are retained under
`../classic/source-gold/`. `generate_windows_toolbar.py` reconstructs the ICO
containers and verifies source hashes. Both large and small images are loaded
explicitly, replacing the shared Communicator icon and missing small icons.
See [the shared artwork provenance](../classic/README.md) for toolbar sources.

`about.html` adapts the native frontend's shared About page to identify the Gold
edition. It is not an extracted original HTML payload.

## Editor Preferences

`editor-preferences.cpp` embeds the unchanged English Gold dialog templates
RT_DIALOG 2008 (General, 1060 bytes) and 2189 (Publish, 1578 bytes) in a native
property sheet. The original password control is disabled and labeled
"Not stored"; credentials are entered for publishing instead.

- General template SHA256: `54d43c9afebcfcb0c25173367eaaf8772554dc6ea6a7fff459110e598b822e56`.
- Publish template SHA256: `be415936922cdfa40053c28a6f77afa5bf2c1a99e64adba76a6f9241d9cde5e3`.
