#pragma once

// The Wi-Fi stack's server socket behind the TcpListener seam. One client at a time is handed out; the
// capability's rules are BoardTcpServer's.

#include <WiFi.h>

#include <board/TcpListener.h>

class Esp32TcpSocket : public board::TcpSocket {
public:
  void attach(WiFiClient client) {
    client_ = client;
    client_.setNoDelay(true);
  }
  int available() override { return client_.available(); }
  int read(uint8_t* buf, size_t size) override { return client_.read(buf, size); }
  size_t write(const uint8_t* data, size_t len) override { return client_.write(data, len); }
  bool connected() override { return client_.connected(); }
  void stop() override { client_.stop(); }

private:
  WiFiClient client_;
};

class Esp32TcpListener : public board::TcpListener {
public:
  ~Esp32TcpListener() override { stopListening(); }

  bool listen(uint16_t port) override {
    stopListening();
    server_ = new WiFiServer(port);
    server_->begin();
    return true;
  }
  void stopListening() override {
    socket_.stop();
    if (server_) {
      server_->end();
      delete server_;
      server_ = nullptr;
    }
  }
  board::TcpSocket* nextClient() override {
    if (!server_) return nullptr;
    WiFiClient client = server_->available();
    if (!client) return nullptr;
    socket_.attach(client);
    return &socket_;
  }

private:
  WiFiServer* server_ = nullptr;
  Esp32TcpSocket socket_;
};
