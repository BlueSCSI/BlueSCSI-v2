// Custom .ini file access caching layer for minIni.
// This reduces boot delay by only reading the ini file once
// after boot or SD-card removal.

#pragma once

void invalidate_ini_cache();

// Note: filename must be statically allocated, pointer is stored.
void reload_ini_cache(const char *filename);

// Returns a pointer to the cached, null-terminated INI file data and its length.
// Returns nullptr if the cache is not valid.
const char* ini_get_cache_ptr(uint32_t* out_len);