#ifndef AUTH_H
#define AUTH_H 1

void signbuf(const char *pvtkey, unsigned char *buf, size_t bufsize, unsigned char **sig,
		size_t *siglen);

int verifysig(const char *pubkey, unsigned char *buf, size_t bufsize, unsigned char *sig,
		size_t *siglen);

#endif /* ifndef AUTH_H */
