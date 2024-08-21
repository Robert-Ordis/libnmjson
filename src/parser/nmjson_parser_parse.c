#include "include/nmjson/json_const.h"
#include "include/nmjson/json_token.h"
#include "include/nmjson/json_parser.h"

#include "../local_header/linear_linker.h"

//#define	IN_DEBUG

#ifdef	IN_DEBUG
//デバッグ用。
static void dbg_out_special(char c){
	printf("%s: %c->", __func__, c);
	switch(c){
	case '{':
		printf("start of object\n");
		break;
	case '[':
		printf("start of array\n");
		break;
	case '"':
		printf("start of string\n");
		break;
	case ',':
		printf("continue of values\n");
		break;
	case '}':
		printf("end of object\n");
		break;
	case ']':
		printf("end of array\n");
		break;
	case ':':
		printf("switch. name->value\n");
		break;
	case '/':
		printf("comment(maybe)\n");
		break;
	case 't':
		printf("bool value->true(maybe)\n");
		break;
	case 'f':
		printf("bool value->false(maybe)\n");
		break;
	case 'n':
		printf("null value(maybe)\n");
		break;
	case '+':
	case 'I':
	case 'N':
		printf("numeric(in JSON5)\n");
		break;
	case '-': 
	case '0': case '1': case '2': case '3': case '4': 
	case '5': case '6': case '7': case '8': case '9':
		printf("numeric value(maybe)\n");
		break;
	default:
		if(isspace(c)){
			printf("standard space character.\n");
		}
		else{
			printf("something goes wrong...\n");
		}
	}
}

static void dbg_print_stat_(nmjson_parser_t *self){
	nmjson_parser_dbg_print(self, printf);
}

#define dbg_printf_(...) do{printf(__VA_ARGS__);}while(0)

#else
#define dbg_out_special(c) do{}while(0)

#define dbg_print_stat_(p) do{}while(0)

#define dbg_printf_(...) do{}while(0)

#endif

#include "../private_funcs/token_parse_.h"
#include "../private_funcs/token_parse_comment_.h"
#include "../private_funcs/token_input_literal_.h"
#include "../private_funcs/token_input_e5ident_.h"

static int is_comment_enabled_(nmjson_parser_t *self){
	switch(self->superset){
	case nmjson_superset_jsonc:
	case nmjson_superset_json5:
		return 1;
	default:
		break;
	}
	return 0;
}

static int is_tail_comma_enabled_(nmjson_parser_t *self){
	switch(self->superset){
	case nmjson_superset_jsonc:
	case nmjson_superset_json5:
		return 1;
	default:
		break;
	}
	return 0;
}

static int is_ecma5_(nmjson_parser_t *self){
	switch(self->superset){
	case nmjson_superset_json5:
		return 1;
	default:
		break;
	}
	return 0;
}

static nmjson_token_t* new_token_(nmjson_parser_t *self){
	nmjson_token_t *ret = NULL;
	//フリーリストからまずはとる。
	LINEAR_LINKER_POP_(&(self->free_head), &ret, nmjson_token_t, alloc_link);
	if(ret == NULL){
		if(self->is_alloc_allowed){
			//malloc許可の場合。
			ret = (nmjson_token_t *)malloc(sizeof(nmjson_token_t));
			ret->is_alloced = 1;
		}
	}
	else{
		ret->is_alloced = 0;
	}
	
	if(ret){
		LINEAR_LINKER_INSERT_AFTER_(&(self->active_head), ret, NULL, nmjson_token_t, alloc_link);
		memset(&(ret->n), 0, sizeof(ret->n));
		memset(&(ret->v), 0, sizeof(ret->v));
		memset(&(ret->sibling_link), 0, sizeof(ret->sibling_link));
		ret->parent = NULL;
	}
	return ret;
}

static void del_token_(nmjson_parser_t *self, nmjson_token_t *token){
	//アクティブから取り上げ。
	LINEAR_LINKER_DEL_(&(self->active_head), token, nmjson_token_t, alloc_link);
	if(token->v.type == nmjson_type_string && token->v.is_alloced){
		if(token->v.value.s){
			free(token->v.value.s);
		}
		token->v.value.s = NULL;
	}
	
	if(token->n.is_alloced){
		if(token->n.name){
			free(token->n.name);
		}
		token->n.name = NULL;
	}
	
	if(token->is_alloced){
		free(token);
	}
	else{
		LINEAR_LINKER_INSERT_AFTER_(&(self->free_head), token, NULL, nmjson_token_t, alloc_link);
	}
}

