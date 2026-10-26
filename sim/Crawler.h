#pragma once

// Finds every picture of the device that the keys and a finger can reach, and how: a breadth-first walk
// over what the three keys under the screen, the arrows, Enter and Back do, and over the places each
// screen says a tap hits (Screen::touchTargets: the soft keys, the tiles, the rows, the buttons). The
// device has no way to save and restore its state, so each step starts a new Simulator on the same
// card, plays the prelude (the world the pictures are taken in: some messages, a missed call), replays
// the path and looks at the panel. Two pictures are the same state when they are the same picture on
// the same screen in the same language.
//
// The language is part of the state. The Language screen puts its cursor on the language in use, so the same
// keys pick different rows in different languages, and a choice leads into the interface of the chosen
// language: one graph over (language, screen, picture) is found, starting once in every language asked for,
// and each state's picture is its own, in its own language. Nothing is cut: every key and every tap of every
// state is pressed, to the end of the graph (the walk is complete) or, past `maxDepth` or `maxStates`, reported
// as incomplete, so that a key which is drawn is never dead because the walk stopped before it.

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <deque>
#include <fstream>
#include <map>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "Simulator.h"

namespace sim {

struct CrawlNode {
  int id = 0;
  std::string screen;            // the name of the screen on top, "lock" while locked
  std::string language;          // the interface language of the picture (it changes where the child chooses another)
  std::string start;             // the language the device was started in; the path is replayed from there
  uint32_t crc = 0;              // of the picture, to see that a replay shows the same
  int depth = 0;
  std::vector<std::string> path;  // the steps from the start, as console lines: "key up", "tap 120 160", "advance 7s"
  std::vector<std::string> soft;  // the soft keys the screen draws, as key names
  std::vector<std::string> idle;  // the keys that were pressed and changed nothing the picture shows
};

// A tap is a target the screen reported: the edge carries its rectangle, so that the page can let a finger
// hit it, and the step of the path taps its middle. The third kind of edge is time: a screen that ends by itself
// (the key test of the self-test page) leads on after `wait` seconds without a key, and the page follows it when
// that time has passed.
struct CrawlEdge {
  int from = 0;
  std::string key;               // empty for a tap and for a wait
  ui::Rect tap;                  // empty for a key and for a wait
  int wait = 0;                  // seconds; 0 for a key and for a tap
  int to = 0;
  bool isTap() const { return key.empty() && wait == 0; }
  bool isWait() const { return wait > 0; }
};

struct Crawl {
  std::vector<CrawlNode> nodes;
  std::vector<CrawlEdge> edges;
  std::map<std::string, int> starts;   // the state a device started in each language begins in
  // True when every state was followed: none was left at `maxDepth` and the walk stopped at its own end. A graph
  // that is not complete has keys that are drawn and lead nowhere.
  bool complete = true;
  // Steps (a state, the step as a console line) that changed nothing, so that no edge exists for them: a key at
  // the end of a list, the choice of the language that is in use.
  std::vector<std::pair<int, std::string>> unchanged;
};

class Crawler {
public:
  // How long a screen that ends by itself is left alone: the self-test's key test ends after six seconds without a key
  // (SelfTestScreen::kIdleSeconds), and the walk lets one more second pass.
  static constexpr int kWaitSeconds = 7;

  static const std::vector<std::string>& keys() {
    static const std::vector<std::string> all = {"up", "down", "select", "back", "home", "inbox", "compose"};
    return all;
  }

  // `base` names the card and the catalogs; the card is copied for every replay, so nothing the pages write reaches it.
  Crawler(Options base, std::string prelude) : options_(std::move(base)), prelude_(std::move(prelude)), base_(options_.workDir) {
    // Every replay starts on a fresh copy of the card (the simulator makes it): the pages write what the child
    // changes (the language, a vibration) back to the card, and the walk must neither carry that from one
    // path to the next nor write to the data directory it was given.
    options_.inPlace = false;
    options_.workDir += "-crawl";
    options_.webPort = 0;
  }

