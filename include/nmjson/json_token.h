/**
 *	\file	json_token.h
 *	\brief	JSONの値を示すトークンの構造体。
 */
#ifndef	NMJSON_JSON_TOKEN_H_
#define	NMJSON_JSON_TOKEN_H_

#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./json_const.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct nmjson_token_s;

/**
 *	\brief	JSONの値本体部分。
 */
typedef union {
	void*					n;
	char*					s;
	int64_t					i;
	double					d;
	struct nmjson_token_s*	o;
	int						b;
} nmjson_value_t;

/**
 *	\brief	JSONのリンクリスト用構造体
 */
struct nmjson_token_link_s {
	struct nmjson_token_s	*next;
	struct nmjson_token_s	**to_self;
};

/**
 *	\brief	JSONトークン
 */
struct nmjson_token_s {
	
	///\note parser/maker内でのalloc管理
	struct nmjson_token_link_s	alloc_link;
	
	/// \note オブジェクトのキーネーム管理
	struct {
		char*			name;
		size_t			len;
		int				is_alloced;
	} n;
	
	/// \note 値管理。
	struct {
		
		/// \brief	値
		nmjson_value_t	value;
		
		/// \brief	文字列の場合は文字列長。オブジェクト系なら子の数。
		size_t			len;
		
		/// \brief	文字列の場合にその文字列がヒープメモリ出身かどうか。
		int				is_alloced;
		nmjson_type_t	type;
	} v;
	
	/// \note 親トークンへのリンク
	struct nmjson_token_s		*parent;
	
	/// \note 同一親トークン内の兄弟リンク。
	struct nmjson_token_link_s	sibling_link;
	
	/// \note	このトークン自身がalloc出身かどうか。
	int					is_alloced;
	
};

typedef struct nmjson_token_s nmjson_token_t;

ssize_t nmjson_token_fout(const nmjson_token_t *self, FILE *fp);
ssize_t	nmjson_token_fout2(const nmjson_token_t *self, FILE *fp, int easy_to_look, int raw_utf);
ssize_t	nmjson_token_fout3(const nmjson_token_t *self, FILE *fp, int easy_to_look, int raw_utf, nmjson_superset_t superset);

/// \brief	トークンの値をある程度強引に合わせつつ整数値として読む。
static inline int64_t nmjson_token_as_int(const nmjson_token_t *token, int64_t def){
	
	switch(token->v.type){
	case nmjson_type_float:
		return (int64_t)(token->v.value.d);
		
	case nmjson_type_integer:
		return token->v.value.i;
		
	case nmjson_type_string:
		{
			char *endptr = NULL;
			const char *startptr = token->v.value.s;
			int cmp_len = 2;
			int base = 10;
			int is_minus = 0;
			while(isspace((uint8_t)*startptr) && *startptr != '\0'){
				startptr ++;
				cmp_len ++;
			}
			switch(*startptr){
			case '-':
				is_minus = 1;
			case '+':
				cmp_len ++;
				startptr ++;
			}
			
			if(token->v.len > cmp_len){
				if(memcmp(startptr, "0x", 2) == 0){
					base = 16;
				}
				else if(memcmp(startptr, "0o", 2) == 0){
					base = 8;
					startptr += 2;
				}
				else if(memcmp(startptr, "0b", 2) == 0){
					base = 2;
					startptr += 2;
				}
			}
			
			int64_t ret = is_minus ? strtoll(token->v.value.s, &endptr, base) : strtoull(token->v.value.s, &endptr, base);
			switch(*endptr){
			case '\0':
			case ',':
			case '"':
			case '\'':
				return ret;
			default:
				return isspace((uint8_t)*endptr) ? ret:def;
			}
		}
	
	case nmjson_type_bool:
		return (int64_t)(token->v.value.b);
	
	default:
		return def;
	}
}

/// \brief	トークンの値をある程度強引に合わせつつ実数値として読む。
static inline double nmjson_token_as_float(const nmjson_token_t *token, double def){
	switch(token->v.type){
	case nmjson_type_float:
		return (token->v.value.d);
	case nmjson_type_integer:
		return (double)(token->v.value.i);
	case nmjson_type_string:
		{
			char *endptr = NULL;
			double ret = strtod(token->v.value.s, &endptr);
			return (*endptr != '\0' && !isspace((uint8_t)*endptr)) ? def:ret;
		}
	
	default:
		return def;
	}
}

/// \brief	トークンの値が文字列であった場合に参照する。
static inline const char* nmjson_token_as_string(const nmjson_token_t *token, const char *def){
	switch(token->v.type){
	case nmjson_type_string:
		return token->v.value.s;
	default:
		return def;
	}
}

/// \brief	トークンの値をある程度強引に合わせつつ、boolとして読む。
static inline int nmjson_token_as_bool(const nmjson_token_t *token){
	switch(token->v.type){
	case nmjson_type_bool:
		return token->v.value.b;
	case nmjson_type_integer:
		return token->v.value.i != 0;
	case nmjson_type_string:
		{
			size_t rm_len;
			const char *cursor = token->v.value.s;
			for(rm_len = token->v.len; rm_len >= 0; rm_len --){
				if(!isspace((uint8_t)*cursor)){ break; }
				cursor ++;
			}
			
			switch(rm_len){
			case 4:
				return strncasecmp(cursor, "true", 4) == 0 ? 1:-1;
			case 5:
				return strncasecmp(cursor, "false", 5) == 0 ? 0:-1;
			default:
				return -1;
			}
		}
		break;
	default:
		return -1;
	}
}

