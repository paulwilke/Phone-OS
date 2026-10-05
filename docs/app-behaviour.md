# Behaviour of the application core

This is the inventory of what the application does, taken from the previous code base: the one class that did everything (`EmergencyApp`), its test suite, the application behaviour exercised by the screen tests, and the Wi-Fi controller with its tests. Every row is one observable behaviour, the component that owns it, and the test that pins it. **Every previous test case appears** in the column *Previous* (`EA:` is the application test, `SC:` the screen tests, `WF:` the Wi-Fi tests), or in the list of what is not ported at the end, with the reason.

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
| PRO-01 | The profile holds every setting the behaviour depends on, with defaults that match the previous defaults: message vibration on, call vibration on, call vibration in school time off, modem sleep allowed, remote-query notice 'history', no radio, no free text | `test_defaults_match_the_previous_defaults` | — | open |
| PRO-02 | A quiet-hour window that cannot be read is refused with a reason (the profile reports it, nothing is logged by the core) | `test_an_unreadable_quiet_hour_window_is_refused_with_a_reason` | EA:test_unreadable_quiet_hours_are_reported | open |
| PRO-03 | Quiet hours are evaluated on the clock's epoch and time zone; without a known time there is no lock | `test_school_time_needs_a_known_time` | EA:test_no_lockout_without_known_time | open |
| PRO-04 | The first call contact of the home actions is the SOS recipient when no target is given; with none, there is no recipient | `test_the_sos_recipient_is_the_target_or_the_first_call_contact` | SC:test_without_a_contact_there_is_no_sos_tile | open |
| PRO-05 | A number may be called when it is in the contact book (any spelling) or a call target of the home actions | `test_who_may_be_called` | EA:test_calls_only_to_known_numbers | open |
| PRO-06 | Settings the child may change at run time (message vibration, remote-query notice visibility) change the profile until restart | `test_run_time_settings_change_the_profile` | SC:test_options_toggle_message_vibration_and_volume, SC:test_status_screen_cycles_command_notice | open |

## Notices

Tests in `test/test_notices/`.

| Id | Behaviour | Test | Previous | Status |
|---|---|---|---|---|
| NOT-01 | There is at most one notice; a newer one replaces the older | `test_a_newer_notice_replaces_the_older` | — | open |
| NOT-02 | A notice expires after kNoticeSeconds (8) of uptime and reads as none afterwards | `test_a_notice_expires` | EA:test_alert_expires | open |
| NOT-03 | A notice can be cleared at once | `test_a_notice_can_be_cleared` | — | open |
| NOT-04 | Notices carry a kind and values, never a sentence; every kind has a text in every language | `test_every_kind_is_worded_in_every_language` | (test_notice) | open |

## Positioning

Tests in `test/test_positioning/`.

| Id | Behaviour | Test | Previous | Status |
|---|---|---|---|---|
| POS-01 | The receiver is off without a request | `test_the_receiver_is_off_without_a_request` | EA:test_gnss_is_off_without_a_request | open |
| POS-02 | The receiver has independent reasons (start-up fix, SOS choice, SOS ping, remote query, position page, test page); it is on while any holds and one cannot switch off another's | `test_reasons_are_independent` | SC:test_a_running_command_keeps_the_receiver, EA:test_gps_page_keeps_the_receiver_after_the_startup_fix_ends | open |
| POS-03 | A reason that is released switches the receiver off when it was the last | `test_the_last_released_reason_switches_the_receiver_off` | SC:test_gnss_screen_powers_the_receiver_while_open, SC:test_locking_switches_the_receiver_off | open |
| POS-04 | Power is switched through the Power capability, once per change | `test_power_is_switched_once_per_change` | EA:test_locating_switches_gnss_power | open |
| POS-05 | The start-up fix ends on the first usable fix (HDOP up to 5) and keeps it | `test_the_startup_fix_ends_on_a_usable_fix` | EA:test_startup_fix_runs_once_then_switches_off | open |
| POS-06 | An inaccurate fix is taken after a minute | `test_an_inaccurate_fix_is_taken_after_a_minute` | EA:test_startup_fix_waits_for_accuracy_but_not_forever | open |
| POS-07 | The start-up fix gives up after three minutes and there is no last fix | `test_the_startup_fix_gives_up_after_three_minutes` | EA:test_startup_fix_gives_up_after_three_minutes | open |
| POS-08 | The last valid fix is kept with its age, whoever asked for the receiver | `test_the_last_fix_keeps_its_age` | EA:test_startup_fix_runs_once_then_switches_off | open |
| POS-09 | A fix from before the last switch-on is never used | `test_a_fix_from_before_the_last_switch_on_is_not_used` | — | open |
| POS-10 | The start-up fix is logged when it starts and when it ends | `test_the_startup_fix_is_logged` | — | open |

