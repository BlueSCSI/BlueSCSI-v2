/*
 * Simple CUE sheet parser suitable for embedded systems.
 *
 *  Copyright (c) 2023 Rabbit Hole Computing
 *
 *  This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

// Refer to e.g. https://www.gnu.org/software/ccd2cue/manual/html_node/CUE-sheet-format.html#CUE-sheet-format
//
// Example of a CUE file:
// FILE "foo bar.bin" BINARY
//   TRACK 01 MODE1/2048
//     INDEX 01 00:00:00
//   TRACK 02 AUDIO
//     PREGAP 00:02:00
//     INDEX 01 02:47:20
//   TRACK 03 AUDIO
//     INDEX 00 07:55:58
//     INDEX 01 07:55:65


#include "CUEParser.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

CUEParser::CUEParser(): CUEParser("")
{

}

CUEParser::CUEParser(const char *cue_sheet):
    m_cue_sheet(cue_sheet)
{
    restart();
}

void CUEParser::restart()
{
    m_parse_pos = m_cue_sheet;
    memset(&m_track_info, 0, sizeof(m_track_info));
}

const CUETrackInfo *CUEParser::next_track()
{
    // Previous track info is needed to track file offset
    uint32_t prev_track_start = m_track_info.track_start;
    uint32_t prev_sector_length = get_sector_length(m_track_info.file_mode, m_track_info.track_mode); // Defaults to 2352 before first track

    bool got_track = false;
    bool got_data = false;
    bool got_pause = false; // true if a period of silence (INDEX 00) was encountered for a track
    while(!(got_track && got_data) && start_line())
    {
        if (strncasecmp(m_parse_pos, "FILE ", 5) == 0)
        {
            const char *p = read_quoted(m_parse_pos + 5, m_track_info.filename, sizeof(m_track_info.filename));
            m_track_info.file_mode = parse_file_mode(skip_space(p));
            m_track_info.file_offset = 0;
            m_track_info.track_mode = CUETrack_AUDIO;
            prev_track_start = 0;
            prev_sector_length = get_sector_length(m_track_info.file_mode, m_track_info.track_mode);
        }
        else if (strncasecmp(m_parse_pos, "TRACK ", 6) == 0)
        {
            const char *track_num = skip_space(m_parse_pos + 6);
            char *endptr;
            m_track_info.track_number = strtoul(track_num, &endptr, 10);
            m_track_info.track_mode = parse_track_mode(skip_space(endptr));
            m_track_info.sector_length = get_sector_length(m_track_info.file_mode, m_track_info.track_mode);
            m_track_info.unstored_pregap_length = 0;
            m_track_info.data_start = 0;
            m_track_info.track_start = 0;
            got_track = true;
            got_data = false;
            got_pause = false;
        }
        else if (strncasecmp(m_parse_pos, "PREGAP ", 7) == 0)
        {
            const char *time_str = skip_space(m_parse_pos + 7);
            m_track_info.unstored_pregap_length = parse_time(time_str);
        }
        else if (strncasecmp(m_parse_pos, "INDEX ", 6) == 0)
        {
            const char *index_str = skip_space(m_parse_pos + 6);
            char *endptr;
            int index = strtoul(index_str, &endptr, 10);

            const char *time_str = skip_space(endptr);
            uint32_t time = parse_time(time_str);

            if (index == 0)
            {
                m_track_info.track_start = time;
                got_pause = true;
            }
            else if (index == 1)
            {
                m_track_info.data_start = time;
                got_data = true;
            }
        }

        next_line();
    }

    if (got_data && !got_pause)
    {
        m_track_info.track_start = m_track_info.data_start;
    }

    if (got_track && got_data)
    {
        m_track_info.file_offset += (uint64_t)(m_track_info.track_start - prev_track_start) * prev_sector_length;
        return &m_track_info;
    }
    else
    {
        return nullptr;
    }
}

bool CUEParser::start_line()
{
    // Skip initial whitespace
    while (isspace(*m_parse_pos))
    {
        m_parse_pos++;
    }
    return *m_parse_pos != '\0';
}

void CUEParser::next_line()
{
    // Find end of current line
    const char *p = m_parse_pos;
    while (*p != '\n' && *p != '\0')
    {
        p++;
    }

    // Skip any linefeeds
    while (*p == '\n' || *p == '\r')
    {
        p++;
    }

    m_parse_pos = p;
}

const char *CUEParser::skip_space(const char *p) const
{
    while (isspace(*p)) p++;
    return p;
}

const char *CUEParser::read_quoted(const char *src, char *dest, int dest_size)
{
    // Search for starting quote
    while (*src != '"')
    {
        if (*src == '\0' || *src == '\n')
        {
            // Unexpected end of line / file
            dest[0] = '\0';
            return src;
        }

        src++;
    }

    src++;

    // Copy text until ending quote
    int len = 0;
    while (*src != '"' && *src != '\0' && *src != '\n')
    {
        if (len < dest_size - 1)
        {
            dest[len++] = *src;
        }

        src++;
    }

    dest[len] = '\0';

    if (*src == '"') src++;
    return src;
}

uint32_t CUEParser::parse_time(const char *src)
{
    char *endptr;
    uint32_t minutes = strtoul(src, &endptr, 10);
    if (*endptr == ':') endptr++;
    uint32_t seconds = strtoul(endptr, &endptr, 10);
    if (*endptr == ':') endptr++;
    uint32_t frames = strtoul(endptr, &endptr, 10);

    return frames + 75 * (seconds + 60 * minutes);
}

CUEFileMode CUEParser::parse_file_mode(const char *src)
{
    if (strncasecmp(src, "BIN", 3) == 0)
        return CUEFile_BINARY;
    else if (strncasecmp(src, "MOTOROLA", 8) == 0)
        return CUEFile_MOTOROLA;
    else if (strncasecmp(src, "MP3", 3) == 0)
        return CUEFile_MP3;
    else if (strncasecmp(src, "WAV", 3) == 0)
        return CUEFile_WAVE;
    else if (strncasecmp(src, "AIFF", 4) == 0)
        return CUEFile_AIFF;
    else
        return CUEFile_BINARY; // Default to binary mode
}

CUETrackMode CUEParser::parse_track_mode(const char *src)
{
    if (strncasecmp(src, "AUDIO", 5) == 0)
        return CUETrack_AUDIO;
    else if (strncasecmp(src, "CDG", 3) == 0)
        return CUETrack_CDG;
    else if (strncasecmp(src, "MODE1/2048", 10) == 0)
        return CUETrack_MODE1_2048;
    else if (strncasecmp(src, "MODE1/2352", 10) == 0)
        return CUETrack_MODE1_2352;
    else if (strncasecmp(src, "MODE2/2048", 10) == 0)
        return CUETrack_MODE2_2048;
    else if (strncasecmp(src, "MODE2/2324", 10) == 0)
        return CUETrack_MODE2_2324;
    else if (strncasecmp(src, "MODE2/2336", 10) == 0)
        return CUETrack_MODE2_2336;
    else if (strncasecmp(src, "MODE2/2352", 10) == 0)
        return CUETrack_MODE2_2352;
    else if (strncasecmp(src, "CDI/2336", 8) == 0)
        return CUETrack_CDI_2336;
    else if (strncasecmp(src, "CDI/2352", 8) == 0)
        return CUETrack_CDI_2352;
    else
        return CUETrack_MODE1_2048; // Default to data track
}

uint32_t CUEParser::get_sector_length(CUEFileMode filemode, CUETrackMode trackmode)
{
    if (filemode == CUEFile_BINARY || filemode == CUEFile_MOTOROLA)
    {
        switch (trackmode)
        {
            case CUETrack_AUDIO:        return 2352;
            case CUETrack_CDG:          return 2448;
            case CUETrack_MODE1_2048:   return 2048;
            case CUETrack_MODE1_2352:   return 2352;
            case CUETrack_MODE2_2048:   return 2048;
            case CUETrack_MODE2_2324:   return 2324;
            case CUETrack_MODE2_2336:   return 2336;
            case CUETrack_MODE2_2352:   return 2352;
            case CUETrack_CDI_2336:     return 2336;
            case CUETrack_CDI_2352:     return 2352;
            default:                    return 2048;
        }
    }
    else
    {
        return 0;
    }
}