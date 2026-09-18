#ifndef LAB_H
#define LAB_H

#include <stdbool.h>

/**
 * Extracts the three-digit status code from an SMTP reply line.
 * 
 * details here
 */
int smtp_reply_code(const char *line);

/**
 * Decides if a reply line is the last line of the reply.
 * 
 * details here
 */
bool smtp_is_final_line(const char *line);

/**
 * Details here
 */
bool smtp_text_is_safe(const char *text);

/**
 * Details here
 */
char *smtp_build_command(const char *verb, const char *arg);

/**
 * Details here
 */
char *smtp_build_path_command(const char *verb, const char *address);

/**
 * Details here
 */
char *smtp_manage_dot(const char *body);

/**
 * Details here
 */
char *smtp_build_message(const char *from, const char *to, const char *subject, const char *body);

#endif // LAB_H
