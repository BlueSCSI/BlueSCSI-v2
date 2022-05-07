// Simple wrapper file that diverts boot from main program to bootloader
// when building the bootloader image by build_bootloader.py.

#ifdef ZULUSCSI_BOOTLOADER_MAIN

extern "C" int bootloader_main(void);

int main(void)
{
    return bootloader_main();
}

#else

extern "C" int zuluscsi_main(void);

int main(void)
{
    return zuluscsi_main();
}

#endif
