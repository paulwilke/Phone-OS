# Third-party material

This repository contains no third-party code. It uses two tools that are not part of it:

| Tool | Licence | Used for |
|---|---|---|
| [PlatformIO Core](https://platformio.org/) | Apache-2.0 | building and running the host tests (`pio test -e native`) |
| [Unity](https://github.com/ThrowTheSwitch/Unity) | MIT | the test framework; PlatformIO downloads it when the tests run and nothing of it is stored here |

The Python scripts in `scripts/` use only the standard library.

Anything that is copied, derived or linked into the repository later is added to this list in the same commit that adds it, with its licence.
