# Submission Report

- Submission generated at 09/19/2026 at 05:53:20

- Machine info: Linux runnervmlun5p 6.17.0-1022-azure #22-Ubuntu SMP Mon Jul 27 17:24:03 UTC 2026 x86_64 x86_64 x86_64 GNU/Linux

## Note to Students

Please read this report carefully before submission.
Ensure that all sections are complete and accurate.
Look for any errors in the build or test outputs.
If you find any issues, correct them before submitting.
Post any questions on the class discussion board for help.


---

## README

# P1 — Simple Mail Client

- Name: Randy Bauer
- Email: randybauer@u.boisestate.edu
- Class: CS425-001

A mail client that talks to an SMTP server directly over a TCP socket.

## Usage

```
Usage: myapp -f <from> -t <to> [-s subject] [-b body] [-p port]
          [-H helo-host] <server>

  -f <from>       envelope sender, for example you@example.com
  -t <to>         envelope recipient
  -s <subject>    subject line (default: empty)
  -b <body>       message body (default: read from stdin)
  -p <port>       port or service name (default: 25)
  -H <helo-host>  host name sent with HELO (default: localhost)
  <server>        host name or address of the mail server
```

```bash
echo "This is the message body." | \
  ./build/release/myapp -f me@boisestate.edu -t you@example.com \
    -s "hello" -H onyx.boisestate.edu -p 2525 <server>
```

Nothing is printed on success.

| Status | Meaning |
| --- | --- |
| 0 | Message queued, or no arguments given and usage printed |
| 1 | Bad command line |
| 2 | Connection or session failed |

On failure it prints the reason and the server's reply:

```
myapp: malformed reply
myapp: server said: 550 5.1.1 <nobody@example.com>: Recipient address rejected
```

Port 25 is blocked; use `-p 2525` or `-p 587`.

## Design

### Layer 1: protocol helpers

- `smtp_reply_code` reads the status code off a reply line.
- `smtp_is_final_line` checks the character after the code. Space is the last
  line, hyphen is not.
- `smtp_text_is_safe` rejects CR or LF in an address or subject.
- `smtp_build_command` builds `VERB arg\r\n`. HELO, DATA, QUIT.
- `smtp_build_path_command` builds VERB:<address>\r\n. MAIL FROM and RCPT TO.
  No space around the colon, angle brackets required.
- `smtp_manage_dot` gives every body line a CRLF ending and doubles a leading
  period.
- `smtp_build_message` assembles headers, blank line, body, final period.

### Layer 2: the session

Reads the greeting. Sends HELO, MAIL FROM, RCPT TO, DATA, the message, QUIT.
Checks the status code each time.

`struct smtp_session` holds a read function pointer, a write function pointer
and a context pointer. All input and output goes through them.

The struct holds the receive buffer. TCP is a byte stream, so a reply can arrive
in pieces and two replies can arrive together. `smtp_read_line` checks the buffer
before reading more, and leftover bytes stay for the next call.

### Layer 3: the socket

`smtp_connect`, `smtp_disconnect`, `smtp_socket_read`, `smtp_socket_write`.

`smtp_connect` resolves the name with `getaddrinfo` and tries each address until
one connects.

`smtp_socket_write` passes `MSG_NOSIGNAL`. Writing to a closed connection
otherwise raises SIGPIPE and kills the process.

### Design Reasoning

A real mail server cannot be unit tested. Layer 2 takes a socket from the
program and a string from the tests, and the session code is the same.

A fake server is one string:

```c
"220 smtp.example.com ESMTP ready\r\n"
"250 smtp.example.com\r\n"
"250 2.1.0 Ok\r\n"
"250 2.1.5 Ok\r\n"
"354 End data with .\r\n"
"250 2.0.0 Ok: queued\r\n"
"221 Bye\r\n"
```

Each error case is that string with one code changed or the end removed.

Layer 3 is tested against a real connection. The test opens a listening socket on
127.0.0.1 port 0 and connects to it from the same process.

Smaller decisions:

- `smtp_read_line` strips the CRLF, so layer 1 never handles line endings.
- Layer 2 allocates nothing, so no error path needs cleanup.
- The CR and LF check is in `main`. A bad subject exits 1, a failed allocation
  exits 2, and only `main` can tell them apart.


## Known Bugs or Issues

No known bugs or issues

## Experience

The hard part of this was reading RFC 5321, but the all caps words were helpful
in pinpointing requirements for the SMTP client. The assignment specs listed the 
seven status codes but doesn't cover some of the RFC 5321 specs. To name a few, a
reply line can be three characters with no space and no text, so checking the 
character after the code has to handle running off the end. The greeting can be multi-line,
not just the 250 replies. An extra CRLF before the terminating period is a MUST
NOT, so the terminator is conditional on whether the body already ends in one.

The main design decision was making layer 2 read and write through function
pointers. That was the only way to test the session without a mail server. It
also decided where the receive buffer goes meaning it has to live in the session struct,
not in the line reader, or leftover bytes from a read that pulled in two replies
get thrown away and the next command waits for a reply that already arrived.

Layer 2 also uses a fixed buffer instead of malloc, so no error path has cleanup.
Input validation is in `main` instead of the message builder, because exit code 1
and exit code 2 mean different things and `main` can differentiate which applies.

Debugging: `make check` segfaulted with no other information. The test binary has
no sanitizer. `make leak-test` runs the same tests under AddressSanitizer and
gives the file and line. The crash had also stopped the run partway through,
which hid a second bug in tests that never executed.

Coverage: One thing that got me was that `gcovr --txt` reports line coverage,
not branch coverage. A condition joined with `||` is two branches and each needs
both outcomes, which is easy to miss when the line count already reads 100%.

---


## Build Output

This section was generated by running `make all` in the project root directory.

```bash
make[1]: Entering directory '/home/runner/work/cs425-p1/cs425-p1'
mkdir -p build/debug
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address -c src/main.c -o build/debug/main.c.o
mkdir -p build/debug
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address -c src/lab.c -o build/debug/lab.c.o
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address build/debug/main.c.o build/debug/lab.c.o -o build/debug/myapp_d -fsanitize=address
make[1]: Leaving directory '/home/runner/work/cs425-p1/cs425-p1'
make[1]: Entering directory '/home/runner/work/cs425-p1/cs425-p1'
mkdir -p build/release
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -c src/main.c -o build/release/main.c.o
mkdir -p build/release
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -c src/lab.c -o build/release/lab.c.o
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion build/release/main.c.o build/release/lab.c.o -o build/release/myapp 
make[1]: Leaving directory '/home/runner/work/cs425-p1/cs425-p1'
make[1]: Entering directory '/home/runner/work/cs425-p1/cs425-p1'
mkdir -p build/tests
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c src/main.c -o build/tests/main.c.o
mkdir -p build/tests
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c src/lab.c -o build/tests/lab.c.o
mkdir -p build/tests/
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c tests/lab-test.c -o build/tests/lab-test.c.o
mkdir -p build/tests/harness/
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c tests/harness/unity.c -o build/tests/harness/unity.c.o
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage build/tests/main.c.o build/tests/lab.c.o build/tests/lab-test.c.o build/tests/harness/unity.c.o -o build/tests/myapp_t -fprofile-arcs -ftest-coverage
make[1]: Leaving directory '/home/runner/work/cs425-p1/cs425-p1'
make[1]: Entering directory '/home/runner/work/cs425-p1/cs425-p1'
mkdir -p build/debug-test
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c src/main.c -o build/debug-test/main.c.o
mkdir -p build/debug-test
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c src/lab.c -o build/debug-test/lab.c.o
mkdir -p build/debug-test/
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c tests/lab-test.c -o build/debug-test/lab-test.c.o
mkdir -p build/debug-test/harness/
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c tests/harness/unity.c -o build/debug-test/harness/unity.c.o
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address build/debug-test/main.c.o build/debug-test/lab.c.o build/debug-test/lab-test.c.o build/debug-test/harness/unity.c.o -o build/debug-test/myapp_td -fsanitize=address
make[1]: Leaving directory '/home/runner/work/cs425-p1/cs425-p1'
Builds completed. You can run the application with: ./build/release/myapp
You can run the debug build with: ./build/debug/myapp_d
You can run the test build with: ./build/tests/myapp_t
You can run the debug-test build with: ./build/debug-test/myapp_td
```

---

## Coverage Report

This section was generated by running `make report` in the project root directory.

