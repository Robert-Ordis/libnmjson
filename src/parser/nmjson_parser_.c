#include "include/nmjson/json_const.h"
#include "include/nmjson/json_token.h"
#include "include/nmjson/json_parser.h"

#include <stdlib.h>

#include "../local_header/linear_linker.h"
#include "../local_header/token_tree.h"

/// \brief	トークンが持つヒープ出身の文字列を解放する
static void drop_strings_(nmjson_token_t *t){
	if(t->alloced.name && t->n.name != NULL){
		free(t->n.name);
	}
	if(t->alloced.value && t->v.type == nmjson_type_string && t->v.value.s != NULL){
		free(t->v.value.s);
	}
	t->n.name = NULL;
	t->alloced.name = 0;
	t->alloced.value = 0;
}

/// \brief	トークンを未使用リストに戻す(malloc出身のものも再利用のため戻す)
static void push_free_(nmjson_parser_t *self, nmjson_token_t *t){
	drop_strings_(t);
	LINEAR_LINKER_INSERT_AFTER_(&(self->free_head), t, NULL, nmjson_token_t, link);
}

/**
 *	\brief		root以下の木を帰りがけ順で未使用リストに戻す(再帰なし)
 *	\remarks	戻したトークンのリンクは書き換わるため、次・親は戻す前に読む。
 */
static void release_tree_(nmjson_parser_t *self, nmjson_token_t *root){
	nmjson_token_t *t = root;
	while(t != NULL){
		//最も左の葉(または空のコンテナ)まで降りる
		while(token_is_container_(t) && t->v.value.o != NULL){
			t = t->v.value.o;
		}
		for(;;){
			nmjson_token_t *sib = (t == root) ? NULL : (nmjson_token_t *)nmjson_token_next(t);
			nmjson_token_t *par = (t == root) ? NULL : t->parent;
			push_free_(self, t);
			if(sib != NULL){
				t = sib;	//兄弟の部分木へ(外側のループで降りる)
				break;
			}
			if(par == NULL){
				t = NULL;	//rootまで戻した
				break;
			}
			t = par;		//子を全て戻したので親を戻す(降りない)
		}
	}
}

nmjson_parser_t*	nmjson_parser_init(nmjson_parser_t *self, nmjson_token_t *tokens, int tokens_len){
	int i;
	memset(self, 0, sizeof(nmjson_parser_t));

	for(i = 0; i < tokens_len; i++){
		nmjson_token_t *ptoken = &tokens[i];
		memset(ptoken, 0, sizeof(nmjson_token_t));
		push_free_(self, ptoken);
	}
	//トークン配列を渡されなかった場合のみmallocを許可する。
	//(resetで判定すると、malloc出身のトークンが未使用リストに戻った時点で許可が外れてしまう)
	self->is_alloc_allowed = (self->free_head == NULL);
	nmjson_parser_reset(self);
	return self;
}

void				nmjson_parser_reset(nmjson_parser_t *self){
	nmjson_token_t *stray = self->token.value;

	//組み立て途中で木に未接続のトークン
	if(stray != NULL && stray->parent == NULL && stray != self->token.root){
		push_free_(self, stray);
	}
	//木全体
	if(self->token.root != NULL){
		release_tree_(self, self->token.root);
	}
	self->is_treating_value = 1;
	self->is_in_input = 1;
	self->cursor = NULL;
	self->is_initial = 1;
	self->superset = nmjson_superset_none;
	memset(&(self->token), 0, sizeof(self->token));
	self->error.code = nmjson_error_incomplete;
	self->error.detail = "Wait to push[misc]";
}

void				nmjson_parser_deinit(nmjson_parser_t *self){
	nmjson_token_t *t;
	
	//使用中のトークンを全て未使用リストに戻す
	nmjson_parser_reset(self);
	
	//未使用リストを切り離し、malloc出身のものだけ解放する(配列出身のものは利用者の持ち物)
	t = self->free_head;
	self->free_head = NULL;
	while(t != NULL){
		nmjson_token_t *next = t->link.next;
		if(t->alloced.self){
			free(t);
		}
		t = next;
	}
	self->is_alloc_allowed = 0;
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
	nmjson_token_t *targets[2];
	int i;

	if(self->cursor != NULL && IN_OLD_(self->cursor)){
		self->cursor = TO_NEW_(self->cursor);
		count ++;
	}
	//使用中のトークン = 木全体 + 組み立て途中で未接続のトークン
	targets[0] = self->token.root;
	targets[1] = (self->token.value != NULL && self->token.value->parent == NULL && self->token.value != self->token.root)
		? self->token.value : NULL;
	for(i = 0; i < 2; i++){
		for(t = targets[i]; t != NULL; t = (i == 0) ? token_tree_next_(t, targets[0]) : NULL){
			if(t->n.name != NULL && !t->alloced.name && IN_OLD_(t->n.name)){
				t->n.name = TO_NEW_(t->n.name);
				count ++;
			}
			// v.valueは共用体なので、文字列の時だけ見る
			if(t->v.type == nmjson_type_string && t->v.value.s != NULL && !t->alloced.value && IN_OLD_(t->v.value.s)){
				t->v.value.s = TO_NEW_(t->v.value.s);
				count ++;
			}
		}
	}
	return count;
	#undef	IN_OLD_
	#undef	TO_NEW_
}
