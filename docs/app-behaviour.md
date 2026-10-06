# Behaviour of the application core

This is the inventory of what the application does, taken from the previous code base: the one class that did everything (`EmergencyApp`), its test suite, the application behaviour exercised by the screen tests, and the Wi-Fi controller with its tests. Every row is one observable behaviour, the component that owns it, and the test that pins it. **Every previous test case appears** in the column *Previous* (`EA:` is the application test, `SC:` the screen tests, `WF:` the Wi-Fi tests), or in the list of what is not ported at the end, with the reason.

**One rule stands above the tables:** only the contact book may write to the device and call it (`Profile::mayReceiveFrom`, [contacts.md](contacts.md)); the rows PRO-09, PRO-10, MSG-46 to 57 and CAL-39 to 49 pin it, and every other row about incoming texts and calls is about an allowed number.

The application core is split into small components with one owner each, composed by a thin facade (`AppCore`). The core contains **no sentence**: it produces notices and system entries as codes with arguments (`src/app/Notice.h`, `src/app/SystemEntry.h`), and the interface words them from the catalogs when it draws. The only text the core produces is the body of an SMS that leaves the device, and that wording also comes from the catalogs.

| Component | Owns |
|---|---|
| Profile | the settings the behaviour depends on, as a plain struct; no parsing |
| Notices | the notice line: one code with arguments and an expiry |
| Positioning | who wants the receiver on, as independent reasons; the last known fix |
| Messaging | incoming SMS and radio messages, routing, the queue, delivery reports, send failures, deletion |
| Calls | dialling and answering rules, school-time rejection, failed-call reasons, volume |
| Emergency | the SOS flow, the SOS ping and the reply to a remote query, one search at a time |
| Connectivity | cellular on and off, save mode, the cell probe, radio start and restart requests, Wi-Fi |
| WifiController | Wi-Fi modes, the idle switch-off, the access point password |
| PowerCoordination | the power policy applied to modem, radio chip and processor; the front light |
| AppCore | the facade: owns the components, `poll()`, the entry points the interface calls, the read-only state |

## How to read the tables

*Test* is the name of the test function in the test directory of the component. *Status* is `open` until the test exists and passes, then `done`. `scripts/check_behaviour.py` checks that every row marked `done` has its test function in the test file, so a row cannot be ticked without a test; with `--complete` it also refuses an `open` row. Rows whose *Previous* column is a dash are behaviours the previous code had but never tested, or that the rewrite makes explicit.


## Profile

Tests in `test/test_profile/`.

| Id | Behaviour | Test | Previous | Status |
|---|---|---|---|---|
| PRO-01 | The profile holds every setting the behaviour depends on, with defaults that match the previous defaults: message vibration on, call vibration on, call vibration in school time off, modem sleep allowed, remote-query notice 'history', no radio, no free text | `test_defaults_match_the_defaults` | — | done |
| PRO-02 | A quiet-hour window that cannot be read is refused with a reason (the profile reports it, nothing is logged by the core) | `test_an_unreadable_quiet_hour_window_is_refused_with_a_reason` | EA:test_unreadable_quiet_hours_are_reported | done |
| PRO-03 | Quiet hours are evaluated on the clock's epoch and time zone; without a known time there is no lock | `test_school_time_needs_a_known_time` | EA:test_no_lockout_without_known_time | done |
| PRO-04 | The first call contact of the home actions is the SOS recipient when no target is given; with none, there is no recipient | `test_the_sos_recipient_is_the_target_or_the_first_call_contact` | SC:test_without_a_contact_there_is_no_sos_tile | done |
| PRO-05 | The core's log applies the profile's log level at every line, also after the level changed, and never drops a line of the always class | `test_the_log_applies_the_log_level_of_the_profile_but_never_drops_what_must_stay` | — | done |
| PRO-06 | A level that changes while the core runs is followed | `test_the_log_follows_a_level_that_changes_while_the_core_runs` | — | done |
| PRO-07 | A call target is compared with the country code of the contact book | `test_a_call_target_is_compared_with_the_country_code_of_the_book` | — | done |
| PRO-08 | A number may be called when it is in the contact book (any spelling) or a call target of the home actions | `test_who_may_be_called` | EA:test_calls_only_to_known_numbers | done |
| PRO-09 | A number may reach the child (SMS or call) exactly when it is in the contact book in any spelling or is a call target; a name, a short code and nothing are refused | `test_who_may_reach_the_child` | — | done |
| PRO-10 | Receiving and calling are one notion: for every number `mayReceiveFrom` and `mayCall` agree | `test_receiving_and_calling_are_one_notion` | — | done |

## Notices

Tests in `test/test_notices/`.

| Id | Behaviour | Test | Previous | Status |
|---|---|---|---|---|
| NOT-01 | There is at most one notice; a newer one replaces the older | `test_a_newer_notice_replaces_the_older` | — | done |
| NOT-02 | A notice expires after kNoticeSeconds (8) of uptime and reads as none afterwards | `test_a_notice_expires` | EA:test_alert_expires | done |
| NOT-03 | A notice can be cleared at once | `test_a_notice_can_be_cleared` | — | done |
| NOT-04 | A notice carries its kind and values, and a newer one forgets the values of the older | `test_a_notice_carries_its_values_and_the_next_one_forgets_them` | — | done |
| NOT-05 | Every notice kind can be set and read back unchanged | `test_every_kind_can_be_shown` | — | done |

## Positioning

Tests in `test/test_positioning/`.

