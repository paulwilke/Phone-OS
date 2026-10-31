# Requirements for the UI testing framework

The framework is built in a repository of its own and used in this one through a configuration file; this document is what it has to do, written from the concept in [ui-testing.md](ui-testing.md) and the owner's decision [D-57](../decisions.md), which settled the golden format, the languages with pictures, the place of the frames, Python, the protocol and who approves a frame. Every requirement is one sentence and can be checked; "must" is a condition of acceptance, "should" is expected unless there is a stated reason. The numbers are stable: a requirement that is dropped keeps its number with the word "dropped".

The **system under test** is the simulator of this repository (`pio run -e sim`, [simulator.md](../simulator.md)), a console program that runs the firmware's screens on fakes; a **scenario** is a file of console commands; a **frame** is the 240 × 320 one-bit picture the simulated panel shows; a **golden frame** is a stored, approved frame.

## Inputs: the scenario format

- **R-1** The framework must run every file under `sim/scripts/` of this repository unchanged as a scenario, so the scenarios that exist are the first suite.
- **R-2** A scenario must be a UTF-8 text file of one console command per line, with `#` starting a comment except inside what is said or typed (`sms`, `type`, `echo`, `radio`), as the simulator reads it.
- **R-3** The framework must pass every command it does not handle itself to the system under test unchanged, so a new console command needs no change of the framework.
- **R-4** The framework must read a header comment `# data: <dir>` in the first lines of a scenario as the card the scenario starts from, as the simulator does.
- **R-5** The framework must read a header comment `# languages: <code> ...` as the languages the scenario runs in, overriding the suite's list for that scenario.
- **R-6** Key events must be the simulator's `key <name>...` with the names `up`, `down`, `select`, `enter`, `back`, `home`, `inbox`, `compose`.
- **R-7** Touch events must be the simulator's `tap <x> <y>` at a pixel of the 240 × 320 panel, and typed text its `type <text>`.
- **R-8** Waits must be the simulator's `advance <n>[ms|s|m|h]`, which moves the simulated clock; the framework must never wait on the wall clock for a scenario step.
- **R-9** Injected events must be the simulator's commands for the world: `sms`, `call`, `remote`, `battery`, `plug`, `unplug`, `signal`, `network`, `simcard`, `gps`, `motion`, `wifi`, `web`, `date`, `radio`, `sos`, `cellular`, `language`.
- **R-10** Assertions on screen text must be the simulator's `see <key> [name=value...]`, `see-not`, `say <text>` and `say-not <text>`.
- **R-11** Assertions on state must be the simulator's `check <what> <value>` and `fails <command>`.
- **R-12** An assertion on a full frame must be the simulator's `golden <name>`, which the framework handles itself: it takes the frame and compares it with the golden frame of that name for the scenario and the language of the run.
- **R-13** An assertion on a region must be written `golden <name> <x> <y> <w> <h>`, and the framework must compare only that rectangle.
- **R-14** The framework must apply the masks and the pixel budget the configuration names for a golden frame (R-54), and must apply none when the configuration names none.
- **R-15** The framework must treat an unknown command of its own namespace (a `golden` with a malformed rectangle) as a failing step with a message, never as a command for the system under test.
- **R-16** A scenario should be able to include another file with the simulator's `run <script>`, which the framework passes through and which counts as steps of the including scenario in the report.

## The interface to the system under test

- **R-17** The framework must start the system under test as a child process from the command and arguments the configuration gives (R-50), with the repository root as the working directory.
- **R-18** The framework must drive the child over its standard input and read its standard output and standard error, line by line, in the simulator's `--interactive --headless` mode.
- **R-19** The framework must take one result line per command, `ok` or `error: <message>`, as the end of that command's output (D-57), and must fall back to sending `echo <unique token>` after each command and reading until the token while the simulator prints no result line.
- **R-20** The framework must treat a result line `error: <message>`, or such a line before the token in the fallback, as that step failing, with the message.
- **R-21** The framework must obtain a frame by sending the simulator's `ascii <file>` with a file of its own under the output directory and reading that file after the result line, until the simulator offers a dump on standard output.
- **R-22** The framework must read a frame in the ASCII format of this repository: one line per row of the panel, `#` a black pixel and `.` a white one, every line of the same length, and must refuse a frame of another size than the configured one.
- **R-23** The framework must store golden frames in the ASCII format of R-22 (D-57), and should be able to read and write a binary PBM (`P4`) of the configured size behind the same setting (R-57), so a later move costs one line of the configuration.
- **R-24** The framework must start a fresh child process for every scenario and language, so no state leaks from one run into the next.
- **R-25** The framework must start a scenario in its language by sending `language <code>` before the first step, so the card keeps the language and a restart in the scenario brings the device back in it.
- **R-26** The framework must add `--data <dir>` from the scenario's header (R-4) or the suite's default card to the start command.
- **R-27** The framework must give every child process a work directory of its own under the output directory (`--work <dir>`), so two scenarios that run at the same time do not share a card.
- **R-28** The framework must give every child process a web port of its own (`--port <n>`) when scenarios run in parallel, so scenarios that use `web` do not collide.
- **R-29** The framework must kill a child process that does not answer a step within a configurable number of wall-clock seconds (default 60) and report that step as failed with the output so far.
- **R-30** The framework should support a second kind of target, a device adapter that sends the same commands to a flashed device over a serial console and reads frames from it, behind the same scenario format, without this being a condition for the first release.
- **R-31** The framework must not need a display, a window system or SDL: it runs on a plain Linux runner and on a laptop without a window.

