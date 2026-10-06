// The simulator program: the device's application on the fakes, with a console and, when built with
// SDL (environment `sim-sdl`), a window. Everything it does is in Simulator.h; this file reads the
// command line and runs the loop.
//
//   .pio/build/sim/program --script sim/scripts/smoke.txt            run a scenario headless and exit
//   .pio/build/sim/program --data sim/data-parent                    interactive console
//   .pio/build/sim/program --help

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include "Crawler.h"
#include "SdlWindow.h"
#include "Simulator.h"

namespace {

void usage() {
  std::cout << "usage: program [options]\n"
               "  --data <dir>      the card: a directory with config/ (and lang/); default sim/data\n"
               "  --work <dir>      where the card is copied to (default out/sim-card); emptied at every start\n"
               "  --in-place        use the data directory itself as the card: saved settings change it\n"
               "  --port <n>        the port of the web page on 127.0.0.1 (default 8080)\n"
               "  --lang <dir>      the shipped catalogs; default lang\n"
               "  --host-lang <dir> words only host tools show (the demo page); default tools/lang\n"
               "  --web <file>      the web page; default web/index.html\n"
               "  --golden <dir>    where `golden` looks for pictures; default sim/golden\n"
               "  --script <file>   run the commands of a file first (a line `# data: <dir>` at its top names its card); the program exits after it unless\n"
               "                    --interactive is given. The exit status is 1 when a command failed\n"
               "  --crawl <dir>     walk every picture the keys can reach: <dir>/graph.json and, for each language,\n"
               "                    <dir>/<language>/n<state>.png; exit status 1 when the walk hit its limits\n"
               "  --languages <a,b> the languages of the pictures of --crawl (default en,de,fr)\n"
               "  --prelude <file>  the world the crawl starts in: commands played after every start\n"
               "  --depth <n>       longest path of the crawl in keys (default 12)\n"
               "  --per-screen <n>  most states kept for one screen (default 30); the rest are counted, not followed\n"
               "  --interactive     read commands from the terminal (the default without --script)\n"
               "  --headless        no window (the only choice in a build without SDL)\n"
               "  --scale <n>       window size in times 240 x 320 (default 2)\n"
               "  --lapse <n>       time runs n times faster than the clock on the wall\n"
               "Type `help` at the prompt for the console commands.\n";
}

// Lines typed at the terminal arrive on their own thread, so a blocking read never stops the clock.
class Terminal {
public:
  Terminal() : reader_([this] { read(); }) { reader_.detach(); }
  bool next(std::string& line) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (lines_.empty()) return false;
    line = lines_.front();
    lines_.pop_front();
    return true;
  }
  bool closed() {
    std::lock_guard<std::mutex> lock(mutex_);
    return closed_ && lines_.empty();
  }

private:
  void read() {
    for (std::string line; std::getline(std::cin, line);) {
      std::lock_guard<std::mutex> lock(mutex_);
      lines_.push_back(line);
    }
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
  }
  std::mutex mutex_;
  std::deque<std::string> lines_;
  bool closed_ = false;
  std::thread reader_;
};

}  // namespace

