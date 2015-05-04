# Author James Laird-Wah <james@laird-wah.net>
# This is free and unencumbered software released into the public domain.
#
# Anyone is free to copy, modify, publish, use, compile, sell, or
# distribute this software, either in source code form or as a compiled
# binary, for any purpose, commercial or non-commercial, and by any
# means.
#
# In jurisdictions that recognize copyright laws, the author or authors
# of this software dedicate any and all copyright interest in the
# software to the public domain. We make this dedication for the benefit
# of the public at large and to the detriment of our heirs and
# successors. We intend this dedication to be an overt act of
# relinquishment in perpetuity of all present and future rights to this
# software under copyright law.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

# For more information, please refer to <http://unlicense.org/>
open HDR, '<trace.h';
$next = 0;
$enum = 0;
for (<HDR>) {
    chomp;
    /^enum trace_event/ || $enum or next;
    $enum++;
    /}/ && break;
    /trace_(\S+)(\s*=\s*(\S+))?\s*,\s*$/ or next;
    ($name, $valmatch, $val) = ($1, $2, $3);
    $next = hex $val if defined $val;
    $names{$next} = $name;
    $next++;
}
    
while (!eof STDIN) {
    $ch = ord getc;
    if ($ch==1 || $ch==9) {
        $data = ord getc;
        print "ISR: " if $ch==9;
        $name = $names{$data} // sprintf "unk: 0x%X", $data;
        print $names{$data}, "\n"
    } else {
        print "<dropped>\n";
    }
}
