//	Copyright (C) 2013 Michael McMaster <michael@codesrc.com>
//
//	This file is part of SCSI2SD.
//
//	SCSI2SD is free software: you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	SCSI2SD is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with SCSI2SD.  If not, see <http://www.gnu.org/licenses/>.
#ifndef SENSE_H
#define SENSE_H

typedef enum
{
	NO_SENSE                                               = 0,
	RECOVERED_ERROR                                        = 1,
	NOT_READY                                              = 2,
	MEDIUM_ERROR                                           = 3,
	HARDWARE_ERROR                                         = 4,
	ILLEGAL_REQUEST                                        = 5,
	UNIT_ATTENTION                                         = 6,
	DATA_PROTECT                                           = 7,
	BLANK_CHECK                                            = 8,
	VENDOR_SPECIFIC                                        = 9,
	COPY_ABORTED                                           = 0xA,
	ABORTED_COMMAND                                        = 0xB,
	EQUAL                                                  = 0xC,
	VOLUME_OVERFLOW                                        = 0xD,
	MISCOMPARE                                             = 0xE,
	RESERVED                                               = 0xF
} SCSI_SENSE;

// Top 8 bits = ASC. Lower 8 bits = ASCQ.
// Enum only contains definitions for direct-access related codes.
typedef enum
{
	ADDRESS_MARK_NOT_FOUND_FOR_DATA_FIELD                  = 0x1300,
	ADDRESS_MARK_NOT_FOUND_FOR_ID_FIELD                    = 0x1200,
	CANNOT_READ_MEDIUM_INCOMPATIBLE_FORMAT                 = 0x3002,
	CANNOT_READ_MEDIUM_UNKNOWN_FORMAT                      = 0x3001,
	CHANGED_OPERATING_DEFINITION                           = 0x3F02,
	COMMAND_PHASE_ERROR                                    = 0x4A00,
	COMMAND_SEQUENCE_ERROR                                 = 0x2C00,
	COMMANDS_CLEARED_BY_ANOTHER_INITIATOR                  = 0x2F00,
	COPY_CANNOT_EXECUTE_SINCE_HOST_CANNOT_DISCONNECT       = 0x2B00,
	DATA_PATH_FAILURE                                      = 0x4100,
	DATA_PHASE_ERROR                                       = 0x4B00,
	DATA_SYNCHRONIZATION_MARK_ERROR                        = 0x1600,
	DEFECT_LIST_ERROR                                      = 0x1900,
	DEFECT_LIST_ERROR_IN_GROWN_LIST                        = 0x1903,
	DEFECT_LIST_ERROR_IN_PRIMARY_LIST                      = 0x1902,
	DEFECT_LIST_NOT_AVAILABLE                              = 0x1901,
	DEFECT_LIST_NOT_FOUND                                  = 0x1C00,
	DEFECT_LIST_UPDATE_FAILURE                             = 0x3201,
	ERROR_LOG_OVERFLOW                                     = 0x0A00,
	ERROR_TOO_LONG_TO_CORRECT                              = 0x1102,
	FORMAT_COMMAND_FAILED                                  = 0x3101,
	GROWN_DEFECT_LIST_NOT_FOUND                            = 0x1C02,
	IO_PROCESS_TERMINATED                                  = 0x0006,
	ID_CRC_OR_ECC_ERROR                                    = 0x1000,
	ILLEGAL_FUNCTION                                       = 0x2200,
	INCOMPATIBLE_MEDIUM_INSTALLED                          = 0x3000,
	INITIATOR_DETECTED_ERROR_MESSAGE_RECEIVED              = 0x4800,
	INQUIRY_DATA_HAS_CHANGED                               = 0x3F03,
	INTERNAL_TARGET_FAILURE                                = 0x4400,
	INVALID_BITS_IN_IDENTIFY_MESSAGE                       = 0x3D00,
	INVALID_COMMAND_OPERATION_CODE                         = 0x2000,
	INVALID_FIELD_IN_CDB                                   = 0x2400,
	INVALID_FIELD_IN_PARAMETER_LIST                        = 0x2600,
	INVALID_MESSAGE_ERROR                                  = 0x4900,
	LOG_COUNTER_AT_MAXIMUM                                 = 0x5B02,
	LOG_EXCEPTION                                          = 0x5B00,
	LOG_LIST_CODES_EXHAUSTED                               = 0x5B03,
	LOG_PARAMETERS_CHANGED                                 = 0x2A02,
	LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE                     = 0x2100,
	LOGICAL_UNIT_COMMUNICATION_FAILURE                     = 0x0800,
	LOGICAL_UNIT_COMMUNICATION_PARITY_ERROR                = 0x0802,
	LOGICAL_UNIT_COMMUNICATION_TIMEOUT                     = 0x0801,
	LOGICAL_UNIT_DOES_NOT_RESPOND_TO_SELECTION             = 0x0500,
	LOGICAL_UNIT_FAILED_SELF_CONFIGURATION                 = 0x4C00,
	LOGICAL_UNIT_HAS_NOT_SELF_CONFIGURED_YET               = 0x3E00,
	LOGICAL_UNIT_IS_IN_PROCESS_OF_BECOMING_READY           = 0x0401,
	LOGICAL_UNIT_NOT_READY_CAUSE_NOT_REPORTABLE            = 0x0400,
	LOGICAL_UNIT_NOT_READY_FORMAT_IN_PROGRESS              = 0x0404,
	LOGICAL_UNIT_NOT_READY_INITIALIZING_COMMAND_REQUIRED   = 0x0402,
	LOGICAL_UNIT_NOT_READY_MANUAL_INTERVENTION_REQUIRED    = 0x0403,
	LOGICAL_UNIT_NOT_SUPPORTED                             = 0x2500,
	MECHANICAL_POSITIONING_ERROR                           = 0x1501,
	MEDIA_LOAD_OR_EJECT_FAILED                             = 0x5300,
	MEDIUM_FORMAT_CORRUPTED                                = 0x3100,
	MEDIUM_NOT_PRESENT                                     = 0x3A00,
	MEDIUM_REMOVAL_PREVENTED                               = 0x5302,
	MESSAGE_ERROR                                          = 0x4300,
	MICROCODE_HAS_BEEN_CHANGED                             = 0x3F01,
	MISCOMPARE_DURING_VERIFY_OPERATION                     = 0x1D00,
	MISCORRECTED_ERROR                                     = 0x110A,
	MODE_PARAMETERS_CHANGED                                = 0x2A01,
	MULTIPLE_PERIPHERAL_DEVICES_SELECTED                   = 0x0700,
	MULTIPLE_READ_ERRORS                                   = 0x1103,
	NO_ADDITIONAL_SENSE_INFORMATION                        = 0x0000,
	NO_DEFECT_SPARE_LOCATION_AVAILABLE                     = 0x3200,
	NO_INDEX_SECTOR_SIGNAL                                 = 0x0100,
	NO_REFERENCE_POSITION_FOUND                            = 0x0600,
	NO_SEEK_COMPLETE                                       = 0x0200,
	NOT_READY_TO_READY_TRANSITION_MEDIUM_MAY_HAVE_CHANGED  = 0x2800,
	OPERATOR_MEDIUM_REMOVAL_REQUEST                        = 0x5A01,
	OPERATOR_REQUEST_OR_STATE_CHANGE_INPUT                 = 0x5A00,
	OPERATOR_SELECTED_WRITE_PERMIT                         = 0x5A03,
	OPERATOR_SELECTED_WRITE_PROTECT                        = 0x5A02,
	OVERLAPPED_COMMANDS_ATTEMPTED                          = 0x4E00,
	PARAMETER_LIST_LENGTH_ERROR                            = 0x1A00,
	PARAMETER_NOT_SUPPORTED                                = 0x2601,
	PARAMETER_VALUE_INVALID                                = 0x2602,
	PARAMETERS_CHANGED                                     = 0x2A00,
	PERIPHERAL_DEVICE_WRITE_FAULT                          = 0x0300,
	POSITIONING_ERROR_DETECTED_BY_READ_OF_MEDIUM           = 0x1502,
	POWER_ON_RESET_OR_BUS_DEVICE_RESET_OCCURRED            = 0x2900,
	POWER_ON_RESET                                         = 0x2901,	
	POWER_ON_OR_SELF_TEST_FAILURE                          = 0x4200,
	PRIMARY_DEFECT_LIST_NOT_FOUND                          = 0x1C01,
	RAM_FAILURE                                            = 0x4000,
	RANDOM_POSITIONING_ERROR                               = 0x1500,
	READ_RETRIES_EXHAUSTED                                 = 0x1101,
	RECORD_NOT_FOUND                                       = 0x1401,
	RECORDED_ENTITY_NOT_FOUND                              = 0x1400,
	RECOVERED_DATA_DATA_AUTO_REALLOCATED                   = 0x1802,
	RECOVERED_DATA_RECOMMEND_REASSIGNMENT                  = 0x1805,
	RECOVERED_DATA_RECOMMEND_REWRITE                       = 0x1806,
	RECOVERED_DATA_USING_PREVIOUS_SECTOR_ID                = 0x1705,
	RECOVERED_DATA_WITH_ERROR_CORRECTION_RETRIES_APPLIED   = 0x1801,
	RECOVERED_DATA_WITH_ERROR_CORRECTION_APPLIED           = 0x1800,
	RECOVERED_DATA_WITH_NEGATIVE_HEAD_OFFSET               = 0x1703,
	RECOVERED_DATA_WITH_NO_ERROR_CORRECTION_APPLIED        = 0x1700,
	RECOVERED_DATA_WITH_POSITIVE_HEAD_OFFSET               = 0x1702,
	RECOVERED_DATA_WITH_RETRIES                            = 0x1701,
	RECOVERED_DATA_WITHOUT_ECC_DATA_AUTO_REALLOCATED       = 0x1706,
	RECOVERED_DATA_WITHOUT_ECC_RECOMMEND_REASSIGNMENT      = 0x1707,
	RECOVERED_DATA_WITHOUT_ECC_RECOMMEND_REWRITE           = 0x1708,
	RECOVERED_ID_WITH_ECC_CORRECTION                       = 0x1E00,
	ROUNDED_PARAMETER                                      = 0x3700,
	RPL_STATUS_CHANGE                                      = 0x5C00,
	SAVING_PARAMETERS_NOT_SUPPORTED                        = 0x3900,
	SCSI_BUS_RESET                                         = 0x2902,
	SCSI_PARITY_ERROR                                      = 0x4700,
	SELECT_OR_RESELECT_FAILURE                             = 0x4500,
	SPINDLES_NOT_SYNCHRONIZED                              = 0x5C02,
	SPINDLES_SYNCHRONIZED                                  = 0x5C01,
	SYNCHRONOUS_DATA_TRANSFER_ERROR                        = 0x1B00,
	TARGET_OPERATING_CONDITIONS_HAVE_CHANGED               = 0x3F00,
	THRESHOLD_CONDITION_MET                                = 0x5B01,
	THRESHOLD_PARAMETERS_NOT_SUPPORTED                     = 0x2603,
	TRACK_FOLLOWING_ERROR                                  = 0x0900,
	UNRECOVERED_READ_ERROR                                 = 0x1100,
	UNRECOVERED_READ_ERROR_AUTO_REALLOCATE_FAILED          = 0x1104,
	UNRECOVERED_READ_ERROR_RECOMMEND_REASSIGNMENT          = 0x110B,
	UNRECOVERED_READ_ERROR_RECOMMEND_REWRITE_THE_DATA      = 0x110C,
	UNSUCCESSFUL_SOFT_RESET                                = 0x4600,
	WRITE_ERROR_AUTO_REALLOCATION_FAILED                   = 0x0C02,
	WRITE_ERROR_RECOVERED_WITH_AUTO_REALLOCATION           = 0x0C01,
	WRITE_PROTECTED                                        = 0x2700
} SCSI_ASC_ASCQ;

typedef struct
{
	uint8 code;
	uint16 asc;
} ScsiSense;

#endif
