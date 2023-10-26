#include <errno.h>
#include <math.h>

static ssize_t token_parse_bool_(nmjson_token_t *self, char *src, nmjson_superset_t superset){
	const char *keyword;
	ssize_t keyword_len;
	do{
		keyword = "true";
		keyword_len = strlen(keyword);
		if(strstr(keyword, src) == keyword){
			//例えばsrc（JSONの文字側）で"true"の部分文字列を検出したら、次のパースに託す。
			//注釈）完全一致であっても、そこで文字が途切れているのなら次に託す
			//　　　→「次に何が入るかわからない」からである。truebとかだったらどうする？という話。
			keyword_len = 0;
			break;
		}
		if(memcmp(keyword, src, keyword_len) == 0){
			self->v.value.b = 1;
			break;
		}
		
		keyword = "false";
		keyword_len = strlen(keyword);
		if(strstr(keyword, src) == keyword){
			keyword_len = 0;
			break;
		}
		if(memcmp(keyword, src, keyword_len) == 0){
			self->v.value.b = 0;
			break;
		}
		
		keyword_len = -1;
	}while(0);
	if(keyword_len > 0){
		self->v.type = nmjson_type_bool;
	}
	return keyword_len;
}

static ssize_t token_parse_null_(nmjson_token_t *self, char *src, nmjson_superset_t superset){
	const char *keyword = "null";
	ssize_t keyword_len = strlen(keyword);
	if(strstr(keyword, src) != NULL){
		return 0;
	}
	if(memcmp(src, keyword, keyword_len) == 0){
		self->v.type = nmjson_type_null;
		self->v.value.n = NULL;
		return keyword_len;
	}
	return -1;
}

