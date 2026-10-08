# The options page

Code: `src/ui/screens/SettingsScreen.{h,cpp}` with the widget `src/ui/OptionRows.{h,cpp}`; tests in `test/test_screen_settings/` and `test/test_widgets/`. Each setting is a box with the question above and the answer below; Enter or a tap changes the one in focus.

| Box | Shown when | Enter does |
|---|---|---|
| Volume in calls, "4 of 7" | the modem has a volume control | one step up, 1 to 7 and round again; never silent, because a call that starts mute is a mistake and no wish |
| Message vibration, on / off | always | switches; switching it **on** buzzes once with the pattern of a message, so that whoever wants it feels it |
| Wi-Fi, on / off | `parentTools` and the core has Wi-Fi (the condition of the Wi-Fi tile) | opens the Wi-Fi page |
| Language, by its own name | always | opens the language page |

The values are read from the core when the page is drawn and nothing is kept: a volume changed by a call, or a Wi-Fi that switched itself off, shows at the next frame. Message vibration is held by the core, and the runtime writes it to `settings.toml` on the card, so it lasts ([runtime.md](runtime.md); the scenario `settings-page` checks the file and the restart).

**The focus** stops at both ends, like every list, and the page scrolls with it when a language or a device with more boxes needs it; it opens with the focus on the first box. A tap on a box moves the focus there and changes it.

**Layout.** The labels are fixed words in the small font and must fit the box (the tests find one that does not). The pseudo-language, 40 % longer than any real one, is held to the whole page; only the action word beside the arrows ("Change") may reach into the soft-key bar there, as on the other pages with an action word.

## Not ported

The previous page listed the school-time windows under the boxes. The status bar says "School time" while it runs and the lock page says until when, and the windows themselves are the parents' to read on their page, so the options do not repeat them.

## Tests

`test_screen_settings`: what the child and the parents are offered, a device without Wi-Fi, a modem without a volume control, the volume round, a silent volume going to one, the vibration and its single buzz, no save-mode box, the Wi-Fi and language boxes opening their pages, the language shown by its own name, the arrows, back, touch, the focus on open, layout in four languages, pictures in English and German. `test_widgets`: the option rows (ends, touch, scrolling, buttons, the focused box, the mark of the current one).
