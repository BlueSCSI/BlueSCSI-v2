#!/usr/bin/env ruby
#
# Copyright (c) 2024 joshua stein <jcs@jcs.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

dir = File.realpath(File.dirname(__FILE__))
Dir.chdir("#{dir}/../tls")

h = File.open("../lib/SCSI2SD/src/firmware/tls-anchors.h", "w+")
certs = Dir.glob("*.cer")

h.puts "/*"
h.puts " * AUTOMATICALLY GENERATED, DO NOT MODIFY"
h.puts " * Run utils/#{File.basename(__FILE__)} to regenerate"
h.puts " */"

IO.popen( [ "brssl", "ta" ] + certs, "r", :err=>["/dev/null"]) do |brssl|
  while brssl && !brssl.eof?
    line = brssl.gets

    if m = line.match(/^static const unsigned char TA(\d+)_DN/)
      cert = certs[m[1].to_i]

      dump = IO.popen([ "openssl", "x509", "-in", cert, "-noout", "-text" ],
        "r").read
      h.puts "/*"
      h.puts " * #{cert}:"
      h.puts " *"
      h.puts dump.split("\n").map{|l| " * #{l}" }.join("\n")
      h.puts " */"
    end

    h.puts line
  end
end
