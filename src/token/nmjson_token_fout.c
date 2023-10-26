#include "include/nmjson/json_const.h"
#include "include/nmjson/json_token.h"
#include "include/nmjson/json_parser.h"

#include "include/nmjson/utf8.h"

#include "../local_header/linear_linker.h"

static ssize_t fout_indent_(FILE *fp, unsigned int indent){
	char c = '\n';
	ssize_t ret = 0;
	
	ret += (fputc(c, fp) == 0);
	if(indent <= 0){
		return ret;
	}
	
	c = '\t';
	for(; indent > 0; indent --){
		ret += fwrite("    ", 1, 4, fp);
		//ret += (fputc(c, fp) == 0);
	}
	return ret;
}

static ssize_t fout_encode_str_(const char* str, size_t str_len, int raw_utf, nmjson_superset_t superset, FILE *fp){
	int i;
	ssize_t ret = 0;
	for(i = 0; i < str_len;){
		switch(str[i]){
		case '"':
			ret += fwrite("\\\"", 1, 2, fp);
			break;
		case '\\':
			ret += fwrite("\\\\", 1, 2, fp);
			break;
		case '\b':
			ret += fwrite("\\b", 1, 2, fp);
			break;
		case '\f':
			ret += fwrite("\\f", 1, 2, fp);
			break;
		case '\n':
			ret += fwrite("\\n", 1, 2, fp);
			break;
		case '\r':
			ret += fwrite("\\r", 1, 2, fp);
			break;
		case '\t':
			ret += fwrite("\\t", 1, 2, fp);
			break;
		default:
			if(iscntrl((uint8_t)str[i])){
				//制御文字は必ずエスケープ
				ret += fprintf(fp, "\\u%04X", (uint8_t)str[i]);
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
						ret += fprintf(fp, "\\x%02x", (uint8_t)str[i]);
					}
					else{
						ret += fprintf(fp, "#%03u", (uint8_t)str[i]);
					}
					i ++;
					continue;
				}
				i += r;
				if(unicode < 0x010000){
					ret += fprintf(fp, "\\u%04X", unicode);
				}
				else{	//サロゲートペアに変換
					unicode -= 0x010000;
					ret += fprintf(fp, "\\u%04X\\u%04X", (unicode / 0x400 + 0xD800), (unicode % 0x400 + 0xDC00));
				}
				continue;
			}
			//エスケープしないASCII
			ret += (fputc(str[i], fp) == 0);
			break;
		}
		
		i++;
	}
	return ret;
}

ssize_t nmjson_token_fout3(const nmjson_token_t *self, FILE *fp, int easy_to_look, int raw_utf, nmjson_superset_t superset){
	ssize_t ret = 0;
	int ret_flg = 0;
	unsigned int indent = 0;
	
	const nmjson_token_t *token = self;
	
	while(token != NULL){
		if(easy_to_look){
			ret += fout_indent_(fp, indent);
		}
		//名前があるなら、コロンと一緒に出す。
		if(token->n.len > 0 && !ret_flg){
			ret += (fputc('"', fp) == 0);
			//ret += fwrite("\"", 1, 1, fp);
			ret += fout_encode_str_(token->n.name, token->n.len, raw_utf, superset, fp);
			ret += fwrite("\":", 1, 2, fp);
		}
		
		//タイプごとにふさわしい出力を出す。
		switch(token->v.type){
		case nmjson_type_object:
		case nmjson_type_array:
			if(!ret_flg){	//このトークンは配下からの帰還したヤツではない。→オブジェクト/配列の始め。
				ret += fputc((token->v.type == nmjson_type_array) ? '[':'{', fp);
				if(token->v.value.o != NULL){		//子がいるのなら、インデントを1個下げる。
					indent ++;
					token = token->v.value.o;
					continue;
				}
			}
			
			//オブジェクト/配列の終わり。子がいない場合は直接ここに来る。
			ret += (fputc((token->v.type == nmjson_type_array) ? ']':'}', fp) == 0);
			ret_flg = 0;
			break;
			
		case nmjson_type_bool:
			ret += fwrite(token->v.value.b ? "true":"false", 1, token->v.value.b ? 4:5, fp);
			break;
			
		case nmjson_type_integer:
			ret += fprintf(fp, "%"PRId64"", token->v.value.i);
			break;
			
		case nmjson_type_float:
			//InfやNaNを投げ込まれていた場合は残念だけど文字列にする。
			if(isnan(token->v.value.d)){
				if(nmjson_superset_has_extnum(superset)){
					ret += fwrite("NaN", 1, 3, fp);
				}
				else{
					ret += fwrite("\"NaN\"", 1, 5, fp);
				}
			}
			else if(isinf(token->v.value.d)){
				if(nmjson_superset_has_extnum(superset)){
					ret += fwrite(token->v.value.d < 0.0 ? "-Infinity":"Infinity", 1, token->v.value.d < 0.0 ? 9:8, fp);
				}
				else{
					ret += fwrite(token->v.value.d < 0.0 ? "\"-Infinity\"":"\"Infinity\"", 1, token->v.value.d < 0.0 ? 11:10, fp);
				}
			}
			else{
				ret += fprintf(fp, "%f", token->v.value.d);
			}
			break;
			
		case nmjson_type_null:
			ret += fwrite("null", 1, 4, fp);
			break;
			
		case nmjson_type_string:
			ret += (fputc('"', fp) == 0);
			ret += fout_encode_str_(token->v.value.s, token->v.len, raw_utf, superset, fp);
			ret += (fputc('"', fp) == 0);
			break;
			
		default:
			break;
		}
		
		if(token == self){
			break;
		}
		else if(token->sibling_link.next){
			token = token->sibling_link.next;
			ret += (fputc(',', fp) == 0);
		}
		else{
			token = token->parent;
			ret_flg = 1;
			indent --;
		}
		
	}
	if(easy_to_look){
		ret += (fputc('\n', fp) == 0);
	}
	return ret;
}

ssize_t	nmjson_token_fout2(const nmjson_token_t *self, FILE *fp, int easy_to_look, int raw_utf){
	return nmjson_token_fout3(self, fp, easy_to_look, raw_utf, nmjson_superset_none);
}

ssize_t nmjson_token_fout(const nmjson_token_t *self, FILE *fp){
	return nmjson_token_fout2(self, fp, 0, 0);
}
