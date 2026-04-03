/**
 *	\file		nmjson_str.h
 *	\brief		指定長文字列を表現するためのもの
 */

#ifndef	NMJSON_NMJSON_STR_H_
#define	NMJSON_NMJSON_STR_H_

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

/**
 *	\brief	文字列表現。キーや文字列などで\0を含む可能性がある。ので、
 *			その時のために「指定長文字列」を扱う器を用意する。
 */
typedef struct {
	size_t			len;
	const char*		s;
} nmjson_str_t;

/**
 *	\brief		文字列を入力する
 *	\return	文字列長
 */
static inline int nmjson_str_init(nmjson_str_t *self, const char *s){
	self->s = s;
	self->len = (s != NULL) ? strlen(s) : 0;
	return self->len;
}

/**
 *	\brief		文字列「リテラル」で初期化する
 *	\remarks	nmjson_str_t nstr = NMJSON_STR_LITERAL("test"); で記述。
 *	\warning	C99以降専用らしい。
 */
#define	NMJSON_STR_LITERAL(lit) (nmjson_str_t) {\
	.len = sizeof(lit) - 1,\
	.s = lit \
}

#ifdef	__cplusplus
}
#endif	/* __cplusplus */


#endif	/* !NMJSON_NMJSON_STR_H_ */
