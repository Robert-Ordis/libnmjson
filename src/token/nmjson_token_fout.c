#include "include/nmjson/json_const.h"
#include "include/nmjson/json_token.h"
#include "include/nmjson/json_parser.h"

#include "include/nmjson/utf8.h"

#include "../local_header/linear_linker.h"
#include "../local_header/swriter.h"

static ssize_t sout_indent_(swriter_t *pwriter, unsigned int indent){
	char c = '\n';
	ssize_t ret = 0;
	ssize_t was = pwriter->written;
	
	if(swriter_putc_(pwriter, c) < 0){
		return -1;
	}
	
	if(indent <= 0){
		return pwriter->written - was;
	}
	
	c = '\t';
	for(; indent > 0; indent --){
		if(swriter_write_(pwriter, "    ", 1, 4) < 0){
			return -1;
		}
		//if(swriter_putc_(pwriter, c) < 0){
		//	return -1;
		//}
	}
	return pwriter->written - was;
}

static ssize_t sout_encode_str_(swriter_t *pwriter, const char* str, size_t str_len, int raw_utf, nmjson_superset_t superset){
	int i;
	ssize_t tmp = 0;
	ssize_t was = pwriter->written;
	for(i = 0; i < str_len;){
		if(tmp < 0){
			return -1;
		}
		switch(str[i]){
		case '"':
			tmp = swriter_write_(pwriter, "\\\"", 1, 2);
			break;
		case '\\':
			tmp = swriter_write_(pwriter, "\\\\", 1, 2);
			break;
		case '\b':
			tmp = swriter_write_(pwriter, "\\b", 1, 2);
			break;
		case '\f':
			tmp = swriter_write_(pwriter, "\\f", 1, 2);
			break;
		case '\n':
			tmp = swriter_write_(pwriter, "\\n", 1, 2);
			break;
		case '\r':
			tmp = swriter_write_(pwriter, "\\r", 1, 2);
			break;
		case '\t':
			tmp = swriter_write_(pwriter, "\\t", 1, 2);
			break;
		default:
			if(iscntrl((uint8_t)str[i])){
				//制御文字は必ずエスケープ
				tmp = swriter_printf_(pwriter, "\\u%04X", (uint8_t)str[i]);
				i++;
				continue;
			}
			if(!raw_utf && (str[i] & 0xE0) >= 0xC0){
				uint32_t unicode;
				ssize_t r = utf8_str_to_unicode(&str[i], &unicode);
				if(r < 0){
					//不正シーケンス: "#\d{3}"の形式にひとまず出しておく。
					//→そのまま出すとエラーを吐いてしまう。
					//ret += fputc(str[i], fp) == 0;
					if(nmjson_superset_has_hexaescape(superset)){
						tmp = swriter_printf_(pwriter, "\\x%02x", (uint8_t)str[i]);
					}
					else{
						tmp = swriter_printf_(pwriter, "#%03u", (uint8_t)str[i]);
					}
					i++;
					continue;
				}
				i += r;
				if(unicode < 0x010000){
					tmp = swriter_printf_(pwriter, "\\u%04X", unicode);
				}
				else{	//サロゲートペアに変換
					unicode -= 0x010000;
					tmp = swriter_printf_(pwriter, "\\u%04X\\u%04X"
						, (unicode / 0x400 + 0xD800), (unicode % 0x400 + 0xDC00));
				}
				continue;
			}
			//エスケープしないASCII
			tmp = swriter_putc_(pwriter, str[i]);
			break;
		}
		
		i++;
	}
	return pwriter->written - was;
}

static inline int do_print_name_(const nmjson_token_t *self){
	const nmjson_token_t *parent = self->parent;
	if(parent == NULL){	//親がいない→独立した値オブジェクト
		return 0;
	}
	if(parent->v.type != nmjson_type_object){
		return 0;
	}
	return 1;
}

