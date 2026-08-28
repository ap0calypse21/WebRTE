// golden
// 12/20/18
//

#include "paramdict.h"

struct paramdict *paramdict_alloc() {
    struct paramdict *pd = (struct paramdict *)malloc(sizeof(struct paramdict));

    pd->size = 4096;
    pd->keys = (char **)malloc(pd->size);
    pd->values = (char **)malloc(pd->size);
    pd->length = 0;

    return pd;
}

// Percent-decoding, in place. Values point into the request buffer and the
// result is never longer than the input, so this needs no allocation.
//
// Nothing decoded before this: a path of "/" arrives from any browser as %2F
// and open() was being handed the literal three characters. Plus is decoded to
// a space to match how URLSearchParams encodes one -- a genuine plus in a
// filename comes through as %2B.
static int hexval(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void url_decode(char *s) {
    char *out = s;

    if(!s) {
        return;
    }

    while(*s) {
        if(*s == '%') {
            int hi = hexval(s[1]);
            int lo = hi < 0 ? -1 : hexval(s[2]);
            if(lo >= 0) {
                *out++ = (char)((hi << 4) | lo);
                s += 3;
                continue;
            }
        } else if(*s == '+') {
            *out++ = ' ';
            s++;
            continue;
        }
        *out++ = *s++;
    }

    *out = 0;
}

int paramdict_add(struct paramdict *pd, char *key, char *value) {
    url_decode(value);

    if(pd->length * sizeof(char *) >= pd->size) {
        pd->size += 4096;
        pd->keys = (char **)realloc(pd->keys, pd->size);
        pd->values = (char **)realloc(pd->values, pd->size);
    }

    pd->keys[pd->length] = key;
    pd->values[pd->length] = value;
    pd->length++;

    return 0;
}

char *paramdict_search(struct paramdict *pd, char *key) {
    int i;

    for(i = 0; i < pd->length; i++) {
        if(!strcmp(pd->keys[i], key)) {
            return pd->values[i];
        }
    }

    return NULL;
}

int paramdict_free(struct paramdict *pd) {
    free(pd->keys);
    free(pd->values);
    free(pd);

    return 0;
}
