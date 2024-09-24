# ZuluSCSI™ - Copyright (c) 2024 Rabbit Hole Computing™
#
# ZuluSCSI™ file is licensed under the GPL version 3 or any later version. 
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
from string import Template 
Import ("env")

template_file = 'lib/ZuluSCSI_platform_RP2350/rp2350-template.ld'
linker_file = env.subst('$BUILD_DIR') + '/rp2350.ld'

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