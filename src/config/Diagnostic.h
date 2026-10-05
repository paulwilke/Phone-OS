#pragma once

// A structured problem report from the configuration loader. Codes name the kind of
// problem; args carry the details (file name, key path, bad value). The host logs
// them in English and the web status page words them from the catalogs.

#include <string>
#include <vector>

namespace config {

struct Diagnostic {
  enum class Code {
    MissingFile,     // file not found; args: file
    OversizedFile,   // file larger than kMaxFileBytes; args: file
    SyntaxError,     // TOML parse error; args: file, line-number, error-code-name
    UnknownKey,      // key not in the schema; args: file, key-path
    WrongType,       // TOML value has wrong type; args: file, key-path, expected-type
    OutOfRange,      // integer or string length outside limits; args: file, key-path
    BadEnum,         // string not in the allowed set; args: file, key-path, value
    BadCatalogCode,  // language code not installed; args: file, key-path, code
    ContactNoNumber, // [[contact]] entry without a number; args: entry-index (1-based)
    UnknownRole,     // contact role not recognised; args: role (treated as family)
    BadQuietHours,   // quiet_hours entry refused by QuietHours::add; args: result-name
    NonCallTile,     // [[tiles]] entry with action other than call (ignored); args: action
    TileNoTarget,    // [[tiles]] call entry without a target (skipped); (no args)
    MeshAlias,       // [mesh] section accepted as [radio] alias; (no args)
    UserCatalogError,// user catalog rejected; args: file, first-error
  };
  Code code;
  std::vector<std::string> args;
};

// A stable lowercase identifier suitable for log lines and catalog key suffixes.
const char* diagnosticName(Diagnostic::Code code);

}  // namespace config
