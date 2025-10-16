# Adds a platformio/Scons target to build the bootloader image.
# It is basically a copy of the main firmware but using BlueSCSI_bootloader.cpp
# as the main() function.
#
# Copyright (c) 2022 Rabbit Hole Computing™

from string import Template 
Import("env")

# Literally just copy the ldscript out into the build directory for now

template_file =  env.GetProjectOption('ldscript_build')
linker_file = env.subst('$BUILD_DIR') + '/rp_linker.ld'

def process_template(source, target, env):
    values = {
        'program_size': env.GetProjectOption('program_flash_allocation'),
        'project_name': env.subst('$PIOENV')
        }
    with open(template_file, 'r') as t:
        src = Template(t.read())
        result = src.substitute(values)

    with open(linker_file, 'w') as linker_script:
        linker_script.write(result)

env.AddPreAction("${BUILD_DIR}/${PROGNAME}.elf",
        env.VerboseAction(process_template, 
        'Generating linker script: "' + linker_file + '" from : "' + template_file + '"'
        )
)