```bash
tests/lab-test.c:1142:test_reply_code_accepts_the_codes_the_session_needs:PASS
tests/lab-test.c:1143:test_reply_code_reads_a_continuation_line:PASS
tests/lab-test.c:1144:test_reply_code_accepts_a_bare_code_with_no_text:PASS
tests/lab-test.c:1145:test_reply_code_rejects_null:PASS
tests/lab-test.c:1146:test_reply_code_rejects_lines_shorter_than_three_digits:PASS
tests/lab-test.c:1147:test_reply_code_rejects_a_first_digit_outside_two_through_five:PASS
tests/lab-test.c:1148:test_reply_code_rejects_non_digits:PASS
tests/lab-test.c:1150:test_is_final_line_true_for_code_then_space:PASS
tests/lab-test.c:1151:test_is_final_line_false_for_code_then_hyphen:PASS
tests/lab-test.c:1152:test_is_final_line_true_for_a_bare_code:PASS
tests/lab-test.c:1153:test_is_final_line_ignores_digits_in_the_reply_text:PASS
tests/lab-test.c:1154:test_is_final_line_true_for_unparsable_lines:PASS
tests/lab-test.c:1156:test_text_is_safe_accepts_ordinary_values:PASS
tests/lab-test.c:1157:test_text_is_safe_rejects_null:PASS
tests/lab-test.c:1158:test_text_is_safe_rejects_embedded_line_breaks:PASS
tests/lab-test.c:1159:test_text_is_safe_rejects_an_injected_command:PASS
tests/lab-test.c:1161:test_build_command_joins_verb_and_argument_with_a_space:PASS
tests/lab-test.c:1162:test_build_command_omits_the_space_when_there_is_no_argument:PASS
tests/lab-test.c:1163:test_build_command_rejects_a_null_verb:PASS
tests/lab-test.c:1165:test_build_path_command_brackets_the_address_and_hugs_the_colon:PASS
tests/lab-test.c:1166:test_build_path_command_rejects_null_arguments:PASS
tests/lab-test.c:1168:test_manage_dot_rejects_null:PASS
tests/lab-test.c:1169:test_manage_dot_leaves_an_empty_body_empty:PASS
tests/lab-test.c:1170:test_manage_dot_terminates_an_unterminated_final_line:PASS
tests/lab-test.c:1171:test_manage_dot_does_not_add_a_second_terminator:PASS
tests/lab-test.c:1172:test_manage_dot_converts_lf_to_crlf:PASS
tests/lab-test.c:1173:test_manage_dot_doubles_a_leading_period:PASS
tests/lab-test.c:1174:test_manage_dot_leaves_interior_periods_alone:PASS
tests/lab-test.c:1175:test_manage_dot_stuffs_every_line_not_just_the_first:PASS
tests/lab-test.c:1176:test_manage_dot_drops_a_bare_carriage_return:PASS
tests/lab-test.c:1178:test_build_message_assembles_headers_blank_line_body_and_dot:PASS
tests/lab-test.c:1179:test_build_message_treats_a_null_subject_as_empty:PASS
tests/lab-test.c:1180:test_build_message_treats_a_null_body_as_empty:PASS
tests/lab-test.c:1181:test_build_message_stuffs_the_body_it_embeds:PASS
tests/lab-test.c:1182:test_build_message_rejects_a_null_sender_or_recipient:PASS
tests/lab-test.c:1184:test_status_text_describes_every_value:PASS
tests/lab-test.c:1186:test_session_init_stores_the_transport_and_empties_the_buffer:PASS
tests/lab-test.c:1188:test_read_line_strips_the_crlf:PASS
tests/lab-test.c:1189:test_read_line_hands_out_two_lines_from_a_single_read:PASS
tests/lab-test.c:1190:test_read_line_reassembles_a_line_delivered_one_byte_at_a_time:PASS
tests/lab-test.c:1191:test_read_line_keeps_a_cr_that_is_not_followed_by_lf:PASS
tests/lab-test.c:1192:test_read_line_reports_a_transport_failure:PASS
tests/lab-test.c:1193:test_read_line_reports_a_peer_that_closed_before_the_crlf:PASS
tests/lab-test.c:1194:test_read_line_reports_a_line_too_long_for_the_session_buffer:PASS
tests/lab-test.c:1195:test_read_line_reports_a_line_too_long_for_the_callers_buffer:PASS
tests/lab-test.c:1196:test_read_line_rejects_bad_arguments:PASS
tests/lab-test.c:1198:test_read_reply_returns_the_code_of_a_single_line_reply:PASS
tests/lab-test.c:1199:test_read_reply_follows_continuation_lines:PASS
tests/lab-test.c:1200:test_read_reply_handles_reply_text_that_begins_with_digits:PASS
tests/lab-test.c:1201:test_read_reply_accepts_a_multiline_greeting:PASS
tests/lab-test.c:1202:test_read_reply_accepts_a_reply_that_is_only_a_code:PASS
tests/lab-test.c:1203:test_read_reply_reassembles_a_multiline_reply_from_fragments:PASS
tests/lab-test.c:1204:test_read_reply_rejects_a_line_that_is_not_a_reply:PASS
tests/lab-test.c:1205:test_read_reply_reports_a_server_that_hangs_up_mid_reply:PASS
tests/lab-test.c:1206:test_read_reply_rejects_bad_arguments:PASS
tests/lab-test.c:1208:test_write_all_sends_the_whole_buffer:PASS
tests/lab-test.c:1209:test_write_all_retries_until_a_short_writing_transport_takes_it_all:PASS
tests/lab-test.c:1210:test_write_all_accepts_a_zero_length_write:PASS
tests/lab-test.c:1211:test_write_all_reports_a_transport_failure:PASS
tests/lab-test.c:1212:test_write_all_reports_a_transport_that_accepts_nothing:PASS
tests/lab-test.c:1213:test_write_all_rejects_bad_arguments:PASS
tests/lab-test.c:1215:test_command_expect_sends_the_command_and_accepts_the_right_code:PASS
tests/lab-test.c:1216:test_command_expect_reports_the_wrong_code_rather_than_ignoring_it:PASS
tests/lab-test.c:1217:test_command_expect_propagates_a_write_failure:PASS
tests/lab-test.c:1218:test_command_expect_propagates_a_read_failure:PASS
tests/lab-test.c:1219:test_command_expect_rejects_a_null_command:PASS
tests/lab-test.c:1221:test_run_session_issues_every_command_in_the_required_order:PASS
tests/lab-test.c:1222:test_run_session_follows_continuations_throughout:PASS
tests/lab-test.c:1223:test_run_session_rejects_a_bad_greeting:PASS
tests/lab-test.c:1224:test_run_session_rejects_a_bad_helo_reply:PASS
tests/lab-test.c:1225:test_run_session_rejects_a_bad_mail_from_reply:PASS
tests/lab-test.c:1226:test_run_session_rejects_a_bad_rcpt_to_reply:PASS
tests/lab-test.c:1227:test_run_session_rejects_a_bad_data_reply:PASS
tests/lab-test.c:1228:test_run_session_rejects_a_bad_end_of_data_reply:PASS
tests/lab-test.c:1229:test_run_session_rejects_a_bad_quit_reply:PASS
tests/lab-test.c:1230:test_run_session_sends_quit_before_giving_up_on_a_bad_code:PASS
tests/lab-test.c:1231:test_run_session_reports_a_server_that_hangs_up_mid_session:PASS
tests/lab-test.c:1232:test_run_session_does_not_send_quit_to_a_peer_that_has_gone:PASS
tests/lab-test.c:1233:test_run_session_reports_a_greeting_that_never_arrives:PASS
tests/lab-test.c:1234:test_run_session_reports_a_malformed_reply:PASS
tests/lab-test.c:1235:test_run_session_reports_a_write_failure:PASS
tests/lab-test.c:1236:test_run_session_rejects_a_mail_with_no_sender:PASS
tests/lab-test.c:1237:test_run_session_rejects_bad_arguments:PASS
tests/lab-test.c:1239:test_connect_reaches_a_listening_socket:PASS
tests/lab-test.c:1240:test_connect_reports_a_refused_connection:PASS
tests/lab-test.c:1241:test_connect_reports_a_service_it_cannot_resolve:PASS
tests/lab-test.c:1242:test_connect_rejects_null_arguments:PASS
tests/lab-test.c:1243:test_socket_read_and_write_move_bytes_over_a_real_connection:PASS
tests/lab-test.c:1244:test_socket_read_and_write_reject_null_arguments:PASS
tests/lab-test.c:1245:test_disconnect_closes_the_socket_and_tolerates_a_negative_descriptor:PASS
tests/lab-test.c:1246:test_a_whole_session_runs_over_a_real_socket:PASS

-----------------------
91 Tests 0 Failures 0 Ignored 
OK
./build/tests/myapp_t
tests/lab-test.c:1142:test_reply_code_accepts_the_codes_the_session_needs:PASS
tests/lab-test.c:1143:test_reply_code_reads_a_continuation_line:PASS
tests/lab-test.c:1144:test_reply_code_accepts_a_bare_code_with_no_text:PASS
tests/lab-test.c:1145:test_reply_code_rejects_null:PASS
tests/lab-test.c:1146:test_reply_code_rejects_lines_shorter_than_three_digits:PASS
tests/lab-test.c:1147:test_reply_code_rejects_a_first_digit_outside_two_through_five:PASS
tests/lab-test.c:1148:test_reply_code_rejects_non_digits:PASS
tests/lab-test.c:1150:test_is_final_line_true_for_code_then_space:PASS
tests/lab-test.c:1151:test_is_final_line_false_for_code_then_hyphen:PASS
tests/lab-test.c:1152:test_is_final_line_true_for_a_bare_code:PASS
tests/lab-test.c:1153:test_is_final_line_ignores_digits_in_the_reply_text:PASS
tests/lab-test.c:1154:test_is_final_line_true_for_unparsable_lines:PASS
tests/lab-test.c:1156:test_text_is_safe_accepts_ordinary_values:PASS
tests/lab-test.c:1157:test_text_is_safe_rejects_null:PASS
tests/lab-test.c:1158:test_text_is_safe_rejects_embedded_line_breaks:PASS
tests/lab-test.c:1159:test_text_is_safe_rejects_an_injected_command:PASS
tests/lab-test.c:1161:test_build_command_joins_verb_and_argument_with_a_space:PASS
tests/lab-test.c:1162:test_build_command_omits_the_space_when_there_is_no_argument:PASS
tests/lab-test.c:1163:test_build_command_rejects_a_null_verb:PASS
tests/lab-test.c:1165:test_build_path_command_brackets_the_address_and_hugs_the_colon:PASS
tests/lab-test.c:1166:test_build_path_command_rejects_null_arguments:PASS
tests/lab-test.c:1168:test_manage_dot_rejects_null:PASS
tests/lab-test.c:1169:test_manage_dot_leaves_an_empty_body_empty:PASS
tests/lab-test.c:1170:test_manage_dot_terminates_an_unterminated_final_line:PASS
tests/lab-test.c:1171:test_manage_dot_does_not_add_a_second_terminator:PASS
tests/lab-test.c:1172:test_manage_dot_converts_lf_to_crlf:PASS
tests/lab-test.c:1173:test_manage_dot_doubles_a_leading_period:PASS
tests/lab-test.c:1174:test_manage_dot_leaves_interior_periods_alone:PASS
tests/lab-test.c:1175:test_manage_dot_stuffs_every_line_not_just_the_first:PASS
tests/lab-test.c:1176:test_manage_dot_drops_a_bare_carriage_return:PASS
tests/lab-test.c:1178:test_build_message_assembles_headers_blank_line_body_and_dot:PASS
tests/lab-test.c:1179:test_build_message_treats_a_null_subject_as_empty:PASS
tests/lab-test.c:1180:test_build_message_treats_a_null_body_as_empty:PASS
tests/lab-test.c:1181:test_build_message_stuffs_the_body_it_embeds:PASS
tests/lab-test.c:1182:test_build_message_rejects_a_null_sender_or_recipient:PASS
tests/lab-test.c:1184:test_status_text_describes_every_value:PASS
tests/lab-test.c:1186:test_session_init_stores_the_transport_and_empties_the_buffer:PASS
tests/lab-test.c:1188:test_read_line_strips_the_crlf:PASS
tests/lab-test.c:1189:test_read_line_hands_out_two_lines_from_a_single_read:PASS
tests/lab-test.c:1190:test_read_line_reassembles_a_line_delivered_one_byte_at_a_time:PASS
tests/lab-test.c:1191:test_read_line_keeps_a_cr_that_is_not_followed_by_lf:PASS
tests/lab-test.c:1192:test_read_line_reports_a_transport_failure:PASS
tests/lab-test.c:1193:test_read_line_reports_a_peer_that_closed_before_the_crlf:PASS
tests/lab-test.c:1194:test_read_line_reports_a_line_too_long_for_the_session_buffer:PASS
tests/lab-test.c:1195:test_read_line_reports_a_line_too_long_for_the_callers_buffer:PASS
tests/lab-test.c:1196:test_read_line_rejects_bad_arguments:PASS
tests/lab-test.c:1198:test_read_reply_returns_the_code_of_a_single_line_reply:PASS
tests/lab-test.c:1199:test_read_reply_follows_continuation_lines:PASS
tests/lab-test.c:1200:test_read_reply_handles_reply_text_that_begins_with_digits:PASS
tests/lab-test.c:1201:test_read_reply_accepts_a_multiline_greeting:PASS
tests/lab-test.c:1202:test_read_reply_accepts_a_reply_that_is_only_a_code:PASS
tests/lab-test.c:1203:test_read_reply_reassembles_a_multiline_reply_from_fragments:PASS
tests/lab-test.c:1204:test_read_reply_rejects_a_line_that_is_not_a_reply:PASS
tests/lab-test.c:1205:test_read_reply_reports_a_server_that_hangs_up_mid_reply:PASS
tests/lab-test.c:1206:test_read_reply_rejects_bad_arguments:PASS
tests/lab-test.c:1208:test_write_all_sends_the_whole_buffer:PASS
tests/lab-test.c:1209:test_write_all_retries_until_a_short_writing_transport_takes_it_all:PASS
tests/lab-test.c:1210:test_write_all_accepts_a_zero_length_write:PASS
tests/lab-test.c:1211:test_write_all_reports_a_transport_failure:PASS
tests/lab-test.c:1212:test_write_all_reports_a_transport_that_accepts_nothing:PASS
tests/lab-test.c:1213:test_write_all_rejects_bad_arguments:PASS
tests/lab-test.c:1215:test_command_expect_sends_the_command_and_accepts_the_right_code:PASS
tests/lab-test.c:1216:test_command_expect_reports_the_wrong_code_rather_than_ignoring_it:PASS
tests/lab-test.c:1217:test_command_expect_propagates_a_write_failure:PASS
tests/lab-test.c:1218:test_command_expect_propagates_a_read_failure:PASS
tests/lab-test.c:1219:test_command_expect_rejects_a_null_command:PASS
tests/lab-test.c:1221:test_run_session_issues_every_command_in_the_required_order:PASS
tests/lab-test.c:1222:test_run_session_follows_continuations_throughout:PASS
tests/lab-test.c:1223:test_run_session_rejects_a_bad_greeting:PASS
tests/lab-test.c:1224:test_run_session_rejects_a_bad_helo_reply:PASS
tests/lab-test.c:1225:test_run_session_rejects_a_bad_mail_from_reply:PASS
tests/lab-test.c:1226:test_run_session_rejects_a_bad_rcpt_to_reply:PASS
tests/lab-test.c:1227:test_run_session_rejects_a_bad_data_reply:PASS
tests/lab-test.c:1228:test_run_session_rejects_a_bad_end_of_data_reply:PASS
tests/lab-test.c:1229:test_run_session_rejects_a_bad_quit_reply:PASS
tests/lab-test.c:1230:test_run_session_sends_quit_before_giving_up_on_a_bad_code:PASS
tests/lab-test.c:1231:test_run_session_reports_a_server_that_hangs_up_mid_session:PASS
tests/lab-test.c:1232:test_run_session_does_not_send_quit_to_a_peer_that_has_gone:PASS
tests/lab-test.c:1233:test_run_session_reports_a_greeting_that_never_arrives:PASS
tests/lab-test.c:1234:test_run_session_reports_a_malformed_reply:PASS
tests/lab-test.c:1235:test_run_session_reports_a_write_failure:PASS
tests/lab-test.c:1236:test_run_session_rejects_a_mail_with_no_sender:PASS
tests/lab-test.c:1237:test_run_session_rejects_bad_arguments:PASS
tests/lab-test.c:1239:test_connect_reaches_a_listening_socket:PASS
tests/lab-test.c:1240:test_connect_reports_a_refused_connection:PASS
tests/lab-test.c:1241:test_connect_reports_a_service_it_cannot_resolve:PASS
tests/lab-test.c:1242:test_connect_rejects_null_arguments:PASS
tests/lab-test.c:1243:test_socket_read_and_write_move_bytes_over_a_real_connection:PASS
tests/lab-test.c:1244:test_socket_read_and_write_reject_null_arguments:PASS
tests/lab-test.c:1245:test_disconnect_closes_the_socket_and_tolerates_a_negative_descriptor:PASS
tests/lab-test.c:1246:test_a_whole_session_runs_over_a_real_socket:PASS

-----------------------
91 Tests 0 Failures 0 Ignored 
OK
mkdir -p ./build/report/html
mkdir -p ./build/report/txt
gcovr -r . --html --html-details --exclude-directories build/tests/harness --exclude '.*main\.c$' --exclude '.*test\.c$' -o ./build/report/html/coverage_report.html
(INFO) Reading coverage data...

(INFO) Writing coverage report...

gcovr -r . --txt                 --exclude-directories build/tests/harness --exclude '.*main\.c$' --exclude '.*test\.c$'
(INFO) Reading coverage data...

(INFO) Writing coverage report...

------------------------------------------------------------------------------
                           GCC Code Coverage Report
Directory: .
------------------------------------------------------------------------------
File                                       Lines     Exec  Cover   Missing
------------------------------------------------------------------------------
src/lab.c                                    234      234   100%
------------------------------------------------------------------------------
TOTAL                                        234      234   100%
------------------------------------------------------------------------------
```

---

## Address Sanitizer Report

This section was generated by running `make leak-test` in the project root directory.

