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
