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