```bash
tests/lab-test.c:1142:test_reply_code_accepts_the_codes_the_session_needs:PASS
tests/lab-test.c:1143:test_reply_code_reads_a_continuation_line:PASS
tests/lab-test.c:1144:test_reply_code_accepts_a_bare_code_with_no_text:PASS
tests/lab-test.c:1145:test_reply_code_rejects_null:PASS
tests/lab-test.c:1146:test_reply_code_rejects_lines_shorter_than_three_digits:PASS
tests/lab-test.c:1147:test_reply_code_rejects_a_first_digit_outside_two_through_five:PASS
tests/lab-test.c:1148:test_reply_code_rejects_non_digits:PASS
tests/lab-test.c:1150:test_is_final_line_true_for_code_then_space:PASS
tests/lab-test.c:1151:test_is_final_line_false_for_code_then_hyphen:PASS
tests/lab-test.c:1152:test_is_final_line_true_for_a_bare_code:PASS
tests/lab-test.c:1153:test_is_final_line_ignores_digits_in_the_reply_text:PASS
tests/lab-test.c:1154:test_is_final_line_true_for_unparsable_lines:PASS
tests/lab-test.c:1156:test_text_is_safe_accepts_ordinary_values:PASS
tests/lab-test.c:1157:test_text_is_safe_rejects_null:PASS
tests/lab-test.c:1158:test_text_is_safe_rejects_embedded_line_breaks:PASS
tests/lab-test.c:1159:test_text_is_safe_rejects_an_injected_command:PASS
tests/lab-test.c:1161:test_build_command_joins_verb_and_argument_with_a_space:PASS
tests/lab-test.c:1162:test_build_command_omits_the_space_when_there_is_no_argument:PASS
tests/lab-test.c:1163:test_build_command_rejects_a_null_verb:PASS
tests/lab-test.c:1165:test_build_path_command_brackets_the_address_and_hugs_the_colon:PASS
tests/lab-test.c:1166:test_build_path_command_rejects_null_arguments:PASS
tests/lab-test.c:1168:test_manage_dot_rejects_null:PASS
tests/lab-test.c:1169:test_manage_dot_leaves_an_empty_body_empty:PASS
tests/lab-test.c:1170:test_manage_dot_terminates_an_unterminated_final_line:PASS
tests/lab-test.c:1171:test_manage_dot_does_not_add_a_second_terminator:PASS
tests/lab-test.c:1172:test_manage_dot_converts_lf_to_crlf:PASS
tests/lab-test.c:1173:test_manage_dot_doubles_a_leading_period:PASS
tests/lab-test.c:1174:test_manage_dot_leaves_interior_periods_alone:PASS
tests/lab-test.c:1175:test_manage_dot_stuffs_every_line_not_just_the_first:PASS
tests/lab-test.c:1176:test_manage_dot_drops_a_bare_carriage_return:PASS
tests/lab-test.c:1178:test_build_message_assembles_headers_blank_line_body_and_dot:PASS
tests/lab-test.c:1179:test_build_message_treats_a_null_subject_as_empty:PASS
tests/lab-test.c:1180:test_build_message_treats_a_null_body_as_empty:PASS
tests/lab-test.c:1181:test_build_message_stuffs_the_body_it_embeds:PASS
tests/lab-test.c:1182:test_build_message_rejects_a_null_sender_or_recipient:PASS
tests/lab-test.c:1184:test_status_text_describes_every_value:PASS
tests/lab-test.c:1186:test_session_init_stores_the_transport_and_empties_the_buffer:PASS
tests/lab-test.c:1188:test_read_line_strips_the_crlf:PASS
tests/lab-test.c:1189:test_read_line_hands_out_two_lines_from_a_single_read:PASS
tests/lab-test.c:1190:test_read_line_reassembles_a_line_delivered_one_byte_at_a_time:PASS
tests/lab-test.c:1191:test_read_line_keeps_a_cr_that_is_not_followed_by_lf:PASS
tests/lab-test.c:1192:test_read_line_reports_a_transport_failure:PASS
tests/lab-test.c:1193:test_read_line_reports_a_peer_that_closed_before_the_crlf:PASS
tests/lab-test.c:1194:test_read_line_reports_a_line_too_long_for_the_session_buffer:PASS
tests/lab-test.c:1195:test_read_line_reports_a_line_too_long_for_the_callers_buffer:PASS
tests/lab-test.c:1196:test_read_line_rejects_bad_arguments:PASS
tests/lab-test.c:1198:test_read_reply_returns_the_code_of_a_single_line_reply:PASS
tests/lab-test.c:1199:test_read_reply_follows_continuation_lines:PASS
tests/lab-test.c:1200:test_read_reply_handles_reply_text_that_begins_with_digits:PASS
tests/lab-test.c:1201:test_read_reply_accepts_a_multiline_greeting:PASS
tests/lab-test.c:1202:test_read_reply_accepts_a_reply_that_is_only_a_code:PASS
tests/lab-test.c:1203:test_read_reply_reassembles_a_multiline_reply_from_fragments:PASS
tests/lab-test.c:1204:test_read_reply_rejects_a_line_that_is_not_a_reply:PASS
tests/lab-test.c:1205:test_read_reply_reports_a_server_that_hangs_up_mid_reply:PASS
tests/lab-test.c:1206:test_read_reply_rejects_bad_arguments:PASS
tests/lab-test.c:1208:test_write_all_sends_the_whole_buffer:PASS
tests/lab-test.c:1209:test_write_all_retries_until_a_short_writing_transport_takes_it_all:PASS
tests/lab-test.c:1210:test_write_all_accepts_a_zero_length_write:PASS
tests/lab-test.c:1211:test_write_all_reports_a_transport_failure:PASS
tests/lab-test.c:1212:test_write_all_reports_a_transport_that_accepts_nothing:PASS
tests/lab-test.c:1213:test_write_all_rejects_bad_arguments:PASS
tests/lab-test.c:1215:test_command_expect_sends_the_command_and_accepts_the_right_code:PASS
tests/lab-test.c:1216:test_command_expect_reports_the_wrong_code_rather_than_ignoring_it:PASS
tests/lab-test.c:1217:test_command_expect_propagates_a_write_failure:PASS
tests/lab-test.c:1218:test_command_expect_propagates_a_read_failure:PASS
tests/lab-test.c:1219:test_command_expect_rejects_a_null_command:PASS
tests/lab-test.c:1221:test_run_session_issues_every_command_in_the_required_order:PASS
tests/lab-test.c:1222:test_run_session_follows_continuations_throughout:PASS
tests/lab-test.c:1223:test_run_session_rejects_a_bad_greeting:PASS
tests/lab-test.c:1224:test_run_session_rejects_a_bad_helo_reply:PASS
tests/lab-test.c:1225:test_run_session_rejects_a_bad_mail_from_reply:PASS
tests/lab-test.c:1226:test_run_session_rejects_a_bad_rcpt_to_reply:PASS
tests/lab-test.c:1227:test_run_session_rejects_a_bad_data_reply:PASS
tests/lab-test.c:1228:test_run_session_rejects_a_bad_end_of_data_reply:PASS
tests/lab-test.c:1229:test_run_session_rejects_a_bad_quit_reply:PASS
tests/lab-test.c:1230:test_run_session_sends_quit_before_giving_up_on_a_bad_code:PASS
tests/lab-test.c:1231:test_run_session_reports_a_server_that_hangs_up_mid_session:PASS
tests/lab-test.c:1232:test_run_session_does_not_send_quit_to_a_peer_that_has_gone:PASS
tests/lab-test.c:1233:test_run_session_reports_a_greeting_that_never_arrives:PASS
tests/lab-test.c:1234:test_run_session_reports_a_malformed_reply:PASS
tests/lab-test.c:1235:test_run_session_reports_a_write_failure:PASS
tests/lab-test.c:1236:test_run_session_rejects_a_mail_with_no_sender:PASS
tests/lab-test.c:1237:test_run_session_rejects_bad_arguments:PASS
tests/lab-test.c:1239:test_connect_reaches_a_listening_socket:PASS
tests/lab-test.c:1240:test_connect_reports_a_refused_connection:PASS
tests/lab-test.c:1241:test_connect_reports_a_service_it_cannot_resolve:PASS
tests/lab-test.c:1242:test_connect_rejects_null_arguments:PASS
tests/lab-test.c:1243:test_socket_read_and_write_move_bytes_over_a_real_connection:PASS
tests/lab-test.c:1244:test_socket_read_and_write_reject_null_arguments:PASS
tests/lab-test.c:1245:test_disconnect_closes_the_socket_and_tolerates_a_negative_descriptor:PASS
tests/lab-test.c:1246:test_a_whole_session_runs_over_a_real_socket:PASS

-----------------------
91 Tests 0 Failures 0 Ignored 
OK
```

---

## Src Files
### lab.c

```c

#include "lab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int smtp_reply_code(const char *line)
{
    if (line == NULL)
    {
        return -1;
    }

    /* Tested in order, so a short line stops at its NUL before any index
       past the end of the string is read. */
    if (line[0] < '2' || line[0] > '5')
    {
        return -1;
    }
    if (line[1] < '0' || line[1] > '9')
    {
        return -1;
    }
    if (line[2] < '0' || line[2] > '9')
    {
        return -1;
    }

    return (line[0] - '0') * 100 + (line[1] - '0') * 10 + (line[2] - '0');
}

bool smtp_is_final_line(const char *line)
{
    if (smtp_reply_code(line) < 0)
    {
        return true;
    }

    /* A valid code guarantees three characters, so index 3 is in bounds: it
       is either the fourth character or the NUL that ends a bare code. */
    return line[3] != '-';
}

bool smtp_text_is_safe(const char *text)
{
    if (text == NULL)
    {
        return false;
    }

    for (size_t i = 0; text[i] != '\0'; i++)
    {
        if (text[i] == '\r' || text[i] == '\n')
        {
            return false;
        }
    }

    return true;
}

char *smtp_build_command(const char *verb, const char *arg)
{
    if (verb == NULL)
    {
        return NULL;
    }

    const char *sep = " ";
    if (arg == NULL)
    {
        arg = "";
        sep = "";
    }

    /* verb, separator, argument, CRLF, NUL */
    size_t size = strlen(verb) + strlen(sep) + strlen(arg) + 3;
    char *command = malloc(size);
    if (command == NULL) // GCOVR_EXCL_START
    {
        return NULL;
    } // GCOVR_EXCL_STOP

    snprintf(command, size, "%s%s%s\r\n", verb, sep, arg);
    return command;
}

char *smtp_build_path_command(const char *verb, const char *address)
{
    if (verb == NULL || address == NULL)
    {
        return NULL;
    }

    /* verb, ":<", address, ">", CRLF, NUL */
    size_t size = strlen(verb) + strlen(address) + 6;
    char *command = malloc(size);
    if (command == NULL) // GCOVR_EXCL_START
    {
        return NULL;
    } // GCOVR_EXCL_STOP

    snprintf(command, size, "%s:<%s>\r\n", verb, address);
    return command;
}

char *smtp_manage_dot(const char *body)
{
    if (body == NULL)
    {
        return NULL;
    }

    /* Each input character yields at most two output characters: an LF
       becomes CRLF, and a period at the start of a line becomes two. Add two
       more for a CRLF on an unterminated final line, and one for the NUL. */
    size_t length = strlen(body);
    char *out = malloc(2 * length + 3);
    if (out == NULL) // GCOVR_EXCL_START
    {
        return NULL;
    } // GCOVR_EXCL_STOP

    size_t written = 0;
    bool at_line_start = true;

    for (size_t i = 0; i < length; i++)
    {
        char c = body[i];

        if (c == '\r')
        {
            continue;
        }

        if (c == '\n')
        {
            out[written++] = '\r';
            out[written++] = '\n';
            at_line_start = true;
            continue;
        }

        if (at_line_start && c == '.')
        {
            out[written++] = '.';
        }

        out[written++] = c;
        at_line_start = false;
    }

    if (!at_line_start)
    {
        out[written++] = '\r';
        out[written++] = '\n';
    }

    out[written] = '\0';
    return out;
}

char *smtp_build_message(const char *from, const char *to,
                         const char *subject, const char *body)
{
    if (from == NULL || to == NULL)
    {
        return NULL;
    }

    if (subject == NULL)
    {
        subject = "";
    }
    if (body == NULL)
    {
        body = "";
    }

    char *stuffed = smtp_manage_dot(body);
    if (stuffed == NULL) // GCOVR_EXCL_START
    {
        return NULL;
    } // GCOVR_EXCL_STOP

    /* "From: " 6, CRLF 2, "To: " 4, CRLF 2, "Subject: " 9, CRLF 2,
       blank line 2, ".", CRLF 3, NUL 1. */
    size_t size = strlen(from) + strlen(to) + strlen(subject)
                + strlen(stuffed) + 31;
    char *message = malloc(size);
    if (message == NULL) // GCOVR_EXCL_START
    {
        free(stuffed);
        return NULL;
    } // GCOVR_EXCL_STOP

    snprintf(message, size,
             "From: %s\r\n"
             "To: %s\r\n"
             "Subject: %s\r\n"
             "\r\n"
             "%s"
             ".\r\n",
             from, to, subject, stuffed);

    free(stuffed);
    return message;
}

/* ------------------------------------------------------------------------ */
/* Layer 2 - the session, over a swappable transport.                        */
/* ------------------------------------------------------------------------ */

void smtp_session_init(struct smtp_session *session, smtp_read_fn read,
                       smtp_write_fn write, void *ctx)
{
    if (session == NULL)
    {
        return;
    }

    session->read = read;
    session->write = write;
    session->ctx = ctx;
    session->held = 0;
}

smtp_status smtp_read_line(struct smtp_session *session, char *out,
                           size_t out_size)
{
    if (session == NULL || out == NULL || out_size == 0)
    {
        return SMTP_ERR_ARGS;
    }

    for (;;)
    {
        /* A complete line is already held whenever a CRLF appears in the
           buffer. Only when none does is the transport asked for more. */
        for (size_t i = 0; i + 1 < session->held; i++)
        {
            if (session->buffer[i] != '\r' || session->buffer[i + 1] != '\n')
            {
                continue;
            }

            if (i >= out_size)
            {
                return SMTP_ERR_OVERFLOW;
            }

            memcpy(out, session->buffer, i);
            out[i] = '\0';

            /* Keep whatever followed the CRLF; it belongs to the next line. */
            size_t consumed = i + 2;
            session->held -= consumed;
            memmove(session->buffer, session->buffer + consumed, session->held);
            return SMTP_OK;
        }

        if (session->held == sizeof session->buffer)
        {
            return SMTP_ERR_OVERFLOW;
        }

        ssize_t n = session->read(session->ctx, session->buffer + session->held,
                                  sizeof session->buffer - session->held);
        if (n < 0)
        {
            return SMTP_ERR_TRANSPORT;
        }
        if (n == 0)
        {
            return SMTP_ERR_CLOSED;
        }

        /* The check above makes the cast safe. */
        session->held += (size_t)n;
    }
}

smtp_status smtp_read_reply(struct smtp_session *session, int *code_out,
                            char *line_out, size_t line_size)
{
    if (session == NULL || code_out == NULL || line_out == NULL
        || line_size == 0)
    {
        return SMTP_ERR_ARGS;
    }

    for (;;)
    {
        smtp_status status = smtp_read_line(session, line_out, line_size);
        if (status != SMTP_OK)
        {
            return status;
        }

        int code = smtp_reply_code(line_out);
        if (code < 0)
        {
            return SMTP_ERR_PROTOCOL;
        }

        if (smtp_is_final_line(line_out))
        {
            *code_out = code;
            return SMTP_OK;
        }
    }
}

const char *smtp_status_text(smtp_status status)
{
    switch (status)
    {
    case SMTP_OK:
        return "ok";
    case SMTP_ERR_ARGS:
        return "invalid argument";
    case SMTP_ERR_TRANSPORT:
        return "transport failure";
    case SMTP_ERR_CLOSED:
        return "server closed the connection";
    case SMTP_ERR_OVERFLOW:
        return "line too long";
    case SMTP_ERR_PROTOCOL:
        return "malformed reply";
    }

    return "unknown error";
}

smtp_status smtp_write_all(struct smtp_session *session, const char *data,
                           size_t length)
{
    if (session == NULL || data == NULL)
    {
        return SMTP_ERR_ARGS;
    }

    size_t sent = 0;
    while (sent < length)
    {
        ssize_t n = session->write(session->ctx, data + sent, length - sent);
        if (n < 0)
        {
            return SMTP_ERR_TRANSPORT;
        }
        if (n == 0)
        {
            return SMTP_ERR_CLOSED;
        }

        /* The checks above make the cast safe. */
        sent += (size_t)n;
    }

    return SMTP_OK;
}

smtp_status smtp_command_expect(struct smtp_session *session,
                                const char *command, int expected,
                                char *line_out, size_t line_size)
{
    if (command == NULL)
    {
        return SMTP_ERR_ARGS;
    }

    smtp_status status = smtp_write_all(session, command, strlen(command));
    if (status != SMTP_OK)
    {
        return status;
    }

    int code = 0;
    status = smtp_read_reply(session, &code, line_out, line_size);
    if (status != SMTP_OK)
    {
        return status;
    }

    if (code != expected)
    {
        return SMTP_ERR_PROTOCOL;
    }

    return SMTP_OK;
}

/**
 * Send QUIT and return the failure that made it necessary. RFC 5321 section
 * 4.1.1.10 forbids closing the channel without one, so this runs on every
 * error path that still has a usable connection. Whatever the server replies
 * is not read: line_out holds the reply the caller needs to report, and with
 * no timeout a wait on an unresponsive server would never return.
 */
static smtp_status abandon(struct smtp_session *session, smtp_status reason)
{
    if (reason == SMTP_ERR_TRANSPORT || reason == SMTP_ERR_CLOSED)
    {
        return reason;
    }

    char *quit = smtp_build_command("QUIT", NULL);
    if (quit == NULL) // GCOVR_EXCL_START
    {
        return reason;
    } // GCOVR_EXCL_STOP

    (void)smtp_write_all(session, quit, strlen(quit));
    free(quit);
    return reason;
}

/**
 * Send one built command and free it, whichever way the exchange goes. Takes
 * ownership of @p command, which is NULL when the builder rejected its input.
 */
static smtp_status send_owned(struct smtp_session *session, char *command,
                              int expected, char *line_out, size_t line_size)
{
    if (command == NULL)
    {
        return SMTP_ERR_ARGS;
    }

    smtp_status status = smtp_command_expect(session, command, expected,
                                             line_out, line_size);
    free(command);
    return status;
}

smtp_status smtp_run_session(struct smtp_session *session,
                             const struct smtp_mail *mail, char *line_out,
                             size_t line_size)
{
    if (session == NULL || mail == NULL || line_out == NULL || line_size == 0)
    {
        return SMTP_ERR_ARGS;
    }

    /* The server speaks first; nothing is sent until it has. Section 3.1. */
    int code = 0;
    smtp_status status = smtp_read_reply(session, &code, line_out, line_size);
    if (status != SMTP_OK)
    {
        return abandon(session, status);
    }
    if (code != 220)
    {
        return abandon(session, SMTP_ERR_PROTOCOL);
    }

    status = send_owned(session, smtp_build_command("HELO", mail->helo_host),
                        250, line_out, line_size);
    if (status != SMTP_OK)
    {
        return abandon(session, status);
    }

    status = send_owned(session,
                        smtp_build_path_command("MAIL FROM", mail->from), 250,
                        line_out, line_size);
    if (status != SMTP_OK)
    {
        return abandon(session, status);
    }

    status = send_owned(session, smtp_build_path_command("RCPT TO", mail->to),
                        250, line_out, line_size);
    if (status != SMTP_OK)
    {
        return abandon(session, status);
    }

    status = send_owned(session, smtp_build_command("DATA", NULL), 354,
                        line_out, line_size);
    if (status != SMTP_OK)
    {
        return abandon(session, status);
    }

    status = send_owned(session,
                        smtp_build_message(mail->from, mail->to, mail->subject,
                                           mail->body),
                        250, line_out, line_size);
    if (status != SMTP_OK)
    {
        return abandon(session, status);
    }

    /* QUIT is the last exchange, so a bad reply here needs no second QUIT. */
    return send_owned(session, smtp_build_command("QUIT", NULL), 221, line_out,
                      line_size);
}

/* ------------------------------------------------------------------------ */
/* Layer 3 - the socket transport.                                           */
/* ------------------------------------------------------------------------ */

int smtp_connect(const char *host, const char *port)
{
    if (host == NULL || port == NULL)
    {
        return -1;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     /* accept IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* TCP */

    struct addrinfo *addresses = NULL;
    if (getaddrinfo(host, port, &hints, &addresses) != 0)
    {
        return -1;
    }

    int fd = -1;
    for (struct addrinfo *a = addresses; a != NULL; a = a->ai_next)
    {
        fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (fd < 0) // GCOVR_EXCL_START
        {
            continue;
        } // GCOVR_EXCL_STOP

        if (connect(fd, a->ai_addr, a->ai_addrlen) == 0)
        {
            break;
        }

        close(fd);
        fd = -1;
    }

    freeaddrinfo(addresses);
    return fd;
}

void smtp_disconnect(int fd)
{
    if (fd >= 0)
    {
        close(fd);
    }
}

ssize_t smtp_socket_read(void *ctx, char *buffer, size_t capacity)
{
    if (ctx == NULL || buffer == NULL)
    {
        return -1;
    }

    return recv(*(const int *)ctx, buffer, capacity, 0);
}

ssize_t smtp_socket_write(void *ctx, const char *data, size_t length)
{
    if (ctx == NULL || data == NULL)
    {
        return -1;
    }

    /* MSG_NOSIGNAL turns a write to a closed connection into an EPIPE return
       rather than a SIGPIPE that would terminate the process. */
    return send(*(const int *)ctx, data, length, MSG_NOSIGNAL);
}

```

### lab.h

```c

#ifndef LAB_H
#define LAB_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

/* ------------------------------------------------------------------------ */
/* Layer 1 - pure protocol helpers.                                          */
/*                                                                           */
/* Strings and integers in, strings and integers out. No sockets, no file    */
/* descriptors, no I/O of any kind, no global state.                         */
/* ------------------------------------------------------------------------ */

/**
 * @brief Extract the three-digit status code from one SMTP reply line.
 *
 * A reply line begins with three ASCII digits (RFC 5321 section 4.2). The
 * first digit carries the severity and a conforming server sends only '2'
 * through '5'; a code outside that range is treated here as unparsable, which
 * matches the RFC's advice that clients treat out-of-range codes as fatal.
 *
 * @param line One reply line, NUL terminated, with its CRLF already removed.
 *             May be NULL.
 * @return The status code in the range 200-599, or -1 if @p line does not
 *         begin with a valid three-digit code.
 */
int smtp_reply_code(const char *line);

/**
 * @brief Decide whether a reply line is the last line of its reply.
 *
 * A multiline reply repeats the same code on every line, with a hyphen after
 * the code on all but the last line and a space after the code on the last
 * (RFC 5321 section 4.2.1). Servers may omit the space and the text entirely,
 * so a line consisting of nothing but the code is a final line.
 *
 * A line that does not parse as a reply at all is reported as final, so that
 * a caller which neglects to check the code cannot loop forever.
 *
 * @param line One reply line, NUL terminated, with its CRLF already removed.
 *             May be NULL.
 * @return true if this line ends the reply, false if more lines follow.
 */
bool smtp_is_final_line(const char *line);

/**
 * @brief Report whether a string is safe to place in a command or a header.
 *
 * RFC 5321 section 2.3.8 forbids a client from transmitting CR or LF except
 * as a line terminator. A CR or LF inside an address or a subject would end
 * the current line early and let the remainder be read as a new SMTP command
 * or a new message header, so such a string must be rejected before it is
 * sent rather than escaped.
 *
 * @param text The string to check. May be NULL.
 * @return true if @p text is non-NULL and contains no CR and no LF.
 */
bool smtp_text_is_safe(const char *text);

/**
 * @brief Build a command line of the form "VERB arg\r\n".
 *
 * Used for the commands whose argument is separated from the verb by a space:
 * HELO, and (with no argument) DATA and QUIT.
 *
 * @param verb The command word, for example "HELO". May not be NULL.
 * @param arg  The argument, or NULL for a command that takes none.
 * @return A newly allocated command line that the caller must free, or NULL
 *         on bad input or allocation failure.
 */
char *smtp_build_command(const char *verb, const char *arg);

/**
 * @brief Build a command line of the form "VERB:<address>\r\n".
 *
 * Used for MAIL FROM and RCPT TO. RFC 5321 section 3.3 forbids whitespace on
 * either side of the colon, and section 4.1.2 makes the angle brackets part
 * of the path syntax rather than decoration; this function is the one place
 * that knows both rules.
 *
 * @param verb    "MAIL FROM" or "RCPT TO". May not be NULL.
 * @param address The bare mailbox, without angle brackets. May not be NULL.
 * @return A newly allocated command line that the caller must free, or NULL
 *         on bad input or allocation failure.
 */
char *smtp_build_path_command(const char *verb, const char *address);

/**
 * @brief Convert a message body into the exact bytes that follow DATA.
 *
 * Performs the two transformations a body needs before it can be sent:
 * every line is terminated with CRLF, and a line beginning with a period
 * gets one extra period prepended so the server does not mistake it for the
 * end of data (RFC 5321 section 4.5.2). A CR is never copied through, which
 * satisfies the prohibition on transmitting a bare CR.
 *
 * The result always ends with exactly one CRLF, or is empty when @p body is
 * empty. It never carries the end-of-data marker; smtp_build_message() adds
 * that.
 *
 * @param body The message body, with lines separated by LF or CRLF. May not
 *             be NULL, but may be empty.
 * @return A newly allocated string that the caller must free, or NULL on bad
 *         input or allocation failure.
 */
char *smtp_manage_dot(const char *body);

/**
 * @brief Build the complete payload sent between DATA and its final reply.
 *
 * The payload is the From, To and Subject headers, a blank line, the dot
 * stuffed body, and the line containing a single period that ends the data
 * (RFC 5321 section 4.1.1.4). Because smtp_manage_dot() already terminates
 * the last body line, no extra CRLF is inserted before the period; adding
 * one would append an empty line to the message.
 *
 * Header values are not checked here. Call smtp_text_is_safe() on them while
 * the command line is being parsed, so that a CR or LF is reported as the
 * command line error it is.
 *
 * @param from    Envelope sender, also written to the From header.
 * @param to      Envelope recipient, also written to the To header.
 * @param subject Subject text, or NULL for an empty subject.
 * @param body    Message body, or NULL for an empty body.
 * @return A newly allocated payload that the caller must free, or NULL if
 *         @p from or @p to is NULL, or on allocation failure.
 */
char *smtp_build_message(const char *from, const char *to,
                         const char *subject, const char *body);

/* ------------------------------------------------------------------------ */
/* Layer 2 - the session, driven over a transport that can be swapped.       */
/*                                                                           */
/* Nothing below calls recv() or send(). All reading and writing goes        */
/* through the two callbacks held in struct smtp_session, so the same code   */
/* runs against a socket in the client and against a string in the tests.    */
/* ------------------------------------------------------------------------ */

/**
 * Largest line this client will read or hold. RFC 5321 section 4.5.3.1.5
 * caps a reply line at 512 octets including the CRLF; that is a minimum a
 * client must accept, not a promise about what a server will send, so the
 * buffer is larger and a line that still does not fit is a clean error.
 */
#define SMTP_LINE_MAX 1024

/** Outcome of a session-layer operation. */
typedef enum
{
    SMTP_OK = 0,        /**< The operation succeeded. */
    SMTP_ERR_ARGS,      /**< A NULL or zero-sized argument was supplied. */
    SMTP_ERR_TRANSPORT, /**< The read or write callback reported a failure. */
    SMTP_ERR_CLOSED,    /**< The peer closed the connection mid-session. */
    SMTP_ERR_OVERFLOW,  /**< A line was longer than the buffer holding it. */
    SMTP_ERR_PROTOCOL   /**< A reply was malformed or carried a bad code. */
} smtp_status;

/**
 * @brief Read bytes from wherever this session's transport gets them.
 *
 * @param ctx      The context pointer stored in the session.
 * @param buffer   Destination for the bytes.
 * @param capacity Maximum number of bytes to store.
 * @return The number of bytes read, 0 if the peer has closed, or a negative
 *         value on failure. A short read is normal and is not an error.
 */
typedef ssize_t (*smtp_read_fn)(void *ctx, char *buffer, size_t capacity);

/**
 * @brief Write bytes to wherever this session's transport sends them.
 *
 * @param ctx    The context pointer stored in the session.
 * @param data   Bytes to send.
 * @param length Number of bytes to send.
 * @return The number of bytes written, or a negative value on failure. A
 *         short write is normal and is not an error.
 */
typedef ssize_t (*smtp_write_fn)(void *ctx, const char *data, size_t length);

/**
 * One SMTP conversation. The buffer holds bytes that have arrived from the
 * transport but have not yet been handed out as a complete line, which is
 * what lets the reader cope with a reply split across several reads and with
 * several replies arriving in one.
 */
struct smtp_session
{
    smtp_read_fn read;             /**< Transport read callback. */
    smtp_write_fn write;           /**< Transport write callback. */
    void *ctx;                     /**< Passed to both callbacks. */

    char buffer[SMTP_LINE_MAX];    /**< Bytes received, not yet consumed. */
    size_t held;                   /**< How many bytes of buffer are in use. */
};

/**
 * @brief Prepare a session to run over the given transport.
 *
 * @param session The session to initialise. Ignored if NULL.
 * @param read    Read callback.
 * @param write   Write callback.
 * @param ctx     Opaque pointer handed to both callbacks.
 */
void smtp_session_init(struct smtp_session *session, smtp_read_fn read,
                       smtp_write_fn write, void *ctx);

/**
 * @brief Read one CRLF-terminated line from the transport.
 *
 * Bytes already buffered are searched first and the transport is only asked
 * for more when the buffer does not already contain a complete line. Anything
 * past the end of the returned line stays buffered for the next call, so two
 * replies delivered in a single read are handed out as two lines.
 *
 * The CRLF is stripped and @p out is NUL terminated, which is the form every
 * layer 1 helper expects.
 *
 * @param session  An initialised session.
 * @param out      Destination for the line.
 * @param out_size Size of @p out, including room for the NUL.
 * @return SMTP_OK, or SMTP_ERR_ARGS, SMTP_ERR_TRANSPORT, SMTP_ERR_CLOSED if
 *         the peer hung up before a line was complete, or SMTP_ERR_OVERFLOW
 *         if the line did not fit in @p out or in the session buffer.
 */
smtp_status smtp_read_line(struct smtp_session *session, char *out,
                           size_t out_size);

/**
 * @brief Read one complete reply, following continuation lines.
 *
 * Reads lines until one is final (RFC 5321 section 4.2.1). Every line must
 * parse as a reply; the code is taken from the last, which the RFC permits
 * because all lines of a reply carry the same code.
 *
 * @param session   An initialised session.
 * @param code_out  Receives the status code on success.
 * @param line_out  Receives the final line, text included, so the caller can
 *                  report what the server actually said.
 * @param line_size Size of @p line_out, including room for the NUL.
 * @return SMTP_OK, or the failure from smtp_read_line(), or
 *         SMTP_ERR_PROTOCOL if a line did not begin with a valid code.
 */
smtp_status smtp_read_reply(struct smtp_session *session, int *code_out,
                            char *line_out, size_t line_size);

/**
 * @brief Describe a status value for an error message.
 *
 * @param status The value to describe.
 * @return A constant string. Never NULL, even for an unrecognised value.
 */
const char *smtp_status_text(smtp_status status);

/**
 * @brief Write every byte, retrying until the transport has taken them all.
 *
 * A transport is free to accept fewer bytes than it was offered, so one call
 * to the write callback is not enough to know a command was sent.
 *
 * @param session An initialised session.
 * @param data    Bytes to send.
 * @param length  Number of bytes to send.
 * @return SMTP_OK, SMTP_ERR_ARGS, SMTP_ERR_TRANSPORT, or SMTP_ERR_CLOSED.
 */
smtp_status smtp_write_all(struct smtp_session *session, const char *data,
                           size_t length);

/**
 * @brief Send one command, read its reply, and require a particular code.
 *
 * This is the whole request-response step of the protocol: RFC 5321 section
 * 4.2 requires every command to produce exactly one reply, and section 4.3.1
 * requires the client to wait for it before sending anything else.
 *
 * @param session   An initialised session.
 * @param command   The command line, CRLF included.
 * @param expected  The status code the protocol requires at this point.
 * @param line_out  Receives the server's final reply line, so a caller can
 *                  report what the server actually said.
 * @param line_size Size of @p line_out.
 * @return SMTP_OK, the failure from the write or the read, or
 *         SMTP_ERR_PROTOCOL if the reply carried a different code.
 */
smtp_status smtp_command_expect(struct smtp_session *session,
                                const char *command, int expected,
                                char *line_out, size_t line_size);

/** The message a session is being asked to hand over. */
struct smtp_mail
{
    const char *helo_host; /**< Name sent with HELO. */
    const char *from;      /**< Envelope sender, also the From header. */
    const char *to;        /**< Envelope recipient, also the To header. */
    const char *subject;   /**< Subject header text, or NULL for empty. */
    const char *body;      /**< Message body, or NULL for empty. */
};

/**
 * @brief Run one complete SMTP transaction from greeting to QUIT.
 *
 * Reads the greeting, then issues HELO, MAIL FROM, RCPT TO, DATA, the message
 * and QUIT, requiring 220, 250, 250, 250, 354, 250 and 221 in that order. The
 * ordering is mandatory: RFC 5321 section 3.3 ends with "Mail transaction
 * commands MUST be used in the order discussed above."
 *
 * On any protocol failure a QUIT is sent before returning, because section
 * 4.1.1.10 forbids a sender from closing the channel without one, even after
 * an error. The reply to that QUIT is not waited for; with no timeouts in
 * this client, waiting on a server that has stopped responding would hang
 * instead of exiting.
 *
 * @param session   An initialised session.
 * @param mail      What to send. @c from and @c to may not be NULL.
 * @param line_out  Receives the last reply line read, which on failure is the
 *                  reply that broke the sequence.
 * @param line_size Size of @p line_out.
 * @return SMTP_OK when the server has queued the message, otherwise the
 *         failure that ended the session.
 */
smtp_status smtp_run_session(struct smtp_session *session,
                             const struct smtp_mail *mail, char *line_out,
                             size_t line_size);

/* ------------------------------------------------------------------------ */
/* Layer 3 - the socket transport.                                           */
/*                                                                           */
/* Thin wrappers over getaddrinfo, connect, recv and send that satisfy the   */
/* two callbacks above. Deliberately thin: logic placed here cannot be       */
/* tested without a network, so it belongs in layer 1 or layer 2 instead.    */
/* ------------------------------------------------------------------------ */

/**
 * @brief Open a TCP connection to a named host.
 *
 * The host is resolved with getaddrinfo rather than assumed to be a dotted
 * quad, and every address it returns is tried in turn, so a name that has
 * both IPv6 and IPv4 records still connects on a host that can only reach
 * one of them.
 *
 * @param host Host name or address literal.
 * @param port Port number or service name, as a string.
 * @return A connected socket descriptor, or -1 if the name could not be
 *         resolved or no address would accept a connection.
 */
int smtp_connect(const char *host, const char *port);

/**
 * @brief Close a descriptor returned by smtp_connect().
 *
 * @param fd The descriptor. A negative value is ignored, so this is safe to
 *           call on the failure path without checking first.
 */
void smtp_disconnect(int fd);

/**
 * @brief Read callback backed by a socket.
 *
 * @param ctx      Pointer to the int descriptor to read from.
 * @param buffer   Destination for the bytes.
 * @param capacity Maximum number of bytes to store.
 * @return Whatever recv() reports, or -1 for a NULL argument.
 */
ssize_t smtp_socket_read(void *ctx, char *buffer, size_t capacity);

/**
 * @brief Write callback backed by a socket.
 *
 * Sends with MSG_NOSIGNAL, so writing to a connection the peer has already
 * closed returns EPIPE instead of raising SIGPIPE and killing the process.
 *
 * @param ctx    Pointer to the int descriptor to write to.
 * @param data   Bytes to send.
 * @param length Number of bytes to send.
 * @return Whatever send() reports, or -1 for a NULL argument.
 */
ssize_t smtp_socket_write(void *ctx, const char *data, size_t length);

#endif // LAB_H

```