static ssize_t token_parse_number_(nmjson_token_t *self, char *src, nmjson_superset_t superset){
	//int64_t val_integer;
	//double val_float;
	//double exp = 1.0;
	
	int tmp_digit = 0;
	ssize_t proceeded = 0;
	int is_minus = 0;
	
	//int is_double = 0;
	int base = 10;
	
	char *cursor = src;
	char *endptr;
	//char bucket;	//退避用
	
	char *start_of_num = cursor;	//strtoxxを行うための文字列ポインタ
	dbg_printf_("%s: src is [%s]\n", __func__, src);
	self->v.type = nmjson_type_unused_;
	
	if(*cursor == '-'){
		cursor ++;
		proceeded ++;
		is_minus = 1;
		start_of_num ++;
	}
	else if(*cursor == '+' && nmjson_superset_has_extnum(superset)){
		cursor ++;
		proceeded ++;
		start_of_num ++;
	}
	
	while(isdigit((uint8_t)*cursor)){		//文字の種類の判別: 主にゼロつめを弾きつつ数字以外を取るためのもの
		tmp_digit ++;
		if(tmp_digit == 1 && *cursor == '0'){
			//先頭ゼロは小数かJSON5での16進数のみ許可されます。逆にゼロ詰めはNGになります。
			cursor ++;
			proceeded ++;
			break;
		}
		cursor ++;
		proceeded ++;
	}
	dbg_printf_("%s: tmp_digit is %d, cursor is [%s]\n", __func__, tmp_digit, cursor);
	//判別開始。
	if(*cursor == '\0'){	//途中で文字列が切れていた
		return 0;
	}
	if(tmp_digit <= 0){	//切られているでもないのに数字が一つも読めない
		//JSON5ではまだもうちょっと戦わねばならない。
		if(nmjson_superset_has_extnum(superset) || nmjson_superset_has_abbrdecimal(superset)){
			if(*cursor != '.'){
				//Infinity, -Infinity, NaNのいずれかの可能性を探る。
				const char *keyword;
				ssize_t keyword_len;
				do{
					keyword = "Infinity";
					keyword_len = strlen(keyword);
					if(strstr(keyword, cursor) == keyword){
						//重要なキーワードのなりそこないであれば、「読み途中」として返す。
						keyword_len = 0;
						break;
					}
					if(memcmp(cursor, keyword, keyword_len) == 0){
						self->v.value.d = is_minus ? -INFINITY : INFINITY;
						break;
					}
					
					keyword = "NaN";
					keyword_len = strlen(keyword);
					if(strstr(keyword, cursor) == keyword){
						keyword_len = 0;
						break;
					}
					if(memcmp(cursor, keyword, keyword_len) == 0){
						//self->v.value.d = is_minus ? -NAN : NAN;
						self->v.value.d = NAN;
						break;
					}
					
					keyword_len = -1;
					
				}while(0);
				
				if(keyword_len <= 0){	//エラーか尻切れトンボの場合、そのまま返す。
					return keyword_len;
				}
				self->v.type = nmjson_type_float;
				
				return (ssize_t)((uintptr_t)(cursor + keyword_len) - (uintptr_t)src);
			}
			//JSON5である場合、整数部分を省略した記述は許可される。
		}
		else{	//通常のJSONでは普通にNGである。
			return -1;
		}
	}
	if(isdigit((uint8_t)*cursor)){	//ゼロ詰めの値が書かれていたらここに来る
		return -1;
	}
	if(nmjson_superset_has_extnum(superset) && tmp_digit == 1){
		//0xの値を見る準備。
		if(memcmp(start_of_num, "0x", 2) == 0){
			if(*(start_of_num + 2) == '\0'){
				return 0;
			}
			dbg_printf_("%s: parse hex\n", __func__);
			base = 16;
			self->v.type = nmjson_type_integer;
		}
	}
	
	//ここまで来てようやくライブラリ関数に任せる準備ができた…と思う。
	if(*(cursor) == '\0'){
		//ただし、文字列末尾であるなら一度見なかったことにする。
		return 0;
	}
	
	dbg_printf_("%s: then start to parse\n", __func__);
	switch(*cursor){
	case '.':
		cursor ++;
		if(*cursor == '\0'){
			return 0;
		}
		if(!(isdigit((uint8_t)*(cursor)) || nmjson_superset_has_abbrdecimal(superset))){
			//"."の後が数字であるかあるいはJSON5であるなら許可される。
			return -1;
		}
		self->v.type = nmjson_type_float;
		break;
	case 'e':
	case 'E':
		self->v.type = nmjson_type_float;
		cursor ++;
		
		while(isdigit((uint8_t)*cursor) || *cursor == '+' || *cursor == '-'){ cursor ++; }
		if(*(cursor) == '\0'){
			return 0;
		}
		break;
	default:
		self->v.type = nmjson_type_integer;
		break;
	}
	
	switch(self->v.type){
	case nmjson_type_float:
		self->v.value.d = strtod(src, &endptr);
		break;
	case nmjson_type_integer:
		errno = 0;
		self->v.value.i = is_minus ? strtoll(src, &endptr, base) : (int64_t) strtoull(src, &endptr, base);
		if(errno == ERANGE){
			self->v.value.d = strtod(src, &endptr);
			self->v.type = nmjson_type_float;
		}
		break;
	default:
		return -1;
	}
	
	dbg_printf_("%s: src was [%s], endptr is %c\n", __func__, src, *endptr);
	
	if(endptr == NULL){
		return 0;
	}
	
	switch(*endptr){	//「書かれた数字より後ろの字」を見る。
	case '\0':			//「文字列末尾」であるなら、続きが来る可能性があるので見なかったことにする。
		return 0;
	
	//JSONとして意味を持つ記号の場合、正当な終了として取り扱う。
	case ']':
	case '}':
	case ',':
		break;
	default:
		if(!isspace((uint8_t)*endptr)){
			//「正当な終了」には標準空白も含まれる。
			return -1;
		}
	}
	
	return (ssize_t)((uintptr_t)endptr - (uintptr_t)src);
}