| Id | Behaviour | Test | Previous | Status |
|---|---|---|---|---|
| POS-01 | The receiver is off without a request | `test_the_receiver_is_off_without_a_request` | EA:test_gnss_is_off_without_a_request | done |
| POS-02 | The receiver has independent reasons (start-up fix, SOS choice, SOS ping, remote query, position page, test page); it is on while any holds and one cannot switch off another's | `test_reasons_are_independent` | SC:test_a_running_command_keeps_the_receiver, EA:test_gps_page_keeps_the_receiver_after_the_startup_fix_ends | done |
| POS-03 | A reason that is released switches the receiver off when it was the last | `test_the_last_released_reason_switches_the_receiver_off` | SC:test_gnss_screen_powers_the_receiver_while_open, SC:test_locking_switches_the_receiver_off | done |
| POS-04 | Power is switched through the Power capability, once per change | `test_power_is_switched_once_per_change` | EA:test_locating_switches_gnss_power | done |
| POS-05 | Each reason alone holds the receiver and no other reason is set by it | `test_every_reason_alone_holds_the_receiver` | — | done |
| POS-06 | Holding a reason twice is holding it once; releasing one that is not held changes nothing | `test_holding_a_reason_twice_is_holding_it_once` | — | done |
| POS-07 | A power switch that fails is logged and tried again at the next poll, in both directions | `test_a_switch_that_fails_is_logged_and_tried_again_at_the_next_poll` | — | done |
| POS-08 | The position page keeps the receiver after the start-up fix ends | `test_the_position_page_keeps_the_receiver_after_the_startup_fix_ends` | EA:test_gps_page_keeps_the_receiver_after_the_startup_fix_ends | done |
| POS-09 | An HDOP of 5.0 is usable, 5.1 is not | `test_the_hdop_limit_is_inclusive` | — | done |
| POS-10 | The receiver answers the SOS ping only while it runs | `test_the_receiver_answers_for_the_sos_ping_only_while_it_runs` | — | done |
| POS-11 | The start-up fix ends on the first usable fix (HDOP up to 5) and keeps it | `test_the_startup_fix_ends_on_a_usable_fix` | EA:test_startup_fix_runs_once_then_switches_off | done |
| POS-12 | An inaccurate fix is taken after a minute | `test_an_inaccurate_fix_is_taken_after_a_minute` | EA:test_startup_fix_waits_for_accuracy_but_not_forever | done |
| POS-13 | The start-up fix gives up after three minutes and there is no last fix | `test_the_startup_fix_gives_up_after_three_minutes` | EA:test_startup_fix_gives_up_after_three_minutes | done |
| POS-14 | The last valid fix is kept with its age, whoever asked for the receiver | `test_the_last_fix_keeps_its_age_whoever_asked_for_the_receiver` | EA:test_startup_fix_runs_once_then_switches_off | done |
| POS-15 | A fix from before the last switch-on is never used | `test_a_fix_from_before_the_last_switch_on_is_not_used` | — | done |
| POS-16 | A start-up fix runs out its time even if the receiver cannot be switched on | `test_a_start_up_fix_runs_out_its_time_even_if_the_receiver_cannot_be_switched_on` | — | done |
| POS-17 | Other reasons never end or log a start-up fix that was not requested | `test_the_end_of_a_startup_fix_is_not_claimed_by_other_reasons` | — | done |
| POS-18 | The start-up fix is logged when it starts and when it ends | `test_the_startup_fix_is_logged` | — | done |

## SchoolTime

Tests in `test/test_school_time/`.

| Id | Behaviour | Test | Previous | Status |
|---|---|---|---|---|
| SCH-01 | School time is the profile's windows on the clock; the end minute itself is free | `test_school_time_follows_the_profile_and_the_clock` | — | done |
| SCH-02 | Without a known time there is no school time and nothing to lift | `test_no_school_time_without_a_known_time` | EA:test_no_lockout_without_known_time | done |
| SCH-03 | An emergency lifts the lock until exactly the end of the running window | `test_lifting_ends_exactly_at_the_end_of_the_window` | SC:test_sos_in_school_time_needs_five_taps_and_lifts_the_lock | done |
| SCH-04 | The end is counted from the second, not the minute | `test_the_end_is_counted_from_the_second_not_the_minute` | — | done |
| SCH-05 | A lift does not reach a window that begins where it ends | `test_a_lift_does_not_reach_a_window_that_begins_where_it_ends` | — | done |
| SCH-06 | A lift does not reach the next window | `test_a_lift_does_not_reach_the_next_window` | — | done |
| SCH-07 | A window across midnight is lifted until its end | `test_a_window_across_midnight_is_lifted_until_its_end` | — | done |
| SCH-08 | Lifting when no school time runs changes nothing | `test_lifting_when_no_school_time_runs_changes_nothing` | — | done |
| SCH-09 | The lift is logged on every log level | `test_lifting_is_logged_and_never_dropped_from_the_log` | SC:test_sos_in_school_time_needs_five_taps_and_lifts_the_lock | done |
| SCH-10 | The windows may change while the device runs | `test_the_profile_may_change_while_the_device_runs` | — | done |

## Messaging

Tests in `test/test_messaging/`.

