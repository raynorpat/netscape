# Original Windows 3.02 comparison

Observed on 2026-11-04 using the preserved English Win32 release and this
repository's native frontend on the same modern Windows desktop. This is a
record of specific checks, not a claim of complete behavioral or pixel parity.

## Provenance

The [Netscape Archive](https://netscape.retropc.se/) supplied both installers:

| File | Download | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| Standard | [n32e302.exe](https://netscape.retropc.se/3/n32e302.exe) | 3686912 | `51f3444e400a40b4ad8459a0744db76c927d00414387f25cbc4eb87425b9428f` |
| Gold | [g32e302.exe](https://netscape.retropc.se/3/g32e302.exe) | 3728384 | `d053540b580809af7d2afb1a770cd26960c3996328fea89f7e2d1641acb1f117` |

These hashes identify the downloaded copies; no publisher signature or separate
historical checksum was authenticated. The standard extracted `netscape.exe`
is 2992128 bytes, SHA-256
`26e86c5b7acebc3af39a66441955720301af4c58c33b9e853f44e44ca3ae0794`.
Its file/product version and running About page identify version 3.02. The Gold
version resource identifies Gold 3.02; Gold was extracted but not runtime tested.

The installers use classic InstallShield archives. The inspected Python
[setup30 extractor](https://github.com/ostrich/setup30) unpacked DATA.Z,
SYSTEM.Z and JAVA.Z without running installation. A local output-path check
was added before extraction. 7-Zip's explicit `-tPE` mode extracted executable
resources for menu/version inspection. No binary decompilation was needed.

All downloads, extracted files and comparison captures remain in ignored
`out/research/navigator302/`; none are distributed in the source or Windows ZIP.
The runtime directory contains the original application, help/license file and
its bundled DLL dependencies. Its mailbox fixtures and generated data are local
to that directory. The port uses a separate `NETSCAPE_PROFILE_DIR`.
This was a native desktop run, not an isolated virtual machine; isolation of the
original program's registry writes was not established. No installer, file
association changes, external mail delivery or news posting was performed.

## Checks and differences

| Area | Original observation | Port comparison / remaining work |
| --- | --- | --- |
| Browser | Running standard 3.02 About page; nine icon/caption toolbar buttons, Location, six directory shortcuts and nine top-level menus. | Corresponding chrome exists. Gold's additional Edit button is retained from the supplied Gold reference, visibly disabled. Native printing is still absent. |
| Mail | Opened Inbox containing the same two synthetic MIME messages used by our fixture; selected and read the base64 message. Ten toolbar buttons, eight menus, folder/message lists above a full-width reader. | Layout and artwork agree in structure. Fresh original folder column and upper band were larger than our supplied screenshot defaults. Original list left the fixture's US-ASCII encoded-word headers literal; the port decodes them. |
| Composer | Opened a new unsent composition. Five toolbar buttons, five menus, compact Mail To/Cc/Subject/Attachment rows and fixed-width editor. | Structure agrees. Original Address is enabled; ours is disabled pending address-book UI. Fresh original window was approximately 534 x 953 pixels; ours uses the wider proportions of the supplied reference. |
| News | Opened the original empty News window: twelve buttons, eight menus, server/group tree, three subscribed default groups and unknown unread counts. Default host `news` failed DNS. | Structure and default group names agree. Original used Re:Both where the port says Re:All, exposed a Date column, and had a white empty reader after the error. The port retains the supplied screenshot's gray reader and shorter column set; exact sizing/captions still differ. No original article retrieval was verified. |
| Preferences | Opened and captured Appearance, Fonts, Colors, Images, Apps, Helpers and Language. | All seven native pages now exist; supported settings persist and affect the browser. Unsupported controls are disabled as detailed below. |

Captures for browser About, all seven original Preferences pages, empty/populated
Mail, empty News and Message Composition are in the ignored `captures/`
subdirectory, with accessibility text where available. `standard-menus.json`
records the parsed original MENU resources (browser 2, Mail 221, composer 224,
News 225).
Window measurements are approximate outer dimensions at the observed desktop
scale. Current Windows decorations/system colors differ from Windows 95, so
the supplied images remain the guide for the port's classic gray styling.

## Preferences details confirmed against the running original

- Appearance has Pictures/Text/Pictures and Text, Browser/Mail/News startup,
  blank/home location and link/history controls. A fresh original profile used
  nine expiration days; the supplied screenshot's eight is not the default.
- Fonts has encoding and proportional/fixed font choices. The port currently
  supports Latin-1; other encodings need separate rendering work.
- Colors has custom link, followed-link and text choices, default/custom
  background and document override. Background-image selection is disabled in
  the port pending integration.
- Images has Automatic/Dither/Substitute Colors and while/after loading choices.
  These use the original image library preferences in the port; true-color
  output alone cannot demonstrate palette-dithering equivalence.
- Apps lists Telnet, TN3270, View Source and Temporary Directory. Unsupported
  external-application configuration is disabled in the port.
- Helpers exposes MIME handling. The port displays supported browser/save
  actions; editing and external helper launching remain disabled.
- Language has available/accepted lists, add/remove and a custom language field.
  The port validates and bounds the resulting HTTP header for the legacy engine.

The original Preferences measured approximately 502 x 442 pixels; the port's
capture is 516 x 449 at 96 DPI. The port provides standard tab keyboard
navigation and applies Expire Now only after a successful OK, preserving Cancel
semantics. Destructive history operations were not exercised in the original.

## Coverage limits and next comparisons

Original runtime inspection covered browser chrome/About, local mail reading,
empty News, new composition and the seven General Preferences pages. It did not verify
original POP/SMTP/NNTP sessions, sending, printing, address-book workflows,
plugins, Java, JavaScript, security, Gold editing, all menu actions, or startup
combinations. Our loopback tests establish the port's tested behavior, not
automatic equivalence with every original operation.

Follow up with a controlled original NNTP session and matching-size pane
captures, then compare each newly implemented printing, address-book, helper,
cache and optional-engine workflow against the original. Keep functional gaps
in [windows-roadmap.md](windows-roadmap.md) until implemented and verified.