## Messaging

Tests in `test/test_messaging/`.

| Id | Behaviour | Test | Previous | Status |
|---|---|---|---|---|
| MSG-01 | An incoming SMS is stored unread with the sender, and counts as unread | `test_an_incoming_sms_is_stored` | EA:test_incoming_sms_is_stored | open |
| MSG-02 | A remote query is recognised before storing: it is never stored as a message | `test_a_remote_query_is_not_stored_as_a_message` | EA:test_command_sms_is_not_stored_but_logged_and_noticed | open |
| MSG-03 | A remote query from a parent is logged, written as a history entry (kind location-queried) and, when the profile says notify, shown as a notice | `test_a_remote_query_is_logged_recorded_and_noticed_as_the_profile_says` | EA:test_command_sms_is_not_stored_but_logged_and_noticed, SC:test_status_screen_cycles_command_notice | open |
| MSG-04 | A remote query from a number that is not a parent is refused, logged and recorded (kind query-refused); no reply, no receiver | `test_a_refused_remote_query_is_recorded` | EA:test_unauthorized_command_is_logged_and_rejected | open |
| MSG-05 | A remote query never vibrates | `test_a_remote_query_does_not_vibrate` | EA:test_remote_command_does_not_vibrate | open |
| MSG-06 | An incoming SMS vibrates with the message pattern | `test_an_incoming_sms_vibrates_the_message_pattern` | EA:test_incoming_message_vibrates_twice_short | open |
| MSG-07 | Message vibration can be switched off | `test_message_vibration_can_be_switched_off` | EA:test_message_vibration_can_be_switched_off | open |
| MSG-08 | In school time only a parent's message vibrates; the message is stored either way | `test_in_school_time_only_parents_vibrate` | EA:test_message_vibration_in_school_time_only_for_parents | open |
| MSG-09 | An incoming SMS counts as the modem being awake | `test_an_incoming_sms_means_the_modem_is_awake` | EA:test_incoming_sms_counts_as_awake | open |
| MSG-10 | A message is sent by cellular when registered | `test_a_message_goes_by_cellular` | EA:test_send_text_over_network | open |
| MSG-11 | Cellular has priority over the radio | `test_cellular_has_priority_over_the_radio` | EA:test_cellular_has_priority_over_mesh | open |
| MSG-12 | Without cellular the radio carries it, with no phone number, to the radio's own contact, and nothing waits; the entry says radio | `test_the_radio_takes_over_when_cellular_is_gone` | EA:test_mesh_takes_over_when_cellular_is_gone | open |
| MSG-13 | With cellular switched off the radio carries messages and calls are refused | `test_cellular_off_uses_the_radio` | EA:test_cellular_off_uses_mesh_and_blocks_calls | open |
| MSG-14 | Without a path the message is queued, shown as waiting, and a notice says so | `test_without_a_path_a_message_is_queued` | EA:test_send_without_network_is_queued_and_sent_later | open |
| MSG-15 | A queued message goes out when the network returns; the waiting notice is replaced by 'message is out' | `test_a_queued_message_goes_out_when_the_network_returns` | EA:test_send_without_network_is_queued_and_sent_later | open |
| MSG-16 | A queued message goes out by radio when the radio comes up, without cellular, and the entry becomes a radio entry | `test_a_queued_message_goes_out_by_radio` | EA:test_without_cellular_and_without_mesh_it_waits | open |
| MSG-17 | The queue holds kOutboxSize (8); one more is refused with a notice | `test_the_queue_is_bounded` | EA:test_queue_is_bounded | open |
| MSG-18 | A message waiting without any path does not keep the modem awake | `test_a_message_without_any_path_does_not_keep_the_modem_awake` | EA:test_waiting_message_without_any_path_does_not_block_sleep | open |
| MSG-19 | A waiting message with a path counts as pending work | `test_a_message_with_a_path_is_pending_work` | EA:test_pending_message_wakes_the_modem | open |
| MSG-20 | Without radio attached everything works as before | `test_without_a_radio_everything_works_as_before` | EA:test_without_mesh_attached_everything_works_as_before | open |
| MSG-21 | A profile without the radio never sends by radio | `test_a_profile_without_the_radio_never_sends_by_radio` | EA:test_persona_without_mesh_never_uses_it | open |
| MSG-22 | A profile without the radio ignores incoming radio messages | `test_a_profile_without_the_radio_ignores_incoming_radio_messages` | EA:test_persona_without_mesh_ignores_incoming_mesh | open |
| MSG-23 | An incoming radio message is stored with the radio contact's name | `test_an_incoming_radio_message_is_stored` | EA:test_incoming_mesh_message_lands_in_the_history | open |
| MSG-24 | An explicit radio send goes to the named radio contact and is not routed through cellular; refused with a notice when the radio is not usable | `test_an_explicit_radio_send` | — | open |
| MSG-25 | A radio message is marked confirmed when the radio says it arrived, and logged | `test_a_radio_message_is_marked_confirmed` | EA:test_mesh_message_is_marked_confirmed | open |
| MSG-26 | A radio message without confirmation is marked so and a notice says so | `test_a_radio_message_without_confirmation_says_so` | EA:test_mesh_message_without_confirmation_says_so | open |
| MSG-27 | Delivery reports are matched to messages in the order of sending | `test_two_radio_messages_keep_their_order` | EA:test_two_mesh_messages_keep_their_order | open |
| MSG-28 | Only a few reports are awaited: the oldest is forgotten and stays unconfirmed | `test_the_oldest_awaited_report_is_forgotten` | — | open |
| MSG-29 | An SMS has no delivery state | `test_an_sms_has_no_delivery_state` | EA:test_sms_has_no_delivery_state | open |
| MSG-30 | A message the modem refuses after accepting it is logged with the modem's reason, written as a history entry (kind sms-rejected), noticed, vibrates the failure pattern and is reported to a running SOS ping | `test_a_modem_send_failure_is_recorded_noticed_and_reported` | — | open |
| MSG-31 | A message is deleted from the history by its number | `test_a_message_can_be_deleted` | — | open |
| MSG-32 | Empty recipient or empty text is refused | `test_empty_recipient_or_text_is_refused` | — | open |
| MSG-33 | Sending writes a log line with the path | `test_sending_is_logged` | — | open |

