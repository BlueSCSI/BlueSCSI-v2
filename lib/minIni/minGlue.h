/*  Glue functions for the minIni library to SdFat library */

#include <SdFat.h>

extern SdFs SD;

#define INI_READONLY 1
#define INI_FILETYPE                    FsFile
#define ini_openread(filename,file)     ((file)->open(SD.vol(), filename, O_RDONLY))
#define ini_close(file)                 ((file)->close())
#define ini_read(buffer,size,file)      ((file)->fgets((buffer),(size)) > 0)

#define INI_FILEPOS                     fspos_t
#define ini_tell(file,pos)              ((file)->fgetpos(pos))
#define ini_seek(file,pos)              ((file)->fsetpos(pos))
