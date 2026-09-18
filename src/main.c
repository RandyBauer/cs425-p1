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
