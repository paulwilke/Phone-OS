#pragma once

// Finds every picture of the device that the keys can reach, and how: a breadth-first walk over what
// the three keys under the screen, the arrows, Enter and Back do. The device has no way to save and
// restore its state, so each step starts a new Simulator on the same card, plays the prelude (the
// world the pictures are taken in: some messages, a missed call), presses the keys of the path and
// looks at the panel. Two pictures are the same state when they are the same picture on the same
// screen.
//
// The graph is the structure of the interface and does not depend on the language: it is found in
// English, and the same path is replayed in the other languages to take their pictures. That a path
// leads to the same screen in every language is what the replay checks (the screen's name is
// compared), so a layout that navigates differently in one language is reported.

#include <cstdint>
#include <deque>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "Simulator.h"

namespace sim {

struct CrawlNode {
  int id = 0;
  std::string screen;            // the name of the screen on top, "lock" while locked
  int depth = 0;
  std::vector<std::string> path;  // the keys from the start
};

struct CrawlEdge {
  int from = 0;
  std::string key;
  int to = 0;
};

struct Crawl {
  std::vector<CrawlNode> nodes;
  std::vector<CrawlEdge> edges;
  // States that were found and not kept because their screen had its share already (see run()).
  size_t trimmed = 0;
};

class Crawler {
public:
  static const std::vector<std::string>& keys() {
    static const std::vector<std::string> all = {"up", "down", "select", "back", "home", "inbox", "compose"};
    return all;
  }

  // `base` names the card and the catalogs; the card is copied for every replay, so nothing the pages write reaches it.
  Crawler(Options base, std::string prelude) : options_(std::move(base)), prelude_(std::move(prelude)) {
    // Every replay starts on a fresh copy of the card (the simulator makes it): the pages write what the child
    // changes (the language, a vibration) back to the card, and the walk must neither carry that from one
    // path to the next nor write to the data directory it was given.
    options_.inPlace = false;
    options_.workDir += "-crawl";
    options_.webPort = 0;
  }

  // Breadth first, so the shallowest states of every screen are kept. A screen has many states that
  // differ only in a selection or a scroll position; `perScreen` keeps the walk finite and the demo
  // readable: a state found after its screen has that many is not kept (and not followed), and the
  // number of those is `trimmed`. `maxDepth` bounds the keys in a path.
  Crawl run(int maxDepth, size_t perScreen) {
    Crawl crawl;
    std::map<std::pair<uint32_t, std::string>, int> seen;
    std::deque<int> queue;
    std::map<std::string, size_t> perScreenCount;
    {
      std::unique_ptr<Simulator> start = reach({}, "");
      crawl.nodes.push_back({0, screenOf(*start), 0, {}});
      seen[{start->panel().shown().crc(), crawl.nodes[0].screen}] = 0;
      perScreenCount[crawl.nodes[0].screen] = 1;
      queue.push_back(0);
    }
    while (!queue.empty()) {
      const int from = queue.front();
      queue.pop_front();
      if (crawl.nodes[static_cast<size_t>(from)].depth >= maxDepth) continue;
      const std::string here = crawl.nodes[static_cast<size_t>(from)].screen;  // a copy: push_back below may move the nodes
      for (const std::string& key : keys()) {
        if (!worldStaysAsItIs(here, key)) continue;
        std::vector<std::string> path = crawl.nodes[static_cast<size_t>(from)].path;
        path.push_back(key);
        std::unique_ptr<Simulator> sim = reach(path, "");
        const std::pair<uint32_t, std::string> identity{sim->panel().shown().crc(), screenOf(*sim)};
        auto known = seen.find(identity);
        if (known == seen.end()) {
          if (perScreenCount[identity.second] >= perScreen) {
            ++crawl.trimmed;
            continue;
          }
          ++perScreenCount[identity.second];
          const int id = static_cast<int>(crawl.nodes.size());
          crawl.nodes.push_back({id, identity.second, crawl.nodes[static_cast<size_t>(from)].depth + 1, path});
          known = seen.emplace(identity, id).first;
          queue.push_back(id);
        }
        if (known->second != from) crawl.edges.push_back({from, key, known->second});
      }
    }
    return crawl;
  }

  // Keys that change the world and not only the picture are not pressed, or every state would multiply
  // into the states of the history it leaves behind: a call that is on (hanging up, answering and the
  // volume belong to the call, and the scenes show them) and the choice of a ready-made message in
  // the compose page, which sends it.
  static bool worldStaysAsItIs(const std::string& screen, const std::string& key) {
    if (screen == "call") return false;
    if (screen == "compose" && key == "select") return false;
    return true;
  }

  // A new device in the state the path leads to, in `language` (empty: the card's own). The prelude
  // is played after the language is chosen, so the entries it makes are worded in it too.
  std::unique_ptr<Simulator> reach(const std::vector<std::string>& path, const std::string& language) {
    sink_.str("");
    auto sim = std::make_unique<Simulator>(options_, sink_);
    if (!language.empty()) sim->execute("language " + language);
    if (!prelude_.empty()) sim->runScript(prelude_);
    for (const std::string& key : path) sim->execute("key " + key);
    sim->settle();
    sim->runtime().render();
    return sim;
  }

  static std::string screenOf(Simulator& sim) {
    static const char* const names[] = {"home", "lock", "clock", "phone", "number", "call", "messages", "compose",
                                        "recipient", "sos", "settings", "wifi", "net", "status", "language",
                                        "gnss", "radio", "motion", "selftest", "power"};
    return names[sim.runtime().stack().visibleId()];
  }

  // The graph as JSON for the demo page.
  static std::string json(const Crawl& crawl) {
    std::ostringstream out;
    out << "{\"start\":0,\"trimmed\":" << crawl.trimmed << ",\"nodes\":[";
    for (size_t i = 0; i < crawl.nodes.size(); ++i)
      out << (i ? "," : "") << "{\"id\":" << crawl.nodes[i].id << ",\"screen\":\"" << crawl.nodes[i].screen
          << "\",\"depth\":" << crawl.nodes[i].depth << "}";
    out << "],\"edges\":[";
    for (size_t i = 0; i < crawl.edges.size(); ++i)
      out << (i ? "," : "") << "{\"from\":" << crawl.edges[i].from << ",\"key\":\"" << crawl.edges[i].key
          << "\",\"to\":" << crawl.edges[i].to << "}";
    out << "]}\n";
    return out.str();
  }

private:
  Options options_;
  std::string prelude_;
  std::ostringstream sink_;
};

}  // namespace sim
