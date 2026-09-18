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
