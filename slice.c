#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct slice slice;
struct slice {
    int len;
    size_t cap;
    char *bytes;
};

void free_slice(slice *s)
{
    free(s->bytes);
}

void append(slice *self, char *str)
{
    size_t n = strlen(str);
    while ((self->len + n + 1) > self->cap){
        self->cap = 1.6*self->cap;
        printf("realloc to cap: %d\n",self->cap);
        self->bytes = realloc(self->bytes, self->cap); 
    } 
    
    char *dst = self->bytes + self->len;
    memcpy(dst, str, n); 
    self->len = self->len+n;
    self->bytes[self->len] = '\0';
}

slice* make_slice()
{
    slice *self = malloc(sizeof(slice));
    self->len = 0;
    self->cap = 16;
    self->bytes = malloc(sizeof(char)*16); 
    return self;
}
