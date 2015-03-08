#ifndef __RCPT_H
#define __RCPT_H

typedef struct rcpt {
    char *email;
    struct rcpt *next;
} rcpt_t;

rcpt_t *newTo(rcpt_t **toList, char *email);
void freeToList(rcpt_t *toList);
void printToList(rcpt_t *toList);

#endif
