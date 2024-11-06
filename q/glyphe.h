#ifndef __GLYPHE_H__
#define __GLYPHE_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifndef GLYPHE_RES
#define GLYPHE_RES malloc
#endif

#ifndef GLYPHE_FRG
#define GLYPHE_FRG free
#endif

// OpenType Tabellenverzeichnis Eintrag
typedef struct {
    char kennung[4];
    uint32_t prüfsumme;
    uint32_t versatz;
    uint32_t länge;
} otf_tabelle_t;

// OpenType Header
typedef struct {
    uint32_t sfnt_version;
    uint16_t tabellen_anzahl;
    uint16_t suchumfang;
    uint16_t suchpunkt;
    uint16_t versatz;
} otf_kopf_t;

// otf_glyphe_t Outline Punkt
typedef struct {
    int16_t x;
    int16_t y;
    uint8_t in_kontur;
} otf_glyphe_punkt_t;

// otf_glyphe_t Kontur
typedef struct {
    otf_glyphe_punkt_t* punkte;
    int punkte_anzahl;
} otf_kontur_t;

// otf_glyphe_t Daten
typedef struct {
    int16_t konturen_anzahl;
    int16_t x_min;
    int16_t y_min;
    int16_t x_max;
    int16_t y_max;
    otf_kontur_t* konturen;
} otf_glyphe_t;

// Font Datenstruktur
typedef struct {
    otf_kopf_t kopf;
    otf_tabelle_t* tabellen;
    otf_glyphe_t* glyphen;
    int glyphen_anzahl;
    // Weitere Metadaten
    char* artname;
    char* stilname;
    uint16_t einheiten_pro_em;
    int16_t oberlänge;
    int16_t unterlänge;
    int16_t zeilenabstand;
} otf_schrift_t;

// Hilfsfunktion zum Lesen von Big-Endian Werten
uint32_t
otf_u32_lesen(FILE* datei)
{
    uint32_t wert;
    fread(&wert, sizeof(uint32_t), 1, datei);
    // Konvertierung von Big-Endian nach Host-Endian
    return ((wert & 0xFF000000) >> 24) |
           ((wert & 0x00FF0000) >> 8) |
           ((wert & 0x0000FF00) << 8) |
           ((wert & 0x000000FF) << 24);
}

uint16_t
otf_u16_lesen(FILE* datei)
{
    uint16_t wert;
    fread(&wert, sizeof(uint16_t), 1, datei);
    // Konvertierung von Big-Endian nach Host-Endian
    return ((wert & 0xFF00) >> 8) |
           ((wert & 0x00FF) << 8);
}

int16_t
otf_s16_lesen(FILE* datei)
{
    int16_t wert;
    fread(&wert, sizeof(int16_t), 1, datei);
    // Konvertierung von Big-Endian nach Host-Endian
    return ((wert & 0xFF00) >> 8) |
           ((wert & 0x00FF) << 8);
}

// Funktion zum Einlesen des Font Headers
void
otf_kopf_lesen(FILE* datei, otf_schrift_t* schrift)
{
    schrift->kopf.sfnt_version = otf_u32_lesen(datei);
    schrift->kopf.tabellen_anzahl = otf_u16_lesen(datei);
    schrift->kopf.suchumfang = otf_u16_lesen(datei);
    schrift->kopf.suchpunkt = otf_u16_lesen(datei);
    schrift->kopf.versatz = otf_u16_lesen(datei);
}

// Funktion zum Einlesen der Tabellen-Verzeichnisse
void
otf_tabellenverzeichnis_lesen(FILE* datei, otf_schrift_t* schrift)
{
    schrift->tabellen = (otf_tabelle_t *)
        GLYPHE_RES(sizeof(otf_tabelle_t) * schrift->kopf.tabellen_anzahl);

    for (int i = 0; i < schrift->kopf.tabellen_anzahl; i++)
    {
        fread(schrift->tabellen[i].kennung, 1, 4, datei);
        schrift->tabellen[i].prüfsumme = otf_u32_lesen(datei);
        schrift->tabellen[i].versatz = otf_u32_lesen(datei);
        schrift->tabellen[i].länge = otf_u32_lesen(datei);
    }
}

// Funktion zum Finden einer spezifischen Tabelle
otf_tabelle_t*
otf_tabelle_finden(otf_schrift_t* schrift, const char* kennung)
{
    for (int i = 0; i < schrift->kopf.tabellen_anzahl; i++)
    {
        if (memcmp(schrift->tabellen[i].kennung, kennung, 4) == 0)
        {
            return &schrift->tabellen[i];
        }
    }

    return NULL;
}

