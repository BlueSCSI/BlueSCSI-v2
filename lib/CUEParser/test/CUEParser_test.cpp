#include "CUEParser.h"
#include <stdio.h>
#include <string.h>

/* Unit test helpers */
#define COMMENT(x) printf("\n----" x "----\n");
#define TEST(x) \
    if (!(x)) { \
        fprintf(stderr, "\033[31;1mFAILED:\033[22;39m %s:%d %s\n", __FILE__, __LINE__, #x); \
        status = 1; \
    } else { \
        printf("\033[32;1mOK:\033[22;39m %s\n", #x); \
    }

int test_basics()
{
    int status = 0;
    const char *cue_sheet = R"(
FILE "Image Name.bin" BINARY
  TRACK 01 MODE1/2048
    INDEX 01 00:00:00
  TRACK 02 AUDIO
    PREGAP 00:02:00
    INDEX 01 02:47:20
  TRACK 03 AUDIO
    INDEX 00 07:55:58
    INDEX 01 07:55:65
FILE "Sound.wav" WAVE
  TRACK 11 AUDIO
    INDEX 00 00:00:00
    INDEX 01 00:02:00
    )";

    CUEParser parser(cue_sheet);

    COMMENT("Test TRACK 01 (data)");
    const CUETrackInfo *track = parser.next_track();
    TEST(track != NULL);
    if (track)
    {
        TEST(strcmp(track->filename, "Image Name.bin") == 0);
        TEST(track->file_mode == CUEFile_BINARY);
        TEST(track->track_number == 1);
        TEST(track->track_mode == CUETrack_MODE1_2048);
        TEST(track->unstored_pregap_length == 0);
        TEST(track->data_start == 0);
    }

    COMMENT("Test TRACK 02 (audio with pregap)");
    track = parser.next_track();
    TEST(track != NULL);
    if (track)
    {
        TEST(strcmp(track->filename, "Image Name.bin") == 0);
        TEST(track->file_mode == CUEFile_BINARY);
        TEST(track->track_number == 2);
        TEST(track->track_mode == CUETrack_AUDIO);
        TEST(track->unstored_pregap_length == 2 * 75);
        TEST(track->data_start == ((2 * 60) + 47) * 75 + 20);
    }

    COMMENT("Test TRACK 03 (audio with index 0)");
    track = parser.next_track();
    TEST(track != NULL);
    if (track)
    {
        TEST(strcmp(track->filename, "Image Name.bin") == 0);
        TEST(track->file_mode == CUEFile_BINARY);
        TEST(track->track_number == 3);
        TEST(track->track_mode == CUETrack_AUDIO);
        TEST(track->pregap_start == ((7 * 60) + 55) * 75 + 58);
        TEST(track->data_start == ((7 * 60) + 55) * 75 + 65);
    }

    COMMENT("Test TRACK 11 (audio from wav)");
    track = parser.next_track();
    TEST(track != NULL);
    if (track)
    {
        TEST(strcmp(track->filename, "Sound.wav") == 0);
        TEST(track->file_mode == CUEFile_WAVE);
        TEST(track->track_number == 11);
        TEST(track->track_mode == CUETrack_AUDIO);
        TEST(track->pregap_start == 0);
        TEST(track->data_start == 2 * 75);
    }

    COMMENT("Test end of file");
    track = parser.next_track();
    TEST(track == NULL);

    COMMENT("Test restart");
    parser.restart();
    track = parser.next_track();
    TEST(track != NULL && track->track_number == 1);

    return status;
}


int main()
{
    return test_basics();
}
