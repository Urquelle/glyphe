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

FILE *dbg_ausgabe = 0;

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
} otf_punkt_t;

// otf_glyphe_t Kontur
typedef struct {
    otf_punkt_t* punkte;
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

// Neue Strukturen für die Name-Tabelle
typedef struct {
    uint16_t platform_id;
    uint16_t encoding_id;
    uint16_t language_id;
    uint16_t name_id;
    uint16_t länge;
    uint16_t versatz;
    char* text;  // UTF-16BE kodierter String
} otf_name_eintrag_t;

typedef struct {
    uint16_t format;
    uint16_t anzahl;
    uint16_t text_versatz;
    otf_name_eintrag_t* einträge;
} otf_name_tabelle_t;

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

// Funktion zum Konvertieren von UTF-16BE zu UTF-8
char *
utf16be_zu_utf8(const uint8_t* utf16be, size_t länge) 
{
    if (länge % 2 != 0) return NULL;

    // Maximal 4 Bytes pro UTF-16 Character in UTF-8
    char* utf8 = (char*)GLYPHE_RES(länge * 2 + 1);
    size_t utf8_pos = 0;

    for (size_t i = 0; i < länge; i += 2)
    {
        uint16_t codepoint = (utf16be[i] << 8) | utf16be[i + 1];

        if (codepoint < 0x80)
        {
            utf8[utf8_pos++] = codepoint;
        }
        else if (codepoint < 0x800)
        {
            utf8[utf8_pos++] = 0xC0 | (codepoint >> 6);
            utf8[utf8_pos++] = 0x80 | (codepoint & 0x3F);
        }
        else
        {
            utf8[utf8_pos++] = 0xE0 | (codepoint >> 12);
            utf8[utf8_pos++] = 0x80 | ((codepoint >> 6) & 0x3F);
            utf8[utf8_pos++] = 0x80 | (codepoint & 0x3F);
        }
    }
    utf8[utf8_pos] = '\0';

    return utf8;
}

// Funktion zum Einlesen der Name-Tabelle
void
otf_name_tabelle_lesen(FILE* datei, otf_schrift_t* schrift, otf_tabelle_t* name_tabelle)
{
    if (!name_tabelle) return;

    fseek(datei, name_tabelle->versatz, SEEK_SET);

    // Format und Anzahl der Name Records
    uint16_t format = otf_u16_lesen(datei);
    uint16_t anzahl = otf_u16_lesen(datei);
    uint16_t text_versatz = otf_u16_lesen(datei);

    // Position des Textspeichers (relativ zum Tabellenanfang)
    long text_basis = name_tabelle->versatz + text_versatz;

    // Temporäres Array für Nameeinträge
    otf_name_eintrag_t* einträge = (otf_name_eintrag_t*)
        GLYPHE_RES(sizeof(otf_name_eintrag_t) * anzahl);

    // Nameeinträge einlesen
    for (int i = 0; i < anzahl; i++)
    {
        einträge[i].platform_id = otf_u16_lesen(datei);
        einträge[i].encoding_id = otf_u16_lesen(datei);
        einträge[i].language_id = otf_u16_lesen(datei);
        einträge[i].name_id = otf_u16_lesen(datei);
        einträge[i].länge = otf_u16_lesen(datei);
        einträge[i].versatz = otf_u16_lesen(datei);

        // Aktuelle Position merken
        long aktuelle_position = ftell(datei);

        // Zum Text springen
        fseek(datei, text_basis + einträge[i].versatz, SEEK_SET);

        // Text einlesen
        uint8_t* text = (uint8_t*)GLYPHE_RES(einträge[i].länge);
        fread(text, 1, einträge[i].länge, datei);

        // Konvertierung zu UTF-8 wenn nötig
        if (einträge[i].platform_id == 0 || // Unicode
            (einträge[i].platform_id == 3 && einträge[i].encoding_id == 1))
        {
            // Windows Unicode
            einträge[i].text = utf16be_zu_utf8(text, einträge[i].länge);
        }
        else
        {
            // Für andere Encodings einfach als ASCII behandeln
            einträge[i].text = (char*)GLYPHE_RES(einträge[i].länge + 1);
            memcpy(einträge[i].text, text, einträge[i].länge);
            einträge[i].text[einträge[i].länge] = '\0';
        }

        GLYPHE_FRG(text);

        // Relevanten Text in der Schrift-Struktur speichern
        if ((einträge[i].platform_id == 3 && einträge[i].language_id == 0x0409))
        {
            // English (US)
            switch (einträge[i].name_id)
            {
                case 1: // Artname (Font Family)
                {
                    if (!schrift->artname) {
                        schrift->artname = _strdup(einträge[i].text);
                    }
                } break;

                case 2: // Stilname (Font Subfamily)
                {
                    if (!schrift->stilname) {
                        schrift->stilname = _strdup(einträge[i].text);
                    }
                } break;
            }
        }

        // Zurück zur ursprünglichen Position
        fseek(datei, aktuelle_position, SEEK_SET);
    }

    // Aufräumen
    for (int i = 0; i < anzahl; i++)
    {
        GLYPHE_FRG(einträge[i].text);
    }
}

// Verbesserte Implementierung für die Schalter-Allokation und -Verarbeitung
uint8_t*
otf_schalter_einlesen(FILE* datei, uint16_t punkte_gesamt, size_t* finale_anzahl)
{
    // Initial mehr Speicher reservieren für mögliche Wiederholungen
    size_t aktuelle_kapazität = punkte_gesamt * 2;  // Start mit doppelter Größe
    uint8_t* schalter = (uint8_t*)GLYPHE_RES(aktuelle_kapazität);
    size_t aktuelle_position = 0;

    for (uint16_t j = 0; j < punkte_gesamt && aktuelle_position < aktuelle_kapazität; j++)
    {
        schalter[aktuelle_position] = fgetc(datei);

        if (schalter[aktuelle_position] & 0x08)
        {
            // Wiederholungsschalter
            uint8_t wiederholungen = fgetc(datei);

            // Prüfen ob mehr Speicher benötigt wird
            size_t benötigter_speicher = aktuelle_position + wiederholungen + 2;
            if (benötigter_speicher > aktuelle_kapazität)
            {
                aktuelle_kapazität = benötigter_speicher * 2;  // Verdoppeln für zukünftige Wiederholungen
                uint8_t* neuer_speicher = (uint8_t*)GLYPHE_RES(aktuelle_kapazität);
                memcpy(neuer_speicher, schalter, aktuelle_position + 1);
                GLYPHE_FRG(schalter);
                schalter = neuer_speicher;
            }

            // Wiederholungen kopieren
            for (uint8_t k = 0; k < wiederholungen; k++)
            {
                schalter[++aktuelle_position] = schalter[aktuelle_position-1];
            }
        }
        aktuelle_position++;
    }

    *finale_anzahl = aktuelle_position;
    return schalter;
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

void
otf_schrift_info_ausgeben(otf_schrift_t* schrift)
{
    printf("Stilname: %s\n", schrift->stilname);
    printf("Artname: %s\n", schrift->artname);
    printf("Anzahl Glyphen: %d\n", schrift->glyphen_anzahl);
    printf("Version: %08X\n", schrift->kopf.sfnt_version);

    // AUFGABE: Anzahl der unterstützten Sprachen ermitteln

    otf_tabelle_t* feat_tabelle = otf_tabelle_finden(schrift, "feat");
    if (feat_tabelle)
    {
        // AUFGABE: OpenType Merkmale ausgeben
        printf("OpenType Merkmale:\n");
    }
    else
    {
        printf("Keine OpenType Merkmale gefunden.\n");
    }
}

// Hauptfunktion zum Einlesen der Font-Datei
otf_schrift_t*
otf_schrift_lesen(const char* dateiname)
{
    FILE* datei;
    fopen_s(&datei, dateiname, "rb");

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
        schrift->kopf.sfnt_version != 0x00010000)   // Version 1.0 für TTF
    {
        fprintf(dbg_ausgabe, "Ungültiges Font-Format\n");
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
    otf_name_tabelle_lesen(datei, schrift, name_tabelle);

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
        for (int i = 0; i < schrift->glyphen_anzahl; i++)
        {
            int16_t glyphen_länge = otf_s16_lesen(datei);

            if (glyphen_länge > 0)
            {
                // Einfacher Glyph
                otf_glyphe_t* glyph = &schrift->glyphen[i];

                // Rahmen
                glyph->x_min = otf_s16_lesen(datei);
                glyph->y_min = otf_s16_lesen(datei);
                glyph->x_max = otf_s16_lesen(datei);
                glyph->y_max = otf_s16_lesen(datei);

                // Anzahl der Konturen
                glyph->konturen_anzahl = otf_s16_lesen(datei);
                glyph->konturen = (otf_kontur_t*) GLYPHE_RES(sizeof(otf_kontur_t) * glyph->konturen_anzahl);

                // Einlesen der einzelnen Konturen
                for (int j = 0; j < glyph->konturen_anzahl; j++)
                {
                    otf_kontur_t* kontur = &glyph->konturen[j];
                    uint16_t punkte_gesamt = otf_u16_lesen(datei);
                    size_t schalter_anzahl;
                    uint8_t* schalter = otf_schalter_einlesen(datei, punkte_gesamt, &schalter_anzahl);

                    kontur->punkte = (otf_punkt_t*) GLYPHE_RES(sizeof(otf_punkt_t) * schalter_anzahl);
                    kontur->punkte_anzahl = (int) schalter_anzahl;

                    int16_t x = 0, y = 0;
                    for (size_t k = 0; k < schalter_anzahl; k++)
                    {
                        if (schalter[k] & 0x01)
                        {
                            // Gerade Linie
                            x += otf_s16_lesen(datei);
                            y += otf_s16_lesen(datei);
                        }
                        else
                        {
                            // Kurve
                            int16_t dx1 = otf_s16_lesen(datei);
                            int16_t dy1 = otf_s16_lesen(datei);
                            int16_t dx2 = otf_s16_lesen(datei);
                            int16_t dy2 = otf_s16_lesen(datei);
                            x += dx1;
                            y += dy1;
                            kontur->punkte[k].x = x;
                            kontur->punkte[k].y = y;
                            k++;
                            x += dx2;
                            y += dy2;
                        }
                        kontur->punkte[k].x = x;
                        kontur->punkte[k].y = y;
                        kontur->punkte[k].in_kontur = 1;
                    }

                    GLYPHE_FRG(schalter);
                }
            }
            else if (glyphen_länge < 0)
            {
                // Zusammengesetzter Glyph
                otf_glyphe_t* glyph = &schrift->glyphen[i];

                // Anzahl der Komponenten
                uint16_t komponenten_anzahl = (uint16_t)(-glyphen_länge / 2);
                glyph->konturen_anzahl = komponenten_anzahl;
                glyph->konturen = (otf_kontur_t*) GLYPHE_RES(sizeof(otf_kontur_t) * komponenten_anzahl);

                // Einlesen der Komponenten
                for (int j = 0; j < komponenten_anzahl; j++)
                {
                    otf_kontur_t* kontur = &glyph->konturen[j];
                    uint16_t glyphen_index = otf_u16_lesen(datei);
                    int16_t x_versatz = otf_s16_lesen(datei);
                    int16_t y_versatz = otf_s16_lesen(datei);
                    uint8_t flags = fgetc(datei);

                    // Verweis auf Subglyphe kopieren
                    kontur->punkte = schrift->glyphen[glyphen_index].konturen[0].punkte;
                    kontur->punkte_anzahl = schrift->glyphen[glyphen_index].konturen[0].punkte_anzahl;

                    // Transformationen anwenden
                    for (int k = 0; k < kontur->punkte_anzahl; k++)
                    {
                        kontur->punkte[k].x += x_versatz;
                        kontur->punkte[k].y += y_versatz;
                    }
                }
            }
            else
            {
                // Leerer Glyph
                // Nichts zu tun
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
