/**
 * BlueSCSI - Copyright (c) 2026 Eric Helgeson
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * ----
 *
 * Panel Protocol Handler Implementation
 *
 * Translates panel protocol commands into BlueSCSI operations.
 */

#include "panel_protocol.h"
#include "panel_protocol_defs.h"
#include "panel_protocol_defs_initiator.h"
#include "panel_transport.h"
#include <BlueSCSI_platform.h>
#include <BlueSCSI_log.h>
#include <BlueSCSI_config.h>

#if defined(ENABLE_PANEL_SPI) || defined(ENABLE_PANEL_I2C)

#include <string.h>
#include <stdio.h>
#include <SdFat.h>
#include "panel_sha256.h"   // pico_sha256 on RP2350, software SHA-256 on RP2040 (v2)
#include <hardware/sync.h>

// Include BlueSCSI headers for image access
#include "BlueSCSI_disk.h"
#include "BlueSCSI_cdrom.h"
#include "BlueSCSI_initiator.h"
#include <scsi.h>

// External SD card
extern SdFs SD;

// External disk images array
extern image_config_t g_DiskImages[S2S_MAX_TARGETS];

// True when the SCSI target bus is active OR a host selection is latched but
// not yet serviced. Panel write commands run their (potentially multi-ms) SD
// I/O in the main loop and block scsiPoll() while doing so, so the transports
// defer them until the bus is genuinely idle — not just during DATA phases.
// (DATA_IN/DATA_OUT are a subset of phase != BUS_FREE.)
bool panel_scsi_bus_busy(void) {
    return scsiDev.phase != BUS_FREE || scsiDev.selFlag;
}

// Panel firmware path on SD card
static const char* PANEL_FW_PATH = "/firmware/frontpanel.bin";

// Maximum cached directory entries. Sized to fit the Ultra (RP2350) RAM
// budget: each entry is ~65 bytes, so this cache dominates panel RAM. 128
// entries keeps the static cost ~8.3KB while still listing large directories;
// directories with more image entries are truncated (logged in scan()).
#define MAX_DIR_ENTRIES 128

// ESP32 firmware version location in binary (esp_app_desc_t.version at offset 0x30)
#define ESP32_VERSION_OFFSET 0x30
#define ESP32_VERSION_MAX_LEN 32

// Case-insensitive extension match (hasExtension() in BlueSCSI_disk.cpp is static).
static bool panel_has_extension(const char* name, const char* ext) {
    const char* dot = strrchr(name, '.');
    return dot && strcasecmp(dot, ext) == 0;
}

