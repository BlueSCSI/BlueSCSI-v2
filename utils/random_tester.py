#!/usr/bin/python3

'''
  ZuluSCSI™ - Copyright (c) 2022 Rabbit Hole Computing™
  
  ZuluSCSI™ file is licensed under the GPL version 3 or any later version. 
  
  https://www.gnu.org/licenses/gpl-3.0.html
  ----
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version. 
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details. 
  
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
'''

'''This script executes random-sized reads and writes to one or more block devices to test them.
It will destroy the contents of the block device.'''

import sys
import os
import mmap
import random
import time

class BlockDevice:
    def __init__(self, path, sectorsize = 512):
        self.path = path
        self.dev = os.fdopen(os.open(path, os.O_RDWR | os.O_DIRECT | os.O_SYNC), "rb+", 0)
        self.sectorsize = sectorsize

    def write_block(self, first_sector, sector_count, seed):
        rnd = random.Random(seed)
        buffer = mmap.mmap(-1, sector_count * self.sectorsize)
        buffer.write(rnd.randbytes(sector_count * self.sectorsize))
        
        start = time.time()
        self.dev.seek(first_sector * self.sectorsize)
        self.dev.write(buffer)
        elapsed = time.time() - start
        speed = sector_count * self.sectorsize / elapsed / 1e6

        print("Wrote  %16s, %8d, %8d, %8d, %8.3f MB/s" % (self.path, first_sector, sector_count, seed, speed))

    def verify_block(self, first_sector, sector_count, seed):
        rnd = random.Random(seed)
        buffer = mmap.mmap(-1, sector_count * self.sectorsize)

        start = time.time()
        self.dev.seek(first_sector * self.sectorsize)
        self.dev.readinto(buffer)
        elapsed = time.time() - start
        speed = sector_count * self.sectorsize / elapsed / 1e6

        print("Verify %16s, %8d, %8d, %8d, %8.3f MB/s" % (self.path, first_sector, sector_count, seed, speed))

        buffer.seek(0)
        actual = buffer.read(sector_count * self.sectorsize)
        expected = rnd.randbytes(sector_count * self.sectorsize)
        if expected != actual:
            print("Compare error, device = %s, sectorsize = %d, first_sector = %d, sector_count = %d, seed = %d"
                % (self.path, self.sectorsize, first_sector, sector_count, seed))
            fname = "%d" % time.time()
            open(fname + ".expected", "wb").write(expected)
            open(fname + ".actual", "wb").write(actual)
            print("Saved data to %s.expected/actual" % fname)
            raise Exception("Compare error")

if __name__ == "__main__":
    blockdevs = []
    for path in sys.argv[1:]:
        sectorsize = 512
        if ':' in path:
            path, sectorsize = path.split(':')
            sectorsize = int(sectorsize)
        blockdevs.append(BlockDevice(path, sectorsize=sectorsize))
    
    maxsectors = 100000
    rnd = random.Random()
    while True:
        blocks = []
        start = 0
        while start + 256 < maxsectors:
            start = min(maxsectors, start + rnd.randint(0, 10000))
            dev = rnd.choice(blockdevs)
            count = rnd.randint(1, 256)
            seed = rnd.randint(1, 10000000)
            blocks.append((dev, start, count, seed))
            start += count
        
        print("Write / verify set size: %d" % len(blocks))

        random.shuffle(blocks)
        for dev, start, count, seed in blocks:
            dev.write_block(start, count, seed)
        
        random.shuffle(blocks)
        for dev, start, count, seed in blocks:
            dev.verify_block(start, count, seed)