## Calls

Tests in `test/test_calls/`.

| Id | Behaviour | Test | Previous | Status |
|---|---|---|---|---|
| CAL-01 | A child may only call allowed numbers; others get a 'not allowed' notice and nothing is dialled | `test_only_allowed_numbers_are_dialled` | EA:test_calls_only_to_known_numbers | open |
| CAL-02 | Dialling is refused in school time with 'school time until HH:MM' | `test_dialling_is_refused_in_school_time` | EA:test_child_cannot_call_during_school_time, SC:test_tapping_call_tile_in_school_time_stays_home_and_says_why | open |
| CAL-03 | Dialling works outside school time | `test_dialling_works_outside_school_time` | EA:test_calling_works_outside_school_time | open |
| CAL-04 | Dialling is refused with cellular switched off | `test_dialling_is_refused_with_cellular_off` | EA:test_cellular_off_uses_mesh_and_blocks_calls | open |
| CAL-05 | Dialling is refused without a network | `test_dialling_is_refused_without_a_network` | EA:test_call_without_network_fails_with_alert | open |
| CAL-06 | A dial the modem refuses leaves no call behind | `test_a_refused_dial_leaves_no_call` | — | open |
| CAL-07 | An incoming call names the caller as the current peer | `test_an_incoming_call_names_the_peer` | EA:test_incoming_call_and_missed_call_note | open |
| CAL-08 | A missed call is a history entry (kind missed-call, peer the caller) and clears the peer, so a callback is possible | `test_a_missed_call_is_recorded` | EA:test_incoming_call_and_missed_call_note, SC:test_missed_call_offers_callback | open |
| CAL-09 | Answering and hanging up go to the modem; hanging up clears the peer | `test_answer_and_hang_up` | SC:test_incoming_call_wakes_and_shows_caller | open |
| CAL-10 | A ringing call vibrates the ring pattern every two seconds until it stops, and not at all while nothing rings | `test_ringing_vibrates_until_it_stops` | EA:test_ringing_vibrates_until_it_stops, EA:test_no_vibration_while_nothing_happens, EA:test_ring_uses_the_ring_pattern | open |
| CAL-11 | Call vibration can be switched off | `test_call_vibration_can_be_switched_off` | EA:test_call_vibration_settings_and_the_priority_of_school_time | open |
| CAL-12 | Every incoming call in school time, parents included, is rejected without ringing; the notice names a known caller | `test_every_call_in_school_time_is_rejected_silently_and_named` | EA:test_every_call_in_school_time_is_rejected_silently_and_named | open |
| CAL-13 | An unknown caller in school time gets a notice without a name | `test_an_unknown_caller_gets_a_notice_without_a_name` | — | open |
| CAL-14 | Vibration for calls in school time is a setting that defaults to off; switched on it vibrates and still rejects | `test_call_vibration_in_school_time_is_a_setting` | EA:test_call_vibration_settings_and_the_priority_of_school_time | open |
| CAL-15 | A call without a number is neither rung nor rejected until the number arrives | `test_a_call_without_a_number_yet_is_neither_rung_nor_rejected` | EA:test_call_without_number_yet_is_neither_rung_nor_rejected | open |
| CAL-16 | A call that never shows a number is rejected after three seconds in school time | `test_a_call_without_a_number_is_rejected_after_three_seconds` | EA:test_call_without_a_number_is_rejected_after_a_short_wait_in_school_time | open |
| CAL-17 | The modem reports an incoming call twice; it is rejected once | `test_an_incoming_call_is_rejected_once` | — | open |
| CAL-18 | A busy line is reported with the callee's name, vibrates the failure pattern and is recorded (call-failed busy) | `test_a_busy_line_is_reported` | EA:test_failed_call_says_why_busy, EA:test_failed_call_vibrates_the_failure_pattern | open |
| CAL-19 | No answer is reported | `test_no_answer_is_reported` | EA:test_failed_call_says_no_answer | open |
| CAL-20 | A dial failure on an SMS-only network says so | `test_a_dial_failure_on_an_sms_only_network_says_so` | EA:test_dial_failure_on_sms_only_network_names_it | open |
| CAL-21 | A dial failure without a network says so | `test_a_dial_failure_without_a_network_says_so` | EA:test_dial_failure_without_network_says_no_network | open |
| CAL-22 | A dial failure otherwise is still reported | `test_a_dial_failure_is_still_reported` | EA:test_dial_failure_otherwise_is_still_reported | open |
| CAL-23 | A call that never connected and ended is a failure, a connected one is not, one the child hung up is not | `test_an_unconnected_end_is_a_failure` | EA:test_unconnected_end_counts_as_failure_but_normal_end_does_not | open |
| CAL-24 | The modem's own reason goes to the log with the failure | `test_the_modems_reason_is_logged` | — | open |
| CAL-25 | Calls are logged: incoming, connected, ended | `test_calls_are_logged` | — | open |
| CAL-26 | Volume changes only during a call and stops at 0 and 7 | `test_volume_only_during_a_call_and_within_range` | EA:test_volume_only_during_a_call, EA:test_volume_stops_at_the_ends, SC:test_call_screen_changes_volume_with_w_and_s, SC:test_no_volume_outside_a_call | open |
| CAL-27 | A modem that cannot set the volume reports so | `test_a_modem_without_volume_support` | — | open |
| CAL-28 | The default volume is set through the modem | `test_the_default_volume_is_set` | SC:test_options_toggle_message_vibration_and_volume | open |
| CAL-29 | A call counts as active while dialling, ringing or connected | `test_a_call_is_active_while_dialling_ringing_or_connected` | SC:test_no_autolock_during_call | open |

