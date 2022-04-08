Performance
-----------
Performance characteristics of AzulSCSI differ depending on the hardware version. 

With AzulSCSI V1.1, and verbose log messages disabled (debug DIP switch OFF), expected SCSI performance is ~3.5 MB/s read and ~2 MB/s write.

With AzulSCSI V1.0, and verbose log messages disabled (debug DIP switch OFF), expected SCSI performance is 2.4 MB/s read and 2 MB/s write.

With AzulSCSI Mini (V1.0), and verbose log messages disabled (default, unless you enable it in azulscsi.ini), expected SCSI performance is 2.4 MB/s read and 2 MB/s write.

Slow SD cards or a fragmented filesystem can slow down access.

Seek performance is best if image files are contiguous.

For ExFAT filesystem this relies on a file flag set by PC.
Current versions of exfat-fuse on Linux have an [issue](https://github.com/relan/exfat/pull/101) that causes the files not to be marked contiguous even when they are.
This is indicated by message `WARNING: file HD00_512.hda is not contiguous. This will increase read latency.` in the log.
