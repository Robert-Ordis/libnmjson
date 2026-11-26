#include <ctype.h>
#include <stdio.h>

#include "include/nmjson/unicode.h"

// https://262.ecma-international.org/5.1/#sec-7.6

#ifndef	NMJSON_TRY_TO_BE_RIGID
/**
 *	\brief	ECMA5のIdentifierNameの規則にがっつり準ずるかどうか
 */
#define	NMJSON_TRY_TO_BE_RIGID 0
#endif	/* !NMJSON_TRY_TO_BE_RIGID */

/**
 *	\brief		識別子を終わらせる文字か(JSON5のWhiteSpace / LineTerminator)
 *	\remarks	識別子に使える文字は膨大で規則性が無いが、終わらせる文字は少数で固定されている。
 *				WhiteSpace: TAB, VT, FF, SP, NBSP(U+00A0), BOM(U+FEFF), カテゴリZs(U+1680, U+2000-200A, U+202F, U+205F, U+3000)
 *				LineTerminator: LF, CR, U+2028, U+2029
 */
static int e5ident_is_separator_(uint32_t u){
	//集合の定義は token_space_.h に一本化
	return json5_is_space_cp_(u);
}

/**
 *	\brief		識別子に含まれてはならず、かつ識別子の終わりとしても不正な文字か
 *	\remarks	構造上の記号や制御文字をキー名に吸収しないため。
 */
static int e5ident_is_forbidden_(uint32_t u){
	switch(u){
	case ',': case '{': case '}': case '[': case ']': case '"': case '\'':
		return 1;
	default:
		break;
	}
	return (u < 0x20 || u == 0x7F);
}

/**
 *	\brief		ECMAScript 5.1 の IdentifierName としての範囲(バイト長)を決定する
 *	\return		 > 0	: 識別子のバイト長。終端は ':', '/'(コメント開始), 空白・行終端
 *	\return		== 0	: 入力が途中で途切れている
 *	\return		 < 0	: 構文エラー
 *	\remarks	NMJSON_TRY_TO_BE_RIGID == 0(既定)では、終端と禁止文字以外は識別子の文字として受け入れる。
 *				正しいJSON5は必ず正しく読めるが、不正な識別子(a-b, 1a等)も受け入れる。
 *				writerはキーを必ずクォートして出力するため、出力側の相互運用性には影響しない。
 */
static ssize_t determine_e5ident_(char *str, nmjson_superset_t superset){
	uint32_t unicode = 0;
	int is_initial = 1;
	int in_escape = 0;
	int hexa_count = 0;
	ssize_t load_len;
	
	char *cursor = str;
	
	(void)superset;
	for(;;){
		load_len = utf8_str_to_unicode(cursor, &unicode);
		if(load_len < 0){
			return -1;	//不正なUTF-8
		}
		dbg_printf_("%s: load[%zd], unicode[U+%04X]\n", __func__, load_len, unicode);
		dbg_printf_("%s: remains [%s]\n", __func__, cursor);
		if(*(cursor + load_len) == '\0'){
			return 0;	//文字が途切れている(終端の判定には次の文字が要る)
		}
		
		if(hexa_count){		//ユニコードのエスケープ。ここでは\uのみ。
			if(load_len != 1 || !isxdigit((uint8_t)unicode)){
				//半角の16進数字でなければNGです。
				return -1;
			}
			hexa_count --;
			cursor += load_len;
			continue;
		}
		
		if(in_escape){			//エスケープ中。
			if(load_len != 1 || unicode != 'u'){
				//エスケープにおいては、'u'以外を許可しない。
				return -1;
			}
			hexa_count = 4;
			in_escape = 0;
			cursor += load_len;
			continue;
		}
		
		//終了条件: ':', コメント開始'/', 空白・行終端(Unicodeのものを含む)
		if((load_len == 1 && (unicode == ':' || unicode == '/')) || e5ident_is_separator_(unicode)){
			break;
		}
		
		//構造上の記号・制御文字は識別子に吸収しない
		if(e5ident_is_forbidden_(unicode)){
			return -1;
		}
		
		if(load_len == 1 && unicode == '\\'){	//エスケープシーケンス
			in_escape = 1;
			cursor += load_len;
			is_initial = 0;
			continue;
		}
		
		//その他文字列
		if(NMJSON_TRY_TO_BE_RIGID){
			//厳しく見るならば、ECMA5の規則に従う。
			if(load_len == 1 && (unicode == '$' || unicode == '_')){
				cursor += load_len;
				is_initial = 0;
				continue;
			}
			else if(unicode_is_letter(unicode)){
				cursor += load_len;
				is_initial = 0;
				continue;
			}
			
			if(!is_initial){
				if(unicode == 0x200C || unicode == 0x200D){
					cursor += load_len;
					continue;
				}
				else{
					unicode_category_t uc = unicode_to_category(unicode);
					switch(uc){
					case unicode_category_mc:
					case unicode_category_mn:
					case unicode_category_nd:
					case unicode_category_pc:
						cursor += load_len;
						continue;
					default:
						break;
					}
				}
			}
		}else{
			//SQLite3の場合、そこまでしていられないということで
			//最低限終了やエスケープをカバーしていればいいという感じでやっているらしいです
			cursor += load_len;
			is_initial = 0;
			continue;
		}
		//ダメな文字に突入した
		return -1;
	}
	
	if(cursor == str){
		return -1;	//空の識別子
	}
	return (ssize_t)((uintptr_t) cursor - (uintptr_t) str);
}

/**
 *	\brief		determine_e5ident_で決めた範囲(len)の識別子をキー名として取り込む(\uXXXXを解除)
 */
static ssize_t token_input_e5ident_(nmjson_token_t *self, char *str, size_t len, nmjson_superset_t superset){
	//ここに来たということは、「識別子を読むべき範囲」はもう見定まっているということである。
	
	//基本的には丸コピを繰り返していく。が、エスケープが存在するのでそこだけ処理する。
	char		*src_cursor = str;
	char		*src_end = str + len;
	char		*dst_cursor = str;
	
	int in_escape = 0;
	ssize_t tmp;
	uint32_t		hi_code = 0;
	
	(void)superset;
	while(src_cursor < src_end){
		//範囲の終わりまで繰り返す(終端の判定はdetermine_e5ident_に一本化)
		if(in_escape){
			if(*src_cursor == 'u'){
				src_cursor ++;
				if(hi_code == 0){
					tmp = utf8_copy_decode(dst_cursor, src_cursor, 4, &hi_code);
				}
				else{
					tmp = utf8_copy_decode_surrogate(dst_cursor, src_cursor, hi_code);
					hi_code = 0;
				}
				if(tmp < 0){
					return -1;
				}
				
				src_cursor += 4;		//srcは4文字進めた。
				dst_cursor += tmp;		//dstは書き込んだutf-8のバイトだけ進んだ。
				in_escape = 0;			//エスケープ解除。
				continue;
			}
			return -1;
		}
		
		if(*src_cursor == '\\'){
			src_cursor ++;
			in_escape = 1;
			continue;
		}
		if(hi_code != 0){
			return -1;
		}
		*dst_cursor = *src_cursor;
		dst_cursor ++;
		src_cursor ++;
	}
	if(in_escape || hi_code != 0){
		//エスケープの途中で終わった / 上位サロゲートが単独で終わった
		return -1;
	}
	self->n.name = str;
	self->n.len = (ssize_t)((uintptr_t)dst_cursor - (uintptr_t)str);
	return (ssize_t)self->n.len;
}
