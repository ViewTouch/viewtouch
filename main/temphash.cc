
#include <stdio.h>

#include "license_hash.hh"
#ifdef DMALLOC
#include <dmalloc.h>
#endif


#define STRLONG 4096

int main(int argc, char *argv[])
{
    char license[STRLONG];

    if (argc > 1)
    {
        if (GenerateTempKey(license, 4096, argv[1]) == 0)
            printf("%s", license);

    }
    else
    {
        printf("Usage:  %s <hardware ID>\n", argv[0]);
    }

    return 0;
}