### main.c

```c

#include "lab.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef TEST
#define main main_exclude
#endif

/** Exit statuses required by the assignment. */
enum
{
    EXIT_QUEUED = 0,  /**< The server accepted the message, or usage was asked for. */
    EXIT_BAD_USAGE = 1, /**< The command line was wrong. */
    EXIT_SESSION_FAILED = 2 /**< The connection or the SMTP session failed. */
};

/**
 * @brief Print the command line synopsis.
 *
 * @param out Stream to print to: stdout for a help request, stderr for a
 *            usage error.
 */
static void usage(FILE *out)
{
    fputs(
        "Usage: myapp -f <from> -t <to> [-s subject] [-b body] [-p port]\n"
        "          [-H helo-host] <server>\n"
        "\n"
        "  -f <from>       envelope sender, for example you@example.com\n"
        "  -t <to>         envelope recipient\n"
        "  -s <subject>    subject line (default: empty)\n"
        "  -b <body>       message body (default: read from stdin)\n"
        "  -p <port>       port or service name (default: 25)\n"
        "  -H <helo-host>  host name sent with HELO (default: localhost)\n"
        "  <server>        host name or address of the mail server\n",
        out);
}

/**
 * @brief Read all of standard input into one NUL terminated string.
 *
 * The buffer doubles as it fills, because the size of a piped message is not
 * known before it has been read.
 *
 * @return A newly allocated string the caller must free, or NULL if the read
 *         failed or memory ran out.
 */
static char *read_stdin(void)
{
    size_t capacity = 4096;
    size_t length = 0;
    char *buffer = malloc(capacity);
    if (buffer == NULL)
    {
        return NULL;
    }

    for (;;)
    {
        if (length + 1 >= capacity)
        {
            char *grown = realloc(buffer, capacity * 2);
            if (grown == NULL)
            {
                free(buffer);
                return NULL;
            }
            buffer = grown;
            capacity *= 2;
        }

        size_t taken = fread(buffer + length, 1, capacity - length - 1, stdin);
        length += taken;
        if (taken == 0)
        {
            break;
        }
    }

    if (ferror(stdin))
    {
        free(buffer);
        return NULL;
    }

    buffer[length] = '\0';
    return buffer;
}

int main(int argc, char *argv[])
{
    /* No arguments at all is a request for help, not an error. This is the
       path `make leak` exercises, so it must allocate nothing and exit 0. */
    if (argc == 1)
    {
        usage(stdout);
        return EXIT_QUEUED;
    }

    const char *from = NULL;
    const char *to = NULL;
    const char *subject = "";
    const char *body = NULL; /* NULL means read the body from stdin */
    const char *port = "25";
    const char *helo_host = "localhost";

    int opt;
    while ((opt = getopt(argc, argv, "f:t:s:b:p:H:")) != -1)
    {
        switch (opt)
        {
        case 'f':
            from = optarg;
            break;
        case 't':
            to = optarg;
            break;
        case 's':
            subject = optarg;
            break;
        case 'b':
            body = optarg;
            break;
        case 'p':
            port = optarg;
            break;
        case 'H':
            helo_host = optarg;
            break;
        default:
            usage(stderr);
            return EXIT_BAD_USAGE;
        }
    }

    if (from == NULL || to == NULL)
    {
        fprintf(stderr, "myapp: both -f and -t are required\n");
        usage(stderr);
        return EXIT_BAD_USAGE;
    }

    if (optind >= argc)
    {
        fprintf(stderr, "myapp: no mail server given\n");
        usage(stderr);
        return EXIT_BAD_USAGE;
    }
    if (optind + 1 < argc)
    {
        fprintf(stderr, "myapp: unexpected argument '%s'\n", argv[optind + 1]);
        usage(stderr);
        return EXIT_BAD_USAGE;
    }
    const char *server = argv[optind];

    /* A CR or LF in any of these would end a command line early and let the
       rest be read as a command of the sender's choosing. The body is exempt:
       its line breaks are legitimate and smtp_manage_dot() handles them. */
    if (!smtp_text_is_safe(from) || !smtp_text_is_safe(to)
        || !smtp_text_is_safe(subject) || !smtp_text_is_safe(helo_host))
    {
        fprintf(stderr,
                "myapp: sender, recipient, subject and HELO host must not "
                "contain a carriage return or line feed\n");
        return EXIT_BAD_USAGE;
    }

    char *piped = NULL;
    if (body == NULL)
    {
        piped = read_stdin();
        if (piped == NULL)
        {
            fprintf(stderr, "myapp: could not read the message body\n");
            return EXIT_SESSION_FAILED;
        }
        body = piped;
    }

    int fd = smtp_connect(server, port);
    if (fd < 0)
    {
        fprintf(stderr, "myapp: cannot connect to %s port %s\n", server, port);
        free(piped);
        return EXIT_SESSION_FAILED;
    }

    struct smtp_session session;
    smtp_session_init(&session, smtp_socket_read, smtp_socket_write, &fd);

    struct smtp_mail mail = {
        .helo_host = helo_host,
        .from = from,
        .to = to,
        .subject = subject,
        .body = body
    };

    char line[SMTP_LINE_MAX] = "";
    smtp_status status = smtp_run_session(&session, &mail, line, sizeof line);

    smtp_disconnect(fd);
    free(piped);

    if (status != SMTP_OK)
    {
        fprintf(stderr, "myapp: %s\n", smtp_status_text(status));
        if (line[0] != '\0')
        {
            fprintf(stderr, "myapp: server said: %s\n", line);
        }
        return EXIT_SESSION_FAILED;
    }

    return EXIT_QUEUED;
}

```

## Tests Files
### lab-test.c