| Id | Behaviour | Test | Previous | Status |
|---|---|---|---|---|
| MSG-01 | An incoming SMS is stored unread with the sender, and counts as unread | `test_an_incoming_sms_is_stored` | EA:test_incoming_sms_is_stored | done |
| MSG-02 | A remote query is recognised before storing: it is never stored as a message | `test_a_remote_query_is_not_stored_as_a_message` | EA:test_command_sms_is_not_stored_but_logged_and_noticed | done |
| MSG-03 | A remote query from a parent is logged, written as a history entry (kind location-queried) and, when the profile says notify, shown as a notice | `test_a_remote_query_is_logged_recorded_and_noticed_as_the_profile_says` | EA:test_command_sms_is_not_stored_but_logged_and_noticed, SC:test_status_screen_cycles_command_notice | done |
| MSG-04 | A remote query from a number that is not a parent is refused, logged and recorded (kind query-refused); no reply, no receiver | `test_a_refused_remote_query_is_recorded` | EA:test_unauthorized_command_is_logged_and_rejected | done |
| MSG-05 | A remote query never vibrates | `test_a_remote_query_does_not_vibrate` | EA:test_remote_command_does_not_vibrate | done |
| MSG-06 | An incoming SMS vibrates with the message pattern | `test_an_incoming_sms_vibrates_the_message_pattern` | EA:test_incoming_message_vibrates_twice_short | done |
| MSG-07 | Message vibration can be switched off | `test_message_vibration_can_be_switched_off` | EA:test_message_vibration_can_be_switched_off | done |
| MSG-08 | In school time only a parent's message vibrates; the message is stored either way | `test_in_school_time_only_parents_vibrate` | EA:test_message_vibration_in_school_time_only_for_parents | done |
| MSG-09 | An incoming SMS counts as the modem being awake | `test_an_incoming_sms_means_the_modem_is_awake` | EA:test_incoming_sms_counts_as_awake | done |
| MSG-10 | A message is sent by cellular when registered | `test_a_message_goes_by_cellular` | EA:test_send_text_over_network | done |
| MSG-11 | Cellular has priority over the radio | `test_cellular_has_priority_over_the_radio` | EA:test_cellular_has_priority_over_mesh | done |
| MSG-12 | Without cellular the radio carries it, with no phone number, to the radio's own contact, and nothing waits; the entry says radio | `test_the_radio_takes_over_when_cellular_is_gone` | EA:test_mesh_takes_over_when_cellular_is_gone | done |
| MSG-13 | With cellular switched off the radio carries messages and calls are refused | `test_cellular_off_uses_the_radio` | EA:test_cellular_off_uses_mesh_and_blocks_calls | done |
| MSG-14 | Without a path the message is queued, shown as waiting, and a notice says so | `test_without_a_path_a_message_is_queued` | EA:test_send_without_network_is_queued_and_sent_later | done |
| MSG-15 | A queued message goes out when the network returns; the waiting notice is replaced by 'message is out' | `test_a_queued_message_goes_out_when_the_network_returns` | EA:test_send_without_network_is_queued_and_sent_later | done |
| MSG-16 | A queued message goes out by radio when the radio comes up, without cellular, and the entry becomes a radio entry | `test_a_queued_message_goes_out_by_radio` | EA:test_without_cellular_and_without_mesh_it_waits | done |
| MSG-17 | The queue holds kOutboxSize (8); one more is refused with a notice | `test_the_queue_is_bounded` | EA:test_queue_is_bounded | done |
| MSG-18 | A message waiting without any path does not keep the modem awake | `test_a_message_without_any_path_does_not_keep_the_modem_awake` | EA:test_waiting_message_without_any_path_does_not_block_sleep | done |
| MSG-19 | A waiting message with a path counts as pending work | `test_a_message_with_a_path_is_pending_work` | EA:test_pending_message_wakes_the_modem | done |
| MSG-20 | Without radio attached everything works as before | `test_without_a_radio_everything_works_as_before` | EA:test_without_mesh_attached_everything_works_as_before | done |
| MSG-21 | A profile without the radio never sends by radio | `test_a_profile_without_the_radio_never_sends_by_radio` | EA:test_persona_without_mesh_never_uses_it | done |
| MSG-22 | A profile without the radio ignores incoming radio messages | `test_a_profile_without_the_radio_ignores_incoming_radio_messages` | EA:test_persona_without_mesh_ignores_incoming_mesh | done |
| MSG-23 | An incoming radio message is stored with the radio contact's name | `test_an_incoming_radio_message_is_stored` | EA:test_incoming_mesh_message_lands_in_the_history | done |
| MSG-24 | An explicit radio send goes to the named radio contact and is not routed through cellular; refused with a notice when the radio is not usable | `test_an_explicit_radio_send` | — | done |
| MSG-25 | A radio message is marked confirmed when the radio says it arrived, and logged | `test_a_radio_message_is_marked_confirmed` | EA:test_mesh_message_is_marked_confirmed | done |
| MSG-26 | A radio message without confirmation is marked so and a notice says so | `test_a_radio_message_without_confirmation_says_so` | EA:test_mesh_message_without_confirmation_says_so | done |
| MSG-27 | Delivery reports are matched to messages in the order of sending | `test_two_radio_messages_keep_their_order` | EA:test_two_mesh_messages_keep_their_order | done |
| MSG-28 | Only a few reports are awaited: the oldest is forgotten and stays unconfirmed | `test_the_oldest_awaited_report_is_forgotten` | — | done |
| MSG-29 | An SMS has no delivery state | `test_an_sms_has_no_delivery_state` | EA:test_sms_has_no_delivery_state | done |
| MSG-30 | A message the modem refuses after accepting it is logged with the modem's reason, written as a history entry (kind sms-rejected), noticed, vibrates the failure pattern and is reported to a running SOS ping | `test_a_modem_send_failure_is_recorded_noticed_and_reported` | — | done |
| MSG-31 | A message is deleted from the history by its number | `test_a_message_can_be_deleted` | — | done |
| MSG-32 | Empty recipient or empty text is refused | `test_empty_recipient_or_text_is_refused` | — | done |
| MSG-33 | Sending writes a log line with the path | `test_sending_is_logged` | — | done |
| MSG-34 | After school time everybody's message vibrates again | `test_after_school_time_everybody_vibrates_again` | — | done |
| MSG-35 | A remote query that finds a search running is logged as busy and still recorded | `test_a_remote_query_while_a_search_runs_is_logged_as_busy` | — | done |
| MSG-36 | The queue goes out in the order it was filled | `test_the_queue_goes_out_in_order_and_keeps_what_cannot_leave_yet` | — | done |
| MSG-37 | A delivery report nobody waits for is ignored | `test_a_report_nobody_waits_for_is_ignored` | — | done |
| MSG-38 | Deleting a waiting message does not cancel its sending | `test_deleting_a_waiting_message_does_not_cancel_it` | — | done |
| MSG-39 | A waiting message pushed out of the history by newer ones is still sent | `test_a_message_that_left_the_history_while_waiting_is_still_sent` | — | done |
| MSG-40 | A message the modem refuses stays in the queue while the others go, and goes at the next poll | `test_a_message_the_modem_refuses_stays_in_the_queue_while_the_others_go` | — | done |
| MSG-41 | A text longer than the modem takes is refused instead of waiting for ever | `test_a_text_longer_than_the_modem_takes_is_refused_instead_of_waiting_for_ever` | — | done |
| MSG-42 | An empty radio text is refused without a notice | `test_an_empty_radio_text_is_refused_quietly` | — | done |
| MSG-43 | A queued message keeps its full text although the history keeps 160 bytes | `test_a_long_queued_message_keeps_its_full_text` | — | done |
| MSG-44 | The history text is cut on a character boundary | `test_the_history_text_is_cut_on_a_character_boundary` | — | done |
| MSG-45 | The failure of a reply the Emergency hook calls silent stays out of the child's sight: logged only | `test_the_failure_of_a_silent_reply_stays_out_of_the_childs_sight` | — | done |
| MSG-46 | An SMS from a number that is not in the contact book is dropped without a trace: not stored, not unread, no vibration, no notice, no reply | `test_an_sms_from_a_stranger_is_dropped_without_a_trace` | — | done |
| MSG-47 | The drop is logged with the number and never the text, on an always-kept line | `test_the_drop_is_logged_with_the_number_and_never_the_text` | — | done |
| MSG-48 | A stranger in another spelling is logged in the canonical form | `test_a_stranger_in_another_spelling_is_logged_in_the_canonical_form` | — | done |
| MSG-49 | A sender that is a name, a short code or empty is dropped and logged as a non-numeric sender | `test_a_sender_that_is_no_number_is_dropped` | — | done |
| MSG-50 | An empty text from a stranger is dropped | `test_an_empty_text_from_a_stranger_is_dropped` | — | done |
| MSG-51 | An empty text from a contact is stored as before | `test_an_empty_text_from_a_contact_is_stored_as_before` | — | done |
| MSG-52 | Every spelling of a contact's number is let through | `test_every_spelling_of_a_contact_is_let_through` | — | done |
| MSG-53 | The target of a call action may write too | `test_the_target_of_a_call_action_may_write` | — | done |
| MSG-54 | A stranger in school time leaves nothing either | `test_a_stranger_in_school_time_leaves_nothing_either` | — | done |
| MSG-55 | A stranger cannot use the remote query: no answer, no refusal entry, no notice | `test_a_stranger_cannot_use_the_remote_query` | — | done |
| MSG-56 | A flood of 100 texts from a stranger leaves the history, the unread count and the vibration untouched | `test_a_flood_from_a_stranger_leaves_the_history_untouched` | — | done |
| MSG-57 | A dropped SMS still proves the modem awake | `test_a_dropped_sms_still_proves_the_modem_awake` | — | done |

