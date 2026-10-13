#pragma once

// Which APN an operator's network wants for the internet bearer, by the operator code (MCC and MNC) that
// the first digits of the SIM's subscriber number carry. The data is generated from the Android Open Source
// Project's sample APN file (scripts/gen_apn_table.py, src/drivers/ApnTable.cpp, THIRD_PARTY.md); decision D-9.
//
// The list knows plain networks, not virtual operators on them: a SIM of a virtual operator carries the code of
// the network it runs on and gets that network's APN, which such a network accepts for its own and its virtual
// operators' customers.

#include <cstddef>
#include <cstdint>

namespace apn {

struct Entry {
  uint16_t mcc;
  uint16_t mnc;
  uint8_t mncDigits;  // 2 or 3: "01" and "001" are different operators
  const char* apn;
};

extern const Entry kTable[];
extern const size_t kTableSize;
extern const char* const kSourceSha256;

// The entry for a subscriber number (IMSI): 6 to 15 digits, of which the first three are the MCC and the next two
// or three the MNC. A three-digit MNC is looked for first, then a two-digit one. Null when the list has no
// such operator or the text is not a subscriber number.
const Entry* find(const char* imsi);

}  // namespace apn
