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


#endif // LAB_H
