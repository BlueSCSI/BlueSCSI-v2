#include "AzulSCSI_log.h"
#include "AzulSCSI_platform.h"

// This memory buffer can be read by debugger and is also saved to azullog.txt
char g_logbuffer[4096];
uint32_t g_logpos = 0;

void azlog(const char *str)
{
    const char *p = str;
    while (*p && g_logpos < sizeof(g_logbuffer) - 1)
    {
        g_logbuffer[g_logpos++] = *p++;
    }

    azplatform_log(str);
}

uint32_t azlog_get_buffer_len()
{
    return g_logpos;
}

const char *azlog_get_buffer()
{
    return g_logbuffer;
}

