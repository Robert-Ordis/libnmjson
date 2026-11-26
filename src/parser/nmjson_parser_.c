#include "include/nmjson/json_const.h"
#include "include/nmjson/json_token.h"
#include "include/nmjson/json_parser.h"

#include "../local_header/linear_linker.h"

nmjson_parser_t*	nmjson_parser_init(nmjson_parser_t *self, nmjson_token_t *tokens, int tokens_len){
	int i;
	memset(self, 0, sizeof(nmjson_parser_t));
	
	for(i = 0; i < tokens_len; i++){
		nmjson_token_t *ptoken = &tokens[i];
		memset(ptoken, 0, sizeof(nmjson_token_t));
		LINEAR_LINKER_INSERT_AFTER_(&(self->free_head), ptoken, NULL, nmjson_token_t, alloc_link);
	}
	nmjson_parser_reset(self);
	return self;
}

void				nmjson_parser_reset(nmjson_parser_t *self){
	
	nmjson_token_t *ptoken;
	LINEAR_LINKER_POP_(&(self->active_head), &ptoken, nmjson_token_t, alloc_link);
	while(ptoken){
		LINEAR_LINKER_INSERT_AFTER_(&(self->free_head), ptoken, NULL, nmjson_token_t, alloc_link);
		LINEAR_LINKER_POP_(&(self->active_head), &ptoken, nmjson_token_t, alloc_link);
	}
	self->is_treating_value = 1;
	self->is_in_input = 1;
	self->cursor = NULL;
	self->is_initial = 1;
	self->is_alloc_allowed = (self->free_head == NULL);
	self->superset = nmjson_superset_none;
	memset(&(self->token), 0, sizeof(self->token));
	self->error.code = nmjson_error_incomplete;
	self->error.detail = "Wait to push[misc]";
}

void				nmjson_parser_reset_as_superset(nmjson_parser_t *self, nmjson_superset_t superset){
	nmjson_parser_reset(self);
	self->superset = superset;
}


const nmjson_token_t*	nmjson_parser_get_root(nmjson_parser_t *self){
	return self->token.root;
}

size_t				nmjson_parser_rebase(nmjson_parser_t *self, uintptr_t old_base, size_t used_len, char *new_base){
	// 旧バッファの[先頭, 終端'\0']を指していれば付け替える。数値として比較するので旧領域は参照しない。
	#define	IN_OLD_(p)	((uintptr_t)(p) >= old_base && (uintptr_t)(p) <= old_base + used_len)
	#define	TO_NEW_(p)	(new_base + ((uintptr_t)(p) - old_base))
	size_t count = 0;
	nmjson_token_t *t;

	if(self->cursor != NULL && IN_OLD_(self->cursor)){
		self->cursor = TO_NEW_(self->cursor);
		count ++;
	}
	for(t = self->active_head; t != NULL; t = LINEAR_LINKER_NEXT_(t, nmjson_token_t, alloc_link)){
		if(t->n.name != NULL && !t->n.is_alloced && IN_OLD_(t->n.name)){
			t->n.name = TO_NEW_(t->n.name);
			count ++;
		}
		// v.valueは共用体なので、文字列の時だけ見る
		if(t->v.type == nmjson_type_string && t->v.value.s != NULL && !t->v.is_alloced && IN_OLD_(t->v.value.s)){
			t->v.value.s = TO_NEW_(t->v.value.s);
			count ++;
		}
	}
	return count;
	#undef	IN_OLD_
	#undef	TO_NEW_
}
