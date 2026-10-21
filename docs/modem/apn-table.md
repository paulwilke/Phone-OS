# The operator list: which APN a SIM gets

Code: `src/drivers/ApnTable.{h,cpp}` (the data, generated), `src/drivers/Apn.cpp` (the lookup), the job `ReadImsi` in `src/drivers/A7682EDriver.cpp`, the generator `scripts/gen_apn_table.py`; tests in `test/test_apn/`, `test/test_modem_driver/` and `test/test_gen_apn/test_gen.py`. Decision D-9: ordinary users do not know their APN and should not have to.

## What happens

When no APN is configured (`[modem] apn` empty, the normal case), the driver, after the SIM is ready, asks it for its subscriber number (`AT+CIMI`) and looks up the operator code, the first digits: the three of the MCC and the two or three of the MNC. A three-digit MNC is looked for first, then a two-digit one; the list is the only way to tell them apart, and "050" and "05" are two operators. The entry's APN goes into context 1 as `IPV4V6`, exactly as a configured APN would, with the radio cycled once when it had to be written ([volte.md](volte.md), [modem-driver.md](modem-driver.md)). The log says "Cellular: APN <apn> chosen for the operator <MCC><MNC>"; **the subscriber number is never kept or logged**, only the operator code. The SIM is asked again after every restart, because the card may be another one.

- **A configured APN always wins**, and then the SIM is not even asked.
- **The operator code found here is the home network** when the driver has to choose a network by hand: a network chosen by hand with this code is not searched again ([modem-driver.md](modem-driver.md), decision D-36). With a configured APN, or an operator the list does not know, the driver has no code to compare.
- **An operator that is not in the list**, or a SIM that does not answer: the modem's own contexts are left alone, as before 4.2.0, and the log says so once ("no APN known for the operator of this SIM", "the subscriber number could not be read"). Calls over LTE then depend on what the modem has stored.
- **A virtual operator** carries the code of the network it runs on and gets that network's entry. The list holds no entries for virtual operators (`mvno_type`), because telling them apart needs more of the SIM than the operator code. Such networks accept their own APN for virtual operators' customers in the cases looked at; where one does not, the explicit APN is the way out.

## Where the list comes from

The Android Open Source Project's sample APN file, apns-full-conf.xml in its sample device repository (Apache-2.0; https://android.googlesource.com/device/sample), 3667 entries when it was fetched on 2026-10-12. `scripts/gen_apn_table.py` takes from it, for every operator, the first entry that is for the plain network (no `mvno_type`), has the type `default` (the internet bearer), has an APN of letters, digits, dots, hyphens and underscores of at most 100 characters (what the driver accepts), and is not a test network (MCC 001): 1146 operators. The result is sorted so that the driver searches it in halves, and carries the licence reference, the source address and the sha256 of the file it was made from. **No entry is typed by hand.**

To renew it: fetch the file again into an empty directory (`https://android.googlesource.com/device/sample/+/refs/heads/main/etc/apns-full-conf.xml?format=TEXT` is base64), decode it, read it only as data, and run `python3 scripts/gen_apn_table.py --source <file>`. `python3 scripts/gen_apn_table.py --check` needs no file and fails when the stored list is not well formed; with `--source` it also fails when the list is not what the file gives. An update of the list comes with a firmware update: the device has no data connection before it knows the APN, so it cannot fetch one.

## What is known and what is not

The list is the Android project's, not a statement of the operators. It is checked here for its shape (sorted, unique, clean APNs, every operator can be found by a number with its code), not against the operators' own pages. For Telekom (262-01) it gives `internet.v6.telekom`, the dual-stack name, where the owner's SIM works with `internet.telekom`; with a configured APN the owner's setting is used as before.

**Not verified on the device:** that a SIM without a configured APN registers the IMS with the list's APN, and that `AT+CIMI` answers as the driver reads it (the subscriber number alone on a line) (open point 21).
