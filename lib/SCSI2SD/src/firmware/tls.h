#ifndef __NETWORK_TLS_H__
#define __NETWORK_TLS_H__ 1

#include <bearssl/bearssl.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCSI_TLS_CMD				0x1d
#define SCSI_TLS_CMD_INIT			0x01
#define SCSI_TLS_CMD_STATUS			0x02
#define SCSI_TLS_CMD_READ_PLAIN		0x03
#define SCSI_TLS_CMD_WRITE_PLAIN	0x04
#define SCSI_TLS_CMD_READ_CIPHER	0x05
#define SCSI_TLS_CMD_WRITE_CIPHER	0x06
#define SCSI_TLS_CMD_CLOSE			0x07

struct __attribute__((packed)) tls_init_request {
	uint8_t flags[2];
#define TLS_INIT_REQUEST_FLAG_NO_VERIFY		(1 << 0)
	uint8_t unix_time[4];
	char hostname[256];
};

struct tls_context_entry {
	uint16_t id;
	br_sslio_context ioc;
	br_ssl_client_context sc;
	br_x509_minimal_context xc;
	unsigned char iobuf[BR_SSL_BUFSIZE_BIDI];
};
#define TLS_CONTEXT_COUNT 1
extern struct tls_context_entry tls_contexts[TLS_CONTEXT_COUNT];

void scsiTLSInit(void);
int scsiTLSCommand(void);

#ifdef __cplusplus
}
#endif

#endif