```c

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "harness/unity.h"
#include "../src/lab.h"

void setUp(void) {}

void tearDown(void) {}

/* ------------------------------------------------------------------------ */
/* smtp_reply_code                                                           */
/* ------------------------------------------------------------------------ */

void test_reply_code_accepts_the_codes_the_session_needs(void)
{
    TEST_ASSERT_EQUAL_INT(220, smtp_reply_code("220 smtp.example.com ESMTP ready"));
    TEST_ASSERT_EQUAL_INT(250, smtp_reply_code("250 2.1.0 Ok"));
    TEST_ASSERT_EQUAL_INT(354, smtp_reply_code("354 End data with ."));
    TEST_ASSERT_EQUAL_INT(221, smtp_reply_code("221 Bye"));
}

void test_reply_code_reads_a_continuation_line(void)
{
    TEST_ASSERT_EQUAL_INT(250, smtp_reply_code("250-PIPELINING"));
}

void test_reply_code_accepts_a_bare_code_with_no_text(void)
{
    TEST_ASSERT_EQUAL_INT(250, smtp_reply_code("250"));
    TEST_ASSERT_EQUAL_INT(250, smtp_reply_code("250 "));
}

void test_reply_code_rejects_null(void)
{
    TEST_ASSERT_EQUAL_INT(-1, smtp_reply_code(NULL));
}

void test_reply_code_rejects_lines_shorter_than_three_digits(void)
{
    TEST_ASSERT_EQUAL_INT(-1, smtp_reply_code(""));
    TEST_ASSERT_EQUAL_INT(-1, smtp_reply_code("2"));
    TEST_ASSERT_EQUAL_INT(-1, smtp_reply_code("25"));
}

void test_reply_code_rejects_a_first_digit_outside_two_through_five(void)
{
    TEST_ASSERT_EQUAL_INT(-1, smtp_reply_code("150 too low"));
    TEST_ASSERT_EQUAL_INT(-1, smtp_reply_code("650 too high"));
}

void test_reply_code_rejects_non_digits(void)
{
    /* Each digit test is two comparisons, so each position is probed from
       both sides: a character below '0' and a character above '9'. */
    TEST_ASSERT_EQUAL_INT(-1, smtp_reply_code("2!0 below zero"));
    TEST_ASSERT_EQUAL_INT(-1, smtp_reply_code("2x0 above nine"));
    TEST_ASSERT_EQUAL_INT(-1, smtp_reply_code("25! below zero"));
    TEST_ASSERT_EQUAL_INT(-1, smtp_reply_code("25x above nine"));
    TEST_ASSERT_EQUAL_INT(-1, smtp_reply_code("hello"));
}

/* ------------------------------------------------------------------------ */
/* smtp_is_final_line                                                        */
/* ------------------------------------------------------------------------ */

void test_is_final_line_true_for_code_then_space(void)
{
    TEST_ASSERT_TRUE(smtp_is_final_line("250 smtp.example.com"));
    TEST_ASSERT_TRUE(smtp_is_final_line("221 Bye"));
}

void test_is_final_line_false_for_code_then_hyphen(void)
{
    TEST_ASSERT_FALSE(smtp_is_final_line("250-smtp.example.com"));
    TEST_ASSERT_FALSE(smtp_is_final_line("250-PIPELINING"));
}

void test_is_final_line_true_for_a_bare_code(void)
{
    /* RFC 5321 section 4.2: clients must be prepared for the space and the
       text to be omitted. Reading index 3 must not run off the end. */
    TEST_ASSERT_TRUE(smtp_is_final_line("250"));
}

void test_is_final_line_ignores_digits_in_the_reply_text(void)
{
    /* The multiline example in RFC 5321 section 4.2.1 includes a line whose
       text begins with digits. Only the character at index 3 decides. */
    TEST_ASSERT_FALSE(smtp_is_final_line("250-234 Text beginning with numbers"));
    TEST_ASSERT_TRUE(smtp_is_final_line("250 234 Text beginning with numbers"));
}

void test_is_final_line_true_for_unparsable_lines(void)
{
    TEST_ASSERT_TRUE(smtp_is_final_line(NULL));
    TEST_ASSERT_TRUE(smtp_is_final_line(""));
    TEST_ASSERT_TRUE(smtp_is_final_line("hello"));
}

/* ------------------------------------------------------------------------ */
/* smtp_text_is_safe                                                         */
/* ------------------------------------------------------------------------ */

void test_text_is_safe_accepts_ordinary_values(void)
{
    TEST_ASSERT_TRUE(smtp_text_is_safe("me@boisestate.edu"));
    TEST_ASSERT_TRUE(smtp_text_is_safe("Subject with spaces and 1234"));
    TEST_ASSERT_TRUE(smtp_text_is_safe(""));
}

void test_text_is_safe_rejects_null(void)
{
    TEST_ASSERT_FALSE(smtp_text_is_safe(NULL));
}

void test_text_is_safe_rejects_embedded_line_breaks(void)
{
    TEST_ASSERT_FALSE(smtp_text_is_safe("hello\rworld"));
    TEST_ASSERT_FALSE(smtp_text_is_safe("hello\nworld"));
    TEST_ASSERT_FALSE(smtp_text_is_safe("trailing\r\n"));
}

void test_text_is_safe_rejects_an_injected_command(void)
{
    /* The reason this check exists: a subject carrying CRLF would end the
       command line early and let the rest be read as a new command. */
    TEST_ASSERT_FALSE(smtp_text_is_safe("hi\r\nRCPT TO:<victim@example.com>"));
}

/* ------------------------------------------------------------------------ */
/* smtp_build_command                                                        */
/* ------------------------------------------------------------------------ */

void test_build_command_joins_verb_and_argument_with_a_space(void)
{
    char *cmd = smtp_build_command("HELO", "onyx.boisestate.edu");
    TEST_ASSERT_NOT_NULL(cmd);
    TEST_ASSERT_EQUAL_STRING("HELO onyx.boisestate.edu\r\n", cmd);
    free(cmd);
}

void test_build_command_omits_the_space_when_there_is_no_argument(void)
{
    char *data = smtp_build_command("DATA", NULL);
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_EQUAL_STRING("DATA\r\n", data);
    free(data);

    char *quit = smtp_build_command("QUIT", NULL);
    TEST_ASSERT_NOT_NULL(quit);
    TEST_ASSERT_EQUAL_STRING("QUIT\r\n", quit);
    free(quit);
}

void test_build_command_rejects_a_null_verb(void)
{
    TEST_ASSERT_NULL(smtp_build_command(NULL, "onyx.boisestate.edu"));
}

/* ------------------------------------------------------------------------ */
/* smtp_build_path_command                                                   */
/* ------------------------------------------------------------------------ */

void test_build_path_command_brackets_the_address_and_hugs_the_colon(void)
{
    char *mail = smtp_build_path_command("MAIL FROM", "me@boisestate.edu");
    TEST_ASSERT_NOT_NULL(mail);
    TEST_ASSERT_EQUAL_STRING("MAIL FROM:<me@boisestate.edu>\r\n", mail);
    free(mail);

    char *rcpt = smtp_build_path_command("RCPT TO", "you@example.com");
    TEST_ASSERT_NOT_NULL(rcpt);
    TEST_ASSERT_EQUAL_STRING("RCPT TO:<you@example.com>\r\n", rcpt);
    free(rcpt);
}

void test_build_path_command_rejects_null_arguments(void)
{
    TEST_ASSERT_NULL(smtp_build_path_command(NULL, "me@boisestate.edu"));
    TEST_ASSERT_NULL(smtp_build_path_command("MAIL FROM", NULL));
}

/* ------------------------------------------------------------------------ */
/* smtp_manage_dot                                                            */
/* ------------------------------------------------------------------------ */

static void assert_stuffed(const char *body, const char *expected)
{
    char *out = smtp_manage_dot(body);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL_STRING(expected, out);
    free(out);
}

void test_manage_dot_rejects_null(void)
{
    TEST_ASSERT_NULL(smtp_manage_dot(NULL));
}

void test_manage_dot_leaves_an_empty_body_empty(void)
{
    assert_stuffed("", "");
}

void test_manage_dot_terminates_an_unterminated_final_line(void)
{
    assert_stuffed("This is the message body.", "This is the message body.\r\n");
}

void test_manage_dot_does_not_add_a_second_terminator(void)
{
    /* RFC 5321 section 4.1.1.4: an extra CRLF would append an empty line. */
    assert_stuffed("Hello\n", "Hello\r\n");
    assert_stuffed("Hello\r\n", "Hello\r\n");
}

void test_manage_dot_converts_lf_to_crlf(void)
{
    assert_stuffed("one\ntwo\n", "one\r\ntwo\r\n");
    assert_stuffed("\n", "\r\n");
    assert_stuffed("a\n\nb\n", "a\r\n\r\nb\r\n");
}

void test_manage_dot_doubles_a_leading_period(void)
{
    assert_stuffed(".", "..\r\n");
    assert_stuffed("..", "...\r\n");
    assert_stuffed(".signature\n", "..signature\r\n");
}

void test_manage_dot_leaves_interior_periods_alone(void)
{
    assert_stuffed("a.b\n", "a.b\r\n");
    assert_stuffed("end.\n", "end.\r\n");
}

void test_manage_dot_stuffs_every_line_not_just_the_first(void)
{
    assert_stuffed("ok\n.\nok\n", "ok\r\n..\r\nok\r\n");
}

void test_manage_dot_drops_a_bare_carriage_return(void)
{
    /* RFC 5321 section 2.3.8 forbids transmitting a CR that is not part of
       a CRLF pair. */
    assert_stuffed("abc\r", "abc\r\n");
    assert_stuffed("a\rb\n", "ab\r\n");
}

/* ------------------------------------------------------------------------ */
/* smtp_build_message                                                        */
/* ------------------------------------------------------------------------ */

void test_build_message_assembles_headers_blank_line_body_and_dot(void)
{
    char *msg = smtp_build_message("me@boisestate.edu", "you@example.com",
                                   "hello", "This is the message body.");
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_EQUAL_STRING(
        "From: me@boisestate.edu\r\n"
        "To: you@example.com\r\n"
        "Subject: hello\r\n"
        "\r\n"
        "This is the message body.\r\n"
        ".\r\n",
        msg);
    free(msg);
}

void test_build_message_treats_a_null_subject_as_empty(void)
{
    char *msg = smtp_build_message("a@b.com", "c@d.com", NULL, "body\n");
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_EQUAL_STRING(
        "From: a@b.com\r\n"
        "To: c@d.com\r\n"
        "Subject: \r\n"
        "\r\n"
        "body\r\n"
        ".\r\n",
        msg);
    free(msg);
}

void test_build_message_treats_a_null_body_as_empty(void)
{
    char *msg = smtp_build_message("a@b.com", "c@d.com", "subj", NULL);
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_EQUAL_STRING(
        "From: a@b.com\r\n"
        "To: c@d.com\r\n"
        "Subject: subj\r\n"
        "\r\n"
        ".\r\n",
        msg);
    free(msg);
}

void test_build_message_stuffs_the_body_it_embeds(void)
{
    char *msg = smtp_build_message("a@b.com", "c@d.com", "subj", "one\n.\ntwo\n");
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_EQUAL_STRING(
        "From: a@b.com\r\n"
        "To: c@d.com\r\n"
        "Subject: subj\r\n"
        "\r\n"
        "one\r\n"
        "..\r\n"
        "two\r\n"
        ".\r\n",
        msg);
    free(msg);
}

void test_build_message_rejects_a_null_sender_or_recipient(void)
{
    TEST_ASSERT_NULL(smtp_build_message(NULL, "c@d.com", "subj", "body"));
    TEST_ASSERT_NULL(smtp_build_message("a@b.com", NULL, "subj", "body"));
}

/* ------------------------------------------------------------------------ */
/* Layer 2 - a scripted transport, so the session can be driven with no      */
/* network at all. An error case is this string truncated, or one digit      */
/* changed, or handed over a few bytes at a time.                            */
/* ------------------------------------------------------------------------ */

struct script
{
    const char *data;
    size_t length;
    size_t pos;
    size_t chunk;  /* bytes to release per read; 0 means as many as fit */
    bool broken;   /* report a read failure instead of data */

    char sent[8192];    /* everything the client has written, NUL terminated */
    size_t sent_len;
    size_t write_chunk; /* bytes accepted per write; 0 means all of them */
    bool write_broken;  /* report a write failure */
    bool write_zero;    /* accept nothing, forever */
};

static struct script g_script;
static struct smtp_session g_session;

static ssize_t script_read(void *ctx, char *buffer, size_t capacity)
{
    struct script *s = (struct script *)ctx;

    if (s->broken)
    {
        return -1;
    }

    size_t available = s->length - s->pos;
    if (available == 0)
    {
        return 0;
    }

    size_t n = available;
    if (n > capacity)
    {
        n = capacity;
    }
    if (s->chunk != 0 && n > s->chunk)
    {
        n = s->chunk;
    }

    memcpy(buffer, s->data + s->pos, n);
    s->pos += n;
    return (ssize_t)n;
}

static ssize_t script_write(void *ctx, const char *data, size_t length)
{
    struct script *s = (struct script *)ctx;

    if (s->write_broken)
    {
        return -1;
    }
    if (s->write_zero)
    {
        return 0;
    }

    size_t n = length;
    if (s->write_chunk != 0 && n > s->write_chunk)
    {
        n = s->write_chunk;
    }

    size_t room = sizeof s->sent - s->sent_len - 1;
    if (n > room)
    {
        n = room;
    }

    memcpy(s->sent + s->sent_len, data, n);
    s->sent_len += n;
    s->sent[s->sent_len] = '\0';
    return (ssize_t)n;
}

static void begin(const char *text, size_t chunk)
{
    memset(&g_script, 0, sizeof g_script);
    g_script.data = text;
    g_script.length = strlen(text);
    g_script.chunk = chunk;
    smtp_session_init(&g_session, script_read, script_write, &g_script);
}

/* ------------------------------------------------------------------------ */
/* smtp_status_text                                                          */
/* ------------------------------------------------------------------------ */

void test_status_text_describes_every_value(void)
{
    TEST_ASSERT_EQUAL_STRING("ok", smtp_status_text(SMTP_OK));
    TEST_ASSERT_EQUAL_STRING("invalid argument", smtp_status_text(SMTP_ERR_ARGS));
    TEST_ASSERT_EQUAL_STRING("transport failure", smtp_status_text(SMTP_ERR_TRANSPORT));
    TEST_ASSERT_EQUAL_STRING("server closed the connection", smtp_status_text(SMTP_ERR_CLOSED));
    TEST_ASSERT_EQUAL_STRING("line too long", smtp_status_text(SMTP_ERR_OVERFLOW));
    TEST_ASSERT_EQUAL_STRING("malformed reply", smtp_status_text(SMTP_ERR_PROTOCOL));
    TEST_ASSERT_EQUAL_STRING("unknown error", smtp_status_text((smtp_status)99));
}

/* ------------------------------------------------------------------------ */
/* smtp_session_init                                                         */
/* ------------------------------------------------------------------------ */

void test_session_init_stores_the_transport_and_empties_the_buffer(void)
{
    struct smtp_session session;
    session.held = 999;

    smtp_session_init(&session, script_read, NULL, &g_script);
    TEST_ASSERT_EQUAL_INT(0, (int)session.held);
    TEST_ASSERT_EQUAL_PTR(&g_script, session.ctx);

    /* Must return quietly rather than dereference NULL. */
    smtp_session_init(NULL, script_read, NULL, NULL);
}

/* ------------------------------------------------------------------------ */
/* smtp_read_line                                                            */
/* ------------------------------------------------------------------------ */

void test_read_line_strips_the_crlf(void)
{
    char line[SMTP_LINE_MAX];
    begin("220 smtp.example.com ESMTP ready\r\n", 0);

    TEST_ASSERT_EQUAL_INT(SMTP_OK, smtp_read_line(&g_session, line, sizeof line));
    TEST_ASSERT_EQUAL_STRING("220 smtp.example.com ESMTP ready", line);
}

void test_read_line_hands_out_two_lines_from_a_single_read(void)
{
    char line[SMTP_LINE_MAX];
    begin("220 one\r\n250 two\r\n", 0);

    TEST_ASSERT_EQUAL_INT(SMTP_OK, smtp_read_line(&g_session, line, sizeof line));
    TEST_ASSERT_EQUAL_STRING("220 one", line);

    /* The whole script arrived in the first read, so the second line has to
       come out of the buffer. Breaking the transport proves it is not used. */
    g_script.broken = true;
    TEST_ASSERT_EQUAL_INT(SMTP_OK, smtp_read_line(&g_session, line, sizeof line));
    TEST_ASSERT_EQUAL_STRING("250 two", line);
}

void test_read_line_reassembles_a_line_delivered_one_byte_at_a_time(void)
{
    char line[SMTP_LINE_MAX];
    begin("250-PIPELINING\r\n", 1);

    TEST_ASSERT_EQUAL_INT(SMTP_OK, smtp_read_line(&g_session, line, sizeof line));
    TEST_ASSERT_EQUAL_STRING("250-PIPELINING", line);
}

void test_read_line_keeps_a_cr_that_is_not_followed_by_lf(void)
{
    char line[SMTP_LINE_MAX];
    begin("250 a\rb\r\n", 0);

    TEST_ASSERT_EQUAL_INT(SMTP_OK, smtp_read_line(&g_session, line, sizeof line));
    TEST_ASSERT_EQUAL_STRING("250 a\rb", line);
}

void test_read_line_reports_a_transport_failure(void)
{
    char line[SMTP_LINE_MAX];
    begin("250 ok\r\n", 0);
    g_script.broken = true;

    TEST_ASSERT_EQUAL_INT(SMTP_ERR_TRANSPORT, smtp_read_line(&g_session, line, sizeof line));
}

void test_read_line_reports_a_peer_that_closed_before_the_crlf(void)
{
    char line[SMTP_LINE_MAX];
    begin("250 no terminator", 0);

    TEST_ASSERT_EQUAL_INT(SMTP_ERR_CLOSED, smtp_read_line(&g_session, line, sizeof line));
}

void test_read_line_reports_a_line_too_long_for_the_session_buffer(void)
{
    static char huge[SMTP_LINE_MAX * 2];
    char line[SMTP_LINE_MAX];

    memset(huge, 'x', sizeof huge - 1);
    huge[sizeof huge - 1] = '\0';
    begin(huge, 0);

    TEST_ASSERT_EQUAL_INT(SMTP_ERR_OVERFLOW, smtp_read_line(&g_session, line, sizeof line));
}

void test_read_line_reports_a_line_too_long_for_the_callers_buffer(void)
{
    char small[8];
    begin("250 this line is far longer than eight bytes\r\n", 0);

    TEST_ASSERT_EQUAL_INT(SMTP_ERR_OVERFLOW, smtp_read_line(&g_session, small, sizeof small));
}

void test_read_line_rejects_bad_arguments(void)
{
    char line[SMTP_LINE_MAX];
    begin("250 ok\r\n", 0);

    TEST_ASSERT_EQUAL_INT(SMTP_ERR_ARGS, smtp_read_line(NULL, line, sizeof line));
    TEST_ASSERT_EQUAL_INT(SMTP_ERR_ARGS, smtp_read_line(&g_session, NULL, sizeof line));
    TEST_ASSERT_EQUAL_INT(SMTP_ERR_ARGS, smtp_read_line(&g_session, line, 0));
}

/* ------------------------------------------------------------------------ */
/* smtp_read_reply                                                           */
/* ------------------------------------------------------------------------ */

void test_read_reply_returns_the_code_of_a_single_line_reply(void)
{
    int code = 0;
    char line[SMTP_LINE_MAX];
    begin("220 smtp.example.com ESMTP ready\r\n", 0);

    TEST_ASSERT_EQUAL_INT(SMTP_OK, smtp_read_reply(&g_session, &code, line, sizeof line));
    TEST_ASSERT_EQUAL_INT(220, code);
    TEST_ASSERT_EQUAL_STRING("220 smtp.example.com ESMTP ready", line);
}

void test_read_reply_follows_continuation_lines(void)
{
    int code = 0;
    char line[SMTP_LINE_MAX];
    begin("250-smtp.example.com\r\n"
          "250-PIPELINING\r\n"
          "250 SIZE 10240000\r\n", 0);

    TEST_ASSERT_EQUAL_INT(SMTP_OK, smtp_read_reply(&g_session, &code, line, sizeof line));
    TEST_ASSERT_EQUAL_INT(250, code);
    TEST_ASSERT_EQUAL_STRING("250 SIZE 10240000", line);
}

void test_read_reply_handles_reply_text_that_begins_with_digits(void)
{
    /* The multiline example printed in RFC 5321 section 4.2.1. */
    int code = 0;
    char line[SMTP_LINE_MAX];
    begin("250-First line\r\n"
          "250-Second line\r\n"
          "250-234 Text beginning with numbers\r\n"
          "250 The last line\r\n", 0);

    TEST_ASSERT_EQUAL_INT(SMTP_OK, smtp_read_reply(&g_session, &code, line, sizeof line));
    TEST_ASSERT_EQUAL_INT(250, code);
    TEST_ASSERT_EQUAL_STRING("250 The last line", line);
}

void test_read_reply_accepts_a_multiline_greeting(void)
{
    /* RFC 5321 section 4.2 gives the greeting its own production, and it has
       a 220- continuation form. */
    int code = 0;
    char line[SMTP_LINE_MAX];
    begin("220-smtp.example.com ESMTP\r\n"
          "220 ready when you are\r\n", 0);

    TEST_ASSERT_EQUAL_INT(SMTP_OK, smtp_read_reply(&g_session, &code, line, sizeof line));
    TEST_ASSERT_EQUAL_INT(220, code);
}

void test_read_reply_accepts_a_reply_that_is_only_a_code(void)
{
    int code = 0;
    char line[SMTP_LINE_MAX];
    begin("250\r\n", 0);

    TEST_ASSERT_EQUAL_INT(SMTP_OK, smtp_read_reply(&g_session, &code, line, sizeof line));
    TEST_ASSERT_EQUAL_INT(250, code);
    TEST_ASSERT_EQUAL_STRING("250", line);
}

void test_read_reply_reassembles_a_multiline_reply_from_fragments(void)
{
    int code = 0;
    char line[SMTP_LINE_MAX];
    begin("250-one\r\n250-two\r\n250 three\r\n", 3);

    TEST_ASSERT_EQUAL_INT(SMTP_OK, smtp_read_reply(&g_session, &code, line, sizeof line));
    TEST_ASSERT_EQUAL_INT(250, code);
    TEST_ASSERT_EQUAL_STRING("250 three", line);
}

void test_read_reply_rejects_a_line_that_is_not_a_reply(void)
{
    int code = 0;
    char line[SMTP_LINE_MAX];
    begin("hello there\r\n", 0);

    TEST_ASSERT_EQUAL_INT(SMTP_ERR_PROTOCOL, smtp_read_reply(&g_session, &code, line, sizeof line));
}

void test_read_reply_reports_a_server_that_hangs_up_mid_reply(void)
{
    int code = 0;
    char line[SMTP_LINE_MAX];
    begin("250-one\r\n250-two\r\n", 0);

    TEST_ASSERT_EQUAL_INT(SMTP_ERR_CLOSED, smtp_read_reply(&g_session, &code, line, sizeof line));
}

void test_read_reply_rejects_bad_arguments(void)
{
    int code = 0;
    char line[SMTP_LINE_MAX];
    begin("250 ok\r\n", 0);

    TEST_ASSERT_EQUAL_INT(SMTP_ERR_ARGS, smtp_read_reply(NULL, &code, line, sizeof line));
    TEST_ASSERT_EQUAL_INT(SMTP_ERR_ARGS, smtp_read_reply(&g_session, NULL, line, sizeof line));
    TEST_ASSERT_EQUAL_INT(SMTP_ERR_ARGS, smtp_read_reply(&g_session, &code, NULL, sizeof line));
    TEST_ASSERT_EQUAL_INT(SMTP_ERR_ARGS, smtp_read_reply(&g_session, &code, line, 0));
}

/* ------------------------------------------------------------------------ */
/* smtp_write_all                                                            */
/* ------------------------------------------------------------------------ */

void test_write_all_sends_the_whole_buffer(void)
{
    begin("", 0);

    TEST_ASSERT_EQUAL_INT(SMTP_OK, smtp_write_all(&g_session, "QUIT\r\n", 6));
    TEST_ASSERT_EQUAL_STRING("QUIT\r\n", g_script.sent);
}

void test_write_all_retries_until_a_short_writing_transport_takes_it_all(void)
{
    begin("", 0);
    g_script.write_chunk = 1;   /* one byte accepted per call */

    TEST_ASSERT_EQUAL_INT(SMTP_OK, smtp_write_all(&g_session, "DATA\r\n", 6));
    TEST_ASSERT_EQUAL_STRING("DATA\r\n", g_script.sent);
}

void test_write_all_accepts_a_zero_length_write(void)
{
    begin("", 0);

    TEST_ASSERT_EQUAL_INT(SMTP_OK, smtp_write_all(&g_session, "", 0));
}

void test_write_all_reports_a_transport_failure(void)
{
    begin("", 0);
    g_script.write_broken = true;

    TEST_ASSERT_EQUAL_INT(SMTP_ERR_TRANSPORT, smtp_write_all(&g_session, "QUIT\r\n", 6));
}

void test_write_all_reports_a_transport_that_accepts_nothing(void)
{
    /* A write callback that keeps returning 0 would spin forever if it were
       not treated as the peer having gone. */
    begin("", 0);
    g_script.write_zero = true;

    TEST_ASSERT_EQUAL_INT(SMTP_ERR_CLOSED, smtp_write_all(&g_session, "QUIT\r\n", 6));
}

void test_write_all_rejects_bad_arguments(void)
{
    begin("", 0);

    TEST_ASSERT_EQUAL_INT(SMTP_ERR_ARGS, smtp_write_all(NULL, "QUIT\r\n", 6));
    TEST_ASSERT_EQUAL_INT(SMTP_ERR_ARGS, smtp_write_all(&g_session, NULL, 6));
}

/* ------------------------------------------------------------------------ */
/* smtp_command_expect                                                       */
/* ------------------------------------------------------------------------ */

void test_command_expect_sends_the_command_and_accepts_the_right_code(void)
{
    char line[SMTP_LINE_MAX];
    begin("250 smtp.example.com\r\n", 0);

    TEST_ASSERT_EQUAL_INT(SMTP_OK,
        smtp_command_expect(&g_session, "HELO onyx.boisestate.edu\r\n", 250,
                            line, sizeof line));
    TEST_ASSERT_EQUAL_STRING("HELO onyx.boisestate.edu\r\n", g_script.sent);
    TEST_ASSERT_EQUAL_STRING("250 smtp.example.com", line);
}

void test_command_expect_reports_the_wrong_code_rather_than_ignoring_it(void)
{
    char line[SMTP_LINE_MAX];
    begin("550 no such user\r\n", 0);

    TEST_ASSERT_EQUAL_INT(SMTP_ERR_PROTOCOL,
        smtp_command_expect(&g_session, "RCPT TO:<nobody@example.com>\r\n", 250,
                            line, sizeof line));
    /* The caller can now say what the server actually sent. */
    TEST_ASSERT_EQUAL_STRING("550 no such user", line);
}

void test_command_expect_propagates_a_write_failure(void)
{
    char line[SMTP_LINE_MAX];
    begin("250 ok\r\n", 0);
    g_script.write_broken = true;

    TEST_ASSERT_EQUAL_INT(SMTP_ERR_TRANSPORT,
        smtp_command_expect(&g_session, "DATA\r\n", 354, line, sizeof line));
}

void test_command_expect_propagates_a_read_failure(void)
{
    char line[SMTP_LINE_MAX];
    begin("", 0);   /* command goes out, nothing comes back */

    TEST_ASSERT_EQUAL_INT(SMTP_ERR_CLOSED,
        smtp_command_expect(&g_session, "DATA\r\n", 354, line, sizeof line));
}

void test_command_expect_rejects_a_null_command(void)
{
    char line[SMTP_LINE_MAX];
    begin("250 ok\r\n", 0);

    TEST_ASSERT_EQUAL_INT(SMTP_ERR_ARGS,
        smtp_command_expect(&g_session, NULL, 250, line, sizeof line));
}

/* ------------------------------------------------------------------------ */
/* smtp_run_session                                                          */
/*                                                                           */
/* Every error case below is the happy script with one code changed or with  */
/* the tail cut off.                                                         */
/* ------------------------------------------------------------------------ */

#define GREETING "220 smtp.example.com ESMTP ready\r\n"
#define HELO_OK  "250 smtp.example.com\r\n"
#define MAIL_OK  "250 2.1.0 Ok\r\n"
#define RCPT_OK  "250 2.1.5 Ok\r\n"
#define DATA_OK  "354 End data with .\r\n"
#define BODY_OK  "250 2.0.0 Ok: queued\r\n"
#define QUIT_OK  "221 Bye\r\n"

#define HAPPY_SCRIPT GREETING HELO_OK MAIL_OK RCPT_OK DATA_OK BODY_OK QUIT_OK

static const struct smtp_mail g_mail = {
    "onyx.boisestate.edu",
    "me@boisestate.edu",
    "you@example.com",
    "hello",
    "This is the message body."
};

static smtp_status run(const char *script)
{
    char line[SMTP_LINE_MAX];
    begin(script, 0);
    return smtp_run_session(&g_session, &g_mail, line, sizeof line);
}

void test_run_session_issues_every_command_in_the_required_order(void)
{
    TEST_ASSERT_EQUAL_INT(SMTP_OK, run(HAPPY_SCRIPT));

    TEST_ASSERT_EQUAL_STRING(
        "HELO onyx.boisestate.edu\r\n"
        "MAIL FROM:<me@boisestate.edu>\r\n"
        "RCPT TO:<you@example.com>\r\n"
        "DATA\r\n"
        "From: me@boisestate.edu\r\n"
        "To: you@example.com\r\n"
        "Subject: hello\r\n"
        "\r\n"
        "This is the message body.\r\n"
        ".\r\n"
        "QUIT\r\n",
        g_script.sent);
}

void test_run_session_follows_continuations_throughout(void)
{
    TEST_ASSERT_EQUAL_INT(SMTP_OK, run(
        "220-smtp.example.com ESMTP\r\n"
        "220 ready\r\n"
        "250-smtp.example.com\r\n"
        "250-PIPELINING\r\n"
        "250 SIZE 10240000\r\n"
        MAIL_OK RCPT_OK DATA_OK BODY_OK QUIT_OK));
}

void test_run_session_rejects_a_bad_greeting(void)
{
    TEST_ASSERT_EQUAL_INT(SMTP_ERR_PROTOCOL, run("421 Service not available\r\n"));
}

void test_run_session_rejects_a_bad_helo_reply(void)
{
    TEST_ASSERT_EQUAL_INT(SMTP_ERR_PROTOCOL, run(GREETING "550 go away\r\n"));
}

void test_run_session_rejects_a_bad_mail_from_reply(void)
{
    TEST_ASSERT_EQUAL_INT(SMTP_ERR_PROTOCOL,
        run(GREETING HELO_OK "553 bad sender\r\n"));
}

void test_run_session_rejects_a_bad_rcpt_to_reply(void)
{
    TEST_ASSERT_EQUAL_INT(SMTP_ERR_PROTOCOL,
        run(GREETING HELO_OK MAIL_OK "550 no such user\r\n"));
}

void test_run_session_rejects_a_bad_data_reply(void)
{
    TEST_ASSERT_EQUAL_INT(SMTP_ERR_PROTOCOL,
        run(GREETING HELO_OK MAIL_OK RCPT_OK "503 bad sequence\r\n"));
}

void test_run_session_rejects_a_bad_end_of_data_reply(void)
{
    TEST_ASSERT_EQUAL_INT(SMTP_ERR_PROTOCOL,
        run(GREETING HELO_OK MAIL_OK RCPT_OK DATA_OK "552 too much mail data\r\n"));
}

void test_run_session_rejects_a_bad_quit_reply(void)
{
    TEST_ASSERT_EQUAL_INT(SMTP_ERR_PROTOCOL,
        run(GREETING HELO_OK MAIL_OK RCPT_OK DATA_OK BODY_OK "500 what\r\n"));
}

void test_run_session_sends_quit_before_giving_up_on_a_bad_code(void)
{
    /* RFC 5321 section 4.1.1.10: the sender must not close the channel
       without a QUIT, even after an error response. */
    TEST_ASSERT_EQUAL_INT(SMTP_ERR_PROTOCOL,
        run(GREETING HELO_OK MAIL_OK "550 no such user\r\n"));

    TEST_ASSERT_EQUAL_STRING(
        "HELO onyx.boisestate.edu\r\n"
        "MAIL FROM:<me@boisestate.edu>\r\n"
        "RCPT TO:<you@example.com>\r\n"
        "QUIT\r\n",
        g_script.sent);
}

void test_run_session_reports_a_server_that_hangs_up_mid_session(void)
{
    TEST_ASSERT_EQUAL_INT(SMTP_ERR_CLOSED,
        run(GREETING HELO_OK MAIL_OK RCPT_OK));
}

void test_run_session_does_not_send_quit_to_a_peer_that_has_gone(void)
{
    TEST_ASSERT_EQUAL_INT(SMTP_ERR_CLOSED, run(GREETING HELO_OK MAIL_OK));

    TEST_ASSERT_EQUAL_STRING(
        "HELO onyx.boisestate.edu\r\n"
        "MAIL FROM:<me@boisestate.edu>\r\n"
        "RCPT TO:<you@example.com>\r\n",
        g_script.sent);
}

void test_run_session_reports_a_greeting_that_never_arrives(void)
{
    TEST_ASSERT_EQUAL_INT(SMTP_ERR_CLOSED, run(""));
}

void test_run_session_reports_a_malformed_reply(void)
{
    TEST_ASSERT_EQUAL_INT(SMTP_ERR_PROTOCOL, run("hello there\r\n"));
}

void test_run_session_reports_a_write_failure(void)
{
    char line[SMTP_LINE_MAX];
    begin(HAPPY_SCRIPT, 0);
    g_script.write_broken = true;

    TEST_ASSERT_EQUAL_INT(SMTP_ERR_TRANSPORT,
        smtp_run_session(&g_session, &g_mail, line, sizeof line));
}

void test_run_session_rejects_a_mail_with_no_sender(void)
{
    struct smtp_mail bad = g_mail;
    char line[SMTP_LINE_MAX];
    bad.from = NULL;

    begin(HAPPY_SCRIPT, 0);
    TEST_ASSERT_EQUAL_INT(SMTP_ERR_ARGS,
        smtp_run_session(&g_session, &bad, line, sizeof line));
}

void test_run_session_rejects_bad_arguments(void)
{
    char line[SMTP_LINE_MAX];
    begin(HAPPY_SCRIPT, 0);

    TEST_ASSERT_EQUAL_INT(SMTP_ERR_ARGS, smtp_run_session(NULL, &g_mail, line, sizeof line));
    TEST_ASSERT_EQUAL_INT(SMTP_ERR_ARGS, smtp_run_session(&g_session, NULL, line, sizeof line));
    TEST_ASSERT_EQUAL_INT(SMTP_ERR_ARGS, smtp_run_session(&g_session, &g_mail, NULL, sizeof line));
    TEST_ASSERT_EQUAL_INT(SMTP_ERR_ARGS, smtp_run_session(&g_session, &g_mail, line, 0));
}

/* ------------------------------------------------------------------------ */
/* Layer 3 - exercised against a listener this process opens on the loopback */
/* interface. No name server, no outside host, nothing that a firewall or a  */
/* CI runner could take away.                                                */
/* ------------------------------------------------------------------------ */

/**
 * Bind a TCP socket to 127.0.0.1 on port 0 so the kernel picks a free port,
 * start listening, and report the port it chose. A connect() to that port
 * completes without anyone calling accept(), because the kernel finishes the
 * handshake into the listen queue on its own, so none of this needs a thread.
 */
static int start_listener(char *port_out, size_t port_size)
{
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0)
    {
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (bind(server, (struct sockaddr *)&addr, sizeof addr) != 0
        || listen(server, 1) != 0)
    {
        close(server);
        return -1;
    }

    socklen_t len = sizeof addr;
    if (getsockname(server, (struct sockaddr *)&addr, &len) != 0)
    {
        close(server);
        return -1;
    }

    snprintf(port_out, port_size, "%u", (unsigned)ntohs(addr.sin_port));
    return server;
}

void test_connect_reaches_a_listening_socket(void)
{
    char port[16];
    int server = start_listener(port, sizeof port);
    TEST_ASSERT_TRUE(server >= 0);

    int fd = smtp_connect("127.0.0.1", port);
    TEST_ASSERT_TRUE(fd >= 0);

    int peer = accept(server, NULL, NULL);
    TEST_ASSERT_TRUE(peer >= 0);

    smtp_disconnect(fd);
    close(peer);
    close(server);
}

void test_connect_reports_a_refused_connection(void)
{
    /* Take a port, then give it up, so nothing is listening when we try. */
    char port[16];
    int server = start_listener(port, sizeof port);
    TEST_ASSERT_TRUE(server >= 0);
    close(server);

    TEST_ASSERT_EQUAL_INT(-1, smtp_connect("127.0.0.1", port));
}

void test_connect_reports_a_service_it_cannot_resolve(void)
{
    /* A numeric host keeps DNS out of it; the service name is what fails. */
    TEST_ASSERT_EQUAL_INT(-1, smtp_connect("127.0.0.1", "not-a-real-service"));
}

void test_connect_rejects_null_arguments(void)
{
    TEST_ASSERT_EQUAL_INT(-1, smtp_connect(NULL, "25"));
    TEST_ASSERT_EQUAL_INT(-1, smtp_connect("127.0.0.1", NULL));
}

void test_socket_read_and_write_move_bytes_over_a_real_connection(void)
{
    char port[16];
    int server = start_listener(port, sizeof port);
    TEST_ASSERT_TRUE(server >= 0);

    int fd = smtp_connect("127.0.0.1", port);
    TEST_ASSERT_TRUE(fd >= 0);
    int peer = accept(server, NULL, NULL);
    TEST_ASSERT_TRUE(peer >= 0);

    TEST_ASSERT_EQUAL_INT(6, (int)smtp_socket_write(&fd, "QUIT\r\n", 6));
    char received[32] = {0};
    TEST_ASSERT_EQUAL_INT(6, (int)recv(peer, received, sizeof received - 1, 0));
    TEST_ASSERT_EQUAL_STRING("QUIT\r\n", received);

    TEST_ASSERT_EQUAL_INT(9, (int)send(peer, "221 Bye\r\n", 9, 0));
    char answered[32] = {0};
    TEST_ASSERT_EQUAL_INT(9, (int)smtp_socket_read(&fd, answered, sizeof answered - 1));
    TEST_ASSERT_EQUAL_STRING("221 Bye\r\n", answered);

    smtp_disconnect(fd);
    close(peer);
    close(server);
}

void test_socket_read_and_write_reject_null_arguments(void)
{
    int fd = -1;
    char buffer[8];

    TEST_ASSERT_TRUE(smtp_socket_read(NULL, buffer, sizeof buffer) < 0);
    TEST_ASSERT_TRUE(smtp_socket_read(&fd, NULL, sizeof buffer) < 0);
    TEST_ASSERT_TRUE(smtp_socket_write(NULL, "x", 1) < 0);
    TEST_ASSERT_TRUE(smtp_socket_write(&fd, NULL, 1) < 0);
}

void test_disconnect_closes_the_socket_and_tolerates_a_negative_descriptor(void)
{
    char port[16];
    int server = start_listener(port, sizeof port);
    TEST_ASSERT_TRUE(server >= 0);

    int fd = smtp_connect("127.0.0.1", port);
    TEST_ASSERT_TRUE(fd >= 0);

    smtp_disconnect(fd);
    /* Closing it a second time fails, which is the proof it was closed. */
    TEST_ASSERT_EQUAL_INT(-1, close(fd));

    smtp_disconnect(-1);   /* must return quietly rather than call close(-1) */

    close(server);
}

void test_a_whole_session_runs_over_a_real_socket(void)
{
    /* The same smtp_run_session() the scripted tests drive, this time with
       the socket transport plugged in instead. Nothing in layer 2 changes. */
    char port[16];
    int server = start_listener(port, sizeof port);
    TEST_ASSERT_TRUE(server >= 0);

    int fd = smtp_connect("127.0.0.1", port);
    TEST_ASSERT_TRUE(fd >= 0);
    int peer = accept(server, NULL, NULL);
    TEST_ASSERT_TRUE(peer >= 0);

    /* Queue every reply up front. The client will find several of them in a
       single recv(), which is exactly the case the line reader exists for. */
    const char *replies = HAPPY_SCRIPT;
    TEST_ASSERT_EQUAL_INT((int)strlen(replies),
                          (int)send(peer, replies, strlen(replies), 0));

    struct smtp_session session;
    char line[SMTP_LINE_MAX];
    smtp_session_init(&session, smtp_socket_read, smtp_socket_write, &fd);

    TEST_ASSERT_EQUAL_INT(SMTP_OK,
        smtp_run_session(&session, &g_mail, line, sizeof line));
    TEST_ASSERT_EQUAL_STRING("221 Bye", line);

    smtp_disconnect(fd);

    char sent[2048] = {0};
    size_t total = 0;
    for (;;)
    {
        ssize_t n = recv(peer, sent + total, sizeof sent - total - 1, 0);
        if (n <= 0)
        {
            break;
        }
        total += (size_t)n;
    }

    TEST_ASSERT_EQUAL_STRING(
        "HELO onyx.boisestate.edu\r\n"
        "MAIL FROM:<me@boisestate.edu>\r\n"
        "RCPT TO:<you@example.com>\r\n"
        "DATA\r\n"
        "From: me@boisestate.edu\r\n"
        "To: you@example.com\r\n"
        "Subject: hello\r\n"
        "\r\n"
        "This is the message body.\r\n"
        ".\r\n"
        "QUIT\r\n",
        sent);

    close(peer);
    close(server);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_reply_code_accepts_the_codes_the_session_needs);
    RUN_TEST(test_reply_code_reads_a_continuation_line);
    RUN_TEST(test_reply_code_accepts_a_bare_code_with_no_text);
    RUN_TEST(test_reply_code_rejects_null);
    RUN_TEST(test_reply_code_rejects_lines_shorter_than_three_digits);
    RUN_TEST(test_reply_code_rejects_a_first_digit_outside_two_through_five);
    RUN_TEST(test_reply_code_rejects_non_digits);

    RUN_TEST(test_is_final_line_true_for_code_then_space);
    RUN_TEST(test_is_final_line_false_for_code_then_hyphen);
    RUN_TEST(test_is_final_line_true_for_a_bare_code);
    RUN_TEST(test_is_final_line_ignores_digits_in_the_reply_text);
    RUN_TEST(test_is_final_line_true_for_unparsable_lines);

    RUN_TEST(test_text_is_safe_accepts_ordinary_values);
    RUN_TEST(test_text_is_safe_rejects_null);
    RUN_TEST(test_text_is_safe_rejects_embedded_line_breaks);
    RUN_TEST(test_text_is_safe_rejects_an_injected_command);

    RUN_TEST(test_build_command_joins_verb_and_argument_with_a_space);
    RUN_TEST(test_build_command_omits_the_space_when_there_is_no_argument);
    RUN_TEST(test_build_command_rejects_a_null_verb);

    RUN_TEST(test_build_path_command_brackets_the_address_and_hugs_the_colon);
    RUN_TEST(test_build_path_command_rejects_null_arguments);

    RUN_TEST(test_manage_dot_rejects_null);
    RUN_TEST(test_manage_dot_leaves_an_empty_body_empty);
    RUN_TEST(test_manage_dot_terminates_an_unterminated_final_line);
    RUN_TEST(test_manage_dot_does_not_add_a_second_terminator);
    RUN_TEST(test_manage_dot_converts_lf_to_crlf);
    RUN_TEST(test_manage_dot_doubles_a_leading_period);
    RUN_TEST(test_manage_dot_leaves_interior_periods_alone);
    RUN_TEST(test_manage_dot_stuffs_every_line_not_just_the_first);
    RUN_TEST(test_manage_dot_drops_a_bare_carriage_return);

    RUN_TEST(test_build_message_assembles_headers_blank_line_body_and_dot);
    RUN_TEST(test_build_message_treats_a_null_subject_as_empty);
    RUN_TEST(test_build_message_treats_a_null_body_as_empty);
    RUN_TEST(test_build_message_stuffs_the_body_it_embeds);
    RUN_TEST(test_build_message_rejects_a_null_sender_or_recipient);

    RUN_TEST(test_status_text_describes_every_value);

    RUN_TEST(test_session_init_stores_the_transport_and_empties_the_buffer);

    RUN_TEST(test_read_line_strips_the_crlf);
    RUN_TEST(test_read_line_hands_out_two_lines_from_a_single_read);
    RUN_TEST(test_read_line_reassembles_a_line_delivered_one_byte_at_a_time);
    RUN_TEST(test_read_line_keeps_a_cr_that_is_not_followed_by_lf);
    RUN_TEST(test_read_line_reports_a_transport_failure);
    RUN_TEST(test_read_line_reports_a_peer_that_closed_before_the_crlf);
    RUN_TEST(test_read_line_reports_a_line_too_long_for_the_session_buffer);
    RUN_TEST(test_read_line_reports_a_line_too_long_for_the_callers_buffer);
    RUN_TEST(test_read_line_rejects_bad_arguments);

    RUN_TEST(test_read_reply_returns_the_code_of_a_single_line_reply);
    RUN_TEST(test_read_reply_follows_continuation_lines);
    RUN_TEST(test_read_reply_handles_reply_text_that_begins_with_digits);
    RUN_TEST(test_read_reply_accepts_a_multiline_greeting);
    RUN_TEST(test_read_reply_accepts_a_reply_that_is_only_a_code);
    RUN_TEST(test_read_reply_reassembles_a_multiline_reply_from_fragments);
    RUN_TEST(test_read_reply_rejects_a_line_that_is_not_a_reply);
    RUN_TEST(test_read_reply_reports_a_server_that_hangs_up_mid_reply);
    RUN_TEST(test_read_reply_rejects_bad_arguments);

    RUN_TEST(test_write_all_sends_the_whole_buffer);
    RUN_TEST(test_write_all_retries_until_a_short_writing_transport_takes_it_all);
    RUN_TEST(test_write_all_accepts_a_zero_length_write);
    RUN_TEST(test_write_all_reports_a_transport_failure);
    RUN_TEST(test_write_all_reports_a_transport_that_accepts_nothing);
    RUN_TEST(test_write_all_rejects_bad_arguments);

    RUN_TEST(test_command_expect_sends_the_command_and_accepts_the_right_code);
    RUN_TEST(test_command_expect_reports_the_wrong_code_rather_than_ignoring_it);
    RUN_TEST(test_command_expect_propagates_a_write_failure);
    RUN_TEST(test_command_expect_propagates_a_read_failure);
    RUN_TEST(test_command_expect_rejects_a_null_command);

    RUN_TEST(test_run_session_issues_every_command_in_the_required_order);
    RUN_TEST(test_run_session_follows_continuations_throughout);
    RUN_TEST(test_run_session_rejects_a_bad_greeting);
    RUN_TEST(test_run_session_rejects_a_bad_helo_reply);
    RUN_TEST(test_run_session_rejects_a_bad_mail_from_reply);
    RUN_TEST(test_run_session_rejects_a_bad_rcpt_to_reply);
    RUN_TEST(test_run_session_rejects_a_bad_data_reply);
    RUN_TEST(test_run_session_rejects_a_bad_end_of_data_reply);
    RUN_TEST(test_run_session_rejects_a_bad_quit_reply);
    RUN_TEST(test_run_session_sends_quit_before_giving_up_on_a_bad_code);
    RUN_TEST(test_run_session_reports_a_server_that_hangs_up_mid_session);
    RUN_TEST(test_run_session_does_not_send_quit_to_a_peer_that_has_gone);
    RUN_TEST(test_run_session_reports_a_greeting_that_never_arrives);
    RUN_TEST(test_run_session_reports_a_malformed_reply);
    RUN_TEST(test_run_session_reports_a_write_failure);
    RUN_TEST(test_run_session_rejects_a_mail_with_no_sender);
    RUN_TEST(test_run_session_rejects_bad_arguments);

    RUN_TEST(test_connect_reaches_a_listening_socket);
    RUN_TEST(test_connect_reports_a_refused_connection);
    RUN_TEST(test_connect_reports_a_service_it_cannot_resolve);
    RUN_TEST(test_connect_rejects_null_arguments);
    RUN_TEST(test_socket_read_and_write_move_bytes_over_a_real_connection);
    RUN_TEST(test_socket_read_and_write_reject_null_arguments);
    RUN_TEST(test_disconnect_closes_the_socket_and_tolerates_a_negative_descriptor);
    RUN_TEST(test_a_whole_session_runs_over_a_real_socket);

    return UNITY_END();
}

```

## Scripts Files
Report generated on 09/19/2026 at 05:53:22


---

## End of Report

SHA-256 Hash of the report: c8737cdef126918b0b13aee96f2acae01afa4c2875ed8c1c1c56049ca5de8cdc

Do not edit the generated report. Any changes will be reported as academic dishonesty

---
## GitHub Info
- GitHub repo name: RandyBauer/cs425-p1
- The repository visibility is public.
- The workflow was triggered by RandyBauer
