#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "slice.c"

int main()
{
    slice *test = make_slice();
    slice *test2 = make_slice();
    
    char *c1 = "hello, test!";
    char *c2 = "test, world?";
    char *c3 = "dylan, dylan, dylan, dylan, and dylan.";
    char *c4 = "012345678901234";
    char *c5 = "0123456789012345";

    append(test2, c4);
    append(test2, c5);

    append(test, c1);
    append(test, c2);
    append(test, c1);
    append(test, c2);
    append(test, c3);

    printf("%s\n",test->bytes);
    printf("%s\n",test2->bytes);

    append(test, test2->bytes);
    printf("slice: %s\n",test->bytes);

    return 0;
}
