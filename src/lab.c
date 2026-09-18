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
