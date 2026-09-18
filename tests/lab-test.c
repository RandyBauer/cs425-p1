#include <stdlib.h>
#include <stdio.h>
#include "harness/unity.h"
#include "../src/lab.h"


void setUp(void) {}

void tearDown(void) {}

/*--------------------*/
/* smtp_reply_code()  */
/*--------------------*/

void test_reply_code_accepts_minimal_session_codes(void)
{
  TEST_ASSERT_EQUAL_INT(220, smtp_reply_code("220 smtp.example.com ESMTP ready"));
  TEST_ASSERT_EQUAL_INT(250, smtp_reply_code("250 2.1.0. Ok"));
  TEST_ASSERT_EQUAL_INT(354, smtp_reply_code("354 End data with ."));
  TEST_ASSERT_EQUAL_INT(221, smtp_reply_code("221 Bye"));
}

void test_reply_codes_reads_a_continuation(void)
{
  TEST_ASSERT_EQUAL_INT(250, smtp_reply_code("250-PIPELINING"));
}

void test_reply_code_accepts_only_code_and_no_text()
{
  TEST_ASSERT_EQUAL_INT(250, smtp_reply_code("250"));
  TEST_ASSERT_EQUAL_INT(250, smtp_reply_code("250 "));
}

void test_reply_code_rejects_null()
{
  TEST_ASSERT_EQUAL_INT(-1, smtp_reply_code(NULL));
}

void test_reply_code_rejects_lines_shorter_than_three_digits(void)
{
  TEST_ASSERT_EQUAL_INT(-1, smtp_reply_code(""));
  TEST_ASSERT_EQUAL_INT(-1, smtp_reply_code("2"));
  TEST_ASSERT_EQUAL_INT(-1, smtp_reply_code("25"));
}

void test_reply_code_rejects_first_digit_outside_two_through_five(void)
{
  TEST_ASSERT_EQUAL_INT(-1, smtp_reply_code("150 too low"));
  TEST_ASSERT_EQUAL_INT(-1, smtp_reply_code("650 too high"));
}

void test_reply_code_rejects_non_digits(void)
{
  TEST_ASSERT_EQUAL_INT(-1, smtp_reply_code("2!0 below zero"));
  TEST_ASSERT_EQUAL_INT(-1, smtp_reply_code("2x0 above nine"));
  TEST_ASSERT_EQUAL_INT(-1, smtp_reply_code("25! below zero"));
  TEST_ASSERT_EQUAL_INT(-1, smtp_reply_code("25x above nine"));
  TEST_ASSERT_EQUAL_INT(-1, smtp_reply_code("hello"));
}

/*-------------------------*/
/* smtp_is_final_line()    */
/*-------------------------*/

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

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_reply_code_accepts_minimal_session_codes);
  RUN_TEST(test_reply_codes_reads_a_continuation);
  RUN_TEST(test_reply_code_accepts_only_code_and_no_text);
  RUN_TEST(test_reply_code_rejects_null);
  RUN_TEST(test_reply_code_rejects_lines_shorter_than_three_digits);
  RUN_TEST(test_reply_code_rejects_first_digit_outside_two_through_five);
  RUN_TEST(test_reply_code_rejects_non_digits);

  RUN_TEST(test_is_final_line_true_for_code_then_space);
  RUN_TEST(test_is_final_line_false_for_code_then_hyphen);
  RUN_TEST(test_is_final_line_true_for_a_bare_code);
  RUN_TEST(test_is_final_line_ignores_digits_in_the_reply_text);
  RUN_TEST(test_is_final_line_true_for_unparsable_lines);
  return UNITY_END();
}
