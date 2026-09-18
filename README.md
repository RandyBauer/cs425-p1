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
| 0 | The server queued the message, or the program was run with no arguments and printed its usage |
| 1 | The command line was wrong |
| 2 | The connection or the SMTP session failed |

On failure the client prints why it stopped and the reply the server
that was sent, for example:

```
myapp: malformed reply
myapp: server said: 550 5.1.1 <nobody@example.com>: Recipient address rejected
```

Port 25 is blocked; use `-p 2525` or `-p 587`.

## Design

The client is split into three layers, declared in that order in `src/lab.h`.

### Layer 1 — pure protocol helpers

`smtp_reply_code`, `smtp_is_final_line`, `smtp_text_is_safe`,
`smtp_build_command`, `smtp_build_path_command`, `smtp_manage_dot`,
`smtp_build_message`.

Strings and integers in, strings and integers out. No sockets, no file
descriptors, no I/O of any kind, no global state. Everything the protocol
requires as a *transformation* lives here: parsing a status code off a reply
line, deciding whether a reply line is the last one, doubling a leading period
so a body line cannot be mistaken for the end of data, and assembling the
headers, the blank line, the stuffed body and the terminating period into the
exact bytes that follow `DATA`.

### Layer 2 — the session, over a transport that can be swapped

`smtp_session_init`, `smtp_read_line`, `smtp_read_reply`, `smtp_write_all`,
`smtp_command_expect`, `smtp_run_session`, `smtp_status_text`.

This layer drives the whole conversation — greeting, `HELO`, `MAIL FROM`,
100% line coverage, and `make leak` and `make leak-test` are both silent.
`RCPT TO`, `DATA`, the message, `QUIT` — and checks the status code at every
step. It never calls `recv` or `send`. Instead, `struct smtp_session` holds a
read callback, a write callback and an opaque context pointer, and every byte
in or out goes through those.

The session also owns the receive buffer. That is what lets `smtp_read_line`
cope with the fact that TCP is a byte stream and not a message queue: a reply
can arrive split across several reads, and several replies can arrive in one.
The reader searches the bytes it already holds for a CRLF and only asks the
transport for more when it does not already have a complete line, keeping
anything past the line it hands back for the next call. A buffer that lived
inside the function would discard those leftovers, and the client would block
waiting for a reply that had already arrived.

### Layer 3 — the socket transport

`smtp_connect`, `smtp_disconnect`, `smtp_socket_read`, `smtp_socket_write`.

Thin wrappers over `getaddrinfo`, `connect`, `recv` and `send` that satisfy the
two callbacks above. The server name is resolved with `getaddrinfo` rather than
assumed to be a dotted quad, and every address it returns is tried in turn, so
a host that publishes both IPv6 and IPv4 records still connects from a machine
that can only reach one of them. `smtp_socket_write` passes `MSG_NOSIGNAL`, so
writing to a connection the peer has already closed returns `EPIPE` instead of
raising `SIGPIPE` and killing the process.

### Why the split is there

**A live mail server cannot be unit tested.** It is slow, it is not always
reachable, a continuous integration runner cannot get to it, and there is no
way to make it return `421` on demand. Any protocol logic that calls `recv`
directly is therefore permanently untestable.

Layer 2 exists to make that problem go away. Because it reads and writes only
through callbacks, the real client hands it a socket and the test suite hands
it a string — and **the session code is byte-for-byte identical in both
cases**, so the tests exercise the shipping code rather than a parallel copy of
it. A complete scripted server is one string literal:

```c
"220 smtp.example.com ESMTP ready\r\n"
"250 smtp.example.com\r\n"
"250 2.1.0 Ok\r\n"
"250 2.1.5 Ok\r\n"
"354 End data with .\r\n"
"250 2.0.0 Ok: queued\r\n"
"221 Bye\r\n"
```

Every failure case is that string with one code changed or with the tail cut
off. All seven wrong-status-code cases, a server that hangs up mid-session, a
reply that arrives one byte at a time, a multiline reply, and a reply too long
for the buffer are all reachable without a network.

