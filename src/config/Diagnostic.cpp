#include "Diagnostic.h"

namespace config {

const char* diagnosticName(Diagnostic::Code code) {
  switch (code) {
    case Diagnostic::Code::MissingFile:      return "missing-file";
    case Diagnostic::Code::OversizedFile:    return "oversized-file";
    case Diagnostic::Code::SyntaxError:      return "syntax-error";
    case Diagnostic::Code::UnknownKey:       return "unknown-key";
    case Diagnostic::Code::WrongType:        return "wrong-type";
    case Diagnostic::Code::OutOfRange:       return "out-of-range";
    case Diagnostic::Code::BadEnum:          return "bad-enum";
    case Diagnostic::Code::BadCatalogCode:   return "bad-catalog-code";
    case Diagnostic::Code::ContactNoNumber:  return "contact-no-number";
    case Diagnostic::Code::UnknownRole:      return "unknown-role";
    case Diagnostic::Code::BadQuietHours:    return "bad-quiet-hours";
    case Diagnostic::Code::NonCallTile:      return "non-call-tile";
    case Diagnostic::Code::TileNoTarget:     return "tile-no-target";
    case Diagnostic::Code::MeshAlias:        return "mesh-alias";
    case Diagnostic::Code::UserCatalogError: return "user-catalog-error";
  }
  return "unknown";
}

}  // namespace config
