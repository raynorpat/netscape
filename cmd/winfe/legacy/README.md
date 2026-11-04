# Recovered Windows frontend components

`cmdparse.cpp`, `cmdparse.h`, `genedit.cpp`, and `genedit.h` are unmodified copies
of `ns/cmd/winfe/` files in the official
[Mozilla April 8, 1998 source archive](https://releases.mozilla.org/pub/mozilla/source/mozilla-19980408.tar.gz).
Archive SHA-256:

```text
2cfcc52d87834663d4aa0b4959c51f66451df965283b28fbfbad62d46741e013
```

`../res/netscape.ico` is the archive's `ns/cmd/winfe/res/communic.ico`.
The original Netscape Public License 1.0 and each source file's copyright notice
are retained. No CVS metadata was imported.

`stdafx.h` is a new, small compatibility header for these reusable MFC components.
It replaces the 1998 application's broad precompiled header, whose backend
dependencies do not match 3.02. The command-line parser's fixed buffer is guarded
by the caller's length check before construction.

The surrounding `cmd/winfe` application and adapter are new porting code targeting
this repository's 3.02 APIs. The 1998 application was not copied wholesale.
