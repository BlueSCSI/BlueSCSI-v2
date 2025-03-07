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

#pragma once

#include <stdint.h>
#include <strings.h>
namespace zipparser
{
    enum class parsing_target {signature, version, flag, method, modify_time, modify_date, 
                                crc, size_compressed, size_uncompressed, filename_len, 
                                extra_field_len, filename, extra_field, end};

    class Parser
    {
        public:
            Parser();
            Parser(char const *filename, const size_t length, const size_t target_total_length);
            void SetMatchingFilename(char const *filename, const size_t length, const size_t target_total_length);
            void Reset();
            static const int32_t PARSE_ERROR = -1;
            static const int32_t PARSE_CENTRAL_DIR = -2;
            static const int32_t PARSE_UNSUPPORTED_COMPRESSION = -3;
            // A state machine that per byte processes the incoming buffer
            // \param buf a pointer to binary data to be processed
            // \param size of data to be processed, can by 1 for a single byte at a time
            // \returns the number of bytes processed or -1 if an error ocurred
            int32_t Parse(uint8_t const *buf, const size_t size);
            bool FoundMatch();
            inline uint32_t GetCompressedSize() {return compressed_data_size;}

        protected:
            bool filename_match;
            char const *filename;
            size_t filename_len;
            size_t current_zip_filename_len;
            size_t target_zip_filename_len;
            size_t extra_field_len;
            uint32_t compressed_data_size;
            uint32_t uncompressed_data_size;
            parsing_target target;
            size_t position;
            uint32_t crc;

    };
}