# Windows 3.02 Gold welcome message

`mailintro.msg` is the original Mozilla welcome message captured from the Inbox
created by the English Windows Gold 3.02 application on first Mail launch. Only
the generated `From - ...` mbox envelope line was removed. The original headers,
HTML, historical URLs and CRLF bytes are retained, rather than substituting the
different Unix `l10n/us/xp/mail.msg` resource.

| Resource | Bytes | SHA-256 |
| --- | ---: | --- |
| `mailintro.msg` | 3513 | `d776835cbba16e0af5bbde1f4e2e1878a58ab813e65486c60a24c25c3dca35dc` |

The original application came from [g32e302.exe](https://netscape.retropc.se/3/g32e302.exe),
SHA-256 `d053540b580809af7d2afb1a770cd26960c3996328fea89f7e2d1641acb1f117`.
The local oracle mailbox is in the ignored research runtime's `mail/Inbox`.
This payload was recovered from the application's generated mailbox, not from
decompilation or a claimed executable resource ID. Original executable/menu
provenance is in [the Windows comparison record](../../../cmake/windows-reference.md).

The frontend embeds the message as RCDATA 561 for `FE_AboutData("mailintro")`.
The original MSG/MIME/LO engines create, select and render the first-run message.
Windows publishes its complete Inbox through an exclusive sibling file and
rename; existing mailboxes, including empty ones, are never replaced.

The message refers to a retired remote `home_igloo.gif` banner. Its URL is
preserved, with no replacement artwork or external download added. The Netscape
logo uses the existing built-in `about:logo` image. Native tests load that local
logo, disable remote image requests and verify the original message bytes and
rendered body. The welcome's historical feature descriptions are not a statement
that every feature is implemented in this frontend.
