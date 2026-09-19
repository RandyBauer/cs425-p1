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