## Calls

Tests in `test/test_calls/`.

| Id | Behaviour | Test | Previous | Status |
|---|---|---|---|---|
| CAL-01 | A child may only call allowed numbers; others get a 'not allowed' notice and nothing is dialled | `test_only_allowed_numbers_are_dialled` | EA:test_calls_only_to_known_numbers | done |
| CAL-02 | A number in another spelling, and a listed contact who is not a parent, may be called | `test_a_number_in_another_spelling_and_a_listed_non_parent_may_be_called` | EA:test_calls_only_to_known_numbers | done |
| CAL-03 | The rules apply in a fixed order: school time, the number, cellular power, the network | `test_the_rules_are_applied_in_this_order` | — | done |
| CAL-04 | Dialling is refused in school time with 'school time until HH:MM' | `test_dialling_is_refused_in_school_time` | EA:test_child_cannot_call_during_school_time, SC:test_tapping_call_tile_in_school_time_stays_home_and_says_why | done |
| CAL-05 | Dialling works outside school time | `test_dialling_works_outside_school_time` | EA:test_calling_works_outside_school_time | done |
| CAL-06 | Dialling is refused with cellular switched off | `test_dialling_is_refused_with_cellular_off` | EA:test_cellular_off_uses_mesh_and_blocks_calls | done |
| CAL-07 | Dialling is refused without a network | `test_dialling_is_refused_without_a_network` | EA:test_call_without_network_fails_with_alert | done |
| CAL-08 | A dial the modem refuses leaves no call behind | `test_a_refused_dial_leaves_no_call` | — | done |
| CAL-09 | An incoming call names the caller as the current peer | `test_an_incoming_call_names_the_peer` | EA:test_incoming_call_and_missed_call_note | done |
| CAL-10 | A missed call is a history entry (kind missed-call, peer the caller) and clears the peer, so a callback is possible | `test_a_missed_call_is_recorded` | EA:test_incoming_call_and_missed_call_note, SC:test_missed_call_offers_callback | done |
| CAL-11 | Answering and hanging up go to the modem; hanging up clears the peer | `test_answer_and_hang_up` | SC:test_incoming_call_wakes_and_shows_caller | done |
| CAL-12 | A ringing call vibrates the ring pattern every two seconds until it stops, and not at all while nothing rings | `test_ringing_vibrates_until_it_stops` | EA:test_ringing_vibrates_until_it_stops, EA:test_no_vibration_while_nothing_happens, EA:test_ring_uses_the_ring_pattern | done |
| CAL-13 | Call vibration can be switched off | `test_call_vibration_can_be_switched_off` | EA:test_call_vibration_settings_and_the_priority_of_school_time | done |
| CAL-14 | Every incoming call in school time, parents included, is rejected without ringing; the notice names a known caller | `test_every_call_in_school_time_is_rejected_silently_and_named` | EA:test_every_call_in_school_time_is_rejected_silently_and_named | done |
| CAL-15 | A call rejected in school time is in the history as a missed call, so the parents see who tried | `test_a_rejected_call_is_in_the_history_as_missed` | — | done |
| CAL-16 | After school time a call rings and is not rejected | `test_a_call_after_school_time_rings_and_is_not_rejected` | — | done |
| CAL-17 | An unknown caller in school time is rejected silently: no notice, no entry (see CAL-39) | `test_a_stranger_is_rejected_in_school_time_too_and_without_a_notice` | — | done |
| CAL-18 | Vibration for calls in school time is a setting that defaults to off; switched on it vibrates and still rejects | `test_call_vibration_in_school_time_is_a_setting` | EA:test_call_vibration_settings_and_the_priority_of_school_time | done |
| CAL-19 | A contact's call without a number yet is neither rung nor rejected until the number arrives or three seconds pass | `test_a_call_without_a_number_yet_is_neither_rung_nor_rejected` | EA:test_call_without_number_yet_is_neither_rung_nor_rejected | done |
| CAL-20 | A call that never shows a number is rejected after three seconds, silently, in school time (the same as at any time: CAL-43) | `test_a_call_without_a_number_is_rejected_after_three_seconds` | EA:test_call_without_a_number_is_rejected_after_a_short_wait_in_school_time | done |
| CAL-21 | The modem reports an incoming call twice; it is rejected once | `test_an_incoming_call_is_rejected_once` | — | done |
| CAL-22 | A busy line is reported with the callee's name, vibrates the failure pattern and is recorded (call-failed busy) | `test_a_busy_line_is_reported` | EA:test_failed_call_says_why_busy, EA:test_failed_call_vibrates_the_failure_pattern | done |
| CAL-23 | No answer is reported | `test_no_answer_is_reported` | EA:test_failed_call_says_no_answer | done |
| CAL-24 | A dial failure on an SMS-only network says so | `test_a_dial_failure_on_an_sms_only_network_says_so` | EA:test_dial_failure_on_sms_only_network_names_it | done |
| CAL-25 | A dial failure without a network says so | `test_a_dial_failure_without_a_network_says_so` | EA:test_dial_failure_without_network_says_no_network | done |
| CAL-26 | A dial failure otherwise is still reported | `test_a_dial_failure_is_still_reported` | EA:test_dial_failure_otherwise_is_still_reported | done |
| CAL-27 | A call that never connected and ended is a failure, a connected one is not, one the child hung up is not | `test_an_unconnected_end_is_a_failure` | EA:test_unconnected_end_counts_as_failure_but_normal_end_does_not | done |
| CAL-28 | A failure event without a number names the number that was dialled | `test_a_failure_without_a_number_in_the_event_names_the_number_dialled` | — | done |
| CAL-29 | A ring that begins at uptime zero is timed from zero | `test_a_ring_at_uptime_zero_is_timed_from_zero` | — | done |
| CAL-30 | A ringing call without a number does not vibrate, whatever the school-time vibration setting says: nothing rings before the caller is known | `test_a_ringing_call_without_a_number_does_not_vibrate_even_when_school_time_would_allow_it` | — | done |
| CAL-31 | Two calls in a row in school time are both rejected | `test_two_calls_in_a_row_in_school_time_are_both_rejected` | — | done |
| CAL-32 | After a failed call the state is clean: a second call works and its normal end reports nothing | `test_the_state_after_a_failed_call_is_clean` | — | done |
| CAL-33 | The modem's own reason goes to the log with the failure | `test_the_modems_reason_is_logged` | — | done |
| CAL-34 | Calls are logged: incoming, connected, ended | `test_calls_are_logged` | — | done |
| CAL-35 | Volume changes only during a call and stops at 0 and 7 | `test_volume_only_during_a_call_and_within_range` | EA:test_volume_only_during_a_call, EA:test_volume_stops_at_the_ends, SC:test_call_screen_changes_volume_with_w_and_s, SC:test_no_volume_outside_a_call | done |
| CAL-36 | A modem that cannot set the volume reports so | `test_a_modem_without_volume_support` | — | done |
| CAL-37 | The default volume is set through the modem | `test_the_default_volume_is_set` | SC:test_options_toggle_message_vibration_and_volume | done |
| CAL-38 | A call counts as active while dialling, ringing or connected | `test_a_call_is_active_while_dialling_ringing_or_connected` | SC:test_no_autolock_during_call | done |
| CAL-39 | A call from a number that is not in the contact book is hung up at once: no ring, no vibration, no notice, no history entry, no peer, and the device shows no call | `test_a_stranger_is_hung_up_at_once_without_a_trace` | — | done |
| CAL-40 | The rejection of a stranger is one log line with the number, on an always-kept line; the number withheld likewise | `test_the_rejection_of_a_stranger_is_one_log_line_with_the_number` | — | done |
| CAL-41 | A stranger does not vibrate even when school-time vibration is on | `test_a_stranger_does_not_vibrate_even_when_school_time_vibration_is_on` | — | done |
| CAL-42 | A stranger whose number arrives late is rejected when it arrives; nothing rang meanwhile | `test_a_stranger_who_is_announced_late_is_rejected_when_the_number_arrives` | — | done |
| CAL-43 | A hidden number is rejected after three seconds at any time of day | `test_a_hidden_number_is_rejected_after_three_seconds_at_any_time_of_day` | — | done |
| CAL-44 | A hidden number is rejected once and leaves no missed call | `test_a_hidden_number_is_rejected_once_and_leaves_no_missed_call` | — | done |
| CAL-45 | A contact whose number arrives late, in any spelling, rings from then on | `test_a_contact_whose_number_arrives_late_rings_from_then_on` | — | done |
| CAL-46 | The target of a call action may call the child | `test_the_target_of_a_call_action_may_call` | — | done |
| CAL-47 | A stranger calling again is rejected again | `test_a_stranger_calling_again_is_rejected_again` | — | done |
| CAL-48 | A stranger does not disturb the call of a contact | `test_a_stranger_does_not_disturb_the_calls_of_contacts` | — | done |
| CAL-49 | A call the child dialled is not hidden by the rule | `test_a_call_we_dialled_is_not_hidden_by_the_rule` | — | done |

