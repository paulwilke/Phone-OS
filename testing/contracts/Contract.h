#pragma once

// Shared helpers of the contract suites.
//
// A contract suite is a class template over a **rig**: a small default-constructible
// class that owns one implementation of the interface and offers the hooks the suite
// needs from outside (advancing time, scripting what the world does). The suite never
// names the implementation. Running it against a new implementation means writing a
// rig for it (see docs/hardware/hardware-capabilities.md) and one line in the test's main().
//
// A hook that sets up a situation returns bool. A rig that cannot create the
// situation (a real network cannot be told to refuse a call) returns false, and the
// test that needs it is reported as ignored instead of passing without testing anything.

#include <unity.h>

#define CONTRACT_NEEDS(hook) \
  do {                       \
    if (!(hook)) TEST_IGNORE_MESSAGE("the rig cannot create this situation"); \
  } while (0)
