#pragma once

// JSON serialisation and deserialisation for the web API, without any
// third-party library. Two directions are supported:
//
//   fromToml   converts a toml::Value tree to a JSON string — used by the
//              GET /api/settings|contacts|secrets handlers.
//   toToml     parses a JSON object into a toml::Value tree — used by the
//              PUT handlers to convert the page's JSON into TOML for
//              validation and storage.
//   getString  extracts the string value of a flat key from a JSON object —
//              used by the restore handler where keys contain dots
//              ("settings.toml") that must not be treated as TOML paths.
//
// Both JSON→TOML and TOML→JSON handle the subset the configuration files use:
// strings, integers, booleans, arrays of scalars, objects, and arrays of
// objects (which become TOML array-of-tables). Floats and null are not stored
// on the card; toToml refuses floats and silently skips null values.

#include <cstddef>
#include <string>

#include "../app/Toml.h"

namespace web::json {

// Deepest nesting of objects and arrays that toToml reads. A configuration file needs four levels (the file,
// a section, its list of tables, a table's list of values); on the device every level is a stack frame of
// the loop task, whose stack is 8 KB, and a request body of 8192 bytes could open 8192 brackets. Deeper
// input is refused with an error, as the TOML reader refuses its own (toml::kMaxDepth).
constexpr size_t kMaxDepth = 16;

// Serialise `v` to a JSON string in `out[0..size-1]`, NUL-terminated.
// Returns the number of bytes written (excluding the NUL), or 0 when the
// result does not fit.
size_t fromToml(const toml::Value& v, char* out, size_t size);

// Parse a JSON object from `data[0..len-1]` into `out`.
// Returns false on any parse error, nesting deeper than kMaxDepth included; `error` then holds a short
// description.
bool toToml(const char* data, size_t len, toml::Value& out, std::string& error);

// Find a top-level string value by `key` in a JSON object. The key may
// contain dots — it is compared as a flat string against JSON keys.
// Returns false when the key is absent or its value is not a string.
bool getString(const char* json, size_t len, const char* key, char* out, size_t outSize);

}  // namespace web::json
