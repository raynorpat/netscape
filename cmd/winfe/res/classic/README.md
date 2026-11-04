# Classic toolbar artwork

Most resource bitmaps are mechanical conversions of the original 3.02 GIFs
under `cmd/xfe/icons`. Colors and artwork are retained; transparent pixels use
the classic Windows button face (192,192,192). The Netscape logo uses
`anim/meteors/AnimHuge00.gif`. Browser icons use `T*.gif` and `ed_edit.gif`; mail
icons use `M*.gif`, `DNewMail.gif`, `DNews.gif`, `CSendM.gif` and `CAttachM.gif`.
Original source-tree notices apply. No screenshot pixels were used as artwork.

Disabled variants use the original `.i.gif` where present, otherwise a gray
silhouette. The native toolbar scales the source artwork to its display size.

Mail list folders use `GFolder.gif`; message envelopes use `HMsg.gif` with the
original `HURead.gif` unread marker. Composition adds `CQuote.gif` and
`CAddress.gif`. These are original 3.02 source assets.

News uses `MMsgPst.gif`, `MMsgPstR.gif`, `MMsgPstF.gif`, `MMarkT.gif` and
`MMarkA.gif` for posting/replying and marking threads/groups read. Server
and group cells use `HHost.gif` and `HGroup.gif`; native tree guides and
expansion glyphs are drawn separately.

Subscription boxes and message header markers use the authentic Windows 3.02
resources retained without modification in `source-302/`: bitmap 719 is the
yellow subscription tick, 720 the empty box, 721 the green message marker,
and 725 the red priority header. They were extracted from the standard
Navigator 3.02 executable's `BITMAP` resources (not from a screen capture).
The generator replaces only the magenta transparency key with the button
face; it preserves the original pixels and dimensions. Header image lists
crop the one-pixel transparent margin so the native headers retain their
compact height. Original source-tree notices apply.
