#ifndef __SCSI_STATUS_H__
#define __SCSI_STATUS_H__

#define SCSI_STATUS_GOOD                            0
#define SCSI_STATUS_CHECK_CONDITION                 0x2
#define SCSI_STATUS_CONDITION_MET                   0x4
#define SCSI_STATUS_BUSY                            0x8
#define SCSI_STATUS_INTERMEDIATE                    0x16
#define SCSI_STATUS_INTERMEDIATE_CONDITION_MET      0x20
#define SCSI_STATUS_RESERVATION_CONFLICT            0x24
#define SCSI_STATUS_COMMAND_TERMINATED              0x34
#define SCSI_STATUS_QUEUE_FULL                      0x40

#endif