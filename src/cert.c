#include <openssl/ssl.h>
#include <openssl/err.h>
#include <assert.h>

#include "h/struct.h"
#include "h/proto.h"

/**
Password for out certificate.
*/
static const char psswd[] = "password";

/**
Certificate stored in memory.
*/
static const char xxx[] =
	"-----BEGIN CERTIFICATE-----\n"
	"MIICGDCCAYECAgEBMA0GCSqGSIb3DQEBBAUAMFcxCzAJBgNVBAYTAlVTMRMwEQYD\n"
	"VQQKEwpSVEZNLCBJbmMuMRkwFwYDVQQLExBXaWRnZXRzIERpdmlzaW9uMRgwFgYD\n"
	"VQQDEw9UZXN0IENBMjAwMTA1MTcwHhcNMDEwNTE3MTYxMDU5WhcNMDQwMzA2MTYx\n"
	"MDU5WjBRMQswCQYDVQQGEwJVUzETMBEGA1UEChMKUlRGTSwgSW5jLjEZMBcGA1UE\n"
	"CxMQV2lkZ2V0cyBEaXZpc2lvbjESMBAGA1UEAxMJbG9jYWxob3N0MIGfMA0GCSqG\n"
	"SIb3DQEBAQUAA4GNADCBiQKBgQCiWhMjNOPlPLNW4DJFBiL2fFEIkHuRor0pKw25\n"
	"J0ZYHW93lHQ4yxA6afQr99ayRjMY0D26pH41f0qjDgO4OXskBsaYOFzapSZtQMbT\n"
	"97OCZ7aHtK8z0ZGNW/cslu+1oOLomgRxJomIFgW1RyUUkQP1n0hemtUdCLOLlO7Q\n"
	"CPqZLQIDAQABMA0GCSqGSIb3DQEBBAUAA4GBAIumUwl1OoWuyN2xfoBHYAs+lRLY\n"
	"KmFLoI5+iMcGxWIsksmA+b0FLRAN43wmhPnums8eXgYbDCrKLv2xWcvKDP3mps7m\n"
	"AMivwtu/eFpYz6J8Mo1fsV4Ys08A/uPXkT23jyKo2hMu8mywkqXCXYF2e+7pEeBr\n"
	"dsbmkWK5NgoMl8eM\n"
	"-----END CERTIFICATE-----"
	"-----BEGIN RSA PRIVATE KEY-----\n"
	"Proc-Type: 4,ENCRYPTED\n"
	"DEK-Info: DES-EDE3-CBC,5772A2A7BE34B611\n"
	"\n"
	"1yJ+xAn4MudcIfXXy7ElYngJ9EohIh8yvcyVLmE4kVd0xeaL/Bqhvk25BjYCK5d9\n"
	"k1K8cjgnKEBjbC++0xtJxFSbUhwoKTLwn+sBoJDcFzMKkmJXXDbSTOaNr1sVwiAR\n"
	"SnB4lhUcHguYoV5zlRJn53ft7t1mjB6RwGH+d1Zx6t95OqM1lnKqwekwmotVAWHj\n"
	"ncu3N8qhmoPMppmzEv0fOo2/pK2WohcJykSeN5zBrZCUxoO0NBNEZkFUcVjR+KsA\n"
	"1ZeI1mU60szqg+AoU/XtFcow8RtG1QZKQbbXzyfbwaG+6LqkHaWYKHQEI1546yWK\n"
	"us1HJ734uUkZoyyyazG6PiGCYV2u/aY0i3qdmyDqTvmVIvve7E4glBrtDS9h7D40\n"
	"nPShIvOatoPzIK4Y0QSvrI3G1vTsIZT3IOZto4AWuOkLNfYS2ce7prOreF0KjhV0\n"
	"3tggw9pHdDmTjHTiIkXqheZxZ7TVu+pddZW+CuB62I8lCBGPW7os1f21e3eOD/oY\n"
	"YPCI44aJvgP+zUORuZBWqaSJ0AAIuVW9S83Yzkz/tlSFHViOebyd8Cug4TlxK1VI\n"
	"q6hbSafh4C8ma7YzlvqjMzqFifcIolcbx+1A6ot0UiayJTUra4d6Uc4Rbc9RIiG0\n"
	"jfDWC6aii9YkAgRl9WqSd31yASge/HDqVXFwR48qdlYQ57rcHviqxyrwRDnfw/lX\n"
	"Mf6LPiDKEco4MKej7SR2kK2c2AgxUzpGZeAY6ePyhxbdhA0eY21nDeFd/RbwSc5s\n"
	"eTiCCMr41OB4hfBFXKDKqsM3K7klhoz6D5WsgE6u3lDoTdz76xOSTg==\n"
	"-----END RSA PRIVATE KEY-----\n"
	"";

static BIO *bio_err = NULL;
static SSL_CTX *ctx = NULL;

/**
Helper function for "reading" of the password.
Useful when some wants to allow user
to enter password by hand.

Does nothing interesting in our case, simply copies password.
*/
static int password_cb(char *buf, int size, int rwflag, void *password) {
	strncpy(buf, (char *)(password), size);
	buf[size - 1] = 0;
	return(strlen(buf));
}

/**
Create BIO output file handler for standard error output.
*/
static int berr_exit(const char *string) {
	BIO_printf(bio_err, "%s\n", string);
	ERR_print_errors(bio_err);
	exit(1);
}

/**
Returns valid SSL context.
When call for the first time, then initialize SSL and the context itself.
*/
SSL_CTX *mossad(void) {
	if (ctx) {
		return ctx;
	}
	if(!bio_err){
		/* Global system initialization*/
		SSL_library_init();
		SSL_load_error_strings();

		/* An error write context */
		bio_err = BIO_new_fp(stderr, BIO_NOCLOSE);
	}

	/* Create our context*/
	const SSL_METHOD *meth = SSLv23_method();
	ctx = SSL_CTX_new(meth);

	X509 *cert = NULL;
	RSA *rsa = NULL;
	BIO *cbio, *kbio;
	
	cbio = BIO_new_mem_buf((void*)xxx, sizeof(cert));
	cert = PEM_read_bio_X509(cbio, NULL, password_cb, (void*)psswd);
	if (cert != NULL) {
		berr_exit("Can't read certificate from memory");
	}
	SSL_CTX_use_certificate(ctx, cert);

	kbio = BIO_new_mem_buf((void*)xxx, -1);
	rsa = PEM_read_bio_RSAPrivateKey(kbio, NULL, password_cb, (void*)psswd);
	if (rsa != NULL) {
		berr_exit("Can't read key from memory");
	}
	SSL_CTX_use_RSAPrivateKey(ctx, rsa);
	return ctx;
}

/**
Free SSL context. After this function is called, SSL should not be in used.
*/
void free_mossad(void) {
	SSL_CTX_free(ctx);
	ctx = NULL;
}