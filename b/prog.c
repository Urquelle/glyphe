#include "glyphe.h"

int main(int argc, char *argv[])
{
    otf_schrift_t *fira_code_schrift = otf_schrift_lesen("FiraCode-Regular.ttf");

    printf("%s: %s\n", fira_code_schrift->artname, fira_code_schrift->stilname);

    return 0;
}
