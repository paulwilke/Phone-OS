# The call page

Code: `src/ui/screens/CallScreen.{h,cpp}`; tests in `test/test_screen_call/`. It follows the core's call state, so it is right however it was opened: from the phone page after a dial, or by the stack when a call rings.

| Phase | Title | Back is | Action |
|---|---|---|---|
| dialling | Calling ... | Hang up | |
| ringing | Incoming call | Reject | Answer |
| connected | In call | Hang up | the arrows are the volume |
| failed | Call failed, with the name and why | Back | |
| no call | No call | Back | |

**Who:** the name (a contact's, or the number, or "Unknown" when the caller sent none) in the large font. A number is never shortened: when it does not fit the large font it is drawn in the next smaller one, so every digit is there.

**A call that did not happen stays on the page until Back.** The core clears the call and tells why in the notice; the page shows "Call failed", the name and the reason (busy, no answer, no network, network allows SMS only, did not connect) for as long as that notice is up, then reads "No call". The page does not close itself: closing is the host's decision, and it leaves the failure to be read. A call that was only missed is no failure.

**Volume:** during a connected call, seven boxes, filled up to the level; the arrows change it and stop at both ends (a step that changes nothing is not "used"). Before the call is connected, or with a modem that cannot set the volume, there are no boxes and the arrows do nothing.

**Lock:** a call in progress keeps the device from locking (`preventsAutoLock`), proven with a real stack.

Back during dialling, ringing or a call hangs up or rejects and does not leave; Enter answers a ringing call.

## Tests

`test_screen_call`: every phase's words, name and buttons; answer, reject, hang up; a number in no book and a caller without one; the number in the next smaller font; the volume (boxes, both ends, none before the call); every failure reason in three languages; the failure going by with the notice; a missed call; touch on the buttons and the arrows in three languages; the lock; layout in four languages; pictures in English and German.