## Outputs

- **R-32** The framework must write a JUnit XML report with one test case per scenario and language, named `<scenario>[<language>]`, with the failing step and its message in the failure element.
- **R-33** The framework must print a human summary to the terminal: every scenario and language with pass or fail, the failing step with its line number and message, and the totals.
- **R-34** The framework must write a Markdown summary of the same content to a file, so a CI job can attach it to the pull request.
- **R-35** For a failing frame the framework must write three PNG files, expected, actual and difference (differing pixels black on white), named after the scenario, step name and language, under the output directory.
- **R-36** For a failing frame the framework must also print the first differing rows as text (expected above actual, at most six) and the count of differing pixels, as `testing/Golden.h` does.
- **R-37** For a failing step of any kind the framework must include the last twenty lines of the device's log (the simulator's `log 20`) and its `state` summary in the report.
- **R-38** For a failing step the framework must print the one command that runs this scenario alone in this language, so a person reproduces it on a laptop.
- **R-39** A scenario must stop at its first failing step, and the suite must run every remaining scenario and language.
- **R-40** The exit status must be 0 when every scenario passed, 1 when at least one failed, 2 on a configuration or usage error, and 3 when the system under test could not be started or built.
- **R-41** The framework must write all output under one directory named in the configuration (default `out/ui-tests/`), which a clean checkout does not have, and must create it.
- **R-42** The framework should render every compared frame as a PNG into the output directory when asked (`--render`), so a reviewer can look at the suite's pictures without reading ASCII.

## Golden-frame management

- **R-43** Golden frames must live in this repository under `ui-tests/golden/` (D-57), the directory named in the configuration, in a subdirectory per language and per scenario: `<dir>/<language>/<scenario>/<name>.<ext>`.
- **R-44** A missing golden frame must fail the step and say the one command that creates it; the framework must never create a golden frame silently.
- **R-45** An approve command (`approve [scenario...] [--language <code>]`) must run the named scenarios and write their compared frames as golden frames, printing every file it created or changed.
- **R-46** The approve command must refuse to run when the environment variable `CI` is set, so a frame is only ever approved on a laptop and reviewed in a pull request.
- **R-47** The framework must write a golden frame byte for byte as it would read it back, so the file in the repository is stable and a diff shows only what changed.
- **R-48** The framework should write, next to the approve command's output, a before and after PNG of every changed golden frame, so the author can attach them to the pull request.
- **R-49** The framework must compare a golden frame exactly, pixel for pixel, when no mask and no pixel budget is configured for it.

## Configuration

- **R-50** The configuration must be one TOML file in this repository, `ui-tests/ui-tests.toml`, and the framework must take its path as an argument with that as the default.
- **R-51** The file must carry a `schema` integer, and the framework must refuse a schema it does not know with a message that names the versions it knows.
- **R-52** The file must name the system under test: a build command, a run command with its arguments, the panel's width and height, and the default card.
- **R-53** The file must list the languages the suite runs in and the languages in which golden frames are kept, English by default with German and French named per scenario where a layout is tight (D-57), and a scenario may restrict both.
- **R-54** The file must allow, per golden frame name, a list of masked rectangles and a pixel budget (a count of pixels that may differ), and the framework must apply them.
- **R-55** The file must list every scenario with its file, and may give it a name, a card, its languages, tags, and the languages in which its frames are kept.
- **R-56** The file must name the output directory, the JUnit file, the summary file and the golden directory.
- **R-57** The file must name the golden format, `ascii` (the setting of this repository, D-57) or `pbm`, and the framework must read and write that format.
- **R-58** The file must allow a `status` for the suite or a scenario, and the framework must report a scenario whose status is `not-runnable` as skipped, not as failed, with the status's reason.
- **R-59** The framework must validate the file before any run and name the line and key of a problem.
- **R-60** The framework must run a subset of the suite by scenario name, by tag or by language from the command line, so a person runs one scenario in one language.
- **R-61** The framework should take a `date` for the suite that it sends to the simulator before the first step, so the clock the status bar shows is fixed.

