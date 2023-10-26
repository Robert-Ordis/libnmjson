#ifndef	NMJSON_UTF8_H_
#define	NMJSON_UTF8_H_

#include <inttypes.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


ssize_t	utf8_copy_decode(char *dst, const char *src, size_t src_len, uint32_t *p_hi_code);

ssize_t	utf8_copy_decode_surrogate(char *dst, const char *src, uint32_t hi_code);

ssize_t	utf8_str_to_unicode(const char *src, uint32_t *p_unicode);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* !NMJSON_UTF8_H_ */