The same swap is what makes the socket layer testable too. `tests/lab-test.c`
opens a listening socket on `127.0.0.1` port 0 — the kernel picks a free port —
and connects to it from the same process. The kernel completes the handshake
into the listen queue on its own, so no second thread is needed. One test runs
the entire `smtp_run_session` over that real socket and asserts the same byte
stream the scripted tests assert, which is the evidence that the abstraction is
real rather than ceremonial.

Three smaller boundaries follow from the same reasoning:

- **`smtp_read_line` strips the CRLF** before handing a line up, so no layer 1
  helper has to know about line terminators.
- **Layer 2 allocates nothing.** The session buffer is a fixed array inside the
  struct, so none of that layer's error returns needs cleanup and a line longer
  than the buffer is a clean error rather than unbounded growth.
- **Input validation lives in `main`,** not in `smtp_build_message`. A carriage
  return in `-s` is a *command line* error and exits 1; a failed allocation is a
  runtime error and exits 2. Only `main` knows which message to print, so only
  `main` does the check.

## Building and Testing

```bash
make all         # builds release, debug, test and debug-test
make check       # runs the unit tests
make report      # coverage
make leak        # the binary under AddressSanitizer with leak detection
make leak-test   # the test suite under AddressSanitizer
```

`make all` builds but does not test, and `make check` has no prerequisites, so
the loop is always `make all` then `make check`. Only the release build carries
the warning flags; the other three overwrite `CFLAGS` and discard them.

91 unit tests cover every function declared in `src/lab.h`, `src/lab.c` reports

## RFC 5321 conformance notes

Places where this client deliberately differs from the specification:

- **No timeouts.** Section 4.5.3.2 says an SMTP client MUST provide per-command
  timeouts. This one does not. A server that accepts a connection and then stops
  responding will hang the client rather than failing it. Adding them would be a
  change to the transport layer only; the session layer would not need to know.
- **Stricter status codes than required.** Section 4.3.2 advises clients to
  "interpret only the first digit of the reply," and permits `251` as well as
  `250` in response to `RCPT TO`. This client requires exactly
  220, 250, 250, 250, 354, 250, 221, which is what the assignment specifies.
- **`QUIT` is sent on the error path, but its reply is not read.** Section
  4.1.1.10 forbids a sender from closing the channel without a `QUIT` even after
  an error, and says the sender SHOULD wait for the reply. The `QUIT` is sent;
  the wait is skipped, because with no timeout a wait on an unresponsive server
  would never return. `QUIT` is also skipped entirely when the failure was the
  transport itself, since the peer is already gone.
- **One recipient per run.** Section 3.3 allows any number of `RCPT TO`
  commands. The command line accepts one.
- **`HELO`, not `EHLO`.** No service extension is negotiated, no TLS, no
  authentication. Section 3.2 permits this for a client that needs no extensions.
- **Reply lines are capped at 1024 octets.** Section 4.5.3.1.5 requires a client
  to accept at least 512; a longer line is reported as an error rather than
  silently truncated.

RFC 5321 is updated by **RFC 7504**, which adds reply codes 521 and 556. Neither
requires a change here: 556 is generated by relays doing MX lookups, which this
client does not do, and a 521 greeting is already fatal because any greeting
that is not 220 is rejected.

## Known Bugs or Issues

No known bugs. The limitations above are deliberate rather than defects; the
first two — the absent timeouts and the strict status codes — are the ones that
would matter most against a real mail server rather than the class test sink.

## Experience

TODO — replace this with your own account. Points worth covering, from the
actual work:

- Reading RFC 5321 selectively rather than end to end: which fifteen pages
  mattered, and what turned up in them that the assignment text never mentions
  (a reply line can be just three characters with no space and no text; the
  greeting itself can be multiline; an extra CRLF before the terminating period
  is a MUST NOT).
- The design decision that made the tests possible, and when it became obvious
  that the buffer had to belong to the session rather than to the line reader.
- Debugging the segmentation fault in `make check`: the test binary carries no
  sanitizer, so the crash said only "Segmentation fault", while `make leak-test`
  runs the same tests under AddressSanitizer and names the file and line. A
  crash also stops the run, which hid a second bug in the tests that had not
  executed yet.
- Branch coverage versus line coverage: `gcovr --txt` reports lines, and a
  condition joined with `||` is two branches, each of which has to be exercised
  from both sides.
