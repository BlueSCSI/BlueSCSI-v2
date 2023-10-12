/*  Glue functions for the minIni library to the cache functions in minIni_cache.cpp */

#include <SdFat.h>

#define INI_OPENREWRITE 1
#define INI_FILETYPE                    FsFile
#define INI_FILEPOS                     fspos_t

bool ini_openread(const char *filename, INI_FILETYPE *fp);
bool ini_close(INI_FILETYPE *fp);
bool ini_read(char *buffer, int size, INI_FILETYPE *fp);
void ini_tell(INI_FILETYPE *fp, INI_FILEPOS *pos);
void ini_seek(INI_FILETYPE *fp, INI_FILEPOS *pos);

bool ini_write(char *buffer, INI_FILETYPE *fp);
bool ini_openwrite(const char *filename, INI_FILETYPE *fp);
bool ini_openrewrite(const char *filename, INI_FILETYPE *fp);
void ini_rename(const char *new_name, const char *old_name);