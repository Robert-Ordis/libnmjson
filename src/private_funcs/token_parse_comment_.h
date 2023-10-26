#include <errno.h>
#include <math.h>

static ssize_t determine_comment_(char *str){
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
		end_ptr = strpbrk(cursor, "\r\n");
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
