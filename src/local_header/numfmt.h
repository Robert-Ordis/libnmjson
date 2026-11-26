/**
 *	\file		numfmt.h
 *	\brief		数値→文字列変換（printfファミリー非依存）
 *	\remarks	snprintfはmalloc/ロケール/スタック消費が実装依存になるため、自前で変換する。
 */
#ifndef	LOCAL_NUMFMT_H_
#define	LOCAL_NUMFMT_H_

#include <inttypes.h>
#include <stddef.h>

/// \brief	64bit整数の10進表現に必要な最大バイト数("-9223372036854775808" = 20文字。終端なし)
#define	NUMFMT_I64_DEC_MAX_	20

/**
 *	\brief		符号なし64bit整数を10進文字列にする
 *	\arg		buf		: 出力先。NUMFMT_I64_DEC_MAX_バイト以上。'\0'終端はしない
 *	\arg		v		: 値
 *	\return		書き込んだ文字数
 */
static inline size_t numfmt_u64_to_dec_(char *buf, uint64_t v){
	char tmp[NUMFMT_I64_DEC_MAX_];
	size_t n = 0;
	size_t i;
	do{
		tmp[n++] = (char)('0' + (int)(v % 10));
		v /= 10;
	}while(v != 0);
	for(i = 0; i < n; i++){
		buf[i] = tmp[n - 1 - i];
	}
	return n;
}

/**
 *	\brief		符号付き64bit整数を10進文字列にする
 *	\arg		buf		: 出力先。NUMFMT_I64_DEC_MAX_バイト以上。'\0'終端はしない
 *	\arg		v		: 値
 *	\return		書き込んだ文字数
 *	\remarks	INT64_MINは符号なしの範囲で反転して扱う（符号付きでの反転は未定義動作）
 */
static inline size_t numfmt_i64_to_dec_(char *buf, int64_t v){
	if(v < 0){
		buf[0] = '-';
		return 1 + numfmt_u64_to_dec_(buf + 1, (uint64_t)0 - (uint64_t)v);
	}
	return numfmt_u64_to_dec_(buf, (uint64_t)v);
}

#endif	/* !LOCAL_NUMFMT_H_ */
