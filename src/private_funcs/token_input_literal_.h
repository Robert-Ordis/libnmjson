#include "include/nmjson/utf8.h"

/// \note 「文字列の処理」は「文字列の検査」と「文字列のデコード」の２つに分かれます
static ssize_t determine_literal_(char *literal, nmjson_superset_t superset){
	//JSONで定められた文字列規則を守りながら、何バイトまで持っているかを特定する。
	//※この段階ではデコードは行わない。
	char literal_end = *literal;
	char *cursor = literal;
	int is_reading = 1;
	int hexa_count = 0;
	int in_escape = 0;
	
	switch(literal_end){
	case '\'':
		if(!nmjson_superset_has_literalsq(superset)){
			return -1;
		}
	case '"':
		break;
	default:
		return -1;
	}
	cursor ++;
	
	while(is_reading){
		dbg_printf_("%s[%p]: '%c'(0x%02x)\n", __func__, literal, *cursor, *cursor);
		dbg_printf_("%s[%p]: escape[%d], hexa[%d]\n", __func__, literal, in_escape, hexa_count);
		if(*cursor == '\0'){	//字が途切れているので0を返す。
			return 0;
		}
		if(hexa_count){
			//ユニコードのエスケープ。\xのエスケープもこれで対応できる。
			if(!isxdigit((uint8_t)*cursor)){
				return -1;
			}
			hexa_count --;
			cursor ++;
			continue;
		}
		
		switch(*cursor){
		case '\0':			//文末: 途中で途切れている
			return 0;
			
		case '\\':			//\: エスケープ
			in_escape = !in_escape;
			break;
			
		case 'x':			//\xXX。
			if(in_escape){
				if(!nmjson_superset_has_hexaescape(superset)){
					return -1;
				}
			}
			
		case 'u':			//\uXXXX: ユニコードのエスケープ。多バイト長は主にこうなる。
			
			if(in_escape){
				hexa_count = (*cursor == 'u') ? 4:2 ;
			}
		case '/':
		case 'b':
		case 'f':
		case 'n':
		case 'r':
		case 't':
			//ごく普通のjsonエスケープ
			if(in_escape){
				in_escape = 0;
			}
			break;
		case 'v':
			if(in_escape){	//垂直タブ。らしい。JSONではNG。
				if(!nmjson_superset_has_extescape(superset)){
					return -1;
				}
				in_escape = 0;
			}
			break;
		case '0':			//NULL文字を文字中に定義する暴挙。何の需要が！？
			if(in_escape){
				if(!nmjson_superset_has_extescape(superset)){
					return -1;//JSONではNG。
				}
				switch(*(cursor + 1)){	//次の文字に処理が行きます。
				case '\0':	//入力が途切れている。
					return 0;
				case '0': case '1': case '2': case '3': case '4':
				case '5': case '6': case '7': case '8': case '9':
					//直後に数字が続くのはナシとのことです。なんで？
					return -1;
				}
				in_escape = 0;
			}
			break;
		case '\r':	//<CR>→それ単体の改行か、<CR><LF>であるかで微妙に処理が変わる。
			if(nmjson_superset_has_linecontinuation(superset)){
				if(in_escape){
					//「次の文字」を見て、\nならOK。\0なら中断。それ以外は<CR>単体改行とみなす。
					switch(*(cursor + 1)){
					case '\0':	//入力が途切れている。
						return 0;
					case '\n':	//<CR><LF>になってるのでカーソル１個進めてエスケープ処理続き。
						cursor ++;
						continue;
					default:	//<CR>単体の場合はそれで改行とみなす。
						in_escape = 0;
						cursor ++;
						continue;
					}
				}
			}
			//JSON5じゃないとか、エスケープしていないとかはめでたくNG。
			return -1;
		case '\n':
			if(nmjson_superset_has_linecontinuation(superset)){
				//JSON5の場合、エスケープを付ければリテラル中の改行が許されている。らしい。
				if(in_escape){
					in_escape = 0;
					break;
				}
			}
		default:
			if(*cursor == literal_end){
				//指定された文字列リテラルの終わり。
				if(!in_escape){
					//エスケープしていない場合にのみ終了とする。
					*cursor = '\0';
					is_reading = 0;
				}
				in_escape = 0;
				cursor ++;
				continue;
			}
			if(in_escape){		//エスケープシーケンスの次に、エスケープ対象ではないものが来た
				return -1;
			}
			if(iscntrl((uint8_t)*cursor)){	//制御文字の存在は許されていない
				return -1;
			}
		}
		
		cursor ++;
	}
	
	dbg_printf_("%s: \n", __func__);
	return (ssize_t)((uintptr_t) cursor - (uintptr_t) literal);
}

