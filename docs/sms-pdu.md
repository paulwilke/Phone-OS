# The SMS PDU

Code: `src/drivers/SmsPdu.{h,cpp}`; tests in `test/test_sms_pdu/` (with `testing/SmsPduDecoder.h`), `test/test_modem_sms/` and the fuzz test in `test/test_at_fuzz/`. Decision D-10: the driver sends a message in PDU mode and builds the message itself, because the modem's text mode sends a text with one non-ASCII character as UCS2 (70 places) and cannot join the parts of a long text.

## What `sms::encode` does

`encode(phone, text, reference, coding, parts)` returns the parts of one message as hex PDUs for `AT+CMGS=<tpduLength>`:

| Text | Coding | One message | A part of a long text |
|---|---|---|---|
| every character is in the GSM alphabet (ITU 03.38) | 7-bit, DCS 0x00 | 160 places | 153 places |
| any other | UCS2, DCS 0x08 | 70 units | 67 units |

**The GSM alphabet** holds the letters and digits of ASCII, the German umlauts and the sharp s, the accented e, u, i, o and a that Italian and French use most (not all of them), capital C-cedilla, n-tilde, the Scandinavian letters, some Greek capitals, the pound, dollar, yen and section signs, upside-down ! and ?, `@` and `_`, and, through an escape, `^ { } \ [ ] ~ |` and the euro sign, which take **two places** each. Missing from it are the lower-case c-cedilla and the circumflexed vowels, typographic quotes and dashes, and everything outside Latin: a text with one of them goes out as UCS2 as a whole. The text of the catalogs for the SMS (`sms.*`) is entirely inside the alphabet in English, German and French; the name of the child or a contact is not in the driver's hands.

**An emoji** takes two UCS2 units.

**The PDU** is an SMS-SUBMIT with a relative validity period of 12 hours (the 167 that the text mode was set to), the message reference left to the modem and the service centre stored in the modem (a leading `00`). The number is 20 digits at most (`*` and `#` are digits of the address), with type 0x91 when it begins with `+` and 0x81 otherwise; **a number that does not fit is refused and never shortened**, because a shortened number is someone else's.

**A long text** is cut into parts that each carry a concatenation header (information element 0x00: an 8-bit reference, the number of parts, the number of the part). In 7-bit the header takes 48 bits and the text starts at the next septet boundary, so a part holds 153 places and its length field counts 7 places for the header. The cut never falls between an escape and its character or between the two halves of an emoji. The reference is the driver's (`smsReference_`) and differs from one long text to the next. At most 255 parts; the driver's limit of 320 bytes (`kMaxSmsBytes`) is three parts.

## What is tested

`test_sms_pdu`: the example PDU that every tutorial uses ("hellohello" to +46708251358; ours differs in the validity octet only); an SOS with an umlaut as one 7-bit message; the umlauts at their septets; the extension characters at two places; the length where one message becomes two (160), two parts (306) and three (307); an escape pair that would straddle the cut; the reference and the count in every part; UCS2 with Cyrillic letters and an emoji; 70 and 67; a surrogate pair at the cut; the number as the specification writes it, 20 digits, 21 refused; bad UTF-8, overlong forms, a surrogate, a NUL and a text cut short. A decoder written in the test from the specification reads back every PDU, so the test does not check the encoder against itself. `test_at_fuzz` encodes random numbers and texts and checks the shape of what comes out.

## Not verified on the device

That the A7682E accepts `AT+CMGF=0` and `AT+CMGS=<length>` with these PDUs, that the service centre stored in the SIM is used, and that a receiving phone shows the parts of a long text as one message (open point 22).
