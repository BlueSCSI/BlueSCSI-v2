// Compile-time configuration parameters.
// Other settings can be set by ini file at runtime.

#pragma once

// Configuration and log file paths
#define CONFIGFILE  "azulscsi.ini"
#define LOGFILE     "azullog.txt"
#define CRASHFILE   "azulerr.txt"

// Log buffer size in bytes, must be a power of 2
#define LOGBUFSIZE 16384

// HDD image file format
#define HDIMG_ID_POS  2                 // Position to embed ID number
#define HDIMG_LUN_POS 3                 // Position to embed LUN numbers
#define HDIMG_BLK_POS 5                 // Position to embed block size numbers
#define MAX_FILE_PATH 32                // Maximum file name length
#define MAX_BLOCKSIZE 1024              // Maximum BLOCK size

// SCSI config
#define NUM_SCSIID  7          // Maximum number of supported SCSI-IDs (The minimum is 0)
#define NUM_SCSILUN 2          // Maximum number of LUNs supported     (The minimum is 0)
#define READ_PARITY_CHECK 0    // Perform read parity check (unverified)

// Default SCSI drive information (can be overridden in INI file)
#define DEFAULT_VENDOR "QUANTUM "
#define DEFAULT_PRODUCT "FIREBALL1       "
#define DEFAULT_VERSION "1.0 "
