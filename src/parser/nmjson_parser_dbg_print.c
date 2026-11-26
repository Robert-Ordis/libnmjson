#include "include/nmjson/json_const.h"
#include "include/nmjson/json_token.h"
#include "include/nmjson/json_parser.h"

#include "../local_header/linear_linker.h"
#include "../local_header/token_tree.h"

ssize_t				nmjson_parser_dbg_print(nmjson_parser_t *self, int (*print_func)(const char *fmt, ...)){
	int act_count, free_count;
	nmjson_token_t *tokenc;
	ssize_t ret = 0;
	//使用中 = 木全体 + 組み立て途中で未接続のトークン
	act_count = 0;
	for(tokenc = self->token.root; tokenc != NULL; tokenc = token_tree_next_(tokenc, self->token.root)){
		act_count ++;
	}
	if(self->token.value != NULL && self->token.value->parent == NULL && self->token.value != self->token.root){
		act_count ++;
	}
	
	tokenc = self->free_head;
	free_count = 0;
	while(tokenc){
		tokenc = LINEAR_LINKER_NEXT_(tokenc, nmjson_token_t, link);
		free_count ++;
	}
	
	ret += print_func("%s:\n", __func__);
	ret += print_func("treating_value[%d], in_input[%d], cursor[%p(%c)],\n", 
		self->is_treating_value, self->is_in_input, self->cursor, *(self->cursor));
	ret += print_func("root[%p], vessel[%p], value[%p], \n", self->token.root, self->token.vessel, self->token.value);
	if(self->token.vessel != NULL){
		tokenc = self->token.vessel;
		ret += print_func("vessel: type[%d](%zu), name[%s](%zu)\n", tokenc->v.type, (size_t)tokenc->v.len, tokenc->n.name, (size_t)tokenc->n.len);
	}
	if(self->token.value != NULL){
		tokenc = self->token.value;
		ret += print_func("value: type[%d](%zu), name[%s](%zu)\n", tokenc->v.type, (size_t)tokenc->v.len, tokenc->n.name, (size_t)tokenc->n.len);
	}
	ret += print_func("freeCount[%d], activeCount[%d],  error[%d]->%s\n", 
		free_count, act_count, self->error.code, self->error.detail);
		
	return ret;
}
