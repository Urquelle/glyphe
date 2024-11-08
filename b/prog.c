#include "glyphe.h"

int main(int argc, char *argv[])
{
    if (fopen_s(&dbg_ausgabe, "fehler.txt", "w") != 0)
    {
        printf("datei konnte nicht geöffnet werden.\n");

        exit(1);
    }

    otf_schrift_t *fira_code_schrift = otf_schrift_lesen("FiraCode-Regular.ttf");
    if (fira_code_schrift)
    {
        otf_schrift_info_ausgeben(fira_code_schrift);
    }

    return 0;
}