// Reject a path with any ".." component. The front panel is a trusted local
// device, but a ".." lets a panel-supplied filename escape its intended
// directory, so we refuse it defensively on every path the panel provides.
static bool panel_path_has_traversal(const char* name) {
    if (!name) return true;
    for (const char* p = name; *p; p++) {
        if (p[0] == '.' && p[1] == '.' &&
            (p[2] == '\0' || p[2] == '/' || p[2] == '\\') &&
            (p == name || p[-1] == '/' || p[-1] == '\\')) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// Device lookup helpers for panel commands
// ============================================================================

// Get device by index (SCSI ID for BlueSCSI)
static image_config_t* get_device_by_index(uint16_t index) {
    if (index >= S2S_MAX_TARGETS) return nullptr;
    image_config_t& img = g_DiskImages[index];
    if (img.scsiId & S2S_CFG_TARGET_ENABLED) {
        return &img;
    }
    return nullptr;
}

// Check if device is ejectable (optical or removable)
static bool device_is_ejectable(image_config_t* img) {
    return img && (img->deviceType == S2S_CFG_OPTICAL ||
                   img->deviceType == S2S_CFG_REMOVABLE);
}

// Snapshot of per-device status, refreshed from the MAIN LOOP and read by the
// IRQ-context read handlers (GET_DEVICE_STATUS / GET_PLAYBACK_STATUS).
//
// The IRQ must NOT touch img->file directly: switchNextImage() reassigns the
// whole ImageBackingStore (img.file = ImageBackingStore(...)) in the main loop,
// and an IRQ read of img->file.isOpen() mid-reassignment is a data race (torn
// read of the struct, with a small chance of faulting on m_fsfile internals).
// The snapshot is plain bytes the IRQ can read safely.
struct panel_device_snapshot_t {
    uint8_t present;      // device configured/enabled
    uint8_t loaded;       // image file open
    uint8_t ejected;      // optical tray open (img.ejected)
    uint8_t device_type;  // S2S_CFG_*
    char image_name[64];  // cached filename; getFilename() is unsafe in the IRQ
};
static volatile panel_device_snapshot_t g_device_snapshot[S2S_MAX_TARGETS];

// getName() reconstructs the long filename and is too costly to run for every
// target on every poll, so the cached names are refreshed on this interval
// (present/loaded/device_type stay exact every call). A device that newly
// loads is refreshed immediately regardless, so inserts show without lag.
#define PANEL_SNAPSHOT_NAME_REFRESH_MS 250
static uint32_t g_snapshot_name_refresh_ms = 0;

// Refresh the device snapshot. MUST be called only from the main loop — it
// reads img->file.isOpen()/getFilename(), which race switchNextImage() if
// called from the IRQ. The main loop is single-threaded w.r.t. switchNextImage,
// so the reads here are consistent.
void panel_protocol_refresh_device_snapshot(void) {
    uint32_t now = millis();
    bool refresh_names = (now - g_snapshot_name_refresh_ms) >= PANEL_SNAPSHOT_NAME_REFRESH_MS;

    for (int i = 0; i < S2S_MAX_TARGETS; i++) {
        image_config_t& img = g_DiskImages[i];
        bool present = (img.scsiId & S2S_CFG_TARGET_ENABLED) != 0;
        bool loaded = present && img.file.isOpen();

        // Cache the filename so the IRQ never calls getFilename(). Re-read on
        // the interval, and immediately whenever a device transitions to loaded.
        if (loaded && (refresh_names || !g_device_snapshot[i].loaded)) {
            char name[sizeof(g_device_snapshot[i].image_name)];
            size_t n = img.file.getFilename(name, sizeof(name));
            name[n < sizeof(name) ? n : sizeof(name) - 1] = '\0';
            memcpy((void *)g_device_snapshot[i].image_name, name, sizeof(name));
        } else if (!loaded) {
            g_device_snapshot[i].image_name[0] = '\0';
        }

        // Write device_type/name/present/ejected before loaded so an IRQ that
        // observes loaded == 1 also sees consistent fields.
        g_device_snapshot[i].device_type = img.deviceType;
        g_device_snapshot[i].ejected = (present && img.ejected) ? 1 : 0;
        g_device_snapshot[i].present = present ? 1 : 0;
        g_device_snapshot[i].loaded = loaded ? 1 : 0;
    }

    if (refresh_names) {
        g_snapshot_name_refresh_ms = now;
    }
}

// Directory browsing state
static struct DirState {
    char current_path[128] = "/";
    uint32_t entry_count = 0;
    bool scanned = false;

    // Cached entry info for current directory
    struct CachedEntry {
        char name[64];
        uint8_t entry_type;
    } entries[MAX_DIR_ENTRIES];

    void reset() {
        strcpy(current_path, "/");
        entry_count = 0;
        scanned = false;
    }

    // Scan current directory and populate entry cache
    bool scan() {
        FsFile dir;
        FsFile entry;

        entry_count = 0;
        scanned = false;

        if (!dir.open(current_path)) {
            logmsg("Panel: Failed to open dir ", current_path);
            return false;
        }

        if (!dir.isDir()) {
            dir.close();
            return false;
        }

        // Add parent directory entry if not at root
        if (strcmp(current_path, "/") != 0) {
            strncpy(entries[entry_count].name, "..", sizeof(entries[0].name) - 1);
            entries[entry_count].entry_type = PANEL_ENTRY_TYPE_DIRECTORY;
            entry_count++;
        }

        // Scan directory entries
        while (entry.openNext(&dir, O_RDONLY) && entry_count < MAX_DIR_ENTRIES) {
            char name[64];
            entry.getName(name, sizeof(name));

            // Skip hidden files (starting with .)
            if (name[0] == '.') {
                entry.close();
                continue;
            }

            if (entry.isDir()) {
                // Add directory
                strncpy(entries[entry_count].name, name, sizeof(entries[0].name) - 1);
                entries[entry_count].name[sizeof(entries[0].name) - 1] = '\0';
                entries[entry_count].entry_type = PANEL_ENTRY_TYPE_DIRECTORY;
                entry_count++;
            } else if (panel_has_extension(name, ".cue")) {
                // Hide .cue sheets: the panel loads a CD by its .bin, which
                // auto-pairs the sidecar .cue on load. Loading the .cue directly
                // flips the device into is_multi_bin_cue() mode, which wedges the
                // eject/next iterator on the same disc (see findNextImageAfter()).
                entry.close();
                continue;
            } else {
                // Add every other (non-hidden) file. The panel browser lists all
                // files and lets the firmware accept or reject them on load,
                // rather than second-guessing which extensions are mountable.
                strncpy(entries[entry_count].name, name, sizeof(entries[0].name) - 1);
                entries[entry_count].name[sizeof(entries[0].name) - 1] = '\0';
                entries[entry_count].entry_type = PANEL_ENTRY_TYPE_FILE;
                entry_count++;
            }

            entry.close();
        }

        dir.close();
        scanned = true;
        if (entry_count >= MAX_DIR_ENTRIES) {
            logmsg("Panel: Directory '", current_path, "' listing truncated to ",
                   (int)MAX_DIR_ENTRIES, " entries");
        }
        logmsg("Panel: Scanned ", current_path, ", found ", (int)entry_count, " entries");
        return true;
    }

    // Change to a subdirectory or parent
    bool change_dir(const char* name) {
        char new_path[128];

        if (strcmp(name, "..") == 0) {
            // Go to parent
            if (strcmp(current_path, "/") == 0) {
                return true;  // Already at root
            }

            // Find last slash and truncate
            strncpy(new_path, current_path, sizeof(new_path) - 1);
            new_path[sizeof(new_path) - 1] = '\0';
            char* last_slash = strrchr(new_path, '/');
            if (last_slash && last_slash != new_path) {
                *last_slash = '\0';
            } else {
                strcpy(new_path, "/");
            }
        } else {
            // Go to subdirectory
            if (strcmp(current_path, "/") == 0) {
                snprintf(new_path, sizeof(new_path), "/%s", name);
            } else {
                snprintf(new_path, sizeof(new_path), "%s/%s", current_path, name);
            }
        }

        // Verify new path is a directory
        FsFile dir;
        if (!dir.open(new_path) || !dir.isDir()) {
            if (dir.isOpen()) dir.close();
            return false;
        }
        dir.close();

        strncpy(current_path, new_path, sizeof(current_path) - 1);
        current_path[sizeof(current_path) - 1] = '\0';
        scanned = false;  // Need to rescan
        return true;
    }
} g_dir;

// Async operation state
static struct AsyncState {
    uint8_t current_command = 0;
    panel_async_state_t state = PANEL_ASYNC_IDLE;

    // Firmware read state
    FsFile fw_file;
    uint32_t fw_size = 0;
    uint32_t fw_offset = 0;
    panel_firmware_info_t fw_info = {};

    void reset() {
        current_command = 0;
        state = PANEL_ASYNC_IDLE;
        fw_size = 0;
        fw_offset = 0;
        memset(&fw_info, 0, sizeof(fw_info));
        if (fw_file.isOpen()) {
            fw_file.close();
        }
        g_dir.reset();
    }
} g_async;

// File upload state
static struct FileUploadState {
    FsFile upload_file;
    char filename[64];
    uint32_t total_size;
    uint32_t bytes_written;
    bool upload_active;
    uint16_t last_chunk_crc16;
    sha256_result_t calculated_hash;
    pico_sha256_state_t* sha256_ctx;

    void reset() {
        if (upload_file.isOpen()) {
            platform_reset_watchdog();
            upload_file.close();
        }
        memset(filename, 0, sizeof(filename));
        total_size = 0;
        bytes_written = 0;
        upload_active = false;
        last_chunk_crc16 = 0;
        memset(&calculated_hash, 0, sizeof(calculated_hash));
        if (sha256_ctx) {
            pico_sha256_cleanup(sha256_ctx);
            delete sha256_ctx;
            sha256_ctx = nullptr;
        }
    }
} g_upload;

// File download state
static struct FileDownloadState {
    FsFile download_file;
    uint32_t file_size;
    bool download_active;

    void reset() {
        if (download_file.isOpen()) {
            download_file.close();
        }
        file_size = 0;
        download_active = false;
    }
} g_download;

// ============================================================================
// Helper functions
// ============================================================================

// Parse version string "YYYY.MM.DD" into 0x00YYMMdd format
static uint32_t parse_version_string(const char* ver) {
    uint32_t year = 0, month = 0, day = 0;

    const char* p = ver;
    while (*p >= '0' && *p <= '9') {
        year = year * 10 + (*p - '0');
        p++;
    }
    if (*p == '.') p++;
    while (*p >= '0' && *p <= '9') {
        month = month * 10 + (*p - '0');
        p++;
    }
    if (*p == '.') p++;
    while (*p >= '0' && *p <= '9') {
        day = day * 10 + (*p - '0');
        p++;
    }

    uint32_t major = (year >= 2000) ? (year - 2000) : year;
    return (major << 16) | (month << 8) | day;
}

// Get info about first configured image
static bool get_first_image_info(loaded_image_status_t* status) {
    memset(status, 0, sizeof(*status));

    // Scan through SCSI IDs to find first configured image
    for (int id = 0; id < 8; id++) {
        image_config_t& img = scsiDiskGetImageConfig(id);

        // Check if this target has an open image file
        if (img.file.isOpen()) {
            status->image_loaded = 1;
            status->device_type = PANEL_DEVICE_TYPE_SCSI;

            // Get filename
            img.file.getFilename(status->image_name, sizeof(status->image_name));

            // Get directory path
            if (img.current_image[0] != '\0') {
                strncpy(status->directory_path, "/", sizeof(status->directory_path) - 1);
            } else {
                strncpy(status->directory_path, "/", sizeof(status->directory_path) - 1);
            }

            return true;
        }
    }

    // Fallback: return basic info without detailed filename
    status->image_loaded = scsiDiskCheckAnyImagesConfigured() ? 1 : 0;
    status->device_type = PANEL_DEVICE_TYPE_SCSI;

    if (status->image_loaded) {
        strncpy(status->image_name, "[SCSI Image]", sizeof(status->image_name) - 1);
        strncpy(status->directory_path, "/", sizeof(status->directory_path) - 1);
    }

    return status->image_loaded;
}

// ============================================================================
// Image-list iteration helpers
//
// scsiDiskGetNextImageName() is a CYCLIC iterator: for INI images (IMG0..N) it
// walks img.image_index and wraps; for an image_directory it advances
// img.current_image to the next entry (via findNextImageAfter) and wraps. It
// only returns 0 when the target has no images at all. Driving it with a plain
// `while (scsiDiskGetNextImageName(...))` therefore spins forever once any
// image exists, blocking the main loop until the watchdog resets the board.
// These helpers walk exactly one full cycle (detected when the first result
// repeats) with a hard safety cap, and restore the iterator state they perturb.
// ============================================================================

// Basename portion of a path (after the last '/').
static const char* panel_path_basename(const char* p) {
    const char* slash = strrchr(p, '/');
    return slash ? slash + 1 : p;
}

// Count the images available for this target, bounded to a single cycle.
static uint16_t panel_count_target_images(image_config_t &img) {
    char saved_current_image[sizeof(img.current_image)];
    strncpy(saved_current_image, img.current_image, sizeof(saved_current_image));
    saved_current_image[sizeof(saved_current_image) - 1] = '\0';
    int saved_index = img.image_index;

    img.image_index = 0;

    char filename[MAX_FILE_PATH];
    char first_filename[MAX_FILE_PATH] = {0};
    uint16_t count = 0;

    for (int guard = 0; guard <= MAX_DIR_ENTRIES; guard++) {
        if (!scsiDiskGetNextImageName(img, filename, sizeof(filename), false)) {
            break;  // no images for this target
        }
        if (first_filename[0] == '\0') {
            strncpy(first_filename, filename, sizeof(first_filename) - 1);
            first_filename[sizeof(first_filename) - 1] = '\0';
        } else if (strcmp(filename, first_filename) == 0) {
            break;  // completed one full cycle
        }
        count++;
    }

    img.image_index = saved_index;
    strncpy(img.current_image, saved_current_image, sizeof(img.current_image));
    img.current_image[sizeof(img.current_image) - 1] = '\0';
    return count;
}

// Find the image immediately before img.current_image in its ring, writing the
// selectable name into out. Returns false if the target has no images. Walks
// one full cycle and restores the iterator state it perturbs.
static bool panel_find_prev_image(image_config_t &img, char *out, size_t outlen) {
    char saved_current_image[sizeof(img.current_image)];
    strncpy(saved_current_image, img.current_image, sizeof(saved_current_image));
    saved_current_image[sizeof(saved_current_image) - 1] = '\0';
    int saved_index = img.image_index;

    const char *orig_base = panel_path_basename(saved_current_image);

    char filename[MAX_FILE_PATH];
    char first_filename[MAX_FILE_PATH] = {0};
    char before[MAX_FILE_PATH] = {0};       // entry returned on the previous step
    char last_filename[MAX_FILE_PATH] = {0};
    char prev_filename[MAX_FILE_PATH] = {0};
    bool found = false;

    for (int guard = 0; guard <= MAX_DIR_ENTRIES; guard++) {
        if (!scsiDiskGetNextImageName(img, filename, sizeof(filename), false)) {
            break;  // no images
        }
        if (first_filename[0] == '\0') {
            strncpy(first_filename, filename, sizeof(first_filename) - 1);
            first_filename[sizeof(first_filename) - 1] = '\0';
        } else if (strcmp(filename, first_filename) == 0) {
            break;  // completed one full cycle
        }
        // When the walk returns the current image, its predecessor is `before`.
        if (!found && before[0] != '\0' &&
            strcasecmp(panel_path_basename(filename), orig_base) == 0) {
            strncpy(prev_filename, before, sizeof(prev_filename) - 1);
            prev_filename[sizeof(prev_filename) - 1] = '\0';
            found = true;
        }
        strncpy(before, filename, sizeof(before) - 1);
        before[sizeof(before) - 1] = '\0';
        strncpy(last_filename, filename, sizeof(last_filename) - 1);
        last_filename[sizeof(last_filename) - 1] = '\0';
    }

    img.image_index = saved_index;
    strncpy(img.current_image, saved_current_image, sizeof(img.current_image));
    img.current_image[sizeof(img.current_image) - 1] = '\0';

    if (!found) {
        // Current image not encountered (e.g. single image): wrap to the last.
        if (last_filename[0] == '\0') {
            return false;  // empty directory
        }
        strncpy(prev_filename, last_filename, sizeof(prev_filename) - 1);
        prev_filename[sizeof(prev_filename) - 1] = '\0';
    }

    strncpy(out, prev_filename, outlen - 1);
    out[outlen - 1] = '\0';
    return true;
}

// ============================================================================
// Read command handlers (synchronous)
// ============================================================================

static size_t handle_poll_status(uint8_t* response) {
    response[0] = PANEL_STATUS_OK;
    return 1;
}

static size_t handle_get_device_status(uint16_t device_index, uint8_t* response) {
    // IRQ context: read the main-loop-maintained snapshot, never img->file.
    if (device_index < S2S_MAX_TARGETS && g_device_snapshot[device_index].present) {
        response[0] = g_device_snapshot[device_index].loaded
                      ? PANEL_DEVICE_STATUS_LOADED : PANEL_DEVICE_STATUS_NO_IMAGE;
        return 1;
    }

    // Backward-compat fallback for an unconfigured device 0: LOADED if any
    // device has an open image. Derived from the snapshot (snapshot.loaded ==
    // enabled && file.isOpen()), matching scsiDiskCheckAnyImagesConfigured()
    // without touching img->file in the IRQ.
    bool any_loaded = false;
    if (device_index == 0) {
        for (int i = 0; i < S2S_MAX_TARGETS; i++) {
            if (g_device_snapshot[i].loaded) { any_loaded = true; break; }
        }
    }
    response[0] = any_loaded ? PANEL_DEVICE_STATUS_LOADED : PANEL_DEVICE_STATUS_NO_IMAGE;
    return 1;
}

static size_t handle_get_firmware_info(uint8_t* response, size_t max_size) {
    if (max_size < sizeof(panel_firmware_info_t)) {
        return 0;
    }

    memcpy(response, &g_async.fw_info, sizeof(panel_firmware_info_t));
    return sizeof(panel_firmware_info_t);
}

static size_t handle_get_command_status(uint8_t* response, size_t max_size) {
    if (max_size < sizeof(panel_command_status_t)) {
        return 0;
    }

    panel_command_status_t* status = (panel_command_status_t*)response;
    status->command = g_async.current_command;
    status->state = g_async.state;
    status->progress = 0;
    status->last_result = 0;

    return sizeof(panel_command_status_t);
}

static size_t handle_get_playback_status(uint16_t device_index, uint8_t* response, size_t max_size) {
    if (max_size < sizeof(panel_playback_status_t)) {
        return 0;
    }

    panel_playback_status_t* status = (panel_playback_status_t*)response;
    memset(status, 0, sizeof(panel_playback_status_t));
    status->audio_status = PANEL_AUDIO_STATUS_NONE;

    // Stamp the liveness magic so the ESP32 can tell a live board apart from a
    // dead/disconnected bus (which reads back uniformly 0x00 or 0xFF).
    status->alive_magic = PANEL_ALIVE_MAGIC;

    // IRQ context: read the main-loop snapshot, never img->file (see
    // panel_protocol_refresh_device_snapshot). The snapshot also caches the
    // filename so we can return it here without calling getFilename() in IRQ.
    if (device_index < S2S_MAX_TARGETS && g_device_snapshot[device_index].present) {
        // Optical tray open (disc ejected): the next disc is already loaded but
        // presented as ejected until the tray is closed. Flag it so the panel can
        // tell the user to load a disc or close the tray.
        status->tray_open = (g_device_snapshot[device_index].device_type == S2S_CFG_OPTICAL &&
                             g_device_snapshot[device_index].ejected) ? 1 : 0;
    }

    if (device_index < S2S_MAX_TARGETS &&
        g_device_snapshot[device_index].present &&
        g_device_snapshot[device_index].loaded) {
        status->disc_inserted = 1;
        status->disc_type = (g_device_snapshot[device_index].device_type == S2S_CFG_OPTICAL)
                            ? PANEL_DISC_TYPE_DATA : PANEL_DISC_TYPE_HDD;
        status->device_status = g_device_snapshot[device_index].ejected
                                ? PANEL_DEVICE_STATUS_TRAY_OPEN : PANEL_DEVICE_STATUS_LOADED;
        memcpy(status->disc_name, (const void *)g_device_snapshot[device_index].image_name,
               sizeof(status->disc_name));
        status->disc_name[sizeof(status->disc_name) - 1] = '\0';
    } else {
        status->device_status = PANEL_DEVICE_STATUS_NO_IMAGE;
    }

    return sizeof(panel_playback_status_t);
}

// ============================================================================
// Write/Async command handlers
// ============================================================================

static void handle_get_loaded_image_status_async(uint16_t device_index) {
    uint8_t* buf = panel_transport_get_tx_buffer();
    loaded_image_status_t* status = (loaded_image_status_t*)buf;
    memset(status, 0, sizeof(loaded_image_status_t));

    image_config_t* img = get_device_by_index(device_index);
    if (img && img->file.isOpen()) {
        status->image_loaded = 1;
        status->device_type = PANEL_DEVICE_TYPE_SCSI;
        img->file.getFilename(status->image_name, sizeof(status->image_name));
        strncpy(status->directory_path, "/", sizeof(status->directory_path) - 1);

        status->image_index = (img->image_index >= 0) ? img->image_index : 0;

        if (!img->image_directory && !img->use_prefix) {
            // INI-based images (IMG0..IMG9): count via the bounded cyclic walk.
            uint16_t count = panel_count_target_images(*img);
            status->total_images = (count > 0) ? count : 1;
        } else {
            // image_directory / use_prefix: counting requires SD directory
            // scan (findNextImageAfter) which is not safe here
            status->total_images = status->image_index + 1;
        }
    } else if (device_index == 0) {
        // Backward compat: fallback to first-image scan for device 0
        get_first_image_info(status);
    } else {
        status->image_loaded = 0;
        status->device_type = PANEL_DEVICE_TYPE_SCSI;
    }

    panel_transport_set_async_result(buf, sizeof(loaded_image_status_t));
}

static void handle_get_device_list_async(void) {
    uint8_t* buf = panel_transport_get_tx_buffer();
    device_list_response_t* list = (device_list_response_t*)buf;

    memset(list, 0, sizeof(device_list_response_t));
    list->max_devices = S2S_MAX_TARGETS;

    // Set operating mode
    if (scsiInitiatorIsActive()) {
        PANEL_DEVLIST_MODE(list) = PANEL_MODE_INITIATOR;
        list->device_count = 0;
        dbgmsg("Panel: Device list: initiator mode active");
        panel_transport_set_async_result(buf, sizeof(device_list_response_t));
        return;
    }
    PANEL_DEVLIST_MODE(list) = PANEL_MODE_TARGET;

    size_t offset = sizeof(device_list_response_t);
    uint8_t count = 0;

    for (int i = 0; i < S2S_MAX_TARGETS; i++) {
        image_config_t& img = g_DiskImages[i];
        if (!(img.scsiId & S2S_CFG_TARGET_ENABLED)) {
            continue;
        }

        device_summary_t* dev = (device_summary_t*)(buf + offset);
        memset(dev, 0, sizeof(device_summary_t));

        dev->device_index = i;
        dev->device_type = img.deviceType;

        if (img.file.isOpen()) {
            // An ejected optical drive still has the next disc open; report the
            // tray-open state so the panel/web can prompt to load or close.
            dev->device_status = (img.deviceType == S2S_CFG_OPTICAL && img.ejected)
                                 ? PANEL_DEVICE_STATUS_TRAY_OPEN : PANEL_DEVICE_STATUS_LOADED;
            img.file.getFilename(dev->image_name, sizeof(dev->image_name));
        } else {
            dev->device_status = PANEL_DEVICE_STATUS_NO_IMAGE;
        }

        // Build device label from SCSI device type
        const char* type_str = "HD";
        switch (img.deviceType) {
            case S2S_CFG_OPTICAL:      type_str = "CD";   break;
            case S2S_CFG_REMOVABLE:    type_str = "REM";  break;
            case S2S_CFG_SEQUENTIAL:   type_str = "TAPE"; break;
            case S2S_CFG_MO:           type_str = "MO";   break;
            case S2S_CFG_FLOPPY_14MB:  type_str = "FD";   break;
            case S2S_CFG_NETWORK:      type_str = "NET";  break;
            case S2S_CFG_ZIP100:       type_str = "ZIP";  break;
            case S2S_CFG_AMIGAWIFI:    type_str = "WiFi"; break;
            case S2S_CFG_PRINTER:      type_str = "Printer"; break;
            default:                   type_str = "HD";   break;
        }
        snprintf(dev->device_label, sizeof(dev->device_label), "SCSI %d (%s)", i, type_str);

        offset += sizeof(device_summary_t);
        count++;
    }

    list->device_count = count;
    dbgmsg("Panel: Device list: ", (int)count, " devices");
    panel_transport_set_async_result(buf, offset);
}

static void handle_get_initiator_status_async(void) {
    uint8_t* buf = panel_transport_get_tx_buffer();
    initiator_status_response_t* resp = (initiator_status_response_t*)buf;

    memset(resp, 0, sizeof(initiator_status_response_t));

    uint8_t phase, current_target, initiator_id, drives_mask;
    scsiInitiatorGetStatus(&phase, &current_target, &initiator_id, &drives_mask);

    resp->phase = phase;
    resp->current_target_id = current_target;
    resp->initiator_id = initiator_id;
    resp->drives_imaged_mask = drives_mask;

    // Populate per-target info
    size_t offset = sizeof(initiator_status_response_t);
    uint8_t targets_found = 0;
    uint8_t targets_imaged = 0;

    for (int id = 0; id < 8; id++) {
        if (id == initiator_id) continue;

        initiator_target_info_t* ti = (initiator_target_info_t*)(buf + offset);
        memset(ti, 0, sizeof(initiator_target_info_t));

        ti->scsi_id = id;
        if (!scsiInitiatorGetTargetInfo(id, &ti->status, &ti->device_type, &ti->ansi_version,
                                         &ti->sectorcount, &ti->sectorsize, &ti->sectors_done,
                                         &ti->bad_sector_count, ti->vendor, ti->product,
                                         &ti->sense_key, &ti->asc, &ti->ascq)) {
            continue;
        }

        targets_found++;
        if (ti->status == 3) targets_imaged++; // PANEL_INITIATOR_TARGET_DONE

        offset += sizeof(initiator_target_info_t);
    }

    resp->targets_found = targets_found;
    resp->targets_imaged = targets_imaged;

    dbgmsg("Panel: Initiator status: phase=", (int)phase, " targets=", (int)targets_found);
    panel_transport_set_async_result(buf, offset);
}

static void handle_get_dir_entry_count_async(void) {
    uint8_t* buf = panel_transport_get_tx_buffer();

    // Scan directory if not already done
    if (!g_dir.scanned) {
        g_dir.scan();
    }

    // Send count in big-endian format (ESP32 expects big-endian)
    uint32_t count = g_dir.entry_count;
    buf[0] = (count >> 24) & 0xFF;
    buf[1] = (count >> 16) & 0xFF;
    buf[2] = (count >> 8) & 0xFF;
    buf[3] = count & 0xFF;
    panel_transport_set_async_result(buf, sizeof(uint32_t));
}

static void handle_get_entry_info_async(uint16_t index) {
    uint8_t* buf = panel_transport_get_tx_buffer();
    dir_entry_info_t* info = (dir_entry_info_t*)buf;
    memset(info, 0, sizeof(dir_entry_info_t));

    // Scan directory if not already done
    if (!g_dir.scanned) {
        g_dir.scan();
    }

    if (index < g_dir.entry_count) {
        strncpy(info->name, g_dir.entries[index].name, sizeof(info->name) - 1);
        info->entry_type = g_dir.entries[index].entry_type;
    } else {
        strncpy(info->name, "(invalid index)", sizeof(info->name) - 1);
        info->entry_type = PANEL_ENTRY_TYPE_FILE;
    }

    panel_transport_set_async_result(buf, sizeof(dir_entry_info_t));
}

static void handle_get_current_path_async(void) {
    uint8_t* buf = panel_transport_get_tx_buffer();
    size_t len = strlen(g_dir.current_path) + 1;
    strncpy((char*)buf, g_dir.current_path, PANEL_PROTOCOL_MAX_PAYLOAD - 1);
    panel_transport_set_async_result(buf, len);
}

static void handle_select_entry_async(uint16_t device_index, int16_t entry_index) {
    uint8_t* buf = panel_transport_get_tx_buffer();

    // Scan directory if not already done
    if (!g_dir.scanned) {
        g_dir.scan();
    }

    // Special case: -1 means go to parent directory
    if (entry_index == -1) {
        g_dir.change_dir("..");
        buf[0] = 0;  // Success
        panel_transport_set_async_result(buf, 1);
        return;
    }

    if ((uint16_t)entry_index >= g_dir.entry_count) {
        buf[0] = 1;  // Error: invalid index
        panel_transport_set_async_result(buf, 1);
        return;
    }

    DirState::CachedEntry& entry = g_dir.entries[entry_index];

    if (entry.entry_type == PANEL_ENTRY_TYPE_DIRECTORY) {
        // Navigate into directory
        if (g_dir.change_dir(entry.name)) {
            buf[0] = 0;  // Success
        } else {
            buf[0] = 2;  // Error: failed to change directory
        }
        panel_transport_set_async_result(buf, 1);
    } else {
        // Select image file - build full path
        char full_path[192];
        if (strcmp(g_dir.current_path, "/") == 0) {
            snprintf(full_path, sizeof(full_path), "/%s", entry.name);
        } else {
            snprintf(full_path, sizeof(full_path), "%s/%s", g_dir.current_path, entry.name);
        }

        // Get target device by index from argument
        image_config_t* img = get_device_by_index(device_index);
        if (!img) {
            logmsg("Panel: Invalid device index ", (int)device_index);
            buf[0] = 3;  // Error: invalid device index
            panel_transport_set_async_result(buf, 1);
            return;
        }

        logmsg("Panel: Loading image ", full_path, " to SCSI ID ", (int)device_index);

        // If the optical tray was open (disc ejected), the user is loading a disc
        // into the open tray. switchNextImage() leaves it ejected awaiting a host
        // GET EVENT STATUS poll, which a classic Mac may not do after a manual
        // eject - so close the tray ourselves to present the selected disc (same
        // path as the working "close" action, which posts UNIT ATTENTION).
        bool was_ejected = (img->deviceType == S2S_CFG_OPTICAL) && img->ejected;

        if (switchNextImage(*img, full_path)) {
            buf[0] = 0;  // Success
            if (was_ejected) {
                cdromCloseTray(*img);
            }
            logmsg("Panel: Image loaded successfully");
        } else {
            buf[0] = 4;  // Error: failed to load image
            logmsg("Panel: Failed to load image");
        }
        panel_transport_set_async_result(buf, 1);
    }
}

// Cache for the most recently computed firmware SHA256.
// CHECK_FIRMWARE may be called repeatedly (every ESP32 update poll); without
// caching we'd re-stream the entire ~1.5MB firmware off the SD card every
// time and stall the main loop for ~150ms each call.
static struct {
    bool valid;
    uint32_t size;
    uint32_t mtime_fingerprint;  // (FAT date << 16) | FAT time
    uint8_t  hash[32];
} g_fw_sha_cache;

// Build a cheap "has the file changed" key from FAT date+time + size.
static uint32_t firmware_mtime_fingerprint(FsFile& file) {
    uint16_t fdate = 0, ftime = 0;
    file.getModifyDateTime(&fdate, &ftime);
    return ((uint32_t)fdate << 16) | (uint32_t)ftime;
}

// Calculate SHA256 hash of firmware file using hardware acceleration.
// Pumps the watchdog periodically — the file can be large enough that the
// SD-bound read loop runs long against the 15s watchdog window on slow cards.
static bool calculate_firmware_sha256(FsFile& file, uint8_t* hash) {
    if (!file.isOpen() || !hash) {
        return false;
    }

    file.seekSet(0);

    pico_sha256_state_t* ctx = new pico_sha256_state_t;
    // try_start (not start_blocking): the SHA-256 hardware lock is held for the
    // whole duration of a file upload, so a blocking acquire here would wait
    // forever if a CHECK_FIRMWARE landed mid-upload. Fail gracefully instead.
    if (pico_sha256_try_start(ctx, SHA256_BIG_ENDIAN, true /* use_dma */) != PICO_OK) {
        logmsg("Panel: SHA256 hardware busy, skipping firmware hash");
        delete ctx;
        return false;
    }

    uint8_t buffer[512];
    size_t bytesRead;
    uint32_t chunks_since_kick = 0;

    while ((bytesRead = file.read(buffer, sizeof(buffer))) > 0) {
        pico_sha256_update(ctx, buffer, bytesRead);
        // Kick watchdog every ~64KB. Cheap, and keeps slow SD cards inside
        // the 15s watchdog window.
        if (++chunks_since_kick >= 128) {
            platform_reset_watchdog();
            chunks_since_kick = 0;
        }
    }

    sha256_result_t result;
    pico_sha256_finish(ctx, &result);
    pico_sha256_cleanup(ctx);
    delete ctx;

    memcpy(hash, &result, 32);

    file.seekSet(0);
    return true;
}

// Compute (or return cached) SHA256 for the firmware file.
// out_recomputed is set to true when we actually re-read the file, false
// when the cache was a hit. Tests use this to assert cache behavior.
static bool firmware_sha256_with_cache(FsFile& file, uint8_t* hash,
                                       bool* out_recomputed) {
    if (!file.isOpen() || !hash) {
        return false;
    }

    uint32_t size = (uint32_t)file.fileSize();
    uint32_t mtime = firmware_mtime_fingerprint(file);

    if (g_fw_sha_cache.valid &&
        g_fw_sha_cache.size == size &&
        g_fw_sha_cache.mtime_fingerprint == mtime) {
        memcpy(hash, g_fw_sha_cache.hash, 32);
        if (out_recomputed) *out_recomputed = false;
        return true;
    }

    if (!calculate_firmware_sha256(file, hash)) {
        g_fw_sha_cache.valid = false;
        if (out_recomputed) *out_recomputed = true;
        return false;
    }

    memcpy(g_fw_sha_cache.hash, hash, 32);
    g_fw_sha_cache.size = size;
    g_fw_sha_cache.mtime_fingerprint = mtime;
    g_fw_sha_cache.valid = true;
    if (out_recomputed) *out_recomputed = true;
    return true;
}

#ifdef UNIT_TEST
// Test accessors so unit tests can drive cache behavior without going through
// the firmware-update glue.
extern "C" {
    void panel_protocol_test_reset_fw_cache(void) {
        memset(&g_fw_sha_cache, 0, sizeof(g_fw_sha_cache));
    }
    bool panel_protocol_test_firmware_sha256(FsFile& file, uint8_t* hash,
                                             bool* out_recomputed) {
        return firmware_sha256_with_cache(file, hash, out_recomputed);
    }
}
#endif

static void handle_check_firmware_async(void) {
    memset(&g_async.fw_info, 0, sizeof(g_async.fw_info));

    if (g_async.fw_file.isOpen()) {
        g_async.fw_file.close();
    }

    if (g_async.fw_file.open(PANEL_FW_PATH, O_RDONLY)) {
        g_async.fw_info.size = g_async.fw_file.fileSize();
        g_async.fw_info.available = 1;
        g_async.fw_size = g_async.fw_info.size;
        g_async.fw_offset = 0;

        // Parse version string from ESP32 binary header (esp_app_desc_t at offset 0x30)
        g_async.fw_info.version = 0;
        if (g_async.fw_file.seekSet(ESP32_VERSION_OFFSET)) {
            char ver_str[ESP32_VERSION_MAX_LEN];
            // read() returns int (-1 on error); keep it signed so an error
            // isn't reinterpreted as a huge size_t and parsed as a version.
            int n = g_async.fw_file.read(ver_str, ESP32_VERSION_MAX_LEN);
            if (n > 0) {
                ver_str[ESP32_VERSION_MAX_LEN - 1] = '\0';
                const char* p = ver_str;
                if (*p == 'v' || *p == 'V') p++;
                uint32_t maj = 0, min = 0, pat = 0;
                while (*p >= '0' && *p <= '9') { maj = maj * 10 + (*p - '0'); p++; }
                if (*p == '.') p++;
                while (*p >= '0' && *p <= '9') { min = min * 10 + (*p - '0'); p++; }
                if (*p == '.') p++;
                while (*p >= '0' && *p <= '9') { pat = pat * 10 + (*p - '0'); p++; }
                if (maj <= 255 && min <= 255 && pat <= 255) {
                    g_async.fw_info.version = (maj << 16) | (min << 8) | pat;
                }
            }
            g_async.fw_file.seekSet(0);
        }

        bool recomputed = false;
        if (!firmware_sha256_with_cache(g_async.fw_file, g_async.fw_info.sha256, &recomputed)) {
            logmsg("Panel: SHA256 calculation failed");
            memset(g_async.fw_info.sha256, 0, sizeof(g_async.fw_info.sha256));
        } else if (recomputed) {
            logmsg("Panel: Calculated SHA256 for ", PANEL_FW_PATH);
        } else {
            dbgmsg("Panel: SHA256 cache hit for ", PANEL_FW_PATH);
        }

        logmsg("Panel: Found firmware ", PANEL_FW_PATH, ", size=", g_async.fw_info.size);
    } else {
        g_async.fw_info.available = 0;
        logmsg("Panel: No firmware at ", PANEL_FW_PATH);
    }

    uint8_t* buf = panel_transport_get_tx_buffer();
    memcpy(buf, &g_async.fw_info, sizeof(panel_firmware_info_t));
    panel_transport_set_async_result(buf, sizeof(panel_firmware_info_t));
}

static void handle_start_firmware_read_async(uint32_t offset) {
    uint8_t* buf = panel_transport_get_tx_buffer();

    if (!g_async.fw_file.isOpen()) {
        if (!g_async.fw_file.open(PANEL_FW_PATH, O_RDONLY)) {
            panel_transport_set_async_error();
            return;
        }
        g_async.fw_size = g_async.fw_file.fileSize();
    }

    if (!g_async.fw_file.seek(offset)) {
        panel_transport_set_async_error();
        return;
    }

    size_t remaining = g_async.fw_size - offset;
    size_t to_read = (remaining > PANEL_FIRMWARE_CHUNK_SIZE) ?
                     PANEL_FIRMWARE_CHUNK_SIZE : remaining;

    ssize_t bytes_read = g_async.fw_file.read(buf, to_read);
    if (bytes_read < 0) {
        panel_transport_set_async_error();
        return;
    }

    panel_transport_set_async_result(buf, bytes_read);
}

static void handle_get_host_fw_status_async(void) {
    uint8_t* buf = panel_transport_get_tx_buffer();
    rp2350_fw_status_t* status = (rp2350_fw_status_t*)buf;

    memset(status, 0, sizeof(rp2350_fw_status_t));

    status->current_version = parse_version_string(FW_VER_NUM);
    status->available_version = 0;
    status->update_progress = 0;
    status->last_update_result = 0;

    panel_transport_set_async_result(buf, sizeof(rp2350_fw_status_t));
}

static void handle_eject_image_async(uint16_t device_index) {
    image_config_t* img = get_device_by_index(device_index);
    if (!img) {
        logmsg("Panel: Invalid device index for eject ", (int)device_index);
        panel_transport_set_async_error();
        return;
    }

    if (!device_is_ejectable(img)) {
        logmsg("Panel: Device ", (int)device_index, " is not ejectable");
        panel_transport_set_async_error();
        return;
    }

    if (img->file.isOpen()) {
        logmsg("Panel: Ejecting image from SCSI ID ", (int)device_index);
        if (img->deviceType == S2S_CFG_OPTICAL) {
            // prefer_cue=false: cycle to the next .bin (like the Toolbox), not the
            // .cue, so the device stays out of is_multi_bin_cue mode and the next
            // eject can actually advance to a different disc.
            cdromPerformEject(*img, false);
        } else {
            img->ejected = true;
            switchNextImage(*img, nullptr);
        }
        panel_transport_set_async_result(nullptr, 0);
    } else {
        logmsg("Panel: No image loaded on SCSI ID ", (int)device_index);
        panel_transport_set_async_error();
    }
}

static void handle_select_next_image_async(uint16_t device_index) {
    image_config_t* img = get_device_by_index(device_index);
    if (!img) {
        logmsg("Panel: Invalid device index for next image ", (int)device_index);
        panel_transport_set_async_error();
        return;
    }

    if (!img->image_directory) {
        logmsg("Panel: Device ", (int)device_index, " not in image_directory mode");
        panel_transport_set_async_error();
        return;
    }

    logmsg("Panel: Selecting next image for SCSI ID ", (int)device_index);

    // prefer_cue=false so optical drives cycle by .bin (Toolbox-style).
    if (switchNextImage(*img, nullptr, false)) {
        logmsg("Panel: Switched to next image successfully");
        panel_transport_set_async_result(nullptr, 0);
    } else {
        logmsg("Panel: Failed to switch to next image");
        panel_transport_set_async_error();
    }
}

static void handle_select_prev_image_async(uint16_t device_index) {
    image_config_t* img = get_device_by_index(device_index);
    if (!img) {
        logmsg("Panel: Invalid device index for prev image ", (int)device_index);
        panel_transport_set_async_error();
        return;
    }

    if (!img->image_directory) {
        logmsg("Panel: Device ", (int)device_index, " not in image_directory mode");
        panel_transport_set_async_error();
        return;
    }

    char prev_filename[MAX_FILE_PATH];
    if (!panel_find_prev_image(*img, prev_filename, sizeof(prev_filename))) {
        logmsg("Panel: No images found for prev on SCSI ID ", (int)device_index);
        panel_transport_set_async_error();
        return;
    }

    logmsg("Panel: Selecting prev image '", prev_filename,
           "' for SCSI ID ", (int)device_index);

    if (switchNextImage(*img, prev_filename)) {
        logmsg("Panel: Switched to prev image successfully");
        panel_transport_set_async_result(nullptr, 0);
    } else {
        logmsg("Panel: Failed to switch to prev image");
        panel_transport_set_async_error();
    }
}

static void handle_select_image_by_name_async(const uint8_t* payload, size_t payload_size) {
    if (!payload || payload_size == 0) {
        logmsg("Panel: SELECT_IMAGE_BY_NAME with empty payload");
        panel_transport_set_async_error();
        return;
    }

    // Payload is a null-terminated filename
    const char* filename = (const char*)payload;

    // Ensure null termination within bounds
    size_t name_len = strnlen(filename, payload_size);
    if (name_len >= payload_size) {
        logmsg("Panel: SELECT_IMAGE_BY_NAME filename not null-terminated");
        panel_transport_set_async_error();
        return;
    }

    if (panel_path_has_traversal(filename)) {
        logmsg("Panel: SELECT_IMAGE_BY_NAME rejected path traversal: ", filename);
        panel_transport_set_async_error();
        return;
    }

    // Find the first configured device to load the image on
    image_config_t* img = nullptr;
    uint16_t device_index = 0;
    for (int i = 0; i < S2S_MAX_TARGETS; i++) {
        if (g_DiskImages[i].scsiId & S2S_CFG_TARGET_ENABLED) {
            img = &g_DiskImages[i];
            device_index = i;
            break;
        }
    }

    if (!img) {
        logmsg("Panel: No configured SCSI device for SELECT_IMAGE_BY_NAME");
        panel_transport_set_async_error();
        return;
    }

    logmsg("Panel: SELECT_IMAGE_BY_NAME '", filename, "' on SCSI ID ", (int)device_index);

    // Present the disc immediately if the optical tray was open (see
    // handle_select_entry_async for why we close the tray here).
    bool was_ejected = (img->deviceType == S2S_CFG_OPTICAL) && img->ejected;

    if (switchNextImage(*img, filename)) {
        if (was_ejected) {
            cdromCloseTray(*img);
        }
        logmsg("Panel: Image loaded by name successfully");
        panel_transport_set_async_result(nullptr, 0);
    } else {
        logmsg("Panel: Failed to load image by name");
        panel_transport_set_async_error();
    }
}

// ============================================================================
// File download handlers
// ============================================================================

static void handle_start_file_download_async(const uint8_t* payload, size_t payload_size) {
    uint8_t* buf = panel_transport_get_tx_buffer();
    panel_file_download_start_result_t* result = (panel_file_download_start_result_t*)buf;
    memset(result, 0, sizeof(panel_file_download_start_result_t));

    if (!payload || payload_size == 0) {
        logmsg("Panel: START_FILE_DOWNLOAD with empty payload");
        result->result_code = PANEL_DOWNLOAD_ERROR_NOT_FOUND;
        panel_transport_set_async_result(buf, sizeof(panel_file_download_start_result_t));
        return;
    }

    // Payload is a null-terminated filename
    const char* filename = (const char*)payload;
    size_t name_len = strnlen(filename, payload_size);
    if (name_len >= payload_size) {
        logmsg("Panel: START_FILE_DOWNLOAD filename not null-terminated");
        result->result_code = PANEL_DOWNLOAD_ERROR_NOT_FOUND;
        panel_transport_set_async_result(buf, sizeof(panel_file_download_start_result_t));
        return;
    }

    if (panel_path_has_traversal(filename)) {
        logmsg("Panel: START_FILE_DOWNLOAD rejected path traversal: ", filename);
        result->result_code = PANEL_DOWNLOAD_ERROR_NOT_FOUND;
        panel_transport_set_async_result(buf, sizeof(panel_file_download_start_result_t));
        return;
    }

    // Close any previous download
    g_download.reset();

    if (!g_download.download_file.open(filename, O_RDONLY)) {
        logmsg("Panel: Failed to open download file: ", filename);
        result->result_code = PANEL_DOWNLOAD_ERROR_NOT_FOUND;
        panel_transport_set_async_result(buf, sizeof(panel_file_download_start_result_t));
        return;
    }

    g_download.file_size = g_download.download_file.fileSize();
    g_download.download_active = true;

    result->result_code = PANEL_DOWNLOAD_OK;
    result->file_size = g_download.file_size;

    logmsg("Panel: File download started: ", filename, " (", (int)g_download.file_size, " bytes)");
    panel_transport_set_async_result(buf, sizeof(panel_file_download_start_result_t));
}

static void handle_read_file_chunk_async(uint32_t chunk_index) {
    uint8_t* buf = panel_transport_get_tx_buffer();

    if (!g_download.download_active || !g_download.download_file.isOpen()) {
        logmsg("Panel: READ_FILE_CHUNK with no active download");
        panel_transport_set_async_error();
        return;
    }

    uint64_t offset = (uint64_t)chunk_index * PANEL_FILE_CHUNK_SIZE;
    if (offset >= g_download.file_size) {
        logmsg("Panel: READ_FILE_CHUNK offset beyond file end");
        panel_transport_set_async_error();
        return;
    }

    if (!g_download.download_file.seek(offset)) {
        logmsg("Panel: Failed to seek in download file");
        panel_transport_set_async_error();
        return;
    }

    size_t remaining = g_download.file_size - offset;
    size_t to_read = (remaining > PANEL_FILE_CHUNK_SIZE) ?
                     PANEL_FILE_CHUNK_SIZE : remaining;

    ssize_t bytes_read = g_download.download_file.read(buf, to_read);
    if (bytes_read < 0) {
        logmsg("Panel: Failed to read download chunk");
        panel_transport_set_async_error();
        return;
    }

    dbgmsg("Panel: Download chunk ", (int)chunk_index, ": ", (int)bytes_read, " bytes");
    panel_transport_set_async_result(buf, bytes_read);
}

// ============================================================================
// File upload handlers
// ============================================================================

static void handle_start_file_upload_async(const uint8_t* payload, size_t payload_size) {
    if (payload_size < sizeof(panel_file_upload_start_t)) {
        logmsg("Panel: Invalid upload start payload size");
        panel_transport_set_async_error();
        return;
    }

    const panel_file_upload_start_t* upload_start =
        reinterpret_cast<const panel_file_upload_start_t*>(payload);

    if (upload_start->filename_len == 0 ||
        upload_start->filename_len >= sizeof(g_upload.filename) ||
        payload_size < sizeof(panel_file_upload_start_t) + upload_start->filename_len) {
        logmsg("Panel: Invalid filename length");
        panel_transport_set_async_error();
        return;
    }

    g_upload.reset();

    const char* filename_ptr = reinterpret_cast<const char*>(payload + sizeof(panel_file_upload_start_t));
    strncpy(g_upload.filename, filename_ptr, upload_start->filename_len);
    g_upload.filename[upload_start->filename_len] = '\0';
    g_upload.total_size = upload_start->file_size;

    if (panel_path_has_traversal(g_upload.filename)) {
        logmsg("Panel: START_FILE_UPLOAD rejected path traversal: ", g_upload.filename);
        panel_transport_set_async_error();
        return;
    }

    // Build full path
    char full_path[128];
    if (g_upload.filename[0] == '/') {
        strncpy(full_path, g_upload.filename, sizeof(full_path) - 1);
        full_path[sizeof(full_path) - 1] = '\0';
    } else {
        FsFile uploads_dir;
        if (!uploads_dir.open("/shared", O_RDONLY)) {
            if (!SD.mkdir("/shared")) {
                logmsg("Panel: Failed to create shared directory");
                panel_transport_set_async_error();
                return;
            }
        } else {
            uploads_dir.close();
        }
        snprintf(full_path, sizeof(full_path), "/shared/%s", g_upload.filename);
    }

    platform_reset_watchdog();
    if (!g_upload.upload_file.open(full_path, O_CREAT | O_WRITE | O_TRUNC)) {
        logmsg("Panel: Failed to create upload file: ", full_path);
        panel_transport_set_async_error();
        return;
    }

    // try_start (not start_blocking): never wait forever for the SHA-256
    // hardware. If it is busy the upload proceeds without an incremental hash.
    g_upload.sha256_ctx = new pico_sha256_state_t;
    if (pico_sha256_try_start(g_upload.sha256_ctx, SHA256_BIG_ENDIAN, true) != PICO_OK) {
        logmsg("Panel: SHA256 hardware busy, upload will not be hashed");
        delete g_upload.sha256_ctx;
        g_upload.sha256_ctx = nullptr;
    }

    g_upload.upload_active = true;
    g_upload.bytes_written = 0;

    logmsg("Panel: File upload started: ", full_path, " (", (int)g_upload.total_size, " bytes)");
    panel_transport_set_async_result(nullptr, 0);
}

static void handle_write_file_chunk_async(uint16_t expected_crc16,
                                          uint16_t computed_crc16,
                                          const uint8_t* payload, size_t payload_size) {
    if (!g_upload.upload_active) {
        logmsg("Panel: Chunk write failed - no active upload");
        panel_transport_set_async_error();
        return;
    }

    if (payload_size == 0 || payload_size > PANEL_FILE_CHUNK_SIZE) {
        logmsg("Panel: Invalid chunk size: ", (int)payload_size);
        panel_transport_set_async_error();
        return;
    }

    if (expected_crc16 != computed_crc16) {
        logmsg("Panel: Chunk CRC mismatch expected=", (int)expected_crc16,
               " computed=", (int)computed_crc16, " size=", (int)payload_size);
        panel_transport_set_async_error();
        return;
    }

    platform_reset_watchdog();
    size_t bytes_written = g_upload.upload_file.write(payload, payload_size);
    if (bytes_written != payload_size) {
        logmsg("Panel: Failed to write chunk: ", (int)bytes_written, "/", (int)payload_size);
        panel_transport_set_async_error();
        return;
    }

    g_upload.bytes_written += bytes_written;

    if (g_upload.sha256_ctx) {
        pico_sha256_update(g_upload.sha256_ctx, payload, payload_size);
    }

    g_upload.last_chunk_crc16 = computed_crc16;

    dbgmsg("Panel: Chunk written: ", (int)bytes_written, " bytes (total: ",
           (int)g_upload.bytes_written, "/", (int)g_upload.total_size, ")");

    panel_transport_set_async_result(nullptr, 0);
}

static void handle_finish_file_upload_async(void) {
    uint8_t* buf = panel_transport_get_tx_buffer();
    uint8_t result_code = PANEL_UPLOAD_OK;

    if (!g_upload.upload_active) {
        logmsg("Panel: Finish failed - no active upload");
        result_code = PANEL_UPLOAD_ERROR_WRITE;
    } else {
        platform_reset_watchdog();
        g_upload.upload_file.sync();
        g_upload.upload_file.close();

        bool hash_valid = false;
        if (g_upload.sha256_ctx) {
            pico_sha256_finish(g_upload.sha256_ctx, &g_upload.calculated_hash);
            pico_sha256_cleanup(g_upload.sha256_ctx);
            delete g_upload.sha256_ctx;
            g_upload.sha256_ctx = nullptr;
            hash_valid = true;
        }

        if (g_upload.bytes_written != g_upload.total_size) {
            logmsg("Panel: Upload size mismatch: ", (int)g_upload.bytes_written,
                   "/", (int)g_upload.total_size);
            result_code = PANEL_UPLOAD_ERROR_WRITE;
        } else if (!hash_valid) {
            // File written OK but never hashed (SHA hardware was busy at start),
            // so we have no valid checksum. Report an error rather than a bogus
            // all-zero hash the ESP32 would accept as a match.
            logmsg("Panel: Upload completed but hash unavailable");
            result_code = PANEL_UPLOAD_ERROR_WRITE;
        } else {
            logmsg("Panel: Upload completed: ", g_upload.filename,
                   " (", (int)g_upload.bytes_written, " bytes)");
        }
    }

    g_upload.upload_active = false;

    // Return result code + 32-byte SHA256 hash. Zero the hash on any error so a
    // stale/meaningless value is never reported as a valid checksum.
    if (result_code != PANEL_UPLOAD_OK) {
        memset(&g_upload.calculated_hash, 0, sizeof(g_upload.calculated_hash));
    }
    buf[0] = result_code;
    memcpy(&buf[1], g_upload.calculated_hash.bytes, 32);

    panel_transport_set_async_result(buf, 33);
}

// True if `path` is a file currently open as an image on any target, so the
// destructive handlers can refuse to pull it out from under a mounted device.
//
// Matches on the file's first data sector, which uniquely identifies a file
// regardless of its name or directory — so a same-named file in a different
// directory is NOT mistaken for the loaded one. SdFat exposes no absolute path
// for an open file, so a literal path-string compare isn't possible here. When
// the sector is unavailable (empty target, or a non-contiguous mounted image)
// it falls back to a basename match, which over-blocks rather than under-blocks.
static bool panel_path_is_loaded(const char* path) {
    FsFile target = SD.open(path, O_RDONLY);
    uint32_t target_sector = target.isOpen() ? (uint32_t)target.firstSector() : 0;
    if (target.isOpen()) target.close();

    const char* target_name = panel_path_basename(path);

    for (int id = 0; id < S2S_MAX_TARGETS; id++) {
        image_config_t& img = scsiDiskGetImageConfig(id);
        if (!img.file.isOpen()) continue;

        uint32_t bgn = 0, end = 0;
        if (target_sector != 0 && img.file.contiguousRange(&bgn, &end) && bgn != 0) {
            if (bgn == target_sector) return true;
        } else {
            char name[MAX_FILE_PATH];
            name[0] = '\0';
            img.file.getFilename(name, sizeof(name));
            if (strcasecmp(panel_path_basename(name), target_name) == 0) return true;
        }
    }
    return false;
}

// Delete a file from the SD card. Payload is a null-terminated path. Returns a
// single PANEL_DELETE_* result byte.
static void handle_delete_file_async(const uint8_t* payload, size_t payload_size) {
    uint8_t* buf = panel_transport_get_tx_buffer();
    uint8_t result = PANEL_DELETE_OK;

    if (!payload || payload_size == 0) {
        logmsg("Panel: DELETE_FILE with empty payload");
        result = PANEL_DELETE_ERROR_PATH;
    } else {
        const char* path = (const char*)payload;
        size_t name_len = strnlen(path, payload_size);
        if (name_len == 0 || name_len >= payload_size) {
            logmsg("Panel: DELETE_FILE path empty or not null-terminated");
            result = PANEL_DELETE_ERROR_PATH;
        } else if (panel_path_has_traversal(path)) {
            logmsg("Panel: DELETE_FILE rejected path traversal: ", path);
            result = PANEL_DELETE_ERROR_PATH;
        } else if (panel_path_is_loaded(path)) {
            logmsg("Panel: DELETE_FILE refused - file is loaded: ", path);
            result = PANEL_DELETE_ERROR_IN_USE;
        } else if (!SD.exists(path)) {
            logmsg("Panel: DELETE_FILE not found: ", path);
            result = PANEL_DELETE_ERROR_NOT_FOUND;
        } else if (!SD.remove(path)) {
            logmsg("Panel: DELETE_FILE failed to remove: ", path);
            result = PANEL_DELETE_ERROR_IO;
        } else {
            logmsg("Panel: File deleted: ", path);
            g_dir.scanned = false;
        }
    }

    buf[0] = result;
    panel_transport_set_async_result(buf, 1);
}

// Rename a file on the SD card. Payload is two back-to-back null-terminated
// strings: oldpath\0newpath\0. Returns a single PANEL_RENAME_* result byte.
static void handle_rename_file_async(const uint8_t* payload, size_t payload_size) {
    uint8_t* buf = panel_transport_get_tx_buffer();
    uint8_t result = PANEL_RENAME_OK;

    if (!payload || payload_size == 0) {
        logmsg("Panel: RENAME_FILE with empty payload");
        result = PANEL_RENAME_ERROR_PATH;
    } else {
        const char* old_path = (const char*)payload;
        size_t old_len = strnlen(old_path, payload_size);
        if (old_len == 0 || old_len >= payload_size) {
            logmsg("Panel: RENAME_FILE old path empty or not null-terminated");
            result = PANEL_RENAME_ERROR_PATH;
        } else {
            const char* new_path = old_path + old_len + 1;
            size_t remaining = payload_size - (old_len + 1);
            size_t new_len = strnlen(new_path, remaining);
            if (new_len == 0 || new_len >= remaining) {
                logmsg("Panel: RENAME_FILE new path empty or not null-terminated");
                result = PANEL_RENAME_ERROR_PATH;
            } else if (panel_path_has_traversal(old_path) || panel_path_has_traversal(new_path)) {
                logmsg("Panel: RENAME_FILE rejected path traversal");
                result = PANEL_RENAME_ERROR_PATH;
            } else if (panel_path_is_loaded(old_path)) {
                logmsg("Panel: RENAME_FILE refused - file is loaded: ", old_path);
                result = PANEL_RENAME_ERROR_IN_USE;
            } else if (!SD.exists(old_path)) {
                logmsg("Panel: RENAME_FILE source not found: ", old_path);
                result = PANEL_RENAME_ERROR_NOT_FOUND;
            } else if (SD.exists(new_path)) {
                logmsg("Panel: RENAME_FILE destination exists: ", new_path);
                result = PANEL_RENAME_ERROR_EXISTS;
            } else if (!SD.rename(old_path, new_path)) {
                logmsg("Panel: RENAME_FILE failed: ", old_path, " -> ", new_path);
                result = PANEL_RENAME_ERROR_IO;
            } else {
                logmsg("Panel: File renamed: ", old_path, " -> ", new_path);
                g_dir.scanned = false;
            }
        }
    }

    buf[0] = result;
    panel_transport_set_async_result(buf, 1);
}

// Create an empty file (touch). Payload is a null-terminated path. Returns a
// single PANEL_TOUCH_* result byte.
static void handle_touch_file_async(const uint8_t* payload, size_t payload_size) {
    uint8_t* buf = panel_transport_get_tx_buffer();
    uint8_t result = PANEL_TOUCH_OK;

    if (!payload || payload_size == 0) {
        logmsg("Panel: TOUCH_FILE with empty payload");
        result = PANEL_TOUCH_ERROR_PATH;
    } else {
        const char* path = (const char*)payload;
        size_t name_len = strnlen(path, payload_size);
        if (name_len == 0 || name_len >= payload_size) {
            logmsg("Panel: TOUCH_FILE path empty or not null-terminated");
            result = PANEL_TOUCH_ERROR_PATH;
        } else if (panel_path_has_traversal(path)) {
            logmsg("Panel: TOUCH_FILE rejected path traversal: ", path);
            result = PANEL_TOUCH_ERROR_PATH;
        } else if (SD.exists(path)) {
            logmsg("Panel: TOUCH_FILE already exists: ", path);
            result = PANEL_TOUCH_ERROR_EXISTS;
        } else {
            FsFile f = SD.open(path, O_WRONLY | O_CREAT);
            if (!f.isOpen()) {
                logmsg("Panel: TOUCH_FILE failed to create: ", path);
                result = PANEL_TOUCH_ERROR_IO;
            } else {
                f.close();
                logmsg("Panel: File created: ", path);
                g_dir.scanned = false;
            }
        }
    }

    buf[0] = result;
    panel_transport_set_async_result(buf, 1);
}

// Create a directory. Payload is a null-terminated path. Returns a single
// PANEL_MKDIR_* result byte.
static void handle_mkdir_async(const uint8_t* payload, size_t payload_size) {
    uint8_t* buf = panel_transport_get_tx_buffer();
    uint8_t result = PANEL_MKDIR_OK;

    if (!payload || payload_size == 0) {
        logmsg("Panel: MKDIR with empty payload");
        result = PANEL_MKDIR_ERROR_PATH;
    } else {
        const char* path = (const char*)payload;
        size_t name_len = strnlen(path, payload_size);
        if (name_len == 0 || name_len >= payload_size) {
            logmsg("Panel: MKDIR path empty or not null-terminated");
            result = PANEL_MKDIR_ERROR_PATH;
        } else if (panel_path_has_traversal(path)) {
            logmsg("Panel: MKDIR rejected path traversal: ", path);
            result = PANEL_MKDIR_ERROR_PATH;
        } else if (SD.exists(path)) {
            logmsg("Panel: MKDIR already exists: ", path);
            result = PANEL_MKDIR_ERROR_EXISTS;
        } else if (!SD.mkdir(path)) {
            logmsg("Panel: MKDIR failed: ", path);
            result = PANEL_MKDIR_ERROR_IO;
        } else {
            logmsg("Panel: Directory created: ", path);
            g_dir.scanned = false;
        }
    }

    buf[0] = result;
    panel_transport_set_async_result(buf, 1);
}

static void handle_reset(void) {
    g_async.reset();
    g_upload.reset();
    g_download.reset();
    // Drop the cached device snapshot; it repopulates on the next main-loop
    // refresh (which re-caches filenames for any loaded device).
    memset((void *)g_device_snapshot, 0, sizeof(g_device_snapshot));
    g_snapshot_name_refresh_ms = 0;
    logmsg("Panel: Reset");
}

// ============================================================================
// Public API
// ============================================================================

// Deferred IRQ-side telemetry. panel_protocol_handle_read() is called from
// the DMA IRQ; logmsg writes a lock-free shared buffer, so we record events
// here and let the main loop drain them via panel_protocol_drain_irq_log().
static volatile uint32_t g_unknown_read_cmd_count = 0;
static volatile uint8_t  g_last_unknown_read_cmd  = 0;

void panel_protocol_init(void) {
    g_async.reset();
    g_unknown_read_cmd_count = 0;
    g_last_unknown_read_cmd  = 0;
    logmsg("Panel protocol initialized");
}

size_t panel_protocol_handle_read(uint8_t cmd, uint16_t arg,
                                  uint8_t* response, size_t max_size) {
    switch (cmd) {
        case PANEL_CMD_POLL_STATUS:
            return handle_poll_status(response);

        case PANEL_CMD_GET_DEVICE_STATUS:
            return handle_get_device_status(arg, response);

        case PANEL_CMD_GET_FIRMWARE_INFO:
            return handle_get_firmware_info(response, max_size);

        case PANEL_CMD_GET_COMMAND_STATUS:
            return handle_get_command_status(response, max_size);

        case PANEL_CMD_GET_PLAYBACK_STATUS:
            return handle_get_playback_status(arg, response, max_size);

        default:
            // IRQ context - do not call logmsg here. The main loop will
            // surface this via panel_protocol_drain_irq_log().
            g_unknown_read_cmd_count++;
            g_last_unknown_read_cmd = cmd;
            return 0;
    }
}

void panel_protocol_drain_irq_log(void) {
    // Snapshot+clear the counter under interrupts-disabled to avoid races
    // with the IRQ handler. Cheap, and only runs from the main loop.
    uint32_t status = save_and_disable_interrupts();
    uint32_t count = g_unknown_read_cmd_count;
    uint8_t  last  = g_last_unknown_read_cmd;
    g_unknown_read_cmd_count = 0;
    restore_interrupts(status);

    if (count > 0) {
        logmsg("Panel: Unknown read cmd 0x", (uint8_t)last, " (", (int)count, " occurrences)");
    }
}

void panel_protocol_handle_write(uint8_t cmd, uint16_t arg,
                                 const uint8_t* payload, size_t payload_size,
                                 uint16_t payload_crc16) {
    // Track current async command
    if (PANEL_CMD_IS_ASYNC(cmd)) {
        g_async.current_command = cmd;
        g_async.state = PANEL_ASYNC_PROCESSING;
    }

    switch (cmd) {
        case PANEL_CMD_GET_DIR_ENTRY_COUNT:
            handle_get_dir_entry_count_async();
            break;

        case PANEL_CMD_GET_ENTRY_INFO:
            handle_get_entry_info_async(arg);
            break;

        case PANEL_CMD_GET_CURRENT_PATH:
            handle_get_current_path_async();
            break;

        case PANEL_CMD_SELECT_ENTRY: {
            // arg contains entry index (signed 16-bit: -1 for parent, 0-N for entry)
            // payload optionally contains device index (first byte)
            uint16_t device_index = 0;
            if (payload && payload_size >= 1) {
                device_index = payload[0];
            } else {
                // Default: find first configured device
                for (int i = 0; i < S2S_MAX_TARGETS; i++) {
                    if (g_DiskImages[i].scsiId & S2S_CFG_TARGET_ENABLED) {
                        device_index = i;
                        break;
                    }
                }
            }
            handle_select_entry_async(device_index, (int16_t)arg);
            break;
        }

        case PANEL_CMD_EJECT_IMAGE:
            handle_eject_image_async(arg);
            break;

        case PANEL_CMD_SELECT_PREV_IMAGE:
            handle_select_prev_image_async(arg);
            break;

        case PANEL_CMD_SELECT_NEXT_IMAGE:
            handle_select_next_image_async(arg);
            break;

        case PANEL_CMD_SELECT_IMAGE_BY_NAME:
            handle_select_image_by_name_async(payload, payload_size);
            break;

        case PANEL_CMD_GET_LOADED_IMAGE_STATUS:
            handle_get_loaded_image_status_async(arg);
            break;

        case PANEL_CMD_GET_DEVICE_LIST:
            handle_get_device_list_async();
            break;

        case PANEL_CMD_GET_INITIATOR_STATUS:
            handle_get_initiator_status_async();
            break;

        case PANEL_CMD_CHECK_FIRMWARE:
            handle_check_firmware_async();
            break;

        case PANEL_CMD_START_FIRMWARE_READ: {
            uint32_t offset = 0;
            if (payload && payload_size >= 4) {
                offset = payload[0] | ((uint32_t)payload[1] << 8) |
                         ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24);
            } else {
                offset = arg * PANEL_FIRMWARE_CHUNK_SIZE;
            }
            handle_start_firmware_read_async(offset);
            break;
        }

        case PANEL_CMD_GET_RP2350_FW_STATUS:
            handle_get_host_fw_status_async();
            break;

        case PANEL_CMD_START_RP2350_UPDATE:
            logmsg("Panel: Firmware update requested, rebooting...");
            panel_transport_set_async_result(nullptr, 0);
            platform_reset_mcu();
            break;

        case PANEL_CMD_START_FILE_DOWNLOAD:
            handle_start_file_download_async(payload, payload_size);
            break;

        case PANEL_CMD_READ_FILE_CHUNK: {
            // chunk_index is 32-bit; the 16-bit header arg truncates it past
            // 256MB, so prefer the 4-byte little-endian payload the device
            // also sends, falling back to arg for small files.
            uint32_t chunk_index = arg;
            if (payload && payload_size >= 4) {
                chunk_index = payload[0] | ((uint32_t)payload[1] << 8) |
                              ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24);
            }
            handle_read_file_chunk_async(chunk_index);
            break;
        }

        case PANEL_CMD_START_FILE_UPLOAD:
            handle_start_file_upload_async(payload, payload_size);
            break;

        case PANEL_CMD_WRITE_FILE_CHUNK:
            // arg = expected CRC16 (sent by ESP32 in header)
            // payload_crc16 = CRC16 calculated by DMA sniffer over received payload
            handle_write_file_chunk_async(arg, payload_crc16, payload, payload_size);
            break;

        case PANEL_CMD_FINISH_FILE_UPLOAD:
            handle_finish_file_upload_async();
            break;

        case PANEL_CMD_DELETE_FILE:
            handle_delete_file_async(payload, payload_size);
            break;

        case PANEL_CMD_RENAME_FILE:
            handle_rename_file_async(payload, payload_size);
            break;

        case PANEL_CMD_TOUCH_FILE:
            handle_touch_file_async(payload, payload_size);
            break;

        case PANEL_CMD_MKDIR:
            handle_mkdir_async(payload, payload_size);
            break;

        case PANEL_CMD_RESET:
            handle_reset();
            break;

        default:
            logmsg("Panel: Unknown write cmd ", cmd);
            if (PANEL_CMD_IS_ASYNC(cmd)) {
                panel_transport_set_async_error();
            }
            break;
    }
}

void panel_protocol_poll(void) {
    // For MVP, async operations complete immediately in their handlers
    // Future: could add background processing here
}

#endif // ENABLE_PANEL_SPI || ENABLE_PANEL_I2C