## Emergency

Tests in `test/test_emergency/`.

| Id | Behaviour | Test | Previous | Status |
|---|---|---|---|---|
| EME-01 | The SOS recipient is the target or the first call contact; without one SOS cannot begin | `test_without_a_recipient_sos_cannot_begin` | SC:test_without_a_contact_there_is_no_sos_tile | open |
| EME-02 | SOS works in school time | `test_sos_works_in_school_time` | EA:test_sos_works_during_school_time | open |
| EME-03 | Pressing SOS starts the receiver at once, before any choice | `test_pressing_sos_starts_the_receiver_at_once` | SC:test_sos_tile_starts_the_ping_and_shows_progress | open |
| EME-04 | Choosing emergency or pickup starts the ping to the recipient; the ping holds the receiver | `test_choosing_starts_the_ping` | SC:test_sos_tile_starts_the_ping_and_shows_progress | open |
| EME-05 | The ping sends the position after the minimum time, to the first contact, with the coordinates | `test_the_ping_sends_the_position` | SC:test_sos_ping_sends_the_position_after_waiting | open |
| EME-06 | The receiver is off again after the ping ends | `test_the_receiver_is_off_after_the_ping` | SC:test_sos_ping_sends_the_position_after_waiting | open |
| EME-07 | Leaving the choice sends nothing and releases the receiver | `test_leaving_the_choice_sends_nothing` | SC:test_sos_can_be_cancelled | open |
| EME-08 | Without a choice the emergency goes out after 40 seconds | `test_the_emergency_goes_out_by_itself_after_40_seconds` | SC:test_sos_goes_out_by_itself_after_the_countdown | open |
| EME-09 | Pickup sends the other text | `test_pickup_sends_the_other_text` | SC:test_sos_pickup_sends_the_other_text | open |
| EME-10 | In school time five taps send the emergency and lift the school-time lock to the end of the current window; fewer do nothing | `test_five_taps_in_school_time_lift_the_lock` | SC:test_sos_in_school_time_needs_five_taps_and_lifts_the_lock | open |
| EME-11 | The lift is logged and ends with the school time | `test_the_lift_is_logged_and_ends_with_the_school_time` | SC:test_sos_in_school_time_needs_five_taps_and_lifts_the_lock | open |
| EME-12 | Leaving the school-time warning has no effect, the lock stays | `test_leaving_the_school_time_warning_has_no_effect` | SC:test_sos_warning_in_school_time_can_be_left_without_effect | open |
| EME-13 | The outcome of a ping is told once by haptic: sent three short, failed short-long | `test_the_outcome_is_vibrated_once` | EA:test_sos_done_vibrates_three_short_once | open |
| EME-14 | A remote query is answered like an SOS ping: receiver on, wait for a good fix, one SMS to the asker with coordinates and no alarm word, receiver off, the child notices nothing, the log says sent | `test_a_remote_query_is_answered_like_an_sos_ping` | EA:test_where_are_you_is_answered_like_an_sos_ping_without_a_link_and_silently | open |
| EME-15 | A remote query never vibrates, also at its end | `test_a_remote_query_never_vibrates` | EA:test_where_are_you_is_answered_like_an_sos_ping_without_a_link_and_silently | open |
| EME-16 | One search at a time: a second query while one runs is answered busy | `test_a_second_query_is_busy` | — | open |
| EME-17 | An emergency replaces a running query reply | `test_an_emergency_replaces_a_query_reply` | — | open |
| EME-18 | A remote query while an SOS runs is answered busy | `test_a_query_during_an_sos_is_busy` | — | open |
| EME-19 | The SMS wording is in the SMS language of the profile, whatever the interface language | `test_the_sms_is_worded_in_the_sms_language` | — | open |
| EME-20 | The child's name and the map template of the profile go into the text | `test_the_name_and_the_map_template_go_into_the_text` | — | open |
| EME-21 | The host's leave() releases the receiver when the screen closes | `test_leave_releases_the_receiver` | — | open |

