
#include <sys/types.h>

#define NROUNDS         16

/* Define IntU32 to be an unsigned in 32 bits long */
typedef unsigned int  IntU32;
typedef unsigned char IntU8;

/* Define IntP to be an integer which
   is the same size as a pointer. */
typedef unsigned long IntP ;

typedef struct
{
    IntU32 p[2][NROUNDS+2];
    IntU32 sbox[4][256];
} BFkey_type ; 

typedef unsigned char bf_cblock[8];

int  BFEncrypt(char *key, int keylen, char *ciphrtext, int ciphrlen, char *plaintext, int *plainlen);
int  BFDecrypt(char *key, int keylen, char *plaintext, int plainlen, char *ciphrtext, int *ciphrlen);
int  InitializeBlowfish(char *key, int keylen);
