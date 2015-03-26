#include "rcpt.h"
#include <stdlib.h>
#include <stdio.h>

rcpt_t *newTo(rcpt_t **toList, char *email)
{
    rcpt_t *newTo = calloc(1, sizeof(rcpt_t));
    rcpt_t *to;

    newTo->email = email;

    if (*toList == NULL) {
        *toList = newTo;
    } else {
        to = *toList;
        while (to->next) {
            to = to->next;
        }
        to->next = newTo;
    }

    return newTo;
}

void freeToList(rcpt_t *toList)
{
    rcpt_t *to;

    while (toList) {
        to = toList;
        toList = toList->next;
        free(to);
    }
}

void printToList(rcpt_t *toList)
{
    rcpt_t *to = toList;

    while (to) {
        printf("%s\n", to->email);
        to = to->next;
    }
}
