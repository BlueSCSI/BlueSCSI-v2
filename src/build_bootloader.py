# Adds a platformio/Scons target to build the bootloader image.
# It is basically a copy of the main firmware but using AzulSCSI_bootloader.cpp
# as the main() function.

import os

Import("env")

# Build a version of AzulSCSI_main.cpp that calls bootloader instead
env2 = env.Clone()
env2.Append(CPPFLAGS = "-DAZULSCSI_BOOTLOADER_MAIN")
bootloader_main = env2.Object(
    os.path.join("$BUILD_DIR", "bootloader_main.o"),
    ["AzulSCSI_main.cpp"]
)

# Include all other dependencies except AzulSCSI_main.cpp
dep_objs = []
for nodelist in env["PIOBUILDFILES"]:
    for node in nodelist:
        filename = str(node.rfile())
        if 'AzulSCSI_main.o' not in filename:
            dep_objs.append(node)
# print("Bootloader dependencies: ", type(dep_objs), str([str(f.rfile()) for f in dep_objs]))

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
temp_src = env.subst(os.path.join("$BUILD_DIR", "bootloader_bin.cpp"))
open(temp_src, 'w').write(
'''
__asm__(
"   .section .text.btldr\\n"
"btldr:\\n"
"   .incbin \\"%s\\"\\n"
);
''' % bootloader_bin[0]
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
