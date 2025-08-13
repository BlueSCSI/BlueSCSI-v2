# This file is originally part of ZuluSCSI adapted for BlueSCSI
#
# ZuluSCSI™ - Copyright (c) 2022-2025 Rabbit Hole Computing™
# Eric Helgeson - Copyright 2025
#
# ZuluSCSI™ firmware is licensed under the GPL version 3 or any later version. 
#
# https://www.gnu.org/licenses/gpl-3.0.html
# ----
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version. 
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details. 
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.


# Adds a platformio/Scons target to build the bootloader image.
# It is basically a copy of the main firmware but using BlueSCSI_bootloader.cpp
# as the main() function.

import os

Import("env")

# Build a version of BlueSCSI_main.cpp that calls bootloader instead
env2 = env.Clone()
env2.Append(CPPFLAGS = "-DBLUESCSI_BOOTLOADER_MAIN")
bootloader_main = env2.Object(
    os.path.join("$BUILD_DIR", "bootloader_main.o"),
    ["BlueSCSI_main.cpp"]
)

# BlueSCSI_msc_initiator, BlueSCSI_msc g_msc_initiator
# BlueSCSI_log_trace, ImageBackingStore -> ROMDrive -^ links to initiator

excluded_files = ['BlueSCSI_main', 'BlueSCSI_Toolbox', 'BlueSCSI_cdrom', 'BlueSCSI_disk',
                  'BlueSCSI_tape', 'BlueSCSI_platform_msc', 'QuirksCheck', 'BlueSCSI_blink', 'BlueSCSI_mode']

# Include all other dependencies except BlueSCSI_main.cpp
dep_objs = []
for nodelist in env["PIOBUILDFILES"]:
    for node in nodelist:
        filename = str(node.rfile())
        if not any(excluded_file in filename for excluded_file in excluded_files):
            dep_objs.append(node)
print("== Bootloader dependencies:\n|--",
      "\n|-- ".join([str(f.path) for f in dep_objs]))

# Use different linker script for bootloader
if env2.GetProjectOption("ldscript_bootloader"):
    env2.Replace(LDSCRIPT_PATH = env2.GetProjectOption("ldscript_bootloader"))
    env2['LINKFLAGS'] = [a for a in env2['LINKFLAGS'] if not a.startswith('-T') and not a.endswith('.ld')]
    env2.Append(LINKFLAGS="-T" + env2.GetProjectOption("ldscript_bootloader"))

# Build bootloader.elf
bootloader_elf = env2.Program(
    os.path.join("$BUILD_DIR", "bootloader.elf"),
    [bootloader_main] + dep_objs
)

# Strip bootloader symbols so that it can be combined with main program
bootloader_bin = env.ElfToBin(
    os.path.join("$BUILD_DIR", "bootloader.bin"),
    bootloader_elf
)

def print_bootloader_size(source, target, env):
    bootloader_bin_path = str(target[0].get_abspath())
    size = os.path.getsize(bootloader_bin_path)
    max_size = 131072  # 128k
    percentage = (size / max_size) * 100
    print(f"Bootloader binary size: {size} / {max_size} bytes ({percentage:.2f}%)")

env.AddPostAction(bootloader_bin, print_bootloader_size)

# Convert back to .o to facilitate linking
def escape_cpp(path):
    '''Apply double-escaping for path name to include in generated C++ file'''
    result = ''
    for c in str(path).encode('utf-8'):
        if 32 <= c <= 127 and c not in (ord('\\'), ord('"')):
            result += chr(c)
        else:
            result += '\\\\%03o' % c
    return result

temp_src = env.subst(os.path.join("$BUILD_DIR", "bootloader_bin.cpp"))
open(temp_src, 'w').write(
'''
__asm__(
"   .section .text.btldr\\n"
"btldr:\\n"
"   .incbin \\"%s\\"\\n"
);
''' % escape_cpp(bootloader_bin[0])
)

bootloader_obj = env.Object(
    os.path.join("$BUILD_DIR", "bootloader_bin.o"),
    temp_src
)

# Add bootloader binary as dependency to the main firmware
main_fw = os.path.join("$BUILD_DIR", env.subst("$PROGNAME$PROGSUFFIX"))
env.Depends(bootloader_obj, bootloader_bin)
env.Depends(main_fw, bootloader_obj)
env.Append(LINKFLAGS = [bootloader_obj])
