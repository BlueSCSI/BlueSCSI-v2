/*
 * Copyright (c) 2023 joshua stein <jcs@jcs.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <string.h>

#include "scsi.h"
#include "scsi2sd_time.h"
#include "scsiPhy.h"
#include "config.h"

#ifdef BLUESCSI_TLS

#include "tls.h"
#include "tls-anchors.h"

struct tls_context_entry tls_contexts[TLS_CONTEXT_COUNT] = { 0 };
extern const br_x509_trust_anchor TAs[];

// Private x509 decoder state
struct br_x509_insecure_context {
	const br_x509_class *vtable;
	bool done_cert;
	const uint8_t *match_fingerprint;
	br_sha1_context sha1_cert;
	bool allow_self_signed;
	br_sha256_context sha256_subject;
	br_sha256_context sha256_issuer;
	br_x509_decoder_context ctx;
} x509_insecure;

void br_x509_insecure_init(struct br_x509_insecure_context *ctx);

void scsiTLSInit()
{
	log_f("Initializing TLS");

	memset(&tls_contexts, 0, sizeof(tls_contexts));
}

int scsiTLSCommand()
{
	int handled = 1;
	int parityError = 0;
	uint32_t size = scsiDev.cdb[4] + (scsiDev.cdb[3] << 8);
	uint8_t command = scsiDev.cdb[0];
	uint8_t tls_id;

	DBGMSG_F("------ in scsiTLSCommand with command 0x%02x (size %d)", command, size);

	switch (command) {
	case SCSI_TLS_CMD:
		tls_id = scsiDev.cdb[2];

		switch (scsiDev.cdb[1]) {
		case SCSI_TLS_CMD_INIT: {
			// create a new tls context
			struct tls_init_request req = { 0 };
			struct tls_context_entry *tce = NULL;

			scsiEnterPhase(DATA_OUT);
			parityError = 0;
			scsiRead((uint8_t *)&req, sizeof(req), &parityError);
			req.hostname[sizeof(req.hostname) - 1] = '\0';

			for (int i = 0; i < TLS_CONTEXT_COUNT; i++)
			{
				if (tls_contexts[i].id == tls_id || tls_contexts[i].id == 0) {
					memset(&tls_contexts[i], 0, sizeof(tls_contexts[i]));
					tls_contexts[i].id = tls_id;
					tce = &tls_contexts[i];
					break;
				}
			}

			if (tce == NULL)
			{
				log_f("SCSI_TLS_CMD_INIT[%d] with no free slots, taking first", tls_id);
				memset(&tls_contexts[0], 0, sizeof(tls_contexts[0]));
				tls_contexts[0].id = tls_id;
				tce = &tls_contexts[0];
			}

			uint16_t flags = (req.flags[0] << 8) + req.flags[1];
			uint32_t now = (req.unix_time[0] << 24) + (req.unix_time[1] << 16) + (req.unix_time[2] << 8) + (req.unix_time[3]);
			log_f("SCSI_TLS_CMD_INIT[%d] for \"%s\" with now %lu, flags %s", tls_id, req.hostname, now, (flags & TLS_INIT_REQUEST_FLAG_NO_VERIFY ? "INSECURE" : ""));

			br_ssl_client_init_full(&tce->sc, &tce->xc, TAs, TAs_NUM);

			if (flags & TLS_INIT_REQUEST_FLAG_NO_VERIFY) {
				br_x509_insecure_init(&x509_insecure);
    			br_ssl_engine_set_x509(&tce->sc.eng, &x509_insecure.vtable);
			}

			br_ssl_engine_set_buffer(&tce->sc.eng, &tce->iobuf, sizeof(tce->iobuf), 1);
			br_ssl_engine_add_flags(&tce->sc.eng, BR_OPT_TOLERATE_NO_CLIENT_AUTH);
			br_ssl_client_reset(&tce->sc, req.hostname, 0);

			uint32_t days = (now / 86400) + 719528;
			uint32_t sec = now % 86400;

			log_f("xc days %d seconds %d itime %d", tce->xc.days, tce->xc.seconds, tce->xc.itime);
			br_x509_minimal_set_time(&tce->xc, days, sec);
			log_f("now xc days %d seconds %d itime %d", tce->xc.days, tce->xc.seconds, tce->xc.itime);

			scsiDev.status = GOOD;
			scsiDev.phase = STATUS;
			break;
		}
		case SCSI_TLS_CMD_STATUS: {
			struct tls_context_entry *tce = NULL;
			unsigned int status;
			size_t avail;

			for (int i = 0; i < TLS_CONTEXT_COUNT; i++)
			{
				if (tls_contexts[i].id == tls_id)
				{
					tce = &tls_contexts[i];
					break;
				}
			}

			if (tce == NULL)
			{
				log_f("SCSI_TLS_CMD_STATUS[%d] with no context?", tls_id);
				scsiDev.status = CHECK_CONDITION;
				scsiDev.phase = STATUS;
				break;
			}

			status = br_ssl_engine_current_state(&tce->sc.eng);

			scsiDev.dataLen = 8;

			// status bits
			scsiDev.data[0] = (status >> 8) & 0xff;
			scsiDev.data[1] = status & 0xff;
			
			if (status & BR_SSL_CLOSED)
			{
				int error = br_ssl_engine_last_error(&tce->sc.eng);

				log_f("SCSI_TLS_CMD_STATUS[%d] closed, error %d", tls_id, error);
				scsiDev.data[6] = (error >> 8) & 0xff;
				scsiDev.data[7] = error & 0xff;
			}
			else
			{
				// size of ciphertext the client can write
				if (status & BR_SSL_RECVREC) {
					br_ssl_engine_recvrec_buf(&tce->sc.eng, &avail);
					scsiDev.data[2] = (avail >> 8) & 0xff;
					scsiDev.data[3] = avail & 0xff;
				}

				// size of plaintext the client can write
				if (status & BR_SSL_SENDAPP) {
					br_ssl_engine_sendapp_buf(&tce->sc.eng, &avail);
					scsiDev.data[4] = (avail >> 8) & 0xff;
					scsiDev.data[5] = avail & 0xff;
				}

				scsiDev.data[6] = 0;
				scsiDev.data[7] = 0;
			}

			log_f("SCSI_TLS_CMD_STATUS[%d] status 0x%02x - %02x %02x %02x %02x %02x %02x %02x %02x",
			  tls_id, status, scsiDev.data[0], scsiDev.data[1], scsiDev.data[2], scsiDev.data[3], scsiDev.data[4],
			  scsiDev.data[5], scsiDev.data[6], scsiDev.data[7]);

			scsiEnterPhase(DATA_IN);
			scsiWrite(scsiDev.data, scsiDev.dataLen);
			while (!scsiIsWriteFinished(NULL))
			{
				platform_poll();
			}
			scsiFinishWrite();

			scsiDev.status = GOOD;
			scsiDev.phase = STATUS;
			break;
		}
		case SCSI_TLS_CMD_READ_PLAIN:
		case SCSI_TLS_CMD_READ_CIPHER: {
			// read plaintext or ciphertext from a tls context
			struct tls_context_entry *tce = NULL;
			unsigned char *buf = NULL;
			int status;
			size_t avail = 0;

			for (int i = 0; i < TLS_CONTEXT_COUNT; i++)
			{
				if (tls_contexts[i].id == tls_id)
				{
					tce = &tls_contexts[i];
					break;
				}
			}

			if (tce == NULL)
			{
				log_f("SCSI_TLS_CMD_READ[%d] no context?", tls_id);
				scsiDev.status = CHECK_CONDITION;
				scsiDev.phase = STATUS;
				break;
			}

			status = br_ssl_engine_current_state(&tce->sc.eng);

			if (status & BR_SSL_CLOSED)
			{
				log_f("SCSI_TLS_CMD_READ[%d] closed, error %d", tls_id, br_ssl_engine_last_error(&tce->sc.eng));
			}
			else if (scsiDev.cdb[1] == SCSI_TLS_CMD_READ_PLAIN)
			{
				// "recvapp: obtain some application data (plaintext) from the engine."
				if (status & BR_SSL_RECVAPP)
				{
					buf = br_ssl_engine_recvapp_buf(&tce->sc.eng, &avail);
					log_f("SCSI_TLS_CMD_READ_PLAIN[%d] %zu available", tls_id, avail);
				}
				else
				{
					log_f("SCSI_TLS_CMD_READ_PLAIN[%d] nothing available", tls_id);
				}
			}
			else if (scsiDev.cdb[1] == SCSI_TLS_CMD_READ_CIPHER)
			{
				// "sendrec: get records from the engine, to be conveyed to the peer."
				if (status & BR_SSL_SENDREC)
				{
					buf = br_ssl_engine_sendrec_buf(&tce->sc.eng, &avail);
					log_f("SCSI_TLS_CMD_READ_CIPHER[%d] %zu available", tls_id, avail);
				}
				else
				{
					log_f("SCSI_TLS_CMD_READ_CIPHER[%d] nothing available", tls_id);
				}
			}
			else
			{
				scsiDev.status = CHECK_CONDITION;
				scsiDev.phase = STATUS;
				break;
			}

			if (avail > size)
			{
				log_f("SCSI_TLS_CMD_READ[%d] clamping avail from %zu to user's buf size %zu", tls_id, avail, size);
				avail = size;
			}
				
			if (avail > sizeof(scsiDev.data) - 2)
			{
				log_f("SCSI_TLS_CMD_READ[%d] clamping avail from %zu to our buf size %zu", tls_id, avail, sizeof(scsiDev.data) - 2);
				avail = sizeof(scsiDev.data) - 2;
			}

			scsiDev.dataLen = 2;
			scsiDev.data[0] = (avail >> 8) & 0xff;
			scsiDev.data[1] = avail & 0xff;

			// send length, then wait and then send data
			scsiEnterPhase(DATA_IN);
			scsiWrite(scsiDev.data, 2);
			while (!scsiIsWriteFinished(NULL))
			{
				platform_poll();
			}
			scsiFinishWrite();

			if (avail > 0) {
				// then pause
				s2s_delay_us(80);

				log_f("sending data of length %zu: ", avail);
				//log_buf(buf, avail);

				// then send the actual data
				scsiWrite(buf, avail);
				while (!scsiIsWriteFinished(NULL))
				{
					platform_poll();
				}
				scsiFinishWrite();
			}

			scsiDev.status = GOOD;
			scsiDev.phase = STATUS;

			if (avail)
			{
				if (scsiDev.cdb[1] == SCSI_TLS_CMD_READ_PLAIN)
				{
					br_ssl_engine_recvapp_ack(&tce->sc.eng, avail);
				}
				else if (scsiDev.cdb[1] == SCSI_TLS_CMD_READ_CIPHER)
				{
					br_ssl_engine_sendrec_ack(&tce->sc.eng, avail);
				}
			}

			break;
		}
		case SCSI_TLS_CMD_WRITE_PLAIN:
		case SCSI_TLS_CMD_WRITE_CIPHER: {
			// write plaintext or ciphertext to a tls context
			struct tls_context_entry *tce = NULL;
			unsigned char *buf, *dataoff;
			size_t avail, wsize;

			for (int i = 0; i < TLS_CONTEXT_COUNT; i++)
			{
				if (tls_contexts[i].id == tls_id)
				{
					tce = &tls_contexts[i];
					break;
				}
			}

			if (tce == NULL)
			{
				log_f("SCSI_TLS_CMD_WRITE[%d] with no context?", tls_id);
				scsiDev.status = CHECK_CONDITION;
				scsiDev.phase = STATUS;
				break;
			}

			if (br_ssl_engine_current_state(&tce->sc.eng) & BR_SSL_CLOSED)
			{
				log_f("SCSI_TLS_CMD_WRITE[%d] closed, error %d", tls_id, br_ssl_engine_last_error(&tce->sc.eng));
				scsiDev.status = CHECK_CONDITION;
				scsiDev.phase = STATUS;
				break;
			}

			if (scsiDev.cdb[1] == SCSI_TLS_CMD_WRITE_PLAIN)
			{
				// "sendapp: push some application data (plaintext) into the engine."
				buf = br_ssl_engine_sendapp_buf(&tce->sc.eng, &avail);
				if (buf == NULL || avail == 0)
				{
					log_f("SCSI_TLS_CMD_WRITE_PLAIN[%d] no space available for %zu", tls_id, size);
					scsiDev.status = BUSY;
					scsiDev.phase = STATUS;
					break;
				}
				log_f("SCSI_TLS_CMD_WRITE_PLAIN[%d] %zu available, need to write %zu", tls_id, avail, size);
			}
			else if (scsiDev.cdb[1] == SCSI_TLS_CMD_WRITE_CIPHER)
			{
				// "recvrec: inject records freshly obtained from the peer, into the engine."
				buf = br_ssl_engine_recvrec_buf(&tce->sc.eng, &avail);
				if (buf == NULL || avail == 0)
				{
					log_f("SCSI_TLS_CMD_WRITE_CIPHER[%d] no space available for %zu", tls_id, size);
					scsiDev.status = BUSY;
					scsiDev.phase = STATUS;
					break;
				}
				log_f("SCSI_TLS_CMD_WRITE_CIPHER[%d] %zu available, need to write %zu", tls_id, avail, size);
			}
			else
			{
				scsiDev.status = CHECK_CONDITION;
				scsiDev.phase = STATUS;
				break;
			}

			scsiEnterPhase(DATA_OUT);
			parityError = 0;
			scsiRead(scsiDev.data, size, &parityError);
			
			//log_f("read data of length %zu: ", size);
			//log_buf(scsiDev.data, size);

			wsize = size;
			dataoff = scsiDev.data;

			while (wsize > 0)
			{
				if (scsiDev.cdb[1] == SCSI_TLS_CMD_WRITE_PLAIN)
				{
					buf = br_ssl_engine_sendapp_buf(&tce->sc.eng, &avail);
				}
				else if (scsiDev.cdb[1] == SCSI_TLS_CMD_WRITE_CIPHER)
				{
					buf = br_ssl_engine_recvrec_buf(&tce->sc.eng, &avail);
				}

				if (buf == NULL || avail == 0)
				{
					log_f("XXX: accepted write data but have no buffer space available to send remaining %zu bytes of %zu", wsize, size);
					log_f("ssl engine status 0x%x", br_ssl_engine_current_state(&tce->sc.eng));
					scsiDev.status = CHECK_CONDITION;
					scsiDev.phase = STATUS;
					break;
				}

				if (avail > wsize)
					avail = wsize;

				memcpy(buf, dataoff, avail);
				wsize -= avail;
				dataoff += avail;

				if (scsiDev.cdb[1] == SCSI_TLS_CMD_WRITE_PLAIN)
				{
					log_f("SCSI_TLS_CMD_WRITE_PLAIN[%d] acked %zu bytes, %zu left", tls_id, avail, wsize);
					br_ssl_engine_sendapp_ack(&tce->sc.eng, avail);
				}
				else if (scsiDev.cdb[1] == SCSI_TLS_CMD_WRITE_CIPHER)
				{
					log_f("SCSI_TLS_CMD_WRITE_CIPHER[%d] acked %zu bytes, %zu left", tls_id, avail, wsize);
					br_ssl_engine_recvrec_ack(&tce->sc.eng, avail);
				}
			}

			if (scsiDev.cdb[1] == SCSI_TLS_CMD_WRITE_PLAIN)
			{
				log_f("SCSI_TLS_CMD_WRITE_PLAIN[%d] flushing buffer", tls_id);
				br_ssl_engine_flush(&tce->sc.eng, 0);
			}
			break;
		}
		case SCSI_TLS_CMD_CLOSE: {
			// close a tls context
			bool found = false;

			for (int i = 0; i < TLS_CONTEXT_COUNT; i++)
			{
				if (tls_contexts[i].id == tls_id)
				{
					br_ssl_engine_close(&tls_contexts[i].sc.eng);
					found = true;
					break;
				}
			}

			if (!found)
			{
				log_f("SCSI_TLS_CMD_CLOSE[%d] no context", tls_id);
			}

			log_f("SCSI_TLS_CMD_CLOSE[%d]", tls_id);

			scsiDev.status = GOOD;
			scsiDev.phase = STATUS;
			break;
		}
		}
		break;

	default:
		handled = 0;
	}


	return handled;
}

// BearSSL doesn't define a true insecure decoder, so we make one ourselves
// from the simple parser.  It generates the issuer and subject hashes and
// the SHA1 fingerprint, only one (or none!) of which will be used to
// "verify" the certificate.


// Callback for the x509_minimal subject DN
static void insecure_subject_dn_append(void *ctx, const void *buf, size_t len)
{
	struct br_x509_insecure_context *xc = (struct br_x509_insecure_context *)ctx;
	br_sha256_update(&xc->sha256_subject, buf, len);
}

// Callback for the x509_minimal issuer DN
static void insecure_issuer_dn_append(void *ctx, const void *buf, size_t len)
{
	struct br_x509_insecure_context *xc = (struct br_x509_insecure_context *)ctx;
	br_sha256_update(&xc->sha256_issuer, buf, len);
}

// Callback on the first byte of any certificate
static void insecure_start_chain(const br_x509_class **ctx, const char *server_name)
{
	struct br_x509_insecure_context *xc = (struct br_x509_insecure_context *)ctx;
	br_x509_decoder_init(&xc->ctx, insecure_subject_dn_append, xc, insecure_issuer_dn_append, xc);
	xc->done_cert = false;
	br_sha1_init(&xc->sha1_cert);
	br_sha256_init(&xc->sha256_subject);
	br_sha256_init(&xc->sha256_issuer);
	(void)server_name;
}

// Callback for each certificate present in the chain (but only operates
// on the first one by design).
static void insecure_start_cert(const br_x509_class **ctx, uint32_t length) {
	(void) ctx;
	(void) length;
}

// Callback for each byte stream in the chain.  Only process first cert.
static void insecure_append(const br_x509_class **ctx, const unsigned char *buf, size_t len)
{
	struct br_x509_insecure_context *xc = (struct br_x509_insecure_context *)ctx;
	// Don't process anything but the first certificate in the chain
	if (!xc->done_cert) {
		br_sha1_update(&xc->sha1_cert, buf, len);
		br_x509_decoder_push(&xc->ctx, (const void*)buf, len);
	}
}

// Callback on individual cert end.
static void insecure_end_cert(const br_x509_class **ctx) {
	struct br_x509_insecure_context *xc = (struct br_x509_insecure_context *)ctx;
	xc->done_cert = true;
}

// Callback when complete chain has been parsed.
// Return 0 on validation success, !0 on validation error
static unsigned insecure_end_chain(const br_x509_class **ctx)
{
	char res[20];

	const struct br_x509_insecure_context *xc = (const struct br_x509_insecure_context *)ctx;
	if (!xc->done_cert)
	{
		log_f("insecure_end_chain: no certificate seen");
		return 1; // error
	}

	// Handle SHA1 fingerprint matching
	br_sha1_out(&xc->sha1_cert, res);
	if (xc->match_fingerprint && memcmp(res, xc->match_fingerprint, sizeof(res)))
	{
		log_f("insecure_end_chain: fingerprint match failed");
		return BR_ERR_X509_NOT_TRUSTED;
	}

	// Default (no validation at all) or no errors in prior checks = success.
	return 0;
}

// Return the public key from the validator (set by x509_minimal)
static const br_x509_pkey *insecure_get_pkey(const br_x509_class *const *ctx, unsigned *usages)
{
    const struct br_x509_insecure_context *xc = (const struct br_x509_insecure_context *)ctx;
    if (usages != NULL) {
      *usages = BR_KEYTYPE_KEYX | BR_KEYTYPE_SIGN; // I said we were insecure!
    }
    return &xc->ctx.pkey;
}

//  Set up the x509 insecure data structures for BearSSL core to use.
void br_x509_insecure_init(struct br_x509_insecure_context *ctx)
{
    static const br_x509_class br_x509_insecure_vtable = {
		sizeof(struct br_x509_insecure_context),
		insecure_start_chain,
		insecure_start_cert,
		insecure_append,
		insecure_end_cert,
		insecure_end_chain,
		insecure_get_pkey
    };

    memset(ctx, 0, sizeof * ctx);
    ctx->vtable = &br_x509_insecure_vtable;
    ctx->done_cert = false;
    ctx->allow_self_signed = 1;
}

#endif
