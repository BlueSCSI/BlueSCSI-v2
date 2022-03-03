// SCSI subroutines using external GreenPAK logic chip for acceleration

#pragma once

#include <stdint.h>
#include "greenpak.h"

void scsi_accel_greenpak_send(const uint32_t *buf, uint32_t num_words, volatile int *resetFlag);