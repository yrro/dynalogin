/*
 * hotpdigest.c - HOTP with HTTP-style digest
 */

#include <apr_pools.h>
#include <apr_strings.h>

#include <config.h>

#include "oath.h"
#include "hotpdigest.h"

#include <stdio.h>		/* For snprintf, getline. */
#include <string.h>		/* For strverscmp. */

#include "gc.h"

/**
 * make_hex_string:
 * @in: bytes
 * @out: string (pre-allocated, with space for trailing 0)
 * @len: sizeof(in)
 **/
void
make_hex_string(const char *in, char *out, size_t len)
{
  char *hex_digit = "0123456789abcdef";
  char *p = out;
  size_t c = 0;
  while ( c < len ) {
    *(p++) = hex_digit[(in[c] >> 4) & 0xf];
    *(p++) = hex_digit[in[c] & 0xf];
    c++;
  }
  *p = 0;
}

/**
 * hotp_validate_otp_digest:
 * @secret: the shared secret string
 * @secret_length: length of @secret
 * @start_moving_factor: start counter in OTP stream
 * @window: how many OTPs from start counter to test
 * @digits: the number of digits to use
 * @response: the HTTP Digest response to validate.
 * @username: the username
 * @realm: the realm
 * @digest_suffix: the nonce and a2 (nonce:ha2)
 *
 * Validate an OTP according to OATH HOTP algorithm per RFC 4226
 * and HTTP Digest RFC 2069 or RFC 2617
 *
 * Currently only OTP lengths of 6, 7 or 8 digits are supported.  This
 * restrictions may be lifted in future versions, although some
 * limitations are inherent in the protocol.
 *
 * Returns: Returns position in OTP window (zero is first position),
 *   or %HOTP_INVALID_OTP if no OTP was found in OTP window, or an
 *   error code.
 **/
/* oath_validate_strcmp_function for use by
   oath_hotp_validate_callback */
int oath_digest_callback(void *handle, const char *test_otp)
{
  char *a1;  /* username:realm:password */
  char ha1_raw[GC_MD5_DIGEST_SIZE];  /* H(a1) */
  char ha1_hex[(GC_MD5_DIGEST_SIZE * 2) + 1];
  char *response_arg;   /* H(a1):digest_suffix */
  char _response_raw[GC_MD5_DIGEST_SIZE];
  char _response[(GC_MD5_DIGEST_SIZE * 2) + 1]; /* our calculation of the response */
  char *password = "";

  struct oath_digest_callback_pvt_t *pvt =
    (struct oath_digest_callback_pvt_t *)handle;

  if(pvt->password != NULL)
    password = pvt->password;

      /* Assemble A1 */
      if((a1 = apr_pstrcat(pvt->pool, 
             pvt->username, ":", pvt->realm, ":", password, test_otp, NULL)) == NULL)
        {
          return -1;
        }

      /* Calculate H(A1) */
      if(gc_md5(a1, strlen(a1), ha1_raw) != 0)
        {
          return -1;
        }
      make_hex_string(ha1_raw, ha1_hex, GC_MD5_DIGEST_SIZE);

      /* Assemble argument for calculating response */
      if((response_arg = apr_pstrcat(pvt->pool,
            ha1_hex, ":", pvt->digest_suffix, NULL)) == NULL)
        {
          return -1;
        }

      /* Calculate response */
      if(gc_md5(response_arg, strlen(response_arg), _response_raw) != 0)
        {
          return -1;
        }
      make_hex_string(_response_raw, _response, GC_MD5_DIGEST_SIZE);

      return strcmp (pvt->response, _response);

}

