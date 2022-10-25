GreenPAK design files
=====================

This folder contains design files for `SLG46824` programmable logic device.
It is optionally used on ZuluSCSI V1.1 to speed up access to SCSI bus. This applies to asynchronous transfers only

What this logic does is implement the `REQ` / `ACK` handshake in hardware.
The CPU only has to write new data to the GPIO, and the external logic will toggle `REQ` signal quickly.
