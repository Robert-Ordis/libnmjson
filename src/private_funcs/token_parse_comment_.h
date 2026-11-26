#include <errno.h>
#include <math.h>

/**
 *	\brief		コメントの範囲を決定する
 *	\return		 > 0	: コメントのバイト長(1行コメントの場合、行終端は含まない)
 *	\return		== 0	: 入力が途中で途切れている
 *	\return		 < 0	: コメントではない
 *	\remarks	1行コメントの終わりは CR / LF。extspaceが有効なら U+2028 / U+2029 も(JSON5のLineTerminator)
 */
static ssize_t determine_comment_(char *str, nmjson_superset_t superset){
	char *end_ptr;
	char *cursor = str + 1;
	const static char* end_multicomment = "*/";
	switch(*(cursor)){
	case '*':	// "/*....*/"の書式。
		end_ptr = strstr(cursor, end_multicomment);
		if(end_ptr){
			end_ptr += strlen(end_multicomment);
		}
		break;
		
	case '/':	// "// ... <CR>or<LF>"の書式。
		end_ptr = NULL;
		for(cursor = cursor + 1; *cursor != '\0'; cursor ++){
			if(*cursor == '\r' || *cursor == '\n'){
				end_ptr = cursor;
				break;
			}
			//U+2028(E2 80 A8) / U+2029(E2 80 A9)
			if(nmjson_superset_has_extspace(superset) && (uint8_t)cursor[0] == 0xE2){
				//後続バイトが途中で途切れている→コメントの終わりはまだ無い(incomplete)
				//(短絡評価により範囲外は読まないが、先読みの前提を明示するため個別に確認する)
				if(cursor[1] == '\0' || cursor[2] == '\0'){
					break;
				}
				if((uint8_t)cursor[1] == 0x80 && ((uint8_t)cursor[2] == 0xA8 || (uint8_t)cursor[2] == 0xA9)){
					end_ptr = cursor;
					break;
				}
			}
		}
		break;
		
	case '\0':	//断定できるほど文字列が出来上がっていない
		end_ptr = NULL;
		break;
		
	default:	//コメントじゃない
		return -1;
	}
	
	if(end_ptr == NULL){
		//コメントの終わりが見つからなかった
		return 0;
	}
	
	return (ssize_t)((uintptr_t) end_ptr - (uintptr_t) str);
}
