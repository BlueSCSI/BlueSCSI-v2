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

#pragma once

#include <stdint.h>

#ifndef CUE_MAX_FILENAME
#define CUE_MAX_FILENAME 64
#endif

enum CUEFileMode
{
    CUEFile_BINARY = 0,
    CUEFile_MOTOROLA,
    CUEFile_MP3,
    CUEFile_WAVE,
    CUEFile_AIFF,
};

enum CUETrackMode
{
    CUETrack_AUDIO = 0,
    CUETrack_CDG,
    CUETrack_MODE1_2048,
    CUETrack_MODE1_2352,
    CUETrack_MODE2_2048,
    CUETrack_MODE2_2324,
    CUETrack_MODE2_2336,
    CUETrack_MODE2_2352,
    CUETrack_CDI_2336,
    CUETrack_CDI_2352,
};

struct CUETrackInfo
{
    char filename[CUE_MAX_FILENAME+1];
    CUEFileMode file_mode;
    int track_number;
    CUETrackMode track_mode;

    uint32_t unstored_pregap_length;
    uint32_t pregap_start;
    uint32_t data_start;
};

class CUEParser
{
public:
    CUEParser();

    // Initialize the class to parse data from string.
    // The string must remain valid for the lifetime of this object.
    CUEParser(const char *cue_sheet);

    // Restart parsing from beginning of file
    void restart();

    // Get information for next track.
    // Returns nullptr when there are no more tracks.
    // The returned pointer remains valid until next call to next_track()
    // or destruction of this object.
    const CUETrackInfo *next_track();

protected:
    const char *m_cue_sheet;
    const char *m_parse_pos;
    CUETrackInfo m_track_info;

    // Skip any whitespace at beginning of line.
    // Returns false if at end of string.
    bool start_line();

    // Advance parser to next line
    void next_line();

    // Skip spaces in string, return pointer to first non-space character
    const char *skip_space(const char *p) const;

    // Read text starting with " and ending with next "
    // Returns pointer to character after ending quote.
    const char *read_quoted(const char *src, char *dest, int dest_size);

    // Parse time from MM:SS:FF format to frame number
    uint32_t parse_time(const char *src);

    // Parse file mode into enum
    CUEFileMode parse_file_mode(const char *src);

    // Parse track mode into enum
    CUETrackMode parse_track_mode(const char *src);
};