static ssize_t token_input_literal_(
	nmjson_token_t *self, 
	char *literal, int is_treating_value, 
	nmjson_superset_t superset
){
	//ここに来たということは、「文字列を読むべき範囲」はもう見定まっているということである。
	char literal_end = *literal;
	
	char		*src_cursor;
	char		*dst_cursor;
	
	int in_escape = 0;
	ssize_t tmp;
	
	uint32_t		hi_code = 0;
	
	switch(literal_end){
	case '\'':
		if(!nmjson_superset_has_literalsq(superset)){
			return -1;
		}
	case '"':
		break;
	default:
		return -1;
	}
	
	src_cursor = literal + 1;
	dst_cursor = src_cursor;
	
	if(is_treating_value){
		self->v.value.s = dst_cursor;
		self->v.type = nmjson_type_string;
	}
	else{
		self->n.name = dst_cursor;
	}
	
	while(*src_cursor != '\0'){
		//dbg_printf_("[%s:%p] in_escape[%d], char[%c]\n", __func__, literal, in_escape, *src_cursor);
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
			if(*src_cursor == 'x'){	//\x12の表現。srcは3バイト進むがdstは1バイトだけ進む。
				char hex[5] = {0};
				src_cursor ++;
				memcpy(hex, "0x", 2);
				memcpy(&hex[2], src_cursor, 2);
				*dst_cursor = (uint8_t)strtol(hex, NULL, 16);
				
				src_cursor += 2;
				dst_cursor += 1;
				continue;
			}
			if(hi_code != 0){
				//現在、サロゲートペアの単独残留を許していない。
				return -1;
			}
			
			//その他エスケープシーケンス
			switch(*src_cursor){
			case '\\':			//バックスラッシュ
				*dst_cursor = '\\';
				break;
			case '/':			//スラッシュ
				*dst_cursor = '/';
				break;
			case 'b':			//バックスペース
				*dst_cursor = '\b';
				break;
			case 'f':			//ページ送り
				*dst_cursor = '\f';
				break;
			case 'n':			//行送り
				*dst_cursor = '\n';
				break;
			case 'r':			//キャリッジリターン
				*dst_cursor = '\r';
				break;
			case 't':			//タブ
				*dst_cursor = '\t';
				break;
			case 'v':			//垂直タブ。
				*dst_cursor = 0x0B;
				break;
			case '0':			//ヌル文字。リテラル中にこいつ書いてデバイスドライバのGPLを回避しようとした天才がいるとかいないとか。
				*dst_cursor = '\0';	//こうなった時点でsrc_cursorの位置はdstの先を行くので問題なし。
				break;
			case '\r':			//リテラル中の改行。JSON5でなら許可されている。
			case '\n':
				dst_cursor --;	//「文字列の接続」
				break;
			default:
				if(*src_cursor == literal_end){
					//ダブルクォート/シングルクォート。リテラルの終わりをエスケープ。
					*dst_cursor = literal_end;
				}
			}
			//次の１文字も見なければならない場合。
			switch(*src_cursor){
			case '\r':	//次に<LF>が続いている場合はエスケープが続いている。
				in_escape = *(src_cursor + 1) == '\n';
				break;
			default:	//普通はその文字１個でエスケープが終わる。
				in_escape = 0;
				break;
			}
			src_cursor ++;
			dst_cursor ++;
			continue;
		}
		if(*src_cursor == '\\'){
			//エスケープ開始。
			src_cursor ++;
			in_escape = 1;
			continue;
		}
		if(hi_code != 0){
			return -1;
		}
		//通常の文字代入。dstの位置がsrcより遅れることはあっても先んずることはない。
		*dst_cursor = *src_cursor;
		dst_cursor ++;
		src_cursor ++;
	}
	
	//終わり。
	if(hi_code != 0){
		return -1;
	}
	*dst_cursor = '\0';
	tmp = (ssize_t)((uintptr_t)dst_cursor - (uintptr_t)(literal + 1));
	if(is_treating_value){
		self->v.len = tmp;
	}
	else{
		self->n.len = tmp;
	}
	
	return tmp;
}


