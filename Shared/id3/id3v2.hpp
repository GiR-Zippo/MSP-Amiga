/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003 M. Bakker, Ahead Software AG, http://www.nero.com
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Ahead Software through Mpeg4AAClicense@nero.com.
**
** $Id: id3v2tag.h,v 1.3 2003/07/29 08:20:11 menno Exp $
**/

#ifndef __ID3V2TAG_H__
#define __ID3V2TAG_H__

#include <cstdio>
#include <string>
#include <vector>
#include "Common.h"

/*
Usage:
- Read the meta
    ID3Meta meta = ID3V2ReaderWriter::ReadID3MetaData(FileName);

- Print Meta
    printf("%s \n", meta.title.c_str());

- mofiy or create
    meta.title = "New Title";

- Save
    ID3V2ReaderWriter::WriteID3MetaData(FileName, ID3Meta);
*/

struct ID3Meta
{
    // Grunddaten
    std::string title;
    std::string artist;
    std::string album;
    std::string year;
    std::string track;
    std::string genre;
    std::string comment;

    // Erweiterte Metadaten
    std::string composer;
    std::string encoder;
    std::string copyright;
    std::string url;

    // Technische Daten
    uint32_t audioOffset; // Offset zu den Audiodaten (Skip Junk)
    uint32_t tagSize;     // Gesamtgröße des ID3-Blocks
    uint8_t version;      // ID3v2.x (z.B. 3 für v2.3)

    float duration;       // in sekunden
    bool hasTag;
};

class ID3V2ReaderWriter
{
    public:
        static ID3Meta ReadID3MetaData(const char *filename);
        static bool WriteID3MetaData(const char *filename, const ID3Meta &meta);

    private:
        /*********************************************************/
        /***                  Helper Functions                 ***/
        /*********************************************************/
        static float calculate_duration(FILE* file, long tagSize);
        static uint32_t calculateFrameSize(const std::string& s);
        static void writeTags(FILE* f, const ID3Meta &meta, int totalFramesSize);
        static void writeTextFrame(FILE* f, const char* id, const std::string& val);
        static void writeFrame(FILE *f, const char *id, const std::string &val);
        static void writeInt32BE(FILE *f, uint32_t val);
        static void writeSyncsafe(FILE *f, uint32_t val);
};

#endif