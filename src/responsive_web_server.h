#pragma once

#include <WebServer.h>

// WebServer/WiFiClient can otherwise spend roughly ten seconds retrying a
// write after a browser abandons a response (for example, during rapid
// reloads). Stop only connections whose TCP writes make no progress for the
// configured interval; slow clients that continue receiving data are kept.
class ResponsiveWebServer : public WebServer {
 public:
  explicit ResponsiveWebServer(int port = 80) : WebServer(port) {}

 protected:
  size_t _currentClientWrite(const char* buffer, size_t length) override;
  size_t _currentClientWrite_P(PGM_P buffer, size_t length) override;

 private:
  size_t writeWithStallTimeout(const uint8_t* buffer, size_t length);
};
