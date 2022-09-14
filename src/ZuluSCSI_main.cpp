// Simple wrapper file that diverts boot from main program to bootloader
// when building the bootloader image by build_bootloader.py.

#ifdef ZULUSCSI_BOOTLOADER_MAIN

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

extern "C" void zuluscsi_setup(void);
extern "C" void zuluscsi_main_loop(void);

#ifdef USE_ARDUINO
extern "C" void setup(void)
{
    zuluscsi_setup();
}

extern "C" void loop(void)
{
    zuluscsi_main_loop();
}
#else
int main(void)
{
    zuluscsi_setup();
    while (1)
    {
        zuluscsi_main_loop();
    }
}
#endif

#endif
