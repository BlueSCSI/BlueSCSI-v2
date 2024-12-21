#ifdef LIB_FREERTOS_KERNEL
#include "FreeRTOS.h"
#include "BlueSCSI_platform.h"
#ifdef __cplusplus
extern "C"
{
#endif
#include "FreeRTOS_CLI.h"
#ifdef __cplusplus
}
#endif
#include "hardware/watchdog.h"
#include <stdio.h>
#include <stdarg.h>

extern const char *g_platform_name;
extern const char *g_log_firmwareversion;

/*
 * Implements the task-stats command.
 */
static BaseType_t prvResetCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
static BaseType_t prvPlatformInfoCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);

/* Structure that defines the command line commands. */
static const CLI_Command_Definition_t xReset =
    {
        "reset",
        "\nreset:\n Resets the processor\n",
        prvResetCommand, /* The function to run. */
        0 /* There are no parameters to this function */
};

/* Structure that defines the command line commands. */
static const CLI_Command_Definition_t xPlatformInfo =
    {
        "info",
        "\ninfo:\n Prints information about the current\n BlueSCSI platform configuration\n",
        prvPlatformInfoCommand, /* The function to run. */
        0 /* There are no parameters to this function */
};
static BaseType_t prvResetCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{

    /* Remove compile time warnings about unused parameters */
    (void)pcCommandString;
    (void)xWriteBufferLen;
    (void)pcWriteBuffer;

    watchdog_reboot(0, 0, 0);

    /* We should't get here, but if we do, return that there is no additional string
    data to return. */
    return pdFALSE;
}


char * cmd_sprintf(char **buf, size_t *remaining_chars, const char *fmt, ...){
    va_list va;
    va_start (va, fmt);
    size_t chars_added = snprintf (*buf, *remaining_chars, fmt, va);
    va_end (va);
    if(chars_added <= *remaining_chars){
        *remaining_chars -= chars_added;
    }
    else{
        *remaining_chars = 0;
        }
    *buf = *buf + chars_added;
    return (char *)(buf + chars_added);
}

static int platform_info_line_num = 0;
static BaseType_t prvPlatformInfoCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    (void)pcCommandString;
    (void)xWriteBufferLen;
    configASSERT(pcWriteBuffer);

    switch(platform_info_line_num++){
        case 0:
            snprintf(pcWriteBuffer, xWriteBufferLen, "Platform Information:\n*************\n");
            break;
        case 1:
            snprintf(pcWriteBuffer, xWriteBufferLen, "  Platform: %s\n", g_platform_name);
            break;
        case 2:
            snprintf(pcWriteBuffer, xWriteBufferLen, "  Firmware Version: %s\n", g_log_firmwareversion);
            break;
        case 3:
            snprintf(pcWriteBuffer, xWriteBufferLen, "  PicoSDK: %s\n", PICO_SDK_VERSION_STRING);
            break;
        case 4:
            snprintf(pcWriteBuffer, xWriteBufferLen, "  Rom Drive Maxsize: %lu\n", platform_get_romdrive_maxsize());
            break;
        case 5:
            snprintf(pcWriteBuffer, xWriteBufferLen, "  Network Supported: %d\n", platform_network_supported());
            break;
        case 6:
            snprintf(pcWriteBuffer, xWriteBufferLen, "  Initiator Mode Enabled: %d\n", platform_is_initiator_mode_enabled());
            break;
        case 7:
            snprintf(pcWriteBuffer, xWriteBufferLen, "  Hardware version 2023.09.a: %d\n", (int)is202309a());
            break;
        default:
            snprintf(pcWriteBuffer, xWriteBufferLen, "****************************************\n");
            platform_info_line_num = 0;
            return pdFALSE;
            break;
    }

    return pdTRUE; // Indicate we have more data to print
}

void platform_register_cli(void){
    FreeRTOS_CLIRegisterCommand(&xReset);
    FreeRTOS_CLIRegisterCommand(&xPlatformInfo);
}

#endif 