  // Where the walk says how far it is (every few hundred states); null for silence.
  void setProgress(std::ostream* out) { progress_ = out; }

  // The number of replays that run at the same time (default 1). A replay is a simulator of its own on a card of its
  // own, so they share nothing: each worker has a work directory beside this one's (`<work>-<n>-crawl`).
  void setJobs(size_t jobs) {
    workers_.clear();
    for (size_t i = 1; i < std::max<size_t>(jobs, 1); ++i) {
      Options o = options_;
      o.workDir = base_ + "-" + std::to_string(i);
      workers_.push_back(std::make_unique<Crawler>(o, prelude_));
    }
  }

  // Calls fn(i, crawler) for every i below n, on as many threads as there are workers, each with a crawler of its own.
  template <class F>
  void each(size_t n, F fn) {
    const size_t jobs = std::min(workers_.size() + 1, n);
    if (jobs <= 1) {
      for (size_t i = 0; i < n; ++i) fn(i, *this);
      return;
    }
    std::atomic<size_t> next{0};
    std::vector<std::thread> threads;
    for (size_t t = 0; t < jobs; ++t) {
      Crawler& crawler = t == 0 ? *this : *workers_[t - 1];
      threads.emplace_back([&next, n, &fn, &crawler] {
        for (size_t i = next++; i < n; i = next++) fn(i, crawler);
      });
    }
    for (std::thread& thread : threads) thread.join();
  }

  // Breadth first, so the shallowest path to every state is the one kept. The device is started once in each of
  // `languages` (empty: once, in the card's own); every key and every tap of every state is followed. `maxDepth`
  // bounds the steps in a path and `maxStates` the number of states, as fuses: when either is reached with
  // something still to find, the crawl is not `complete` and the caller must not use it. Nothing else cuts a
  // state off, so a key that is drawn leads where the device would lead.
  Crawl run(int maxDepth, const std::vector<std::string>& languages = {}, size_t maxStates = 20000) {
    Crawl crawl;
    std::map<std::tuple<uint32_t, std::string, std::string>, int> seen;
    std::deque<int> queue;
    error_.clear();
    const std::vector<std::string> starts = languages.empty() ? std::vector<std::string>{""} : languages;
    for (const std::string& language : starts) {
      std::unique_ptr<Simulator> start = reach({}, language);
      if (!start) return crawl;   // the device did not start: error() says why, and there is nothing to walk
      CrawlNode node = describe(*start, static_cast<int>(crawl.nodes.size()), language, 0, {});
      const auto identity = std::make_tuple(node.crc, node.screen, node.language);
      auto known = seen.find(identity);
      if (known == seen.end()) {
        known = seen.emplace(identity, node.id).first;
        crawl.nodes.push_back(node);
        queue.push_back(node.id);
      }
      crawl.starts[node.language] = known->second;
    }
    size_t done = 0;
    while (!queue.empty() && error_.empty()) {
      const int from = queue.front();
      queue.pop_front();
      if (progress_ && ++done % 250 == 0)
        *progress_ << "crawl: " << done << " states followed, " << queue.size() << " waiting, " << crawl.nodes.size()
                   << " found, " << crawl.edges.size() << " edges\n";
      // Copies: push_back below may move the nodes.
      const std::vector<std::string> here = crawl.nodes[static_cast<size_t>(from)].path;
      const std::string screen = crawl.nodes[static_cast<size_t>(from)].screen;
      const std::string start = crawl.nodes[static_cast<size_t>(from)].start;
      const int depth = crawl.nodes[static_cast<size_t>(from)].depth;
      // What the state `from` leads to: every key, every place a tap hits (the device is started once more to ask the
      // screen, which says them as drawn, so the page can lay them over the picture), and, on a screen that ends by
      // itself, the time passing.
      struct Step {
        std::string line, key;
        ui::Rect tap;
        int wait = 0;
      };
      std::vector<Step> steps;
      for (const std::string& key : keys()) steps.push_back({"key " + key, key, {}, 0});
      {
        std::unique_ptr<Simulator> at = reach(here, start);
        if (!at) return crawl;
        for (const ui::TouchTarget& target : targets(*at)) {
          const ui::Rect r = target.rect;
          steps.push_back({"tap " + std::to_string(r.x + r.w / 2) + " " + std::to_string(r.y + r.h / 2), "", r, 0});
        }
      }
      if (endsByItself(screen)) steps.push_back({"advance " + std::to_string(kWaitSeconds) + "s", "", {}, kWaitSeconds});
      // The steps do not depend on each other, so they are taken at the same time when there are workers; what they
      // find is put into the graph one after the other in the order of the steps, so the graph is the same whatever
      // the number of workers and their luck.
      std::vector<CrawlNode> found(steps.size());
      std::vector<char> started(steps.size(), 0);
      each(steps.size(), [&](size_t i, Crawler& worker) {
        std::vector<std::string> path = here;
        path.push_back(steps[i].line);
        std::unique_ptr<Simulator> sim = worker.reach(path, start);
        if (!sim) return;
        found[i] = describe(*sim, 0, start, depth + 1, path);
        started[i] = 1;
      });
      for (size_t i = 0; i < steps.size(); ++i) {
        if (!started[i]) {
          adoptError();
          return crawl;
        }
        // The state is new, or known; an edge is recorded when it leads somewhere else, and the step is noted as
        // unchanged when it does not.
        CrawlNode now = std::move(found[i]);
        const auto identity = std::make_tuple(now.crc, now.screen, now.language);
        auto known = seen.find(identity);
        if (known == seen.end()) {
          if (depth + 1 > maxDepth || crawl.nodes.size() >= maxStates) {
            crawl.complete = false;
            if (crawl.nodes.size() >= maxStates) error_ = "more than " + std::to_string(maxStates) + " states: the walk is stopped";
            continue;
          }
          now.id = static_cast<int>(crawl.nodes.size());
          known = seen.emplace(identity, now.id).first;
          crawl.nodes.push_back(std::move(now));
          queue.push_back(known->second);
        }
        if (known->second == from) {
          crawl.unchanged.emplace_back(from, steps[i].line);
          if (!steps[i].key.empty()) crawl.nodes[static_cast<size_t>(from)].idle.push_back(steps[i].key);
        } else {
          crawl.edges.push_back({from, steps[i].key, steps[i].tap, steps[i].wait, known->second});
        }
      }
    }
    return crawl;
  }

  // The places a tap hits on the screen the device shows now, as the visible screen reports them after a frame
  // was drawn (the soft keys as drawn last, the rows on show). Empty when nothing on it answers a tap.
  static std::vector<ui::TouchTarget> targets(Simulator& sim) {
    ui::Screen* screen = sim.runtime().stack().visible();
    std::vector<ui::TouchTarget> out = screen ? screen->touchTargets() : std::vector<ui::TouchTarget>{};
    out.erase(std::remove_if(out.begin(), out.end(), [](const ui::TouchTarget& t) { return t.rect.empty(); }), out.end());
    return out;
  }

  // A screen that goes on by itself when nobody presses a key: the self-test page leaves the test of the keys
  // and of the touch screen after a few seconds. The walk has no clock of its own, so it lets that time pass once
  // in every state of the screen, and a state that does not change then has no such edge.
  static bool endsByItself(const std::string& screen) { return screen == "selftest"; }

  static std::string keyName(ui::Key key) {
    switch (key) {
      case ui::Key::Up: return "up";
      case ui::Key::Down: return "down";
      case ui::Key::Select: return "select";
      case ui::Key::Back: return "back";
      case ui::Key::Home: return "home";
      case ui::Key::Inbox: return "inbox";
      case ui::Key::Compose: return "compose";
    }
    return "";
  }

