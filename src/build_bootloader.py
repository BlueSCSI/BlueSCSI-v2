# ZuluSCSI™ - Copyright (c) 2022-2025 Rabbit Hole Computing™
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
# It is basically a copy of the main firmware but using ZuluSCSI_bootloader.cpp
# as the main() function.

import os

Import("env")

# Build a version of ZuluSCSI_main.cpp that calls bootloader instead
env2 = env.Clone()
env2.Append(CPPFLAGS = "-DZULUSCSI_BOOTLOADER_MAIN")
bootloader_main = env2.Object(
    os.path.join("$BUILD_DIR", "bootloader_main.o"),
    ["ZuluSCSI_main.cpp"]
)

# Include all other dependencies except ZuluSCSI_main.cpp
dep_objs = []
for nodelist in env["PIOBUILDFILES"]:
    for node in nodelist:
        filename = str(node.rfile())
        if 'ZuluSCSI_main' not in filename:
            dep_objs.append(node)
# print("Bootloader dependencies: ", type(dep_objs), str([str(f.rfile()) for f in dep_objs]))

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