## Connectivity

Tests in `test/test_connectivity/`.

| Id | Behaviour | Test | Previous | Status |
|---|---|---|---|---|
| CON-01 | Cellular can be switched off and on; each is logged and noticed; a switch that fails is logged and returns false | `test_cellular_can_be_switched` | EA:test_cellular_off_uses_mesh_and_blocks_calls | open |
| CON-02 | A switched-off or just restarted modem is not asleep | `test_a_switched_modem_is_not_asleep` | — | open |
| CON-03 | Registered means powered on, in a state that is registered and with an operator | `test_registered_needs_power_state_and_operator` | EA:test_cellular_off_uses_mesh_and_blocks_calls | open |
| CON-04 | The network label is data: the operator name shortened, radio, radio only, or none | `test_the_network_label_is_data` | EA:test_network_label_names_the_provider_or_the_radio | open |
| CON-05 | The radio starts only on request, and only if the profile allows it | `test_the_radio_starts_only_on_request_and_if_permitted` | EA:test_mesh_starts_only_on_request_and_only_if_permitted | open |
| CON-06 | A restart can be requested | `test_a_restart_can_be_requested` | — | open |
| CON-07 | Save mode needs the radio; without it it is refused with a notice | `test_save_mode_needs_the_radio` | EA:test_save_mode_switches_cellular_off_with_a_timed_probe | open |
| CON-08 | Save mode switches cellular and Wi-Fi off; leaving it switches cellular back on | `test_save_mode_switches_cellular_and_wifi_off` | EA:test_save_mode_switches_cellular_off_with_a_timed_probe | open |
| CON-09 | The cell probe switches cellular on for 180 seconds and off again by itself; save mode stays | `test_the_cell_probe_ends_by_itself` | EA:test_save_mode_switches_cellular_off_with_a_timed_probe | open |
| CON-10 | The probe only works in save mode and its seconds left are readable | `test_the_probe_works_only_in_save_mode` | EA:test_save_mode_switches_cellular_off_with_a_timed_probe | open |
| CON-11 | Wi-Fi is refused in save mode | `test_wifi_is_refused_in_save_mode` | — | open |
| CON-12 | Wi-Fi is refused on a nearly empty battery unless on the cable; switching it off always works | `test_wifi_is_refused_on_a_nearly_empty_battery` | EA:test_wifi_is_refused_with_a_nearly_empty_battery, EA:test_wifi_on_cable_ignores_the_battery_limit | open |
| CON-13 | Without a Wi-Fi capability nothing happens | `test_without_wifi_nothing_happens` | EA:test_wifi_without_a_web_service_does_nothing | open |
| CON-14 | Wi-Fi requests are logged | `test_wifi_requests_are_logged` | — | open |
| CON-15 | Wi-Fi is off until asked | `test_wifi_is_off_until_someone_asks` | WF:test_wifi_is_off_until_someone_asks | open |