  // A new device in the state the path leads to, in `language` (empty: the card's own), or null when the
  // simulator could not start (a work directory that is not its own, a card that cannot be copied): error()
  // then says why. A simulator that did not start has no device behind it, and asking it to run is a crash,
  // not an error message (seen on 2026-10-13 with a stale work directory in the temp folder). The prelude
  // is played after the language is chosen, so the entries it makes are worded in it too. The pseudo-language
  // is not on the device and not in the demo: `Options::pseudoLanguage` is off for the walk that makes it.
  std::unique_ptr<Simulator> reach(const std::vector<std::string>& path, const std::string& language) {
    sink_.str("");
    auto sim = std::make_unique<Simulator>(options_, sink_);
    if (!sim->ok()) {
      error_ = sim->error();
      return nullptr;
    }
    if (!language.empty()) sim->execute("language " + language);
    if (!prelude_.empty()) sim->runScript(prelude_);
    for (const std::string& line : path) sim->execute(line);
    sim->settle();
    sim->runtime().render();
    return sim;
  }

  // What the device shows now, as a state: its screen, language and picture, and the soft keys it draws.
  static CrawlNode describe(Simulator& sim, int id, const std::string& start, int depth, std::vector<std::string> path) {
    CrawlNode node;
    node.id = id;
    node.screen = screenOf(sim);
    node.language = sim.translator().language();
    node.start = start;
    node.crc = sim.panel().shown().crc();
    node.depth = depth;
    node.path = std::move(path);
    for (const ui::TouchTarget& target : targets(sim)) {
      if (!target.key) continue;
      const std::string name = keyName(*target.key);
      if (std::find(node.soft.begin(), node.soft.end(), name) == node.soft.end()) node.soft.push_back(name);
    }
    return node;
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
    const auto list = [&](const std::vector<std::string>& names) {
      for (size_t i = 0; i < names.size(); ++i) out << (i ? "," : "") << "\"" << names[i] << "\"";
    };
    out << "{\"starts\":{";
    bool first = true;
    for (const auto& entry : crawl.starts) {
      out << (first ? "" : ",") << "\"" << entry.first << "\":" << entry.second;
      first = false;
    }
    out << "},\"complete\":" << (crawl.complete ? "true" : "false") << ",\"nodes\":[";
    for (size_t i = 0; i < crawl.nodes.size(); ++i) {
      const CrawlNode& n = crawl.nodes[i];
      out << (i ? "," : "") << "{\"id\":" << n.id << ",\"screen\":\"" << n.screen << "\",\"lang\":\"" << n.language
          << "\",\"depth\":" << n.depth << ",\"soft\":[";
      list(n.soft);
      out << "],\"idle\":[";
      list(n.idle);
      out << "]}";
    }
    out << "],\"edges\":[";
    for (size_t i = 0; i < crawl.edges.size(); ++i) {
      const CrawlEdge& e = crawl.edges[i];
      out << (i ? "," : "") << "{\"from\":" << e.from;
      if (e.isWait())
        out << ",\"wait\":" << e.wait;
      else if (e.isTap())
        out << ",\"tap\":{\"x\":" << e.tap.x << ",\"y\":" << e.tap.y << ",\"w\":" << e.tap.w << ",\"h\":" << e.tap.h << "}";
      else
        out << ",\"key\":\"" << e.key << "\"";
      out << ",\"to\":" << e.to << "}";
    }
    out << "]}\n";
    return out.str();
  }

  // Why the last reach() or run() stopped short; empty while every start worked.
  const std::string& error() const { return error_; }

  // The reason a worker's start failed, as this crawler's own (after each()).
  void adoptError() {
    for (const std::unique_ptr<Crawler>& worker : workers_)
      if (error_.empty() && !worker->error_.empty()) error_ = worker->error_;
  }

private:
  Options options_;
  std::string prelude_;
  std::string base_;   // the work directory as it was given, before the suffixes
  std::ostringstream sink_;
  std::string error_;
  std::ostream* progress_ = nullptr;
  std::vector<std::unique_ptr<Crawler>> workers_;
};

}  // namespace sim