int main(int argc, char** argv) {
  sim::Options options;
  std::string script;
  bool interactive = false, headless = false, dataGiven = false;
  int scale = 2;
  std::string crawlDir, prelude, languages = "en,de,fr";
  int depth = 12;
  size_t perScreen = 30;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto value = [&](std::string& into) {
      if (i + 1 >= argc) {
        std::cerr << "error: " << a << " needs a value\n";
        return false;
      }
      into = argv[++i];
      return true;
    };
    std::string v;
    if (a == "--help" || a == "-h") { usage(); return 0; }
    else if (a == "--data") { if (!value(options.dataDir)) return 2; dataGiven = true; }
    else if (a == "--lang") { if (!value(options.langDir)) return 2; }
    else if (a == "--host-lang") { if (!value(options.hostLangDir)) return 2; }
    else if (a == "--web") { if (!value(options.webPage)) return 2; }
    else if (a == "--work") { if (!value(options.workDir)) return 2; }
    else if (a == "--in-place") options.inPlace = true;
    else if (a == "--port") { if (!value(v)) return 2; options.webPort = static_cast<uint16_t>(std::atoi(v.c_str())); }
    else if (a == "--golden") { if (!value(options.goldenDir)) return 2; }
    else if (a == "--script") { if (!value(script)) return 2; }
    else if (a == "--scale") { if (!value(v)) return 2; scale = std::max(1, std::atoi(v.c_str())); }
    else if (a == "--lapse") { if (!value(v)) return 2; options.lapse = static_cast<uint32_t>(std::max(1, std::atoi(v.c_str()))); }
    else if (a == "--crawl") { if (!value(crawlDir)) return 2; }
    else if (a == "--languages") { if (!value(languages)) return 2; }
    else if (a == "--prelude") { if (!value(prelude)) return 2; }
    else if (a == "--depth") { if (!value(v)) return 2; depth = std::max(1, std::atoi(v.c_str())); }
    else if (a == "--per-screen") { if (!value(v)) return 2; perScreen = static_cast<size_t>(std::max(1, std::atoi(v.c_str()))); }
    else if (a == "--interactive") interactive = true;
    else if (a == "--headless") headless = true;
    else { std::cerr << "error: unknown option " << a << "\n"; usage(); return 2; }
  }

  if (!crawlDir.empty()) {
    sim::Crawler crawler(options, prelude);
    const sim::Crawl crawl = crawler.run(depth, perScreen);
    std::filesystem::create_directories(crawlDir);
    std::ofstream(crawlDir + "/graph.json") << sim::Crawler::json(crawl);
    std::vector<std::string> codes;
    for (size_t at = 0; at <= languages.size();) {
      const size_t comma = languages.find(',', at);
      codes.push_back(languages.substr(at, comma == std::string::npos ? std::string::npos : comma - at));
      at = comma == std::string::npos ? languages.size() + 1 : comma + 1;
    }
    for (const std::string& code : codes) {
      std::filesystem::create_directories(crawlDir + "/" + code);
      for (const sim::CrawlNode& node : crawl.nodes) {
        std::unique_ptr<sim::Simulator> there = crawler.reach(node.path, code);
        if (sim::Crawler::screenOf(*there) != node.screen) {
          std::cerr << "error: in " << code << " the path to state " << node.id << " leads to "
                    << sim::Crawler::screenOf(*there) << ", not " << node.screen << "\n";
          return 1;
        }
        if (!there->panel().writePng(crawlDir + "/" + code + "/n" + std::to_string(node.id) + ".png")) {
          std::cerr << "error: cannot write the picture of state " << node.id << "\n";
          return 1;
        }
      }
    }
    std::cout << "crawl: " << crawl.nodes.size() << " states, " << crawl.edges.size() << " edges, " << codes.size()
              << " languages, " << crawl.trimmed << " more states of the same screens not followed\n";
    return 0;
  }
  if (!dataGiven && !script.empty() && !sim::scriptData(script).empty()) options.dataDir = sim::scriptData(script);
  sim::Simulator simulator(options, std::cout);
  if (!simulator.ok()) {
    std::cerr << "error: " << simulator.error() << "\n";
    return 2;
  }
  simulator.execute("lapse " + std::to_string(options.lapse));
  if (!script.empty() && !simulator.runScript(script)) return 1;
  if (!script.empty() && !interactive) return 0;
  if (simulator.quitRequested()) return 0;

#if PHONEPLUS_SIM_SDL
  sim::SdlWindow window(scale);
  const bool windowed = !headless && window.ok();
  if (!headless && !windowed) std::cerr << "warning: no window could be opened, the console only\n";
#else
  (void)scale;
  const bool windowed = false;
  if (!headless) std::cerr << "note: built without SDL: the console only (see sim/README.md)\n";
#endif

  Terminal terminal;
  std::cout << "phoneplus simulator: type help\n";
  auto last = std::chrono::steady_clock::now();
  while (!simulator.quitRequested()) {
    std::string line;
    while (terminal.next(line)) simulator.execute(line);
    if (!windowed && terminal.closed()) break;  // nobody is there any more
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count();
    last = now;
    simulator.advance(static_cast<uint64_t>(elapsed) * simulator.lapse());
#if PHONEPLUS_SIM_SDL
    if (windowed) {
      if (!window.pump(simulator)) break;
      window.present(simulator.panel(), simulator.nowMs());
    }
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  return 0;
}