## WifiController

Tests in `test/test_wifi_controller/`.

| Id | Behaviour | Test | Previous | Status |
|---|---|---|---|---|
| WIF-01 | Station mode connects | `test_station_connects` | WF:test_station_connects | open |
| WIF-02 | Station mode tries the next network after a failure or a timeout, then gives up and powers the chip off | `test_station_tries_the_next_network` | WF:test_station_tries_the_next_network_after_a_failure_or_timeout | open |
| WIF-03 | Station mode without networks says so | `test_station_without_networks_says_so` | WF:test_station_without_networks_says_so | open |
| WIF-04 | An access point without a configured password gets a random eight-digit one | `test_access_point_gets_a_random_password` | WF:test_access_point_without_password_gets_a_random_one | open |
| WIF-05 | The random password is padded to eight digits | `test_the_password_is_padded` | WF:test_access_point_password_is_padded_to_eight_digits | open |
| WIF-06 | A configured password is used | `test_a_configured_password_is_used` | WF:test_configured_ap_password_is_used | open |
| WIF-07 | The access point password is forgotten when off | `test_the_password_is_forgotten_when_off` | WF:test_ap_password_is_forgotten_when_off | open |
| WIF-08 | Wi-Fi switches itself off after the idle time | `test_it_switches_itself_off_after_the_idle_time` | WF:test_wifi_switches_itself_off_after_the_idle_time | open |
| WIF-09 | Activity postpones the shutdown | `test_activity_postpones_the_shutdown` | WF:test_activity_postpones_the_shutdown | open |
| WIF-10 | A lost connection is retried | `test_a_lost_connection_is_retried` | WF:test_lost_connection_is_retried | open |
| WIF-11 | An access point that fails to start is reported | `test_an_access_point_failure_is_reported` | WF:test_ap_start_failure_is_reported | open |
| WIF-12 | Requesting off stops | `test_requesting_off_stops` | WF:test_request_off_stops | open |
| WIF-13 | Wi-Fi settings: networks, access point, idle minutes with their limits (the file format is a later package) | `test_the_settings_are_validated` | WF:test_config_reads_networks_and_ap, WF:test_config_rejects_short_passwords_and_reports_them, WF:test_config_caps_networks_and_clamps_timeout | open |

## PowerCoordination

Tests in `test/test_power_coordination/`.

