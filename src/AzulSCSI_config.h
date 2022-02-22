// Compile-time configuration parameters.
// Other settings can be set by ini file at runtime.

#pragma once

// Configuration and log file paths
#define CONFIGFILE  "azulscsi.ini"
#define LOGFILE     "azullog.txt"
#define CRASHFILE   "azulerr.txt"

// Log buffer size in bytes, must be a power of 2
#define LOGBUFSIZE 16384
#define LOG_SAVE_INTERVAL_MS 1000

// Watchdog timeout
// Watchdog will first issue a bus reset and if that does not help, crashdump.
#define WATCHDOG_BUS_RESET_TIMEOUT 15000
#define WATCHDOG_CRASH_TIMEOUT 30000

// HDD image file format
#define HDIMG_ID_POS  2                 // Position to embed ID number
#define HDIMG_LUN_POS 3                 // Position to embed LUN numbers
#define HDIMG_BLK_POS 5                 // Position to embed block size numbers
#define MAX_FILE_PATH 64                // Maximum file name length
#define MAX_BLOCKSIZE 1024              // Maximum BLOCK size

// Read buffer size
// Should be at least MAX_BLOCKSIZE.
#define READBUFFER_SIZE 4096

// SCSI config
#define NUM_SCSIID  7          // Maximum number of supported SCSI-IDs (The minimum is 0)
#define NUM_SCSILUN 1          // Maximum number of LUNs supported     (Currently has to be 1)
#define READ_PARITY_CHECK 0    // Perform read parity check (unverified)

// Default SCSI drive information (can be overridden in INI file)
#define DEFAULT_VENDOR "QUANTUM "
#define DEFAULT_PRODUCT "FIREBALL1       "
#define DEFAULT_VERSION "1.0 "
#define DEFAULT_SERIAL  "0123456789ABCDEF"

// Default delay for SCSI phases.
// Can be adjusted in ini file
#define DEFAULT_SCSI_DELAY_US 10
#define DEFAULT_REQ_TYPE_SETUP_NS 500

// Use streaming SCSI and SD transfers for higher performance
#define STREAM_SD_TRANSFERS 1

// Uncomment for building on revision 2022a prototype board
/* #define AZULSCSI_2022A_REVISION 1 */
