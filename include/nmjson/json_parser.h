/**
 *	\file	json_token.h
 *	\brief	JSONの値を示すトークンの構造体。
 */
#ifndef	NMJSON_JSON_PARSER_H_
#define	NMJSON_JSON_PARSER_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <inttypes.h>
#include <stddef.h>

#include <sys/types.h>

#include "./json_const.h"

#include "./json_token.h"

struct nmjson_parser_s {
	
	int		is_treating_value;
	int		is_in_input;
	char	*cursor;
	int		is_initial;
	int		is_alloc_allowed;
	nmjson_superset_t	superset;
	
	struct nmjson_token_s	*free_head;
	
	struct {
		struct nmjson_token_s	*root;
		struct nmjson_token_s	*vessel;
		struct nmjson_token_s	*value;
	}token;
	
	struct {
		nmjson_error_t		code;
		const char			*detail;
	}error;
};

typedef struct nmjson_parser_s nmjson_parser_t;

nmjson_parser_t*	nmjson_parser_init(nmjson_parser_t *self, nmjson_token_t *tokens, int tokens_len);
void				nmjson_parser_reset(nmjson_parser_t *self);

/**
 *	\brief		パーサーが確保した資源(malloc出身のトークン等)を解放する
 *	\arg		self		: パーサー
 *	\remarks	init と対で呼ぶこと。以後、再度 init するまで解析には使えない(トークン不足になる)。
 *	\remarks	トークン配列を渡して初期化した場合(mallocなし)は、配列には触れず実質何もしない。
 *				mallocの有無に関わらず常に呼んでおけば、後からmalloc版へ切り替えても利用側を直さずに済む。
 *	\remarks	2回呼んでも安全。
 */
void				nmjson_parser_deinit(nmjson_parser_t *self);
void				nmjson_parser_reset_as_superset(nmjson_parser_t *self, nmjson_superset_t superset);
size_t				nmjson_parser_parse(nmjson_parser_t *self, char *src);
ssize_t				nmjson_parser_dbg_print(nmjson_parser_t *self, int (*print_func)(const char *fmt, ...));

const nmjson_token_t*	nmjson_parser_get_root(nmjson_parser_t *self);

/**
 *	\brief		読み込み元バッファの移動に合わせて、パーサー内のポインタを付け替える
 *	\arg		self		: パーサー
 *	\arg		old_base	: 移動前のバッファ先頭アドレス(数値)。realloc等で解放された後でも使えるよう、移動前に控えておくこと
 *	\arg		used_len	: 使用中の長さ(終端'\0'を除く)
 *	\arg		new_base	: 移動後のバッファ先頭
 *	\return		付け替えたポインタの数
 *	\pre		new_base の先頭 used_len + 1 バイトは、移動前のバッファと同じ内容であること。
 *				解析済みの部分はその場でエスケープ解除・終端されているため、受信した生データを入れ直してはならない。
 *	\remarks	付け替え対象: 解析の再開位置、使用中トークンのキー名・文字列値。
 *				利用者が保持している文字列ポインタ(nmjson_token_as_string等の戻り値)は付け替えられない。
 */
size_t				nmjson_parser_rebase(nmjson_parser_t *self, uintptr_t old_base, size_t used_len, char *new_base);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* !NMJSON_JSON_PARSER_H_ */