## Performance

- **R-62** The whole suite of this repository must run in under five minutes on a GitHub-hosted Linux runner, the simulator already built.
- **R-63** The framework must run scenarios in parallel, with the number of workers configurable and the default the number of processors, at most eight.
- **R-64** The framework should start a scenario in under one second beyond the simulator's own start.
- **R-65** Comparing a frame should take under ten milliseconds, so a scenario with a frame at every step does not change its duration noticeably.

## Portability

- **R-66** The framework must run on macOS and on Linux (Ubuntu as GitHub Actions ships it).
- **R-67** The framework must be written in Python 3.12 or newer with the standard library only (D-57).
- **R-68** The framework must need no GUI, display server, browser or SDL at any time.
- **R-69** The framework must install in one command on a fresh runner (`pip install` from a tag of its repository), and the CI of this repository must pin that tag.
- **R-70** The framework must write PNG files with the standard library alone (the simulator's `sim/Png.h` shows the stored-deflate way, and Python's `zlib` does the rest).

## Licensing

- **R-71** The framework must be published under the MIT licence or a licence compatible with it, and every dependency must be MIT, BSD, Apache 2.0 or ISC.
- **R-72** The framework must contain no proprietary component, no service call to a server, and no telemetry.
- **R-73** The repository of the framework must carry a `THIRD_PARTY` list like this one, naming every dependency with its licence.

## Non-goals

- **R-74** The framework does not flash, drive or read the real device in its first release (R-30 is a later door).
- **R-75** The framework does not test the HTML pages of this repository (the demo, the parents' page, the site), which keep their Node and Python tests.
- **R-76** The framework does not measure time, performance or power; the simulated clock is a counter.
- **R-77** The framework does not simulate the e-ink panel's ghosting or temperature behaviour.
- **R-78** The framework does not replace the host tests of the widgets and the screens (`pio test -e native`), which keep the pseudo-language and the clipping checks.
- **R-79** The framework does not generate scenarios, walk the interface or build the demo; `scripts/make_demo.py` keeps that.

## Example scenario

A scenario in the simulator's command language; the two header comments are the framework's, and the three `golden` lines are handled by it. The file runs unchanged with `.pio/build/sim/program --script`, where `golden` compares with `sim/golden/`.

```text
# data: sim/data
# languages: en de fr
# A text from a parent: the home screen shows it on the inbox tile, the inbox lists it with the sender's name.
advance 5s
check screen home
golden home
sms +491701111111 Hi Alex, call me
advance 2s
check unread 1
see home.inbox
golden home-unread 0 0 240 290
key inbox
check screen messages
say Parent One
say Hi Alex, call me
golden inbox
```

The region of `home-unread` leaves the soft keys out; a mask for the clock of the status bar is in the configuration, not in the scenario.

## Example configuration

The file of this repository, [ui-tests/ui-tests.toml](../../ui-tests/ui-tests.toml), is the example; its schema in short:

```toml
schema = 1

[suite]
name = "Phone OS screens"
status = "not-runnable"              # omit once the framework exists
reason = "waits for the framework repository (open point 47)"

[target]
build = ["pio", "run", "-e", "sim"]
run = [".pio/build/sim/program", "--interactive", "--headless", "--no-pseudo"]
width = 240
height = 320
card = "sim/data"
date = "2026-01-06 09:00"
timeout_seconds = 60

[languages]
run = ["en", "de", "fr"]
golden = ["en"]

[golden]
dir = "ui-tests/golden"
format = "ascii"                      # D-57; "pbm" is read too

[output]
dir = "out/ui-tests"
junit = "out/ui-tests/junit.xml"
summary = "out/ui-tests/summary.md"
workers = 0                           # 0: the processors, at most eight

[masks]
"status-clock" = { x = 0, y = 0, w = 60, h = 14 }

[[scenario]]
name = "smoke"
file = "sim/scripts/smoke.txt"
tags = ["smoke"]
golden_languages = ["en", "de"]
[scenario.frames."smoke-home"]
masks = ["status-clock"]
budget = 0
```