ssize_t nmjson_token_fout3(const nmjson_token_t *self, FILE *fp, int easy_to_look, int raw_utf, nmjson_superset_t superset){
	ssize_t ret = 0;
	int ret_flg = 0;
	unsigned int indent = 0;
	
	const nmjson_token_t *token = self;
	swriter_t swriter;
	swriter_init_(&swriter, fp);
	
	while(token != NULL){
		const char *s;
		size_t s_len;
		char c;
		if(easy_to_look){
			ret += sout_indent_(&swriter, indent);
		}
		//名前があるなら、コロンと一緒に出す。
		//if(token->n.len > 0 && !ret_flg){
		if(do_print_name_(token) && !ret_flg){
			if(swriter_putc_(&swriter, '"') < 0){
				return -1; 
			}
			if(sout_encode_str_(&swriter, token->n.name, token->n.len, raw_utf, superset) < 0){
				return -1;
			}
			if(swriter_write_(&swriter, "\":", 1, 2) < 0){
				return -1;
			}
		}
		
		//タイプごとにふさわしい出力を出す。
		switch(token->v.type){
		case nmjson_type_object:
		case nmjson_type_array:
			if(!ret_flg){	//このトークンは配下からの帰還したヤツではない。→オブジェクト/配列の始め。
				c = (token->v.type == nmjson_type_array) ? '[':'{';
				if(swriter_putc_(&swriter, c) < 0){
					return -1;
				}
				if(token->v.value.o != NULL){		//子がいるのなら、インデントを1個下げる。
					indent ++;
					token = token->v.value.o;
					continue;
				}
			}
			
			//オブジェクト/配列の終わり。子がいない場合は直接ここに来る。
			c = (token->v.type == nmjson_type_array) ? ']':'}';
			if(swriter_putc_(&swriter, c) < 0){
				return -1;
			}
			ret_flg = 0;
			break;
			
		case nmjson_type_bool:
			s = token->v.value.b ? "true":"false";
			if(swriter_write_(&swriter, s, 1, strlen(s)) < 0){
				return -1;
			}
			break;
			
		case nmjson_type_integer:
			if(swriter_printf_(&swriter, "%"PRId64"", token->v.value.i) < 0){
				return -1;
			}
			break;
			
		case nmjson_type_float:
			//InfやNaNを投げ込まれていた場合は残念だけど文字列にする。
			if(isnan(token->v.value.d)){
				s = nmjson_superset_has_extnum(superset) ? "NaN" : "\"NaN\"";
				if(swriter_write_(&swriter, s, 1, strlen(s)) < 0){
					return -1;
				}
			}
			else if(isinf(token->v.value.d)){
				if(nmjson_superset_has_extnum(superset)){
					s = (token->v.value.d < 0.0) ? "-Infinity":"Infinity";
				}
				else{
					s = (token->v.value.d < 0.0) ? "\"-Infinity\"":"\"Infinity\"";
				}
				if(swriter_write_(&swriter, s, 1, strlen(s)) < 0){
					return -1;
				}
			}
			else{
				if(swriter_printf_(&swriter, "%f", token->v.value.d) < 0){
					return -1;
				}
			}
			break;
			
		case nmjson_type_null:
			if(swriter_write_(&swriter, "null", 1, 4) < 0){
				return -1;
			}
			break;
			
		case nmjson_type_string:
			if(swriter_putc_(&swriter, '"') < 0){ return -1; }
			if(sout_encode_str_(&swriter, token->v.value.s, token->v.len, raw_utf, superset) < 0){
				return -1;
			}
			if(swriter_putc_(&swriter, '"') < 0){ return -1; }
			break;
			
		default:
			break;
		}
		
		if(token == self){
			break;
		}
		else if(token->sibling_link.next){
			token = token->sibling_link.next;
			if(swriter_putc_(&swriter, ',') < 0){ return -1; }
		}
		else{
			token = token->parent;
			ret_flg = 1;
			indent --;
		}
		
	}
	if(easy_to_look){
		if(swriter_putc_(&swriter, '\n') < 0){ return -1; }
	}
	return swriter.written;
}

ssize_t	nmjson_token_fout2(const nmjson_token_t *self, FILE *fp, int easy_to_look, int raw_utf){
	return nmjson_token_fout3(self, fp, easy_to_look, raw_utf, nmjson_superset_none);
}

ssize_t nmjson_token_fout(const nmjson_token_t *self, FILE *fp){
	return nmjson_token_fout2(self, fp, 0, 0);
}
