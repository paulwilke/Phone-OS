// The simulator's card and socket are held to the same contracts as the fakes and the hardware's
// implementations (testing/contracts/): a directory of the computer is a ConfigStore, a socket on the
// loopback address is a TcpServer. Both are written for the simulator and live in sim/.

#include <unity.h>

#include <ifaddrs.h>
#include <poll.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>

#include <DirectoryStore.h>
#include <SocketTcp.h>
#include <contracts/ConfigStoreContract.h>
#include <contracts/TcpServerContract.h>

void setUp() {}
void tearDown() {}

namespace {

namespace fs = std::filesystem;

struct DirectoryStoreRig {
  fs::path dir = fs::temp_directory_path() / ("phoneplus-dirstore-" + std::to_string(::getpid()) + "-" + std::to_string(counter()++));
  sim::DirectoryStore impl;
  DirectoryStoreRig() : impl((fs::create_directories(dir), dir)) {}
  ~DirectoryStoreRig() {
    fs::permissions(dir, fs::perms::owner_all, fs::perm_options::replace);  // so that it can be removed again
    fs::remove_all(dir);
  }
  static int& counter() {
    static int n = 0;
    return n;
  }
  ConfigStore& store() { return impl; }
  // A write that fails halfway (a full disk) cannot be provoked on a directory; the contract skips it.
  bool failNextWrite() { return false; }
  // A directory that may not be written to: every write fails before it starts.
  bool failAllWrites(bool fail) {
    fs::permissions(dir, fail ? fs::perms::owner_read | fs::perms::owner_exec : fs::perms::owner_all, fs::perm_options::replace);
    return true;
  }
};

class SocketPeer : public TcpPeer {
public:
  explicit SocketPeer(int fd) : fd_(fd) {}
  ~SocketPeer() override { close(); }
  void send(const std::string& bytes) override {
    ::send(fd_, bytes.data(), bytes.size(), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));  // loopback, but the system is not instant
  }
  void close() override {
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  std::string received() override {
    std::string out;
    char buf[256];
    pollfd p{fd_, POLLIN, 0};
    while (fd_ >= 0 && ::poll(&p, 1, 20) > 0) {
      const ssize_t n = ::recv(fd_, buf, sizeof(buf), 0);
      if (n <= 0) break;
      out.append(buf, static_cast<size_t>(n));
    }
    return out;
  }

private:
  int fd_;
};

// Not the simulator's port: a simulator that is running must not make the contract fail.
constexpr uint16_t kTestPort = 38080;

struct SocketTcpRig {
  sim::SocketTcpServer impl{sim::SocketTcpServer::Listen::Fixed, kTestPort};
  TcpServer& server() { return impl; }
  std::unique_ptr<TcpPeer> connect() {
    if (impl.boundPort() == 0) return nullptr;
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(impl.boundPort());
    if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
      ::close(fd);
      return nullptr;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));  // the system queues the connection a moment after connect() returns
    return std::make_unique<SocketPeer>(fd);
  }
};

}  // namespace

// ---- what the contracts do not say ---------------------------------------------------------------------

void test_a_file_the_disk_spells_in_another_case_is_found_and_replaced_not_duplicated() {
  DirectoryStoreRig rig;
  fs::create_directories(rig.dir / "Config");
  std::ofstream(rig.dir / "Config" / "Settings.TOML") << "old";
  std::string out;
  TEST_ASSERT_TRUE(rig.store().read("/config/settings.toml", out, 100));
  TEST_ASSERT_TRUE(rig.store().write("/CONFIG/SETTINGS.toml", "new"));
  size_t files = 0;
  for (const auto& e : fs::recursive_directory_iterator(rig.dir)) files += e.is_regular_file();
  TEST_ASSERT_EQUAL(1, static_cast<int>(files));
  TEST_ASSERT_TRUE(rig.store().read("/Config/Settings.Toml", out, 100));
  TEST_ASSERT_EQUAL_STRING("new", out.c_str());
}

void test_a_path_that_climbs_out_of_the_card_reaches_nothing_outside_it() {
  DirectoryStoreRig rig;
  std::ofstream(rig.dir.parent_path() / "outside-the-card.txt") << "secret";
  std::string out;
  TEST_ASSERT_FALSE(rig.store().read("/../outside-the-card.txt", out, 100));
  TEST_ASSERT_FALSE(rig.store().write("/../written-outside.txt", "x"));
  TEST_ASSERT_FALSE(fs::exists(rig.dir.parent_path() / "written-outside.txt"));
  TEST_ASSERT_FALSE(rig.store().remove("/../outside-the-card.txt"));
  TEST_ASSERT_TRUE(fs::exists(rig.dir.parent_path() / "outside-the-card.txt"));
  fs::remove(rig.dir.parent_path() / "outside-the-card.txt");
}

void test_a_scratch_file_left_on_the_disk_is_invisible() {
  DirectoryStoreRig rig;
  TEST_ASSERT_TRUE(rig.store().write("/a.txt", "content"));
  std::ofstream(rig.dir / "a.txt.tmp") << "half";
  std::vector<FileEntry> entries;
  TEST_ASSERT_TRUE(rig.store().list("/", entries));
  TEST_ASSERT_EQUAL(1, static_cast<int>(entries.size()));
  std::string out;
  TEST_ASSERT_FALSE(rig.store().read("/a.txt.tmp", out, 100));
}

