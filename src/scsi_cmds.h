#ifndef __SCSI_CMDS_H__
#define __SCSI_CMDS_H__

// defines for SCSI commands
#define SCSI_TEST_UNIT_READY        0
#define SCSI_REZERO_UNIT            0x1
#define SCSI_REQUEST_SENSE          0x3
#define SCSI_FORMAT_UNIT4           0x4
#define SCSI_FORMAT_UNIT6           0x6
#define SCSI_REASSIGN_BLOCKS        0x7
#define SCSI_READ6                  0x8
#define SCSI_WRITE6                 0xA
#define SCSI_SEEK6                  0xB
#define SCSI_INQUIRY                0x12
#define SCSI_MODE_SELECT6           0x15
#define SCSI_RESERVE                0x16
#define SCSI_RELEASE                0x17
#define SCSI_COPY                   0x18
#define SCSI_MODE_SENSE6            0x1A
#define SCSI_START_STOP_UNIT        0x1B
#define SCSI_RECV_DIAG_RESULTS      0x1C
#define SCSI_SEND_DIAG              0x1D
#define SCSI_PREVENT_ALLOW_REMOVAL  0x1E
#define SCSI_READ_CAPACITY          0x25
#define SCSI_READ10                 0x28
#define SCSI_WRITE10                0x2A
#define SCSI_SEEK10                 0x2B
#define SCSI_WRITE_AND_VERIFY       0x2E
#define SCSI_VERIFY10               0x2F
#define SCSI_SEARCH_DATA_HIGH       0x30
#define SCSI_SEARCH_DATA_EQUAL      0x31
#define SCSI_SEARCH_DATA_LOW        0x32
#define SCSI_SET_LIMITS             0x33
#define SCSI_PREFETCH               0x34
#define SCSI_SYNCHRONIZE_CACHE      0x35
#define SCSI_LOCK_UNLOCK_CACHE      0x36
#define SCSI_READ_DEFECT_DATA       0x37
#define SCSI_COMPARE                0x39
#define SCSI_COPY_AND_VERIFY        0x3A
#define SCSI_WRITE_BUFFER           0x3B
#define SCSI_READ_BUFFER            0x3C
#define SCSI_READ_LONG              0x3E
#define SCSI_WRITE_LONG             0x3F
#define SCSI_CHANGE_DEFINITION      0x40
#define SCSI_WRITE_SAME             0x41
#define SCSI_LOG_SELECT             0x4C
#define SCSI_LOG_SENSE              0x4D
#define SCSI_MODE_SELECT10          0x55
#define SCSI_MODE_SENSE10           0x5A
#define SCSI_READ12                 0xA8
#define SCSI_VERIFY12               0xAF


#define SCSI_TOC_LENGTH 20 // length for default CDROM TOC

// SCSI CDROM commands
#define SCSI_AUDIO_SCAN1            0xBA
#define SCSI_AUDIO_SCAN2            0xCD
#define SCSI_PAUSE_RESUME           0x4B
#define SCSI_PLAY_AUDIO10           0x45
#define SCSI_PLAY_AUDIO12           0xA5
#define SCSI_PLAY_AUDIO_MSF         0x47
#define SCSI_PLAY_AUDIO_TRACK_IDX   0x48
#define SCSI_PLAY_TRACK_RELATIVE10  0x49
#define SCSI_PLAY_TRACK_RELATIVE12  0xA9
#define SCSI_READ_CD                0xBE
#define SCSI_READ_CD_DD             0xD8
#define SCSI_READ_CD_MSF            0xB9
#define SCSI_READ_CDDA_MSF          0xD9
#define SCSI_READ_CDXA              0xDB
#define SCSI_READ_ALL_SUBCODE       0xDF
#define SCSI_READ_HEADER            0x44
#define SCSI_READ_SUBCHANNEL        0x42
#define SCSI_READ_TOC               0x43
#define SCSI_READ_DISC_INFORMATION  0x51
#define SCSI_READ_DVD_STRUCTURE     0xAD
#define SCSI_SET_CDROM_SPEED1       0xBB
#define SCSI_SET_CDROM_SPEED2       0xDA
#define SCSI_STOP_PLAY_SCAN         0x4E
#define SCSI_READ_CDP               0xE4
#define SCSI_READ_DRIVE_STATUS      0xE0
#define SCSI_WRITE_CDP              0xE3

#endif // __SCSI_CMDS_H__