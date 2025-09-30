

#include "../main/license_hash.hh"

#define STRLONG 4096

int main(int argc, const char* argv[])
{
    char buffer[STRLONG];

    GetMacAddress(buffer, STRLONG);
    printf("MAC:  %s\n", buffer);

    GetUnameInfo(buffer, STRLONG);
    printf("UNAME:  %s\n", buffer);

    GetMachineDigest(buffer, STRLONG);
    printf("MID:  %s\n", buffer);

    return 0;
}

