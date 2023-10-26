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
	struct nmjson_token_s	*active_head;
	
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
void				nmjson_parser_reset_as_superset(nmjson_parser_t *self, nmjson_superset_t superset);
size_t				nmjson_parser_parse(nmjson_parser_t *self, char *src);
ssize_t				nmjson_parser_dbg_print(nmjson_parser_t *self, int (*print_func)(const char *fmt, ...));

const nmjson_token_t*	nmjson_parser_get_root(nmjson_parser_t *self);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* !NMJSON_JSON_PARSER_H_ */