void test_a_successful_write_leaves_no_scratch_file_behind() {
  DirectoryStoreRig rig;
  TEST_ASSERT_TRUE(rig.store().write("/a.txt", "content"));
  size_t files = 0;
  for (const auto& e : fs::directory_iterator(rig.dir)) files += e.is_regular_file();
  TEST_ASSERT_EQUAL(1, static_cast<int>(files));
}

void test_a_write_that_cannot_make_its_scratch_file_leaves_the_old_file_whole() {
  DirectoryStoreRig rig;
  TEST_ASSERT_TRUE(rig.store().write("/a.txt", "old content"));
  fs::create_directory(rig.dir / "a.txt.tmp");   // the scratch file cannot be created: a directory is in its place
  TEST_ASSERT_FALSE(rig.store().write("/a.txt", "new content"));
  std::string out;
  TEST_ASSERT_TRUE(rig.store().read("/a.txt", out, 100));
  TEST_ASSERT_EQUAL_STRING("old content", out.c_str());
}

void test_a_directory_cannot_be_written_as_a_file() {
  DirectoryStoreRig rig;
  TEST_ASSERT_TRUE(rig.store().write("/config/a.txt", "x"));
  TEST_ASSERT_FALSE(rig.store().write("/config", "x"));
  std::string out;
  TEST_ASSERT_FALSE(rig.store().read("/config", out, 100));
}

void test_the_server_opens_the_port_it_is_told_to_whatever_is_asked_for() {
  sim::SocketTcpServer fixed(sim::SocketTcpServer::Listen::Fixed, kTestPort);
  TEST_ASSERT_TRUE(fixed.begin(80));
  TEST_ASSERT_EQUAL(kTestPort, fixed.boundPort());
  TEST_ASSERT_FALSE(fixed.begin(0));                                 // 0 is never a port to open
  TEST_ASSERT_EQUAL(0, fixed.boundPort());
  sim::SocketTcpServer any(sim::SocketTcpServer::Listen::Any);
  TEST_ASSERT_TRUE(any.begin(80));
  TEST_ASSERT_TRUE(any.boundPort() != 0 && any.boundPort() != 80);
}

void test_a_port_that_is_taken_is_an_error_that_says_why() {
  sim::SocketTcpServer first(sim::SocketTcpServer::Listen::Fixed, kTestPort);
  sim::SocketTcpServer second(sim::SocketTcpServer::Listen::Fixed, kTestPort);
  TEST_ASSERT_TRUE(first.begin(80));
  TEST_ASSERT_FALSE(second.begin(80));
  TEST_ASSERT_FALSE(second.lastError().empty());
  TEST_ASSERT_EQUAL(0, second.boundPort());
}

void test_the_server_listens_on_the_loopback_address_only() {
  sim::SocketTcpServer server(sim::SocketTcpServer::Listen::Any);
  TEST_ASSERT_TRUE(server.begin(80));
  auto connectTo = [&](uint32_t address) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in probe{};
    probe.sin_family = AF_INET;
    probe.sin_port = htons(server.boundPort());
    probe.sin_addr.s_addr = address;
    const bool connected = ::connect(fd, reinterpret_cast<sockaddr*>(&probe), sizeof(probe)) == 0;
    ::close(fd);
    return connected;
  };
  TEST_ASSERT_TRUE(connectTo(htonl(INADDR_LOOPBACK)));
  // The machine's own address on the network must not reach it. A runner without a network has none.
  ifaddrs* all = nullptr;
  uint32_t other = 0;
  if (::getifaddrs(&all) == 0) {
    for (ifaddrs* a = all; a; a = a->ifa_next) {
      if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET) continue;
      const uint32_t address = reinterpret_cast<sockaddr_in*>(a->ifa_addr)->sin_addr.s_addr;
      if ((ntohl(address) >> 24) != 127) other = address;
    }
    ::freeifaddrs(all);
  }
  if (other == 0) TEST_IGNORE_MESSAGE("this machine has no address but the loopback");
  TEST_ASSERT_FALSE(connectTo(other));
}

int main() {
  UNITY_BEGIN();
  RUN_CONFIG_STORE_CONTRACT(DirectoryStoreRig);
  RUN_TCP_SERVER_CONTRACT(SocketTcpRig);
  RUN_TEST(test_a_file_the_disk_spells_in_another_case_is_found_and_replaced_not_duplicated);
  RUN_TEST(test_a_path_that_climbs_out_of_the_card_reaches_nothing_outside_it);
  RUN_TEST(test_a_scratch_file_left_on_the_disk_is_invisible);
  RUN_TEST(test_a_successful_write_leaves_no_scratch_file_behind);
  RUN_TEST(test_a_write_that_cannot_make_its_scratch_file_leaves_the_old_file_whole);
  RUN_TEST(test_a_directory_cannot_be_written_as_a_file);
  RUN_TEST(test_the_server_opens_the_port_it_is_told_to_whatever_is_asked_for);
  RUN_TEST(test_a_port_that_is_taken_is_an_error_that_says_why);
  RUN_TEST(test_the_server_listens_on_the_loopback_address_only);
  return UNITY_END();
}