/// \brief	トークンの配下にあるオブジェクトの一番最初を参照する。
static inline const nmjson_token_t* nmjson_token_head_child(const nmjson_token_t *token){
	return (token->v.type & nmjson_type_parent_) ? token->v.value.o : NULL;
}

/// \brief	（配列下で使用）同じ配列下の次トークンを取る。
static inline const nmjson_token_t* nmjson_token_next(const nmjson_token_t *token){
	return token->sibling_link.next;
}

/// \brief	（主に配列で使用？）インデックスを指定して子トークンを取る。マイナスの場合は末尾からの指定。
static inline const nmjson_token_t* nmjson_token_nth_child(const nmjson_token_t *token, int index){
	if(!(token->v.type & nmjson_type_parent_)){
		return NULL;
	}
	
	int i = 0;
	const nmjson_token_t *child;
	if(index < 0){
		index += token->v.len;
		if(index < 0){
			return NULL;
		}
	}
	
	for(child = nmjson_token_head_child(token); child != NULL; child = nmjson_token_next(child)){
		if(i == index){
			break;
		}
		i++;
	}
	return child;
}

/// \brief	オブジェクト配下にあるトークンを、名前を指定して取得する。
static inline const nmjson_token_t* nmjson_object_token(const nmjson_token_t *obj, const char *name){
	
	if(obj->v.type != nmjson_type_object){
		return NULL;
	}
	
	size_t name_len = strlen(name);
	const nmjson_token_t *child;
	ssize_t cmpret;
	
	/// \note オブジェクトでの並び順→「文字列長」,「memcmp」の順で比較した結果の昇順
	for(child = nmjson_token_head_child(obj); child != NULL; child = nmjson_token_next(child)){
		cmpret = (ssize_t)name_len - (ssize_t)child->n.len;
		//printf("%s[%p]: length compare(%zd vs %zd -> %zd)\n", __func__, obj, name_len, child->n.len, cmpret);
		if(cmpret > 0){
			continue;
		}
		else if(cmpret < 0){
			child = NULL;
			break;
		}
		
		cmpret = memcmp(name, child->n.name, name_len);
		//printf("%s[%p]: memcmp(%s vs %s -> %zd)\n", __func__, obj, name, child->n.name, cmpret);
		if(cmpret > 0){
			continue;
		}
		else if(cmpret < 0){
			child = NULL;
			break;
		}
		break;
	}
	
	return child;
}

/// \brief	オブジェクト配下にある特定名の値を、整数値として読む。
static inline int64_t nmjson_object_get_int(const nmjson_token_t *obj, const char *name, int64_t def){
	const nmjson_token_t *child = nmjson_object_token(obj, name);
	return (child == NULL) ? def:nmjson_token_as_int(child, def);
}

/// \brief	オブジェクト配下にある特定名の値を、実数値として読む。
static inline double nmjson_object_get_float(const nmjson_token_t *obj, const char *name, double def){
	const nmjson_token_t *child = nmjson_object_token(obj, name);
	return (child == NULL) ? def:nmjson_token_as_float(child, def);
}

/// \brief	オブジェクト配下にある特定名の値を真偽値として読む。存在しない、あるいはboolではない場合は-1。
static inline int nmjson_object_get_bool(const nmjson_token_t *obj, const char *name){
	const nmjson_token_t *child = nmjson_object_token(obj, name);
	return (child == NULL) ? -1 : (int)(nmjson_token_as_bool(child));
}

/// \brief	オブジェクト配下にある特定名の値が明確にtrueである場合に1。
static inline int nmjson_object_is_true(const nmjson_token_t *obj, const char *name){
	return nmjson_object_get_bool(obj, name) == 1;
}

/// \brief オブジェクト配下にある特定名の値が明確にfalseである場合に1。
static inline int nmjson_object_is_false(const nmjson_token_t *obj, const char *name){
	return nmjson_object_get_bool(obj, name) == 0;
}

/// \brief オブジェクト配下にある特定名の値が明確にnullである場合に1。
static inline int nmjson_object_is_nullobj(const nmjson_token_t *obj, const char *name){
	const nmjson_token_t *child = nmjson_object_token(obj, name);
	return (child == NULL) ? 0 : child->v.type == nmjson_type_null;
}

/// \brief	オブジェクト配下にある特定名の値を、文字列として読む。
static inline const char* nmjson_object_get_string(const nmjson_token_t *obj, const char *name, const char *def){
	const nmjson_token_t *child = nmjson_object_token(obj, name);
	return (child == NULL) ? def : nmjson_token_as_string(child, def);
}

/// \brief	オブジェクト配下にある特定名の値を、オブジェクトとして取り出す。
static inline const nmjson_token_t* nmjson_object_get_object(const nmjson_token_t *obj, const char *name){
	const nmjson_token_t *child = nmjson_object_token(obj, name);
	if(child != NULL && child->v.type == nmjson_type_object){
		return child;
	}
	return NULL;
}

/// \brief	オブジェクト配下にある特定名の値を、配列として取り出す。
static inline const nmjson_token_t* nmjson_object_get_array(const nmjson_token_t *obj, const char *name){
	const nmjson_token_t *child = nmjson_object_token(obj, name);
	if(child != NULL && child->v.type == nmjson_type_array){
		return child;
	}
	return NULL;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* !NMJSON_JSON_TOKEN_H_ */
