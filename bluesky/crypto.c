/* Blue Sky: File Systems in the Cloud
 *
 * Copyright (C) 2009  The Regents of the University of California
 * Written by Michael Vrable <mvrable@cs.ucsd.edu>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <glib.h>
#include <string.h>
#include <gcrypt.h>

#include "bluesky-private.h"

/* Cryptographic operations.  The rest of the BlueSky code merely calls into
 * the functions in this file, so this is the only point where we interface
 * with an external cryptographic library. */

/* TODO: We ought to switch to an authenticated encryption mode like EAX. */

GCRY_THREAD_OPTION_PTHREAD_IMPL;

void bluesky_crypt_init()
{
    gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);

    if (gcry_control(GCRYCTL_INITIALIZATION_FINISHED_P))
        return;

    if (!gcry_check_version(GCRYPT_VERSION))
        g_error("libgcrypt version mismatch\n");

    gcry_control(GCRYCTL_DISABLE_SECMEM, 0);
    gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
}

/* Return cryptographically-strong random data. */
void bluesky_crypt_random_bytes(guchar *buf, gint len)
{
    gcry_randomize(buf, len, GCRY_STRONG_RANDOM);
}

/* Hash a string down to an encryption key. */
void bluesky_crypt_hash_key(const char *keystr, uint8_t *out)
{
    guint8 raw_csum[32];
    gsize csum_len = sizeof(raw_csum);

    assert(CRYPTO_KEY_SIZE == 16);

    GChecksum *csum = g_checksum_new(G_CHECKSUM_SHA256);
    g_checksum_update(csum, (const guchar *)keystr, strlen(keystr));
    g_checksum_get_digest(csum, raw_csum, &csum_len);
    g_checksum_free(csum);

    memcpy(out, raw_csum, CRYPTO_KEY_SIZE);
}

/* Compute an HMAC of a given data block with the given key.  The key and
 * output should both as large as the hash output size. */
#define MD_ALGO GCRY_MD_SHA256
void bluesky_crypt_hmac(const char *buf, size_t bufsize,
                        const uint8_t key[CRYPTO_HASH_SIZE],
                        uint8_t output[CRYPTO_HASH_SIZE])
{
    gcry_error_t status;
    gcry_md_hd_t handle;

    g_assert(gcry_md_get_algo_dlen(MD_ALGO) == CRYPTO_HASH_SIZE);

    status = gcry_md_open(&handle, MD_ALGO, GCRY_MD_FLAG_HMAC);
    if (status) {
        g_error("gcrypt error setting up message digest: %s\n",
                gcry_strerror(status));
        g_assert(FALSE);
    }

    status = gcry_md_setkey(handle, key, CRYPTO_HASH_SIZE);
    if (status) {
        g_error("gcrypt error setting HMAC key: %s\n",
                gcry_strerror(status));
        g_assert(FALSE);
    }

    gcry_md_write(handle, buf, bufsize);

    unsigned char *digest = gcry_md_read(handle, MD_ALGO);
    memcpy(output, digest, CRYPTO_HASH_SIZE);

    gcry_md_close(handle);
}

void bluesky_crypt_derive_keys(BlueSkyCryptKeys *keys, const gchar *master)
{
    uint8_t outbuf[CRYPTO_HASH_SIZE];

    const char *key_type = "ENCRYPT";
    bluesky_crypt_hmac(key_type, strlen(key_type),
                       (const uint8_t *)master, outbuf);
    memcpy(keys->encryption_key, outbuf, sizeof(keys->encryption_key));

    key_type = "AUTH";
    bluesky_crypt_hmac(key_type, strlen(key_type),
                       (const uint8_t *)master, outbuf);
    memcpy(keys->authentication_key, outbuf, sizeof(keys->authentication_key));
}

/* A boolean: are blocks of the specified type encrypted in the BlueSky file
 * system? */
gboolean bluesky_crypt_block_needs_encryption(uint8_t type)
{
    type -= '0';

    switch (type) {
    case LOGTYPE_DATA:
    case LOGTYPE_INODE:
        return TRUE;
    case LOGTYPE_INODE_MAP:
    case LOGTYPE_CHECKPOINT:
        return FALSE;
    default:
        g_warning("Unknown log item type in crypto layer: %d!\n",
                  type);
        return TRUE;
    }
}