## Emergency

Tests in `test/test_emergency/`.

| Id | Behaviour | Test | Previous | Status |
|---|---|---|---|---|
| EME-01 | The SOS recipient is the target or the first call contact; without one SOS cannot begin | `test_without_a_recipient_sos_cannot_begin` | SC:test_without_a_contact_there_is_no_sos_tile | done |
| EME-02 | SOS works in school time | `test_sos_works_in_school_time` | EA:test_sos_works_during_school_time | done |
| EME-03 | Pressing SOS starts the receiver at once, before any choice | `test_pressing_sos_starts_the_receiver_at_once` | SC:test_sos_tile_starts_the_ping_and_shows_progress | done |
| EME-04 | Choosing emergency or pickup starts the ping to the recipient; the ping holds the receiver | `test_choosing_starts_the_ping` | SC:test_sos_tile_starts_the_ping_and_shows_progress | done |
| EME-05 | The ping sends the position after the minimum time, to the first contact, with the coordinates | `test_the_ping_sends_the_position` | SC:test_sos_ping_sends_the_position_after_waiting | done |
| EME-06 | The receiver is off again after the ping ends | `test_the_receiver_is_off_after_the_ping` | SC:test_sos_ping_sends_the_position_after_waiting | done |
| EME-07 | Leaving the choice sends nothing and releases the receiver | `test_leaving_the_choice_sends_nothing` | SC:test_sos_can_be_cancelled | done |
| EME-08 | Without a choice the emergency goes out after 40 seconds | `test_the_emergency_goes_out_by_itself_after_40_seconds` | SC:test_sos_goes_out_by_itself_after_the_countdown | done |
| EME-09 | Pickup sends the other text | `test_pickup_sends_the_other_text` | SC:test_sos_pickup_sends_the_other_text | done |
| EME-10 | In school time five taps send the emergency and lift the school-time lock to the end of the current window; fewer do nothing | `test_five_taps_in_school_time_lift_the_lock` | SC:test_sos_in_school_time_needs_five_taps_and_lifts_the_lock | done |
| EME-11 | The lift is logged and ends with the school time | `test_the_lift_is_logged_and_ends_with_the_school_time` | SC:test_sos_in_school_time_needs_five_taps_and_lifts_the_lock | done |
| EME-12 | Leaving the school-time warning has no effect, the lock stays | `test_leaving_the_school_time_warning_has_no_effect` | SC:test_sos_warning_in_school_time_can_be_left_without_effect | done |
| EME-13 | The outcome of a ping is told once by haptic: sent three short, failed short-long | `test_the_outcome_is_vibrated_once` | EA:test_sos_done_vibrates_three_short_once | done |
| EME-14 | A remote query is answered like an SOS ping: receiver on, wait for a good fix, one SMS to the asker with coordinates and no alarm word, receiver off, the child notices nothing, the log says sent | `test_a_remote_query_is_answered_like_an_sos_ping` | EA:test_where_are_you_is_answered_like_an_sos_ping_without_a_link_and_silently | done |
| EME-15 | A remote query never vibrates, also at its end | `test_a_remote_query_never_vibrates` | EA:test_where_are_you_is_answered_like_an_sos_ping_without_a_link_and_silently | done |
| EME-16 | One search at a time: a second query while one runs is answered busy | `test_a_second_query_is_busy` | — | done |
| EME-17 | An emergency replaces a running query reply | `test_an_emergency_replaces_a_query_reply` | — | done |
| EME-18 | A remote query while an SOS runs is answered busy | `test_a_query_during_an_sos_is_busy` | — | done |
| EME-19 | The SMS wording is in the SMS language of the profile, whatever the interface language | `test_the_sms_is_worded_in_the_sms_language` | — | done |
| EME-20 | The child's name and the map template of the profile go into the text | `test_the_name_and_the_map_template_go_into_the_text` | — | done |
| EME-21 | The host's leave() releases the receiver when the screen closes | `test_leave_releases_the_receiver` | — | done |
| EME-22 | Without an explicit target the first call target is the recipient | `test_the_first_call_target_is_the_default_recipient` | — | done |
| EME-23 | A second press keeps the countdown, and a press while a ping runs changes nothing | `test_pressing_sos_again_keeps_the_countdown_and_a_running_ping` | — | done |
| EME-24 | A text the network refuses after the modem took it is vibrated as failed once, not twice | `test_a_text_the_network_refuses_is_vibrated_as_failed_once` | — | done |
| EME-25 | A text that cannot even be queued fails the ping, vibrates once and releases the receiver | `test_a_text_that_cannot_even_be_queued_is_vibrated_as_failed` | — | done |
| EME-26 | Without any position a text with the waiting time goes out after the timeout | `test_without_a_position_a_text_goes_out_anyway` | — | done |
| EME-27 | The failure of a query reply is logged and written once to the history as `location-query-failed` naming the parent who asked; the child gets no notice, no vibration and no entry about the refused text | `test_the_failure_of_a_query_reply_is_logged_and_told_in_the_history_but_not_to_the_child` | — | done |
| EME-28 | A query while the SOS choice is open is answered and the choice keeps its receiver | `test_a_query_while_the_choice_is_open_is_answered_and_the_choice_stays` | — | done |
| EME-29 | An unknown SMS language falls back to English | `test_an_unknown_sms_language_falls_back_to_english` | — | done |
| EME-30 | A name changed in the profile is used at the next press | `test_a_changed_name_is_used_at_the_next_press` | — | done |
| EME-31 | Only the reply to a query is silent when the network refuses it: not a message to the same number during the search, not another number | `test_only_the_reply_to_a_query_is_silent_when_the_network_refuses_it` | — | done |
| EME-32 | Leaving after the choice does not stop a running ping | `test_leave_does_not_stop_a_running_ping` | — | done |
| EME-33 | A query reply that cannot even be queued is also told in the history | `test_a_query_reply_that_cannot_even_be_queued_is_told_in_the_history` | — | done |
| EME-34 | A query that the profile keeps out of the history keeps its failure out too (the log has it) | `test_a_query_that_is_kept_out_of_the_history_keeps_its_failure_out_too` | — | done |
| EME-35 | A failed emergency is not written as a failed query | `test_a_failed_emergency_is_not_a_failed_query` | — | done |

