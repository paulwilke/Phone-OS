#include "BoardTcpServer.h"

namespace board {

int BoardTcpConnection::read(uint8_t* buf, size_t size) {
  if (!socket_ || closed_) return -1;
  const int available = socket_->available();
  if (available <= 0) return socket_->connected() ? 0 : -1;  // gone, and nothing is left to read
  if (size == 0) return 0;
  const size_t n = size < (size_t)available ? size : (size_t)available;
  return socket_->read(buf, n);
}

size_t BoardTcpConnection::write(const uint8_t* data, size_t len) {
  if (!socket_ || closed_ || len == 0 || !socket_->connected()) return 0;
  return socket_->write(data, len);
}

void BoardTcpConnection::close() {
  if (socket_ && !closed_) socket_->stop();
  closed_ = true;
}

bool BoardTcpServer::begin(uint16_t port) {
  stop();
  if (port == 0) return false;
  listening_ = listener_.listen(port);
  return listening_;
}

void BoardTcpServer::stop() {
  connection_.close();
  if (listening_) listener_.stopListening();
  listening_ = false;
}

TcpConnection* BoardTcpServer::accept() {
  if (!listening_) return nullptr;
  TcpSocket* client = listener_.nextClient();
  if (!client) return nullptr;
  // The one before is over when the next one is taken. A listener that hands out one socket object again
  // and again (the Wi-Fi stack's, device/Esp32TcpListener.h) has put the new client into it already:
  // stopping that object now would stop the new client, not the old one.
  if (client != connection_.socket()) connection_.close();
  connection_.attach(client);
  return &connection_;
}

}  // namespace board