// Hauptfunktion zum Einlesen der Font-Datei
otf_schrift_t*
otf_schrift_lesen(const char* dateiname)
{
    FILE* datei = fopen(dateiname, "rb");
    if (!datei)
    {
        printf("Fehler beim Öffnen der Datei\n");
        return NULL;
    }

    otf_schrift_t* schrift = (otf_schrift_t *) GLYPHE_RES(sizeof(otf_schrift_t));
    memset(schrift, 0, sizeof(otf_schrift_t));

    // Header einlesen
    otf_kopf_lesen(datei, schrift);

    // Prüfen ob es sich um eine gültige OTF/TTF Datei handelt
    if (schrift->kopf.sfnt_version != 0x4F54544F && // 'OTTO' für OTF
        schrift->kopf.sfnt_version != 0x00010000)  // Version 1.0 für TTF
    {
        printf("Ungültiges Font-Format\n");
        GLYPHE_FRG(schrift);
        fclose(datei);
        return NULL;
    }

    // Tabellen-Verzeichnis einlesen
    otf_tabellenverzeichnis_lesen(datei, schrift);

    // head Tabelle für Metadaten finden und einlesen
    otf_tabelle_t* kopf_tabelle = otf_tabelle_finden(schrift, "head");
    if (kopf_tabelle)
    {
        fseek(datei, kopf_tabelle->versatz, SEEK_SET);
        // Version und Revision überspringen
        fseek(datei, 8, SEEK_CUR);
        // Weitere Metadaten einlesen
        schrift->einheiten_pro_em = otf_u16_lesen(datei);
    }

    // hhea Tabelle für vertikale Metriken
    otf_tabelle_t* hhea_tabelle = otf_tabelle_finden(schrift, "hhea");
    if (hhea_tabelle)
    {
        fseek(datei, hhea_tabelle->versatz, SEEK_SET);
        // Version überspringen
        fseek(datei, 4, SEEK_CUR);
        schrift->oberlänge = otf_u16_lesen(datei);
        schrift->unterlänge = otf_u16_lesen(datei);
        schrift->zeilenabstand = otf_u16_lesen(datei);
    }

    // name Tabelle für Font-Namen
    otf_tabelle_t* name_tabelle = otf_tabelle_finden(schrift, "name");
    if (name_tabelle)
    {
        fseek(datei, name_tabelle->versatz, SEEK_SET);
        // Hier würde die Verarbeitung der name Tabelle folgen
        // Dies ist komplex und erfordert zusätzliche Strukturen für String-IDs
    }

    // maxp Tabelle für Glyph-Anzahl
    otf_tabelle_t* maxp_tabelle = otf_tabelle_finden(schrift, "maxp");
    if (maxp_tabelle)
    {
        fseek(datei, maxp_tabelle->versatz, SEEK_SET);
        // Version überspringen
        fseek(datei, 4, SEEK_CUR);
        schrift->glyphen_anzahl = otf_u16_lesen(datei);
        schrift->glyphen = (otf_glyphe_t *) GLYPHE_RES(sizeof(otf_glyphe_t) * schrift->glyphen_anzahl);
        memset(schrift->glyphen, 0, sizeof(otf_glyphe_t) * schrift->glyphen_anzahl);
    }

    // glyf Tabelle für Glyph-Daten
    otf_tabelle_t* glyf_tabelle = otf_tabelle_finden(schrift, "glyf");
    if (glyf_tabelle)
    {
        fseek(datei, glyf_tabelle->versatz, SEEK_SET);
        for (uint16_t i = 0; i < schrift->glyphen_anzahl; i++)
        {
            otf_glyphe_t* glyphe = &schrift->glyphen[i];

            // Lese Konturanzahl
            int16_t konturen_anzahl = otf_s16_lesen(datei);
            glyphe->konturen_anzahl = (konturen_anzahl < 0) ? 0 : konturen_anzahl;

            // Lese Bounding Box
            glyphe->x_min = otf_s16_lesen(datei);
            glyphe->y_min = otf_s16_lesen(datei);
            glyphe->x_max = otf_s16_lesen(datei);
            glyphe->y_max = otf_s16_lesen(datei);

            if (glyphe->konturen_anzahl > 0)
            {
                // Speicher für Konturen allozieren
                glyphe->konturen = (otf_kontur_t *) GLYPHE_RES(sizeof(otf_kontur_t) * glyphe->konturen_anzahl);

                // Endpunkte der Konturen einlesen
                uint16_t* endpunkte = (uint16_t *) GLYPHE_RES(sizeof(uint16_t) * glyphe->konturen_anzahl);
                for (int j = 0; j < glyphe->konturen_anzahl; j++)
                {
                    endpunkte[j] = otf_u16_lesen(datei);
                }

                // Gesamtanzahl der Punkte bestimmen
                uint16_t punkte_gesamt = endpunkte[glyphe->konturen_anzahl - 1] + 1;

                // Schalter einlesen
                uint8_t* schalter = (uint8_t *) GLYPHE_RES(punkte_gesamt);
                for (uint16_t j = 0; j < punkte_gesamt; j++)
                {
                    schalter[j] = fgetc(datei);
                    if (schalter[j] & 0x08) // Wiederholungsschalter
                    {
                        uint8_t wiederholungen = fgetc(datei);
                        for (uint8_t k = 0; k < wiederholungen; k++)
                        {
                            schalter[++j] = schalter[j-1];
                        }
                    }
                }

                // X-Koordinaten einlesen
                int16_t* x_koord = (int16_t*) GLYPHE_RES(sizeof(int16_t) * punkte_gesamt);
                int16_t x = 0;
                for (uint16_t j = 0; j < punkte_gesamt; j++)
                {
                    if (schalter[j] & 0x02) // Kurze Form
                    {
                        x += (schalter[j] & 0x10) ? fgetc(datei) : -fgetc(datei);
                    }
                    else if (!(schalter[j] & 0x10))
                    {
                        x += otf_s16_lesen(datei);
                    }
                    x_koord[j] = x;
                }

                // Y-Koordinaten einlesen
                int16_t* y_koord = (int16_t *) GLYPHE_RES(sizeof(int16_t) * punkte_gesamt);
                int16_t y = 0;
                for (uint16_t j = 0; j < punkte_gesamt; j++)
                {
                    if (schalter[j] & 0x04) // Kurze Form
                    {
                        y += (schalter[j] & 0x20) ? fgetc(datei) : -fgetc(datei);
                    }
                    else if (!(schalter[j] & 0x20))
                    {
                        y += otf_s16_lesen(datei);
                    }
                    y_koord[j] = y;
                }

                // Punkte den Konturen zuordnen
                uint16_t punkt_index = 0;
                for (int j = 0; j < glyphe->konturen_anzahl; j++)
                {
                    uint16_t punkte_in_kontur = (j == 0) ?
                        endpunkte[0] + 1 :
                        endpunkte[j] - endpunkte[j-1];

                    glyphe->konturen[j].punkte_anzahl = punkte_in_kontur;
                    glyphe->konturen[j].punkte = (otf_glyphe_punkt_t *) GLYPHE_RES(sizeof(otf_glyphe_punkt_t) * punkte_in_kontur);

                    for (uint16_t k = 0; k < punkte_in_kontur; k++)
                    {
                        glyphe->konturen[j].punkte[k].x = x_koord[punkt_index];
                        glyphe->konturen[j].punkte[k].y = y_koord[punkt_index];
                        glyphe->konturen[j].punkte[k].in_kontur = (schalter[punkt_index] & 0x01);
                        punkt_index++;
                    }
                }

                // Temporäre Arrays freigeben
                GLYPHE_FRG(endpunkte);
                GLYPHE_FRG(schalter);
                GLYPHE_FRG(x_koord);
                GLYPHE_FRG(y_koord);
            }
        }
    }

    fclose(datei);
    return schrift;
}

// Speicherfreigabe
void
otf_schrift_freigeben(otf_schrift_t* schrift)
{
    if (!schrift) return;

    // Tabellen freigeben
    if (schrift->tabellen)
    {
        GLYPHE_FRG(schrift->tabellen);
    }

    // Glyphen freigeben
    if (schrift->glyphen)
    {
        for (int i = 0; i < schrift->glyphen_anzahl; i++)
        {
            if (schrift->glyphen[i].konturen)
            {
                for (int j = 0; j < schrift->glyphen[i].konturen_anzahl; j++)
                {
                    if (schrift->glyphen[i].konturen[j].punkte)
                    {
                        GLYPHE_FRG(schrift->glyphen[i].konturen[j].punkte);
                    }
                }
                GLYPHE_FRG(schrift->glyphen[i].konturen);
            }
        }
        GLYPHE_FRG(schrift->glyphen);
    }

    // Strings freigeben
    if(schrift->artname) GLYPHE_FRG(schrift->artname);
    if(schrift->stilname) GLYPHE_FRG(schrift->stilname);

    // Font-Struktur selbst freigeben
    GLYPHE_FRG(schrift);
}

#endif
