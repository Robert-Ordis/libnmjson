/**
 *	\file		nmjson_numfmt_strtod_.c
 *	\brief		ロケール非依存の strtod
 *	\remarks	strtod(3)は LC_NUMERIC に従って小数点を解釈するため、
 *				アプリ側が setlocale(LC_ALL, "de_DE") 等をしていると
 *				"0.5" を読めない / "1,5" を1.5と読んでJSONの区切りを食う、という問題が起きる。
 *
 *				- POSIX 2008 のロケールAPI(uselocale)がある場合:
 *				  変換中だけ「そのスレッドの」ロケールをCに切り替える。長さ制限なし。
 *				- ない場合(NMJSON_STRTOD_NO_USELOCALE 定義時も同様):
 *				  数値部分をスタックにコピーし、'.'を現在ロケールの小数点に置き換えてから変換する。
 *				  NMJSON_STRTOD_BUF_ バイトを超える数値は、現在ロケールの小数点が"."の場合のみ直接変換し、
 *				  それ以外は変換失敗(*endptr == s)とする。
 */
#include <errno.h>
#include <locale.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "include/nmjson/json_token.h"

#if !defined(NMJSON_STRTOD_NO_USELOCALE) && defined(LC_ALL_MASK)
#define	NMJSON_STRTOD_USE_USELOCALE_
#endif

#if defined(NMJSON_STRTOD_USE_USELOCALE_)

double nmjson_strtod(const char *s, char **endptr){
	// "C"ロケールオブジェクトは生成後ずっと使い回す。
	// (glibcでは"C"指定のnewlocaleは静的オブジェクトを返すため、確保は発生しない)
	static locale_t c_locale = (locale_t)0;
	locale_t prev;
	double ret;
	int err;

	if(c_locale == (locale_t)0){
		c_locale = newlocale(LC_ALL_MASK, "C", (locale_t)0);
		if(c_locale == (locale_t)0){
			if(endptr != NULL){ *endptr = (char *)s; }
			return 0.0;
		}
	}
	prev = uselocale(c_locale);
	errno = 0;
	ret = strtod(s, endptr);
	err = errno;
	uselocale(prev);
	errno = err;	// ERANGE判定を呼び出し元に残す
	return ret;
}

#else	/* !NMJSON_STRTOD_USE_USELOCALE_ */

/// \brief	スタックへコピーして変換できる数値の最大長
#define	NMJSON_STRTOD_BUF_	128

/**
 *	\brief		s から始まる「10進数値っぽい部分」の長さを返す
 *	\remarks	[+-] digits [. digits] [eE [+-] digits] の範囲。厳密な構文チェックは呼び出し元(parser)の責務。
 */
static size_t number_span_(const char *s, const char **pdot){
	const char *p = s;
	*pdot = NULL;
	if(*p == '+' || *p == '-'){ p++; }
	while(*p >= '0' && *p <= '9'){ p++; }
	if(*p == '.'){
		*pdot = p;
		p++;
		while(*p >= '0' && *p <= '9'){ p++; }
	}
	if(*p == 'e' || *p == 'E'){
		const char *q = p + 1;
		if(*q == '+' || *q == '-'){ q++; }
		if(*q >= '0' && *q <= '9'){
			while(*q >= '0' && *q <= '9'){ q++; }
			p = q;
		}
	}
	return (size_t)(p - s);
}

double nmjson_strtod(const char *s, char **endptr){
	const char *dot;
	const char *dp = localeconv()->decimal_point;
	size_t dp_len = (dp != NULL) ? strlen(dp) : 0;
	size_t span = number_span_(s, &dot);
	char buf[NMJSON_STRTOD_BUF_];
	char *bend;
	size_t pre;
	size_t used;
	double ret;

	if(dp_len == 0){ dp = "."; dp_len = 1; }

	{
		// 16進(JSON5で整数範囲外の場合に来る)は小数点を含まないのでそのまま変換する
		const char *h = (*s == '+' || *s == '-') ? s + 1 : s;
		if(h[0] == '0' && (h[1] == 'x' || h[1] == 'X')){
			return strtod(s, endptr);
		}
	}

	if(span + dp_len > sizeof(buf)){
		if(dp_len == 1 && dp[0] == '.'){
			// ロケールの小数点が"."なら、数値部分の後ろ(JSONの区切り等)を食うことはない
			return strtod(s, endptr);
		}
		if(endptr != NULL){ *endptr = (char *)s; }
		errno = 0;
		return 0.0;
	}

	// 数値部分だけをコピーし、'.'をロケールの小数点に置き換える
	pre = (dot != NULL) ? (size_t)(dot - s) : span;
	memcpy(buf, s, pre);
	used = pre;
	if(dot != NULL){
		memcpy(&buf[used], dp, dp_len);
		used += dp_len;
		memcpy(&buf[used], dot + 1, span - pre - 1);
		used += span - pre - 1;
	}
	buf[used] = '\0';

	errno = 0;
	ret = strtod(buf, &bend);
	if(endptr != NULL){
		// buf上の位置を s 上の位置へ戻す
		size_t consumed = (size_t)(bend - buf);
		if(dot != NULL && consumed > pre){
			consumed = (consumed >= pre + dp_len) ? consumed - (dp_len - 1) : pre;
		}
		*endptr = (char *)s + consumed;
	}
	return ret;
}

#endif	/* NMJSON_STRTOD_USE_USELOCALE_ */
