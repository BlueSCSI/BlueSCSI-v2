// Simple wrapper file that diverts boot from main program to bootloader
// when building the bootloader image by build_bootloader.py.

#ifdef BlueSCSI_BOOTLOADER_MAIN

extern "C" int bootloader_main(void);

#ifdef USE_ARDUINO
extern "C" void setup(void)
{
    bootloader_main();
}
extern "C" void loop(void)
{
}
#else
int main(void)
{
    return bootloader_main();
}
#endif

#else

extern "C" void bluescsi_setup(void);
extern "C" void bluescsi_main_loop(void);

#ifdef USE_ARDUINO
extern "C" void setup(void)
{
    bluescsi_setup();
}

extern "C" void loop(void)
{
    bluescsi_main_loop();
}
#else
int main(void)
{
    bluescsi_setup();
    while (1)
    {
        bluescsi_main_loop();
    }
}
#endif

#endif
