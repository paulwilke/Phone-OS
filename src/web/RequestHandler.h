#pragma once

// The interface WebServer calls to handle a request or an HTTP error. WebApi
// implements this; tests use stubs. The separation keeps WebServer independent
// of the API layer so both can be tested without the other.

#include <cstdint>

#include "Http.h"

namespace web {

class RequestHandler {
public:
  virtual ~RequestHandler() = default;
  virtual void handle(const http::Request& req, http::Response& res, uint32_t nowSeconds) = 0;
  virtual void error(int httpStatus, http::Response& res) = 0;
};

}  // namespace web
