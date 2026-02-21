// Custom .ini file access caching layer for minIni.
// This reduces boot delay by only reading the ini file once
// after boot or SD-card removal.

#include <minGlue.h>
#include <SdFat.h>

// This can be overridden in platformio.ini
// Set to 0 to disable the cache.
#ifndef INI_CACHE_SIZE
#define INI_CACHE_SIZE 4096
#endif

// Use the SdFs instance from main program
extern SdFs SD;

static struct {
    bool valid;
    INI_FILETYPE *fp;

#if INI_CACHE_SIZE > 0
    const char *filename;
    uint32_t filelen;
    INI_FILEPOS current_pos;
    char cachedata[INI_CACHE_SIZE];
#endif
} g_ini_cache;

// Invalidate any cached file contents
void invalidate_ini_cache()
{
    g_ini_cache.valid = false;
    g_ini_cache.fp = NULL;
}

// Read the config file into RAM
void reload_ini_cache(const char *filename)
{
    g_ini_cache.valid = false;
    g_ini_cache.fp = NULL;

#if INI_CACHE_SIZE > 0
    g_ini_cache.filename = filename;
    FsFile config = SD.open(filename, O_RDONLY);
    g_ini_cache.filelen = config.fileSize();
    if (config.isOpen() && g_ini_cache.filelen <= INI_CACHE_SIZE)
    {
        if (config.read(g_ini_cache.cachedata, g_ini_cache.filelen) == g_ini_cache.filelen)
        {
            g_ini_cache.valid = true;
        }
    }
    config.close();
#endif
}


// Open .ini file either from cache or from SD card
bool ini_openread(const char *filename, INI_FILETYPE *fp)
{
#if INI_CACHE_SIZE > 0
    if (SD.exists(filename) &&
        g_ini_cache.valid &&
        (filename == g_ini_cache.filename || strcmp(filename, g_ini_cache.filename) == 0))
    {
        fp->close();
        g_ini_cache.fp = fp;
        g_ini_cache.current_pos.position = 0;
        return true;
    }
    invalidate_ini_cache(); // new file?
#endif

    return fp->open(SD.vol(), filename, O_RDONLY);
}

bool ini_openwrite(const char *filename, INI_FILETYPE *fp) {
#if INI_CACHE_SIZE > 0
    invalidate_ini_cache(); // We'll be writing so just use the actual fp.
#endif
    return fp->open(SD.vol(), filename, O_WRITE | O_CREAT);
}

bool ini_openrewrite(const char *filename, INI_FILETYPE *fp) {
#if INI_CACHE_SIZE > 0
    invalidate_ini_cache(); // We'll be writing so just use the actual fp.
#endif
    return fp->open(SD.vol(), filename, O_RDWR | O_CREAT);
}

// Close previously opened file
bool ini_close(INI_FILETYPE *fp)
{
#if INI_CACHE_SIZE > 0
    if (g_ini_cache.valid && g_ini_cache.fp == fp)
    {
        g_ini_cache.fp = NULL;
        return true;
    }
#endif
    {
        return fp->close();
    }
}

// Read a single line from cache or from SD card
bool ini_read(char *buffer, int size, INI_FILETYPE *fp)
{
#if INI_CACHE_SIZE > 0
    if (g_ini_cache.fp == fp)
    {
        if (!g_ini_cache.valid)
        {
            reload_ini_cache(g_ini_cache.filename);
        }

        if (g_ini_cache.valid)
        {
            // Read one line from cache
            uint32_t srcpos = g_ini_cache.current_pos.position;
            int dstpos = 0;
            while (srcpos < g_ini_cache.filelen &&
                   dstpos < size - 1)
            {
                char b = g_ini_cache.cachedata[srcpos++];
                buffer[dstpos++] = b;

                if (b == '\n') break;
            }
            buffer[dstpos] = 0;
            g_ini_cache.current_pos.position = srcpos;
            return dstpos > 0;
        }
    }
#endif
    {
        // Read from SD card
        return fp->fgets(buffer, size) > 0;
    }
}

// Get the position inside the file
void ini_tell(INI_FILETYPE *fp, INI_FILEPOS *pos)
{
#if INI_CACHE_SIZE > 0
    if (g_ini_cache.fp == fp)
    {
        *pos = g_ini_cache.current_pos;
    }
    else
#endif
    {
        fp->fgetpos(pos);
    }
}

// Go back to previously saved position
void ini_seek(INI_FILETYPE *fp, INI_FILEPOS *pos)
{
#if INI_CACHE_SIZE > 0
    if (g_ini_cache.fp == fp)
    {
        g_ini_cache.current_pos = *pos;
    }
    else
#endif
    {
        fp->fsetpos(pos);
    }
}



void ini_rename(const char *source, const char *destination) {
    if (SD.exists(destination)) {
        SD.remove(destination);
    }
    SD.rename(source, destination);
#if INI_CACHE_SIZE > 0
    invalidate_ini_cache();
#endif
}

void ini_write(char *buffer, INI_FILETYPE *fp) {
    fp->write(buffer);
    invalidate_ini_cache(); // for next open
}