void bluesky_crypt_block_encrypt(gchar *cloud_block, size_t len,
                                 BlueSkyCryptKeys *keys)
{
    if (bluesky_options.disable_crypto)
        return;

    gcry_error_t status;
    gcry_cipher_hd_t handle;

    status = gcry_cipher_open(&handle, GCRY_CIPHER_AES, GCRY_CIPHER_MODE_CTR,
                              0);
    if (status) {
        g_error("gcrypt error setting up encryption: %s\n",
                gcry_strerror(status));
    }

    struct cloudlog_header *header = (struct cloudlog_header *)cloud_block;
    g_assert(memcmp(header->magic, CLOUDLOG_MAGIC, sizeof(header->magic)) == 0);

    gcry_cipher_setkey(handle, keys->encryption_key, CRYPTO_KEY_SIZE);
    if (status) {
        g_error("gcrypt error setting key: %s\n",
                gcry_strerror(status));
    }

    gboolean encrypted = bluesky_crypt_block_needs_encryption(header->type);

    if (encrypted) {
        header->magic[3] ^= 0x10;

        bluesky_crypt_random_bytes(header->crypt_iv, sizeof(header->crypt_iv));
        status = gcry_cipher_setctr(handle, header->crypt_iv,
                                    sizeof(header->crypt_iv));
        if (status) {
            g_error("gcrypt error setting IV: %s\n",
                    gcry_strerror(status));
        }

        status = gcry_cipher_encrypt(handle,
                                     cloud_block + sizeof(struct cloudlog_header),
                                     GUINT32_FROM_LE(header->size1),
                                     NULL, 0);
        if (status) {
            g_error("gcrypt error encrypting: %s\n",
                    gcry_strerror(status));
        }
    } else {
        memset(header->crypt_iv, 0, sizeof(header->crypt_iv));
    }

    bluesky_crypt_hmac((char *)&header->crypt_iv,
                       cloud_block + len - (char *)&header->crypt_iv - GUINT32_FROM_LE(header->size3),
                       keys->authentication_key,
                       header->crypt_auth);

    gcry_cipher_close(handle);
}

gboolean bluesky_crypt_block_decrypt(gchar *cloud_block, size_t len,
                                     BlueSkyCryptKeys *keys,
                                     gboolean allow_unauth)
{
    gcry_error_t status;
    uint8_t hmac_check[CRYPTO_HASH_SIZE];

    gboolean encrypted = TRUE;

    struct cloudlog_header *header = (struct cloudlog_header *)cloud_block;
    if (memcmp(header->magic, CLOUDLOG_MAGIC,
                    sizeof(header->magic)) == 0)
        encrypted = FALSE;
    else
        g_assert(memcmp(header->magic, CLOUDLOG_MAGIC_ENCRYPTED,
                        sizeof(header->magic)) == 0);

    if (bluesky_options.disable_crypto) {
        g_assert(encrypted == FALSE);
        return TRUE;
    }

    if (encrypted != bluesky_crypt_block_needs_encryption(header->type)) {
        g_warning("Encrypted status of item does not match expected!\n");
    }

    bluesky_crypt_hmac((char *)&header->crypt_iv,
                       cloud_block + len - (char *)&header->crypt_iv - GUINT32_FROM_LE(header->size3),
                       keys->authentication_key,
                       hmac_check);
    if (memcmp(hmac_check, header->crypt_auth, CRYPTO_HASH_SIZE) != 0) {
        g_warning("Cloud block HMAC does not match!");
        if (allow_unauth
            && (header->type == LOGTYPE_INODE_MAP + '0'
                || header->type == LOGTYPE_CHECKPOINT + '0'))
        {
            g_warning("Allowing unauthenticated data from cleaner");
        } else {
            return FALSE;
        }
    }

    if (encrypted) {
        gcry_cipher_hd_t handle;
        status = gcry_cipher_open(&handle, GCRY_CIPHER_AES,
                                  GCRY_CIPHER_MODE_CTR, 0);
        if (status) {
            g_error("gcrypt error setting up encryption: %s\n",
                    gcry_strerror(status));
        }

        gcry_cipher_setkey(handle, keys->encryption_key, CRYPTO_KEY_SIZE);
        if (status) {
            g_error("gcrypt error setting key: %s\n",
                    gcry_strerror(status));
        }

        status = gcry_cipher_setctr(handle, header->crypt_iv,
                                    sizeof(header->crypt_iv));
        if (status) {
            g_error("gcrypt error setting IV: %s\n",
                    gcry_strerror(status));
        }

        status = gcry_cipher_decrypt(handle,
                                     cloud_block + sizeof(struct cloudlog_header),
                                     GUINT32_FROM_LE(header->size1),
                                     NULL, 0);
        if (status) {
            g_error("gcrypt error decrypting: %s\n",
                    gcry_strerror(status));
        }
        header->magic[3] ^= 0x10;
        memset(header->crypt_iv, 0, sizeof(header->crypt_iv));

        gcry_cipher_close(handle);
    }

    return TRUE;
}
