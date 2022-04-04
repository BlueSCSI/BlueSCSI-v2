GreenPAK design files
=====================

This folder contains design files for `SLG46824` programmable logic device.
It is optionally used on AzulSCSI V1.1 to speed up access to SCSI bus.

What this logic does is implement the `REQ` / `ACK` handshake in hardware.
The CPU only has to write new data to the GPIO, and the external logic will toggle `REQ` signal quickly.
