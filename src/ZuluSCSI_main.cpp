/** 
 * ZuluSCSI™ - Copyright (c) 2022 Rabbit Hole Computing™
 * 
 * ZuluSCSI™ firmware is licensed under the GPL version 3 or any later version. 
 * 
 * https://www.gnu.org/licenses/gpl-3.0.html
 * ----
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/

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
