#pragma once

// The web API for the parent's browser: all routes, password, lockout, CSRF
// protection, validation before writing and logging of every change.
//
// Security rules — every rule has a counter-check test:
//   1. Every /api/ route requires Basic-Auth with the password from
//      /config/secrets.toml. Without a stored password the API is locked.
//   2. Five wrong passwords in a row lock everyone out for one minute.
//   3. Mutating requests (PUT, POST, DELETE) require the header X-Phoneplus: 1.
//      A foreign page cannot set this header (no CORS), blocking CSRF.
//   4. /api/secrets returns the whole file including all passwords; only for
//      the authenticated parent (deliberate trade-off: 2026-10-01).
//   5. Config files are validated before writing; a file the device would reject
//      is never stored. secrets.toml is validated more strictly (any unknown key
//      is an error — a typo in "sim_pin" means the PIN silently does nothing).
//   6. Every authenticated request restarts the idle time of Wi-Fi, so a parent who is still editing
//      is never cut off; a request without the right password, from a locked-out client, or for the
//      login page does not, or anybody could keep Wi-Fi on. POST /api/wifi-extend does it on purpose.
//   7. Every change is logged (path and size only, never content). Deleting a
//      log is allowed but never silent: the deletion itself is logged first.
//      secrets.toml cannot be deleted through the web.

#include <cstddef>
#include <cstdint>
#include <string>

#include "../app/AppCore.h"
#include "../hal/ConfigStore.h"
#include "RequestHandler.h"
#include "WebAssets.h"


class WebApi : public web::RequestHandler {
public:
  static constexpr uint32_t kMaxFailures = 5;
  static constexpr uint32_t kLockSeconds = 60;
  // Minimum web password length (four characters, owner's decision 2026-10-01;
  // see docs/web-interface.md for the security trade-off).
  static constexpr size_t kMinWebPassword = 4;
  // Maximum bytes per config file, also for each file in a backup.
  static constexpr size_t kMaxConfigFile = 4096;
  // Maximum response body size.
  static constexpr size_t kResponseMax = 12288;

  WebApi(ConfigStore& store, WebAssets& assets, WebHost& host, AppCore& app,
         i18n::Translator& translator);

  // web::RequestHandler
  void handle(const http::Request& req, http::Response& res, uint32_t nowSeconds) override;
  void error(int status, http::Response& res) override;

private:
  void jsonResp(http::Response& res, int status, const char* body);
  void jsonError(http::Response& res, int status, const char* message);
  void ok(http::Response& res, const char* warnings = nullptr);

  bool authenticate(const http::Request& req, http::Response& res, uint32_t now);
  bool webPassword(char* out, size_t size);

  void state(http::Response& res, uint32_t now);
  void getConfigJson(const char* path, http::Response& res);
  void putConfigJson(const char* path, const http::Request& req, http::Response& res);
  void languages(http::Response& res);
  void langCatalog(const char* code, http::Response& res);
  void listFiles(const http::Request& req, http::Response& res);
  void getFile(const http::Request& req, http::Response& res);
  void putFile(const http::Request& req, http::Response& res);
  void deleteFile(const http::Request& req, http::Response& res);
  void backup(http::Response& res);
  void restore(const http::Request& req, http::Response& res);

  // Validates a config file before writing. Returns false and sets `why` on
  // hard errors; soft problems accumulate in `warnings`.
  bool checkConfigText(const char* path, const char* text, size_t len,
                       std::string& why, std::string& warnings);
  // Writes the file, logs the change, sends the response.
  bool commit(const char* path, const char* data, size_t len,
              http::Response& res, const std::string& warnings);
  // Extracts the `path=` query parameter and validates it.
  bool queryPath(const http::Request& req, char* out, size_t size, http::Response& res);

  ConfigStore& store_;
  WebAssets& assets_;
  WebHost& host_;
  AppCore& app_;
  i18n::Translator& translator_;

  uint32_t failures_ = 0;
  uint32_t lockedUntil_ = 0;
  bool locked_ = false;

  char resp_[kResponseMax];
  char scratch_[kMaxConfigFile + 1];
  char scratch2_[kMaxConfigFile + 1];

  static constexpr const char* kSettingsPath = "/config/settings.toml";
  static constexpr const char* kContactsPath = "/config/contacts.toml";
  static constexpr const char* kSecretsPath  = "/config/secrets.toml";
};