## Connectivity

Tests in `test/test_connectivity/`.

| Id | Behaviour | Test | Previous | Status |
|---|---|---|---|---|
| CON-01 | Cellular can be switched off and on; each is logged and noticed; a switch that fails is logged and returns false | `test_cellular_can_be_switched` | EA:test_cellular_off_uses_mesh_and_blocks_calls | done |
| CON-02 | A switched-off or just restarted modem is not asleep | `test_a_switched_modem_is_not_asleep` | — | done |
| CON-03 | Registered means powered on, in a state that is registered and with an operator | `test_registered_needs_power_state_and_operator` | EA:test_cellular_off_uses_mesh_and_blocks_calls | done |
| CON-04 | The network label is data: the operator name shortened, radio, radio only, or none | `test_the_network_label_is_data` | EA:test_network_label_names_the_provider_or_the_radio | done |
| CON-05 | The radio starts only on request, and only if the profile allows it | `test_the_radio_starts_only_on_request_and_if_permitted` | EA:test_mesh_starts_only_on_request_and_only_if_permitted | done |
| CON-06 | A restart can be requested | `test_a_restart_can_be_requested` | — | done |
| CON-07 | Save mode needs the radio; without it it is refused with a notice | `test_save_mode_needs_the_radio` | EA:test_save_mode_switches_cellular_off_with_a_timed_probe | done |
| CON-08 | Save mode switches cellular and Wi-Fi off; leaving it switches cellular back on | `test_save_mode_switches_cellular_and_wifi_off` | EA:test_save_mode_switches_cellular_off_with_a_timed_probe | done |
| CON-09 | The cell probe switches cellular on for 180 seconds and off again by itself; save mode stays | `test_the_cell_probe_ends_by_itself` | EA:test_save_mode_switches_cellular_off_with_a_timed_probe | done |
| CON-10 | The probe only works in save mode and its seconds left are readable | `test_the_probe_works_only_in_save_mode` | EA:test_save_mode_switches_cellular_off_with_a_timed_probe | done |
| CON-11 | Wi-Fi is refused in save mode | `test_wifi_is_refused_in_save_mode` | — | done |
| CON-12 | Wi-Fi is refused on a nearly empty battery unless on the cable; switching it off always works | `test_wifi_is_refused_on_a_nearly_empty_battery` | EA:test_wifi_is_refused_with_a_nearly_empty_battery, EA:test_wifi_on_cable_ignores_the_battery_limit | done |
| CON-13 | Without a Wi-Fi capability nothing happens | `test_without_wifi_nothing_happens` | EA:test_wifi_without_a_web_service_does_nothing | done |
| CON-14 | Wi-Fi requests are logged | `test_wifi_requests_are_logged` | — | done |
| CON-15 | Wi-Fi is off until asked | `test_wifi_is_off_until_someone_asks` | WF:test_wifi_is_off_until_someone_asks | done |
| CON-16 | A switch-on that fails is logged | `test_a_failed_switch_on_is_logged` | — | done |
| CON-17 | A new probe starts the 180 seconds anew, and leaving save mode ends it | `test_a_probe_can_be_started_again_and_leaving_save_mode_ends_it` | — | done |
| CON-18 | An unknown battery level does not refuse Wi-Fi | `test_an_unknown_battery_does_not_refuse_wifi` | — | done |
| CON-19 | poll() keeps the Wi-Fi controller going | `test_poll_keeps_wifi_going` | — | done |

