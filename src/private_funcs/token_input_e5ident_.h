#include <ctype.h>
#include <stdio.h>

#include "include/nmjson/unicode.h"

// https://262.ecma-international.org/5.1/#sec-7.6

static ssize_t determine_e5ident_(char *str, nmjson_superset_t superset){
	//ECMAScript 5.1で定められた「IdentiferName」としての範囲を決定する
	//標準空白か":"で終わる。
	//fprintf(stderr, "Scan: Ident of ECMAScript is still not Supported...\n");
	uint32_t unicode = 0;
	int is_initial = 1;
	int in_escape = 0;
	int hexa_count = 0;
	ssize_t load_len;
	
	char *cursor = str;
	
	while((load_len = utf8_str_to_unicode(cursor, &unicode)) > 0){
		//終わる条件: 標準空白, コロン(:), 文末(\0)
		dbg_printf_("%s: load[%zd], unicode[U+%04X]\n", __func__, load_len, unicode);
		dbg_printf_("%s: remains [%s]\n", __func__, cursor);
		if(*(cursor + load_len) == '\0'){
			return 0;	//文字が途切れている
		}
		
		if(hexa_count){		//ユニコードのエスケープ。ここでは\uのみ。
			if(!isxdigit(unicode) || load_len != 1){
				//半角の数字でなければNGです。
				return -1;
			}
			hexa_count --;
			cursor += load_len;
			continue;
		}
		
		if(in_escape){			//エスケープ中。
			if(unicode != 'u' || load_len != 1){
				//エスケープにおいては、'u'以外を許可しない。
				return -1;
			}
			hexa_count = 4;
			in_escape = 0;
			cursor += load_len;
			continue;
		}
		
		/// \note 特殊な意味を持つ文字は、「1バイト」で表現しなければならない。
		if((unicode == ':' || isspace(unicode)) && load_len == 1){	//終了条件。
			break;
		}
		
		if(unicode == '\\' && load_len == 1){	//エスケープシーケンス
			in_escape = 1;
			cursor += load_len;
			is_initial = 0;
			continue;
		}
		
		//その他文字列
		
		if((unicode == '$' || unicode == '_') && load_len == 1){
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
		
		//ダメな文字に突入した
		return -1;
	}
	
	return (ssize_t)((uintptr_t) cursor - (uintptr_t) str);
}

static ssize_t token_input_e5ident_(nmjson_token_t *self, char *str, nmjson_superset_t superset){
	//ここに来たということは、「識別子を読むべき範囲」はもう見定まっているということである。
	//fprintf(stderr, "Input: Ident of ECMAScript is still not Supported...\n");
	
	//基本的には丸コピを繰り返していく。が、エスケープが存在するのでそこだけ処理する。
	char		*src_cursor = str;
	char		*dst_cursor = str;
	
	int in_escape = 0;
	ssize_t tmp;
	uint32_t		hi_code = 0;
	
	while(*src_cursor != ':' && !isspace((uint8_t)*src_cursor)){
		//終了条件に引っかかるまで繰り返す。
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
	self->n.name = str;
	self->n.len = (ssize_t)((uintptr_t)dst_cursor - (uintptr_t)str);
	return (ssize_t)self->n.len;
}
