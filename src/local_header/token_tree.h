/**
 *	\file		token_tree.h
 *	\brief		トークン木の走査（再帰なし）
 *	\remarks	使用中トークンの一覧(リスト)を持たないため、リセット・rebase・デバッグ出力では
 *				ルートから木をたどって使用中のトークンを列挙する。
 *				組み立て途中のトークン(parser.token.value)は木に未接続なので、呼び出し側で別途扱うこと。
 */
#ifndef	LOCAL_TOKEN_TREE_H_
#define	LOCAL_TOKEN_TREE_H_

#include "include/nmjson/json_token.h"

/// \brief	子を持つトークンか(v.typeは未確定時に負値を取るため、ビット演算ではなく値で比較する)
static inline int token_is_container_(const nmjson_token_t *t){
	return t->v.type == nmjson_type_object || t->v.type == nmjson_type_array;
}

/**
 *	\brief		行きがけ順で次のトークンを返す
 *	\arg		t		: 現在のトークン
 *	\arg		root	: 走査範囲のルート
 *	\return		次のトークン。root の部分木を出たら NULL
 */
static inline nmjson_token_t *token_tree_next_(nmjson_token_t *t, const nmjson_token_t *root){
	if(token_is_container_(t) && t->v.value.o != NULL){
		return t->v.value.o;
	}
	while(t != NULL && t != root){
		if(nmjson_token_next(t) != NULL){
			return (nmjson_token_t *)nmjson_token_next(t);
		}
		t = t->parent;
	}
	return NULL;
}

#endif	/* !LOCAL_TOKEN_TREE_H_ */
