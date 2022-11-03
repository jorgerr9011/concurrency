#include <sys/types.h>
#include <md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PASS_LEN 6

long ipow(long base, int exp)
{
    long res = 1;
    for (;;)
    {
        if (exp & 1)
            res *= base;
        exp >>= 1;
        if (!exp)
            break;
        base *= base;
    }

    return res;
}

long pass_to_long(char *str) {
    long res = 0;

    for(int i=0; i < PASS_LEN; i++)
        res = res * 26 + str[i]-'a';

    return res;
};

void long_to_pass(long n, char *str) {  // str should have size PASS_SIZE+1
    for(int i=PASS_LEN-1; i >= 0; i--) {
        str[i] = n % 26 + 'a';
        n /= 26;
    }
    str[PASS_LEN] = '\0';
}

char *break_pass(char *md5) {
    MD5_CTX ctx;
    char hex_res[MD5_DIGEST_LENGTH * 2 + 1];
    char *pass = malloc((PASS_LEN + 1) * sizeof(char));
    long bound = ipow(26, PASS_LEN); // we have passwords of PASS_LEN
                                     // lowercase chars =>
                                    //     26 ^ PASS_LEN  different cases
    for(long i=0; i < bound; i++) {
        long_to_pass(i, pass);

        MD5Init(&ctx);
        MD5Update(&ctx, (unsigned char *) pass, PASS_LEN);
        MD5End(&ctx, hex_res);

        if(!strcmp(hex_res, md5)) break; // Found it!
    }

    return (char *) pass;
}

int main(int argc, char *argv[]) {
    if(argc < 2) {
        printf("Use: %s string\n", argv[0]);
        exit(0);
    }

    char *pass = break_pass(argv[1]);

    printf("%s: %s\n", argv[1], pass);

    free(pass);
    return 0;
}