| Id | Behaviour | Test | Previous | Status |
|---|---|---|---|---|
| POW-01 | The modem sleeps after a quiet minute, once | `test_the_modem_sleeps_after_a_quiet_minute` | EA:test_modem_sleeps_after_a_quiet_minute | open |
| POW-02 | A key press wakes the modem | `test_a_key_press_wakes_the_modem` | EA:test_keypress_wakes_the_modem | open |
| POW-03 | A profile without modem sleep keeps modem and processor awake | `test_no_modem_sleep_keeps_everything_awake` | EA:test_persona_without_modem_sleep_keeps_it_awake | open |
| POW-04 | On external power nothing sleeps, also with a full battery that no longer charges | `test_external_power_keeps_everything_awake` | EA:test_full_battery_on_cable_still_counts_as_external_power | open |
| POW-05 | Running Wi-Fi keeps the processor awake; a failed Wi-Fi does not | `test_running_wifi_keeps_the_processor_awake` | EA:test_running_wifi_keeps_the_cpu_awake | open |
| POW-06 | With cellular off the modem is neither put to sleep nor woken | `test_a_switched_off_modem_is_left_alone` | EA:test_cellular_off_uses_mesh_and_blocks_calls | open |
| POW-07 | The radio chip gets power only while the radio is usable, and the change is logged | `test_the_radio_chip_follows_the_need` | — | open |
| POW-08 | Pending work is a queue with a path or a running search | `test_pending_work` | EA:test_pending_message_wakes_the_modem | open |
| POW-09 | A battery-powered idle device may sleep the processor with the modem asleep | `test_the_processor_may_sleep_when_everything_is_idle` | EA:test_modem_sleeps_after_a_quiet_minute | open |
| POW-10 | The front light toggles with a notice, and can be switched off | `test_the_front_light_toggles` | SC:test_light_tile_switches_the_frontlight, SC:test_locking_switches_the_light_off | open |
| POW-11 | A refused power-off clears the request and says so | `test_a_refused_power_off_is_noticed` | — | open |
| POW-12 | A test pattern can be played on the vibration motor | `test_a_test_pattern_can_be_played` | SC:test_selftest_screen_triggers_vibration_and_light | open |

## AppCore

Tests in `test/test_app_core/`.

| Id | Behaviour | Test | Previous | Status |
|---|---|---|---|---|
| APP-01 | poll() drains SMS, radio, call events and send failures, advances the searches and the queue and the probe, in one pass | `test_poll_drains_everything` | — | open |
| APP-02 | The read-only state offers what the interface draws: network label data, unread count, notice, call state and peer, SOS phase and countdown, school-time end, positioning active, save mode, battery, SIM | `test_the_state_offers_what_the_interface_draws` | SC:test_net_screen_explains_sms_only, SC:test_locked_sim_is_named_not_called_no_network, SC:test_status_screen_shows_charging | open |
| APP-03 | A full day: boot, start-up fix, message, calls in and out of school time | `test_a_full_day` | — | open |
| APP-04 | An SOS in school time with the override | `test_an_sos_in_school_time_with_the_override` | — | open |
| APP-05 | A remote query during an SOS | `test_a_remote_query_during_an_sos` | — | open |
| APP-06 | The network dropping and returning | `test_the_network_dropping_and_returning` | — | open |
| APP-07 | Save mode and the cell probe | `test_save_mode_and_the_cell_probe` | — | open |
| APP-08 | The core contains no sentence: every notice and system entry is a code | `test_the_core_produces_only_codes` | — | open |

## Not ported

| Previous | Why |
|---|---|
| EA:test_persona_and_warnings_reach_the_log | Parsing persona.json and whitelist.txt and logging their warnings belongs to the configuration package. The core receives a finished profile. |
| SC (all UI screen tests) | Screens, soft keys, status bar, tile layout, lock screen, compose, number pad, wifi tiles, clock and test pages are user-interface work. The behaviours they exercised in the application are listed above; the stack rules are in docs/ui-framework.md. |
| SC:test_motion_* | The motion sensor is a capability the screen uses directly; the application core does not wrap it. |
| WF:test_broken_json_is_survivable | WLAN configuration file parsing is the configuration package. |
| WF:test_cpu_does_not_sleep_while_wifi_runs | Already covered by the power-policy tests; the core only passes the Wi-Fi state in (see Power coordination). |
| test_web_api, test_web_server | The web code is a later package. |
