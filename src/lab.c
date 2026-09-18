#include "lab.h"
#include <stdio.h>
#include <stdlib.h>

int smtp_reply_code(const char *line)
{
  if (line == NULL)
  {
    return -1;
  }

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
  return verb;
}

char *smtp_build_path_command(const char *verb, const char *address)
{
  return verb;
}

char *smtp_manage_dot(const char *body)
{
  return body;
}

char *smtp_build_message(const char *from, const char *to, const char *subject, const char *body)
{
  return from;
}
