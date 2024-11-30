/**
 * ZuluSCSI™ - Copyright (c) 2024 Rabbit Hole Computing™
 *
 * ZuluSCSI™ firmware is licensed under the GPL version 3 or any later version. 
 *
 * https://www.gnu.org/licenses/gpl-3.0.html
 * ----
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. 
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/

#include "zip_parser.h"


namespace zipparser
{

    Parser::Parser()
    {
        filename_len = 0;
        Reset();
    }
    Parser::Parser(char const *filename, const size_t length)
    {
        Reset();
        SetMatchingFilename(filename, length);
    }

    void Parser::Reset()
    {
        target = parsing_target::signature;
        position = 0;
        deflate = false;
        filename_match = false;
        crc = 0;
    }

    void Parser::SetMatchingFilename(char const *filename, const size_t length)
    {
        if (filename[0] == '\0')
            filename_len = 0;
        else
        {
            this->filename = filename;
            filename_len = length;
        }
    }


    int32_t Parser::Parse(uint8_t const *buf, const size_t size)
    {
        if (filename_len == 0)
            return PARSE_ERROR;

        static bool matching = true;
        static bool central_dir = false;
        for (size_t idx = 0; idx < size; idx++)
        {
            switch (target)
            {
                case parsing_target::signature:
                    if (++position == 1 && buf[idx] == 'P')
                        break;
                    if (position == 2 && buf[idx] == 'K')
                    {
                        central_dir = false;
                        break;
                    }
                    if (position == 3 && buf[idx] == 0x03)
                        break;
                    if (position == 3 && buf[idx] == 0x01)
                    {
                        central_dir = true;
                        break;
                    }
                    if (position == 4 && central_dir && buf[idx] == 0x2)
                    {
                        return PARSE_CENTRAL_DIR;
                    }
                    if (position == 4 && buf[idx] == 0x04)
                    {
                        position = 0;
                        target = parsing_target::version;
                        break;
                    }
                    return PARSE_ERROR;
                break;
                case parsing_target::version:
                    if (++position == 2)
                    {
                        position = 0;
                        target = parsing_target::flag;
                    }
                break;
                case parsing_target::flag:
                    if (++position == 2)
                    {
                        position = 0;
                        target = parsing_target::method;
                    }
                break;
                case parsing_target::method:
                    if (++position == 1)
                    {
                        if (buf[idx] == 0x08 || buf[idx] == 0x00)
                            deflate = buf[idx] == 0x08;
                        else
                            return PARSE_UNSUPPORTED_COMPRESSION;
                    }
                    if (position == 2)
                    {
                        if (buf[idx] == 0)
                        {
                            position = 0;
                            target = parsing_target::modify_time;
                        }
                        else
                            return PARSE_UNSUPPORTED_COMPRESSION;
                    }
                break;
                case parsing_target::modify_time:
                    if (++position == 2)
                    {
                        position = 0;
                        target = parsing_target::modify_date;
                    }
                break;
                case parsing_target::modify_date:
                    if (++position == 2)
                    {
                        position = 0;
                        target = parsing_target::crc;
                        crc = 0;
                    }
                break;
                case parsing_target::crc:
                    crc |= buf[idx] << (8 * position++);
                    if (position == 4)
                    {
                        target = parsing_target::size_compressed;
                        compressed_data_size = 0;
                        position = 0;
                    }
                break;
                case parsing_target::size_compressed:
                    compressed_data_size |= buf[idx] << (8 * position++);
                    if (position == 4)
                    {
                        target = parsing_target::size_uncompressed;
                        uncompressed_data_size = 0;
                        position = 0;
                    }
                break;
                case parsing_target::size_uncompressed:
                    uncompressed_data_size |= buf[idx] << (8 * position++);
                    if (position == 4)
                    {
                        target = parsing_target::filename_len;
                        current_zip_filename_len = 0;
                        position = 0;
                    }
                break;
                case parsing_target::filename_len:
                    current_zip_filename_len |= buf[idx] << (8 * position++);
                    if (position == 2)
                    {
                        target = parsing_target::extra_field_len;
                        extra_field_len = 0;
                        position = 0;
                    }
                break;
                case parsing_target::extra_field_len:
                    extra_field_len |= buf[idx] << (8 * position++);
                    if (position == 2)
                    {
                        target = parsing_target::filename;
                        position = 0;
                        filename_match = false;
                        matching = true;
                    }
                break;
                case parsing_target::filename:
                    if (position <= current_zip_filename_len - 1)
                    {    
                        if (matching && position < filename_len && filename[position] != buf[idx])
                            matching = false;
                        if (position == filename_len - 1 && matching)
                            filename_match = true;
                        if (position == current_zip_filename_len -1)
                        {
                            target = parsing_target::extra_field;
                            matching = true;
                            position = 0;
                            if (extra_field_len == 0)
                            {
                                target = parsing_target::end;
                                return idx + 1;
                            }
                        }
                        else
                            position++;
                    }
                break;
                case parsing_target::extra_field:
                    // extra_field_len should be at least 1 by this time
                    if (++position == extra_field_len)
                    {
                        target = parsing_target::end;
                        return idx + 1;
                    }
                break;
                case parsing_target::end:
                    return 0;
                break;
            }
        }
        return size;
    }
    bool Parser::FoundMatch()
    {
        return filename_match;
    }

    uint32_t Parser::GetCompressedSize()
    {
        return compressed_data_size;
    }
}