## One Wi-Fi controller and the parents' page

Tests in `test/test_web_api/`.

| Id | Behaviour | Test | Previous | Status |
|---|---|---|---|---|
| WEB-01 | Every authenticated request restarts the Wi-Fi idle time | `test_every_authenticated_request_restarts_the_idle_time` | — | done |
| WEB-02 | A request with no or a wrong password does not restart it | `test_a_request_without_the_password_does_not_restart_the_idle_time` | — | done |
| WEB-03 | The login page does not restart it | `test_the_login_page_does_not_restart_the_idle_time` | — | done |
| WEB-04 | A locked-out client does not restart it, even with the right password | `test_a_locked_out_client_does_not_restart_the_idle_time_even_with_the_right_password` | — | done |
| WEB-05 | Without a stored password nothing restarts it | `test_without_a_stored_password_nothing_restarts_the_idle_time` | — | done |
| WEB-06 | The button keeps Wi-Fi on: behind the password and the header, logged, answers with the time left | `test_the_button_keeps_wifi_on_behind_the_password_and_the_header_and_is_logged` | — | done |
| WEB-07 | The page's Wi-Fi state is the one controller's state | `test_the_state_is_the_one_controllers_state` | — | done |
| WEB-08 | A device without Wi-Fi answers and says off | `test_a_device_without_wifi_answers_and_says_off` | — | done |

## The web service on the one controller

Tests in `test/test_runtime/`.

| Id | Behaviour | Test | Previous | Status |
|---|---|---|---|---|
| WSV-01 | The server runs exactly while the controller says Wi-Fi is up, whoever switched it | `test_the_server_runs_exactly_while_the_one_controller_says_wifi_is_up` | — | done |
| WSV-02 | Silence switches Wi-Fi and the page off after the idle time | `test_silence_switches_wifi_and_the_page_off_after_the_idle_time` | — | done |
| WSV-03 | A parent who keeps using the page is never cut off; a stranger cannot keep it on | `test_a_parent_who_keeps_using_the_page_is_never_cut_off_and_a_stranger_cannot_keep_it_on` | — | done |

## WifiController

Tests in `test/test_wifi_controller/`.

| Id | Behaviour | Test | Previous | Status |
|---|---|---|---|---|
| WIF-01 | Station mode connects | `test_station_connects` | WF:test_station_connects | done |
| WIF-02 | Station mode tries the next network after a failure or a timeout, then gives up and powers the chip off | `test_station_tries_the_next_network` | WF:test_station_tries_the_next_network_after_a_failure_or_timeout | done |
| WIF-03 | Station mode without networks says so | `test_station_without_networks_says_so` | WF:test_station_without_networks_says_so | done |
| WIF-04 | An access point without a configured password gets a random eight-digit one | `test_access_point_gets_a_random_password` | WF:test_access_point_without_password_gets_a_random_one | done |
| WIF-05 | The random password is padded to eight digits | `test_the_password_is_padded` | WF:test_access_point_password_is_padded_to_eight_digits | done |
| WIF-06 | A configured password is used | `test_a_configured_password_is_used` | WF:test_configured_ap_password_is_used | done |
| WIF-07 | The access point password is forgotten when off | `test_the_password_is_forgotten_when_off` | WF:test_ap_password_is_forgotten_when_off | done |
| WIF-08 | Wi-Fi switches itself off after the idle time | `test_it_switches_itself_off_after_the_idle_time` | WF:test_wifi_switches_itself_off_after_the_idle_time | done |
| WIF-09 | Activity postpones the shutdown | `test_activity_postpones_the_shutdown` | WF:test_activity_postpones_the_shutdown | done |
| WIF-10 | A lost connection is retried | `test_a_lost_connection_is_retried` | WF:test_lost_connection_is_retried | done |
| WIF-11 | An access point that fails to start is reported | `test_an_access_point_failure_is_reported` | WF:test_ap_start_failure_is_reported | done |
| WIF-12 | Requesting off stops | `test_requesting_off_stops` | WF:test_request_off_stops | done |
| WIF-13 | Wi-Fi settings: networks, access point, idle minutes with their limits (the file format is a later package) | `test_the_settings_are_validated` | WF:test_config_reads_networks_and_ap, WF:test_config_rejects_short_passwords_and_reports_them, WF:test_config_caps_networks_and_clamps_timeout | done |
| WIF-14 | An access point that stops while it runs is reported as lost | `test_a_lost_access_point_is_reported` | — | done |
| WIF-15 | A lost connection starts again from the first network | `test_a_lost_connection_starts_again_from_the_first_network` | — | done |
| WIF-16 | The idle time starts when the station has connected | `test_the_idle_time_starts_when_the_station_has_connected` | — | done |

## PowerCoordination

Tests in `test/test_power_coordination/`.

| Id | Behaviour | Test | Previous | Status |
|---|---|---|---|---|
| POW-01 | The modem sleeps after a quiet minute, once | `test_the_modem_sleeps_after_a_quiet_minute` | EA:test_modem_sleeps_after_a_quiet_minute | done |
| POW-02 | A key press wakes the modem | `test_a_key_press_wakes_the_modem` | EA:test_keypress_wakes_the_modem | done |
| POW-03 | A profile without modem sleep keeps modem and processor awake | `test_no_modem_sleep_keeps_everything_awake` | EA:test_persona_without_modem_sleep_keeps_it_awake | done |
| POW-04 | On external power nothing sleeps, also with a full battery that no longer charges | `test_external_power_keeps_everything_awake` | EA:test_full_battery_on_cable_still_counts_as_external_power | done |
| POW-05 | Running Wi-Fi keeps the processor awake; a failed Wi-Fi does not | `test_running_wifi_keeps_the_processor_awake` | EA:test_running_wifi_keeps_the_cpu_awake | done |
| POW-06 | With cellular off the modem is neither put to sleep nor woken | `test_a_switched_off_modem_is_left_alone` | EA:test_cellular_off_uses_mesh_and_blocks_calls | done |
| POW-07 | The radio chip gets power only while the radio is usable, and the change is logged | `test_the_radio_chip_follows_the_need` | — | done |
| POW-08 | Pending work is a queue with a path or a running search | `test_pending_work` | EA:test_pending_message_wakes_the_modem | done |
| POW-09 | A battery-powered idle device may sleep the processor with the modem asleep | `test_the_processor_may_sleep_when_everything_is_idle` | EA:test_modem_sleeps_after_a_quiet_minute | done |
| POW-10 | The front light toggles with a notice, and can be switched off | `test_the_front_light_toggles` | SC:test_light_tile_switches_the_frontlight, SC:test_locking_switches_the_light_off | done |
| POW-11 | A refused power-off clears the request and says so | `test_a_refused_power_off_is_noticed` | — | done |
| POW-12 | A radio chip that will not switch is not claimed in the log and is tried again | `test_a_radio_chip_that_will_not_switch_is_tried_again` | — | done |
| POW-13 | A call keeps the modem awake | `test_a_call_keeps_the_modem_awake` | — | done |
| POW-14 | A modem without a network does not sleep | `test_a_modem_without_a_network_does_not_sleep` | — | done |
| POW-15 | A front light that will not switch says nothing | `test_a_light_that_will_not_switch_says_nothing` | — | done |
| POW-16 | Battery level, charge state and minutes remaining are read through | `test_the_battery_is_read_through` | — | done |

## AppCore

Tests in `test/test_app_core/`.

| Id | Behaviour | Test | Previous | Status |
|---|---|---|---|---|
| APP-01 | poll() drains SMS, radio, call events and send failures, advances the searches and the queue and the probe, in one pass | `test_poll_drains_everything` | — | done |
| APP-02 | The read-only state offers what the interface draws: network label data, unread count, notice, call state and peer, SOS phase and countdown, school-time end, positioning active, save mode, battery, SIM | `test_the_state_offers_what_the_interface_draws` | SC:test_net_screen_explains_sms_only, SC:test_locked_sim_is_named_not_called_no_network, SC:test_status_screen_shows_charging | done |
| APP-03 | A full day: boot, start-up fix, message, calls in and out of school time | `test_a_full_day` | — | done |
| APP-04 | An SOS in school time with the override | `test_an_sos_in_school_time_with_the_override` | — | done |
| APP-05 | A remote query during an SOS | `test_a_remote_query_during_an_sos` | — | done |
| APP-06 | The network dropping and returning | `test_the_network_dropping_and_returning` | — | done |
| APP-07 | Save mode and the cell probe | `test_save_mode_and_the_cell_probe` | — | done |
| APP-08 | Settings the child may change at run time (message vibration, how visible a remote query is) change the profile until restart | `test_run_time_settings_change_the_profile` | SC:test_options_toggle_message_vibration_and_volume, SC:test_status_screen_cycles_command_notice | done |
| APP-09 | The time zone of the profile sets the clock's offset from the epoch, so school time stays on the wall clock across the change of the clocks | `test_the_clock_follows_the_time_zone` | — | done |
| APP-10 | The core contains no sentence: every notice and system entry is a code | `test_the_core_produces_only_codes` | — | done |
| APP-11 | A test pattern can be played on the vibration motor | `test_a_test_pattern_can_be_played` | SC:test_selftest_screen_triggers_vibration_and_light | done |
| APP-12 | A remote query alone is answered without the child noticing: no vibration, receiver off afterwards | `test_a_remote_query_alone_is_answered_without_the_child_noticing` | — | done |
| APP-13 | The trigger word of every language and the profile's extra words trigger the query; anything else is a message | `test_the_trigger_words_come_from_every_language` | — | done |
| APP-14 | Without a known time the zone is left alone | `test_without_a_known_time_the_zone_is_left_alone` | — | done |
| APP-15 | poll() returns the decision on sleeping, and the host's requests (restart, power off, front light) are readable | `test_the_host_gets_the_decision_and_the_requests` | — | done |
| APP-16 | Wi-Fi stays on while it is used, switches itself off afterwards, and keeps the processor awake meanwhile | `test_wifi_stays_on_while_it_is_used` | — | done |
| APP-17 | The position page and the test page each hold the receiver; one closing does not take it from the other | `test_the_position_page_and_the_test_page_hold_the_receiver` | — | done |
| APP-20 | A stored time (a message's) is shown on the wall clock of the clock's zone; epoch 0 is not known | `test_a_stored_time_is_shown_on_the_wall_clock_of_the_zone` | — | done |
| APP-18 | The radio's contacts are listed; without a radio there are none | `test_radio_contacts_are_listed` | — | done |
| APP-19 | The state has the wall clock as numbers (year, month, day, weekday from Sunday, minute of the day) in the clock's zone, and nothing when the time is unknown | `test_the_state_has_the_wall_clock_as_numbers` | — | done |

## Not ported

| Previous | Why |
|---|---|
| EA:test_persona_and_warnings_reach_the_log | Parsing persona.json and whitelist.txt and logging their warnings belongs to the configuration package. The core receives a finished profile. |
| SC (all UI screen tests) | Screens, soft keys, status bar, tile layout, lock screen, compose, number pad, wifi tiles, clock and test pages are user-interface work. The behaviours they exercised in the application are listed above; the stack rules are in docs/ui-framework.md. |
| SC:test_motion_* | The motion sensor is a capability the screen uses directly; the application core does not wrap it. |
| WF:test_broken_json_is_survivable | WLAN configuration file parsing is the configuration package. |
| WF:test_cpu_does_not_sleep_while_wifi_runs | Already covered by the power-policy tests; the core only passes the Wi-Fi state in (see Power coordination). |
| test_web_server | The web code is a later package. |
