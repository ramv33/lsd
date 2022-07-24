#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include "auth.h"

int signbuf(const char *keyfile, unsigned char *buf, size_t bufsize,
		unsigned char **sig, size_t *siglen)
{
	FILE *keyfp;
	EC_KEY *ec_key;
	EVP_PKEY *key;

	*siglen = 0;
	if ((keyfp = fopen(keyfile, "r")) == NULL) {
		fprintf(stderr, "error opening private key '%s' for signing: %s\n",
			keyfile, strerror(errno));
		return -1;
	}
	ec_key = PEM_read_ECPrivateKey(keyfp, NULL, NULL, NULL);
	if (!ec_key) {
		ERR_print_errors_fp(stderr);
		return -1;
	}
	fclose(keyfp);

	assert(EC_KEY_check_key(ec_key) == 1);
	key = EVP_PKEY_new();
	assert (EVP_PKEY_assign_EC_KEY(key, ec_key) == 1); 
	EVP_PKEY_CTX *key_ctx = EVP_PKEY_CTX_new(key, NULL);
	assert(EVP_PKEY_sign_init(key_ctx) == 1);
	assert(EVP_PKEY_CTX_set_signature_md(key_ctx, EVP_sha256()));
	assert(EVP_PKEY_sign(key_ctx, NULL, siglen, buf, bufsize) == 1);
	assert(EVP_PKEY_sign(key_ctx, (unsigned char *)sig, siglen, buf, bufsize));

	EVP_PKEY_CTX_free(key_ctx);
	EVP_PKEY_free(key);

	return 0;
}

int verifysig(const char *pubkey, unsigned char *buf, size_t bufsize,
		unsigned char *sig, size_t *siglen)
{
	FILE *fp;
	EC_KEY *ec_key;

	if ((fp = fopen(pubkey, "r")) == NULL) {
		fprintf(stderr, "error opening public key '%s' for verification: %s\n",
			pubkey, strerror(errno));
		return 0;
	}
	printf("opened public key '%s' successfully\n", pubkey);
	ec_key = PEM_read_EC_PUBKEY(fp, NULL, NULL, NULL);
	fclose(fp);

	EVP_PKEY *key = EVP_PKEY_new();
	assert(EVP_PKEY_assign_EC_KEY(key, ec_key) == 1);

	EVP_PKEY_CTX *key_ctx = EVP_PKEY_CTX_new(key, NULL);
	assert(EVP_PKEY_verify_init(key_ctx) == 1);
	assert(EVP_PKEY_CTX_set_signature_md(key_ctx, EVP_sha256()));
	const int ret = EVP_PKEY_verify(key_ctx, sig, *siglen, buf, bufsize);
	EVP_PKEY_CTX_free(key_ctx);
	EVP_PKEY_free(key);

	return ret;
}
