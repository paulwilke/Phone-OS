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

// Ignoring a test with TEST_IGNORE leaves it by longjmp, which skips the destructors of its locals:
// the rig would be leaked. So the test is marked ignored here and then returns the ordinary way.
inline void contractIgnore(const char* message, UNITY_LINE_TYPE line) {
  UnityPrint(Unity.TestFile);  // the line Unity itself prints before a result: file:line:test:
  UNITY_OUTPUT_CHAR(':');
  UnityPrintNumberUnsigned(line);
  UNITY_OUTPUT_CHAR(':');
  UnityPrint(Unity.CurrentTestName);
  UNITY_OUTPUT_CHAR(':');
  UnityPrint("IGNORE: ");
  UnityPrint(message);
  Unity.CurrentTestIgnored = 1;
}

// Only for use directly in a test function that returns void.
#define CONTRACT_IGNORE_AND_RETURN(message) \
  do {                                      \
    contractIgnore((message), __LINE__);    \
    return;                                 \
  } while (0)

#define CONTRACT_NEEDS(hook) \
  do {                       \
    if (!(hook)) CONTRACT_IGNORE_AND_RETURN("the rig cannot create this situation"); \
  } while (0)
