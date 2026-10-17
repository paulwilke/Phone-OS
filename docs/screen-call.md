# The call page

Code: `src/ui/screens/CallScreen.{h,cpp}`; tests in `test/test_screen_call/`. It follows the core's call state, so it is right however it was opened: from the phone page after a dial, or by the stack when a call rings.

| Phase | Title | Back is | Action |
|---|---|---|---|
| dialling | Calling ... | Hang up, the whole bar | |
| ringing | Incoming call | Reject | Answer |
| connected | In call | Hang up, the whole bar | the volume has buttons on the page |
| failed | Call failed, with the name and why | Back | |
| no call | No call | Back | |

**Who:** the name (a contact's, or the number of a call target that is no contact) in the large font. A stranger or a caller without a number never gets this page: the call is hung up before anything shows ([calls.md](calls.md)). A number is never shortened: when it does not fit the large font it is drawn in the next smaller one, so every digit is there.

**A call that did not happen stays on the page until Back.** The core clears the call and tells why in the notice; the page shows "Call failed", the name and the reason (busy, no answer, no network, network allows SMS only, calls not ready, modem restarted, did not connect) for as long as that notice is up, then reads "No call". The page does not close itself: closing is the host's decision, and it leaves the failure to be read. A call that was only missed is no failure.

**Volume:** during a connected call, seven boxes, filled up to the level, and below them two buttons, **Quieter** and **Louder** (`call.quieter`, `call.louder`), which share the width of the page; a tap on one changes the volume, as do the keys Up and Down, and both stop at both ends (a step that changes nothing is not "used"). Before the call is connected, or with a modem that cannot set the volume, there are no boxes and no buttons. **Hang up is the whole key bar** (`SoftKeys::wide`), easy to hit (owner, 2026-10-08).

**Lock:** a call in progress keeps the device from locking (`preventsAutoLock`), proven with a real stack.

**The keys under the screen** are the page's while a call is dialled, rings or is connected (`wantsHardwareKeys`): they are touch keys, a cheek or a finger touches them by accident, and the stack would take the page away with nothing to bring it back during the call: no way to answer or to hang up. They do nothing on the page; after the call they are the stack's again. A call that rang through the lock leaves the device locked again when it is over ([ui-framework.md](ui-framework.md)).

Back during dialling, ringing or a call hangs up or rejects and does not leave; Enter answers a ringing call.

## Tests

`test_runtime` and the scenario `call-in-the-pocket`: the keys under the screen leave neither a ringing nor a connected call, and work again after it; a call through the lock locks again. `test_screen_call`: every phase's words, name and buttons; answer, reject, hang up; a number that is no contact, and a stranger and a hidden number that are not on the page at all; the number in the next smaller font; the volume (boxes, both ends, none before the call); every failure reason in three languages; the failure going by with the notice; a missed call; touch on the buttons, Hang up at the right end of the bar, Quieter and Louder found by their words in three languages; the lock; layout in four languages; pictures in English and German.
