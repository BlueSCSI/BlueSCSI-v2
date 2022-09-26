Performance
-----------
Performance characteristics of ZuluSCSI differ depending on the hardware version. 

Additionally, on older, slower computers, particularly 68000 Macs with 8-16MHz CPUs, performance will be gated by the speed of your CPU, and the local bus between the CPU and your SCSI controller.


With ZuluSCSI V1.1, and verbose log messages disabled (debug DIP switch OFF), expected SCSI performance is ~3.5 MB/s read and ~2 MB/s write.

With ZuluSCSI V1.0, and verbose log messages disabled (debug DIP switch OFF), expected SCSI performance is 2.4 MB/s read and 2 MB/s write, assuming you're using a fast enough machine, 
With ZuluSCSI Mini (V1.0), and verbose log messages disabled (default, unless you enable it in zuluscsi.ini), 
expected SCSI performance is 2.4 MB/s read and 2 MB/s write.

Slow SD cards or a fragmented filesystem can slow down access. The use of Speed Class 4 SD cards may result in the bottleneck being the SD card itself. We recommend using Speed Class 10 or above SDHC-marked cards. 

Seek performance is best if image files are contiguous.

For ExFAT filesystem this relies on a file flag set by PC.
Current versions of exfat-fuse on Linux have an [issue](https://github.com/relan/exfat/pull/101) that causes the files not to be marked contiguous even when they are.
This is indicated by message `WARNING: file HD00_512.hda is not contiguous. This will increase read latency.` in the log.