static int json_token_add_(nmjson_token_t *vessel, nmjson_token_t *value){
	
	if(!(vessel->v.type & nmjson_type_parent_)){
		//vessel(=器)といいながらオブジェクト/配列ではないのでエラー。
		return -1;
	}
	if(value->v.type == nmjson_type_unused_){
		//未確定なので×。
		return -2;
	}
	
	//加える。
	value->parent = vessel;
	vessel->v.len ++;
	LINEAR_LINKER_ADD_(&(vessel->v.value.o), value, nmjson_token_t, sibling_link);
	return 0;
}

static inline void parser_put_err_(nmjson_parser_t *self, nmjson_error_t code, const char *detail, size_t increment){
	self->error.code = code;
	self->error.detail = detail;
	self->cursor += increment;
}

static int token_compare_name_(nmjson_token_t *a, nmjson_token_t *b){
	int ret = a->n.len - b->n.len;
	if(!ret){
		ret = memcmp(a->n.name, b->n.name, a->n.len);
	}
	return ret;
}

//内部関数ここまで？
size_t				nmjson_parser_parse(nmjson_parser_t *self, char *src){
	
	nmjson_type_t input_type = nmjson_type_unused_;
	
	int break_loop = 0;
	ssize_t increment;
	
	if(self->is_initial){
		self->cursor = src;
		self->is_initial = 0;
	}
	
	while(*(self->cursor) != '\0'){
		dbg_out_special(*(self->cursor));
		if(break_loop){
			break;
		}
		if(isspace((uint8_t)*(self->cursor))){
			self->cursor ++;
			continue;
		}
		
		dbg_print_stat_(self);
		
		//特別な意味を持つ文字を処理する→「特別な意味」を処理するので、そうした場合はcontinueで続きを促す。
		switch(*(self->cursor)){
		//器の開始→器を１個作り、ネストを一つ下げる
		case '{':
		case '[':
			if(!(self->is_in_input && self->is_treating_value)){
				//[入力準備完了&&"値"の入力待ち]ではない
				break_loop = 1;
				parser_put_err_(self, nmjson_error_syntax, "Not state:[waiting input && treating value]", 1);
				continue;
			}
			
			if(self->token.vessel == NULL){
				//器がいない→こいつがルート。
				dbg_printf_("get root\n");
				if((self->token.value = new_token_(self)) == NULL){
					parser_put_err_(self, nmjson_error_syntax, "No more tokens(root)", 1);
					break_loop = 1;
					continue;
				}
				self->token.value->v.len = 0;			//値トークンがまだ一つもない。
				self->token.value->v.type = (*(self->cursor) == '[') ? nmjson_type_array : nmjson_type_object;
				
				//ルートに指定する
				self->token.root = self->token.value;
				
				//ネストを一つ下げる
				self->token.vessel = self->token.value;
				self->token.value = NULL;
				
				//配列ならば、次に来るのは値である
				self->is_treating_value = self->token.vessel->v.type == nmjson_type_array;
				self->is_in_input = 1;
				self->cursor ++;
				continue;
			}
			
			//ルートではない
			if(self->token.vessel->v.type == nmjson_type_array){
				//器が配列であった場合、トークンの取得をまだやっていない
				self->token.value = new_token_(self);
			}
			//値トークンが取れていない場合、エラー
			if(self->token.value == NULL){
				parser_put_err_(self, nmjson_error_tokens, "No more tokens(array)", 1);
				break_loop = 1;
				continue;
			}
			
			//値の型を確定→器に入れる
			self->token.value->v.len = 0;			//値トークンがまだ一つもない。
			self->token.value->v.type = (*(self->cursor) == '[') ? nmjson_type_array : nmjson_type_object;
			json_token_add_(self->token.vessel, self->token.value);
			
			//ネストを一つ下げる。
			self->token.vessel = self->token.value;
			self->token.value = NULL;
			
			//配列ならば、次に来るのは値である
			self->is_treating_value = self->token.vessel->v.type == nmjson_type_array;
			self->is_in_input = 1;
			self->cursor ++;
			continue;
		
		//文字列の開始
		case '\'':
			if(!is_ecma5_(self)){
				//JSON5で許可されたものなので、それ以外はＮＧ。
				parser_put_err_(self, nmjson_error_syntax, "Compat:[\"'\" is allowed on ECMA5]", 1);
				break_loop = 1;
				continue;
			}
		case '"':
			if(!(self->is_in_input) || self->token.vessel == NULL){
				//「入力準備完了」の条件を満たしていない or 「入れるべき器」が存在しない
				parser_put_err_(self, nmjson_error_syntax, "Not state:[!(waiting input) or No any vessel]", 1);
				break_loop = 1;
				continue;
			}
			if(self->token.vessel->v.type == nmjson_type_array || !self->is_treating_value){
				//値トークン無しが許される条件: 配列に入れようとしている or 名前入力モードである
				self->token.value = new_token_(self);
			}
			if(self->token.value == NULL){
				parser_put_err_(self, nmjson_error_tokens, "No more tokens(array | input name)", 1);
				break_loop = 1;
				continue;
			}
			
			//扱う文字列長(=入力が終わったら何バイト飛ばすか)を見定める。
			increment = determine_literal_(self->cursor, self->superset);
			if(increment < 0){
				//文字列としてみなせない
				parser_put_err_(self, nmjson_error_syntax, "Invalid[as literal]", 1);
				break_loop = 1;
				continue;
			}
			else if(increment == 0){
				//尻切れトンボ→不完全として次の展開に託す。
				if(self->token.vessel->v.type == nmjson_type_array || !self->is_treating_value){
					//「値トークン無しが許される条件」を通過した→取った場合なので、二重取得対策が必要。
					del_token_(self, self->token.value);
					self->token.value = NULL;
				}
				parser_put_err_(self, nmjson_error_incomplete, "Waiting to push[as literal]", 0);
				break_loop = 1;
				continue;
			}
			
			//正常に文字列を取り扱えたので、デコードしつつ文字列を確定させる。
			if(token_input_literal_(self->token.value, self->cursor, self->is_treating_value, self->superset) < 0){
				//エスケープの問題やサロゲートペアなどでNGを食らった
				break_loop = 1;
				parser_put_err_(self, nmjson_error_syntax, "Invalid[on decoding literal]", 1);
				continue;
			}
			
			if(self->is_treating_value){
				//値モードで入力→トークンの全てが確定: 晴れて器に加わる。
				json_token_add_(self->token.vessel, self->token.value);
				dbg_printf_("value %s\n", self->token.value->v.value.s);
				self->token.value = NULL;
			}
			else{
				dbg_printf_("name %s\n", self->token.value->n.name);
			}
			dbg_printf_("increment is %zd\n", increment);
			//入力モード終わり。
			self->is_in_input = 0;
			self->cursor += increment;
			continue;
		
		//取り扱い対象を名前→値へ変更する。
		case ':':
			self->cursor ++;
			if(!(!self->is_in_input && !self->is_treating_value)){
				//「入力終了済み、かつ名前モードだった」という条件を満たさなかった
				break_loop = 1;
				parser_put_err_(self, nmjson_error_syntax, "Not state[!is_in_input && !is_treating_value]", 1);
				continue;
			}
			//入力モードON、対象を値へと切り替えて１文字進める。
			self->is_in_input = 1;
			self->is_treating_value = 1;
			continue;
			
		//次の値トークンがあることを示す。
		case ',':
			self->cursor ++;
			if(!(!self->is_in_input && self->is_treating_value)){
				//「入力終了済み、かつ値を扱っていた」という条件を満たせていない
				break_loop = 1;
				parser_put_err_(self, nmjson_error_syntax, "Not state[!is_in_input && is_treating_value]", 1);
				continue;
			}
			
			//入力モードON、配列の下なら取り扱いを値に、オブジェクトの下なら名前から開始。
			self->is_in_input = 1;
			self->is_treating_value = (self->token.vessel->v.type == nmjson_type_array);
			continue;
			
		//器の終わり
		case '}':
		case ']':
			//ここにきてはいけない条件
			//1: 器がない→root作っている最中なのに終わっている。
			//2: 何かの入力中であり、値を確実に構えている(文字的に','の後、':'の前後)
			//→','の後の場合→token.value == NULL, is_in_input == 1, token.vessel->v.len > 0
			//→':'の前後の時→token.value != NULL, is_in_input is x, token.vessel->v.len is x
			do{
				if(self->token.vessel == NULL){
					break_loop = 1;
					parser_put_err_(self, nmjson_error_syntax, "Not state[closing without vessel]", 1);
					break;
				}
				if(self->token.value != NULL){
					//何かしらの値を入力している最中だった(':'の前後)
					break_loop = 1;
					parser_put_err_(self, nmjson_error_syntax, "Not state[closing in inputting]", 1);
					break;
				}
				
				if(self->is_in_input && self->token.vessel->v.len > 0){
					//末尾にカンマがあったのに器が閉じられた: JSONC/JSON5では問題なし！
					break_loop = !is_tail_comma_enabled_(self);
					if(break_loop){
						parser_put_err_(self, nmjson_error_syntax, "Not state[closing after comma]", 1);
					}
					break;
				}
				
			}while(0);
			if(break_loop){
				continue;
			}
			
			
			if(
				(self->token.vessel->v.type == nmjson_type_array && *(self->cursor) != ']') ||
				(self->token.vessel->v.type == nmjson_type_object && *(self->cursor) != '}')
			){
				//閉じる括弧が違う
				break_loop = 1;
				parser_put_err_(self, nmjson_error_syntax, "Invalid[different closure]", 1);
				continue;
			}
			
			if(self->token.vessel->v.type == nmjson_type_object){
				LINEAR_LINKER_SORT_(&(self->token.vessel->v.value.o), token_compare_name_, nmjson_token_t, sibling_link);
			}
			
			//器が終わったのでネストを１個上げる。
			self->token.vessel = self->token.vessel->parent;
			self->is_in_input = 0;
			self->is_treating_value = 1;
			if(self->token.vessel == NULL){
				//ルートの終わり。解析終了
				self->error.code = nmjson_error_complete;
				break_loop = 1;
				parser_put_err_(self, nmjson_error_complete, "COMPLETE", 1);
				continue;
			}
			
			parser_put_err_(self, nmjson_error_incomplete, "Waiting to push[end of obj]", 1);
			continue;
			
		//コメント
		case '/':
			if(!is_comment_enabled_(self)){
				break_loop = 1;
				parser_put_err_(self, nmjson_error_syntax, "Compat[Comment is not allowed plain json]", 1);
				continue;
			}
			
			//どこからどこまでコメントか見極めよう。
			increment = determine_comment_(self->cursor);
			if(increment == 0){
				parser_put_err_(self, nmjson_error_incomplete, "Waiting to push[comment]", 0);
				break_loop = 1;
				continue;
			}
			else if(increment < 0){
				//コメントっぽかったのにコメントの書式に従っていなかった
				parser_put_err_(self, nmjson_error_syntax, "Invalid[as comment]", 1);
				break_loop = 1;
				continue;
			}
			self->cursor += increment;
			continue;
			
		//値専門の文字
		case 't': case 'f':
			//bool値: true/false。大文字を考慮しろと言われたらここに追加。
			input_type = nmjson_type_bool;
			break;
			
		case 'n':
			//null
			input_type = nmjson_type_null;
			break;
			
		case '+': case 'I': case 'N': case '.':
			//JSON5のみ: 明示的な正の値(+), 無限大(Infinity), 無効値(NaN), 小数部のみ(.nnnnn)
			input_type = is_ecma5_(self) ? nmjson_type_number_ : nmjson_type_unused_;
			break;
			
		case '-': 
		case '0': case '1': case '2': case '3': case '4': 
		case '5': case '6': case '7': case '8': case '9': 
			//数字
			input_type = nmjson_type_number_;
			break;
			
		default:
			//その他文字: JSON5で「名前である可能性」が入ってしまったため、相手しなければいけない。
			input_type = is_ecma5_(self) ? nmjson_type_rawstr_:nmjson_type_unused_;
			break;
		}
		
		//値などの入力処理を開始する。
		do{
			//値の入力の前処理: 最後の判定。
			if(input_type == nmjson_type_rawstr_ || input_type == nmjson_type_unused_){
				//値の入力ではない。
				break;
			}
			
			if(self->token.vessel == NULL || !self->is_in_input){
				//入れるべき「器」が存在しない: rootすら作れていない→うちではノーカン。
				//それと、「入力モードではない」というのも普通にNG対象です。はい。
				input_type = nmjson_type_unused_;
				break;
			}
			
			if(!self->is_treating_value){
				//「入力対象が値である」を満たしていない。
				//→JSON5では、ここでもまだ負けを認めない。objectのkeyである可能性があるからだ。
				input_type = is_ecma5_(self) ? nmjson_type_rawstr_ : nmjson_type_unused_;
				break;
			}
			
		}while(0);
		
		if(input_type == nmjson_type_rawstr_){
			//IdentifierName(ecma5.1)を入力するよと言われた
			//→条件: JSON5であること、オブジェクトの配下であること、値が対象でないこと、入力モードであること。
			if(!(
				is_ecma5_(self) && 
				self->token.vessel != NULL && 
				self->token.vessel->v.type == nmjson_type_object && 
				!self->is_treating_value
			)){
				//以上を全て満たせなかったので構文エラー。
				break_loop = 1;
				parser_put_err_(self, nmjson_error_syntax, "Not state[name(ECMA5) && under the object]", 1);
				continue;
			}
			//「名前の入力を始めようとしている」ので、値トークンがまだないことになる。
			if((self->token.value = new_token_(self)) == NULL){
				break_loop = 1;
				parser_put_err_(self, nmjson_error_tokens, "No more tokens[name(ECMA5)]", 1);
				continue;
			}
			
			increment = determine_e5ident_(self->cursor, self->superset);
			if(increment <= 0){
				//identnameとしての解析を行い、打ち切られる
				break_loop = 1;
				if(increment < 0){
					parser_put_err_(self, nmjson_error_syntax, "Invalid[as ident(ecma5)]", 1);
				}
				else{
					parser_put_err_(self, nmjson_error_incomplete, "Waiting to push[name(ECMA5)]", 0);
				
					//値トークンを取ったばかり→次に備えて一度しまう。
					del_token_(self, self->token.value);
					self->token.value = NULL;
				}
				continue;
			}
			
			if(token_input_e5ident_(self->token.value, self->cursor, self->superset) < 0){
				//文句なしの構文エラー
				break_loop = 1;
				parser_put_err_(self, nmjson_error_syntax, "Invalid[as decoding ident(ecma5)]", 1);
				continue;
			}
			self->cursor += increment;
			dbg_printf_("name from ident->%zu[%s]\n", self->token.value->n.len, self->token.value->n.name);
			//入力モード終わり。
			self->is_in_input = 0;
		}
		else if(input_type != nmjson_type_unused_){
			
			if(self->token.vessel->v.type == nmjson_type_array){
				//値トークン無しが許される条件→親が配列。親がオブジェクトならもうトークン持っているはず。
				self->token.value = new_token_(self);
			}
			if(self->token.value == NULL){
				parser_put_err_(self, nmjson_error_tokens, "No more tokens[in inputing values]", 1);
				break_loop = 1;
				continue;
			}
			
			//本格的に値を入れる。
			switch(input_type){
			case nmjson_type_number_:
				increment = token_parse_number_(self->token.value, self->cursor, self->superset);
				break;
			case nmjson_type_bool:
				increment = token_parse_bool_(self->token.value, self->cursor, self->superset);
				break;
			case nmjson_type_null:
				increment = token_parse_null_(self->token.value, self->cursor, self->superset);
				break;
			default:
				increment = -1;
			}
			
			if(increment < 0){
				parser_put_err_(self, nmjson_error_syntax, "Invalid[in inputing values]", 1);
				break_loop = 1;
				continue;
			}
			else if(increment == 0){
				//尻切れトンボ→不完全として次の展開に託す。
				if(self->token.vessel->v.type == nmjson_type_array){
					//「値トークン無しが許される条件」を通過した場合、一回解放しておく。
					del_token_(self, self->token.value);
					self->token.value = NULL;
				}
				parser_put_err_(self, nmjson_error_incomplete, "Waiting to push[in inputing values]", 0);
				break_loop = 1;
				continue;
			}
			
			//終わったので値トークンを格納し、文字を進める。
			json_token_add_(self->token.vessel, self->token.value);
			self->token.value = NULL;
			self->cursor += increment;
			self->is_in_input = 0;
			
		}
		else{
			//不正値
			parser_put_err_(self, nmjson_error_syntax, "Compat[May be identifier(ecma5).Do in JSON5]", 1);
			break_loop = 1;
			continue;
		}
		input_type = nmjson_type_unused_;
		
	}
	
	return (size_t)((uintptr_t)self->cursor - (uintptr_t)src);
}


