/**
 *	\file	28-token-pool.c
 *	\brief	トークンプールの検証（失敗で exit(1)）
 *			- どんな入力(正常/構文エラー/トークン不足/途中切れ/1バイトずつ)の後でも、
 *			  リセットで全トークンが未使用リストに戻ること(プールが目減りしないこと)
 *			- トークン構造体のサイズ
 *			- malloc版: リセット後もmallocが許可されたままであること
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/nmjson/nmjson.h"

#define POOL_N		24
#define SRC_NUM		512

/// \brief	未使用リストのリンク(構造体の変更に追従するためここに集約)
#define	FREE_NEXT_(t)	((t)->link.next)

static int g_ng = 0;

static int count_free_(const nmjson_parser_t *p){
	int n = 0;
	const nmjson_token_t *t;
	for(t = p->free_head; t != NULL; t = FREE_NEXT_(t)){
		n ++;
		if(n > POOL_N * 2){
			return -1;	//循環している
		}
	}
	return n;
}

static const char *k_inputs[] = {
	// 正常
	"{\"a\":1,\"b\":[1,2,{\"c\":\"x\"}],\"d\":{\"e\":{\"f\":[]}},\"g\":null}",
	"[[[[[\"deep\"]]]],{},[],\"s\",-1.5e3,true,false,null]",
	"{a:1, 'b':'x', c:[.5, +1, 0x10, Infinity], // c\n d:{e:NaN,},}",
	// 構文エラー(様々な深さ・状態)
	"{\"a\":1,\"b\":[1,2,{\"c\":x}]}",
	"{\"a\":[1,2,]]}",
	"{\"a\" \"b\"}",
	"[1,{\"k\":}]",
	"{\"a\":{\"b\":{\"c\":1}}}}",
	"[\"bad\\q\"]",
	"{a b:1}",
	"[1 2]",
	// トークン不足(POOL_N を超える要素)
	"[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30]",
	"{\"a\":[[[[[[[[[[[[[[[[[[[[[[[[[[1]]]]]]]]]]]]]]]]]]]]]]]]]}",
};

#define ARRAY_LEN_(a) (sizeof(a) / sizeof((a)[0]))

static void check_pool_(nmjson_parser_t *p, const char *label, size_t idx, size_t cut){
	int n = count_free_(p);
	if(n != POOL_N){
		fprintf(stderr, "test28: NG: [%s] input %zu cut %zu: free=%d (expected %d)\n", label, idx, cut, n, POOL_N);
		g_ng ++;
	}
}

/// \brief	各入力の全ての長さの途中切れでパース→リセット
static void test_prefixes_(void){
	static nmjson_token_t tokens[POOL_N];
	static char src[SRC_NUM];
	nmjson_parser_t p;
	size_t i, cut;
	nmjson_parser_init(&p, tokens, POOL_N);
	for(i = 0; i < ARRAY_LEN_(k_inputs); i++){
		size_t len = strlen(k_inputs[i]);
		for(cut = 1; cut <= len; cut++){
			memcpy(src, k_inputs[i], cut);
			src[cut] = '\0';
			nmjson_parser_reset_as_superset(&p, nmjson_superset_json5);
			nmjson_parser_parse(&p, src);
			nmjson_parser_reset(&p);
			check_pool_(&p, "prefix", i, cut);
		}
	}
}

/// \brief	1バイトずつ供給(パーサーの再開を繰り返す)→リセット
static void test_bytewise_(void){
	static nmjson_token_t tokens[POOL_N];
	static char src[SRC_NUM];
	nmjson_parser_t p;
	size_t i, k;
	nmjson_parser_init(&p, tokens, POOL_N);
	for(i = 0; i < ARRAY_LEN_(k_inputs); i++){
		size_t len = strlen(k_inputs[i]);
		nmjson_parser_reset_as_superset(&p, nmjson_superset_json5);
		for(k = 1; k <= len; k++){
			memcpy(src, k_inputs[i], k);
			src[k] = '\0';
			nmjson_parser_parse(&p, src);
			if(!nmjson_error_is_continuable(p.error.code) || p.error.code == nmjson_error_complete){
				break;
			}
		}
		nmjson_parser_reset(&p);
		check_pool_(&p, "bytewise", i, k);
	}
}

/// \brief	リセットを挟まず init し直した場合も問題ないこと / 正常入力の値が壊れていないこと
static void test_values_after_reuse_(void){
	static nmjson_token_t tokens[POOL_N];
	static char src[SRC_NUM];
	nmjson_parser_t p;
	int round;
	nmjson_parser_init(&p, tokens, POOL_N);
	for(round = 0; round < 50; round++){
		const nmjson_token_t *root;
		snprintf(src, sizeof(src), "%s", k_inputs[round % 3]);
		nmjson_parser_reset_as_superset(&p, nmjson_superset_json5);
		nmjson_parser_parse(&p, src);
		if(p.error.code != nmjson_error_complete || (root = nmjson_parser_get_root(&p)) == NULL){
			fprintf(stderr, "test28: NG: reuse round %d error=%d\n", round, p.error.code);
			g_ng ++;
			break;
		}
		if(round % 3 == 0 && nmjson_object_get_int(root, "a", -1) != 1){
			fprintf(stderr, "test28: NG: reuse round %d value\n", round);
			g_ng ++;
		}
		// わざとエラー入力を挟む
		snprintf(src, sizeof(src), "%s", k_inputs[3 + round % 8]);
		nmjson_parser_reset_as_superset(&p, nmjson_superset_json5);
		nmjson_parser_parse(&p, src);
	}
	nmjson_parser_reset(&p);
	check_pool_(&p, "reuse", 0, 0);
}

/// \brief	malloc版: トークン配列なしで初期化し、リセット後も「前回より大きな」入力を読めること
///	\remarks	前回と同じ大きさだと未使用リストに戻ったトークンだけで足りてしまい、mallocの許可が外れる不具合を検出できない
static void test_malloc_mode_(void){
	static char src[SRC_NUM];
	nmjson_parser_t p;
	int round;
	nmjson_parser_init(&p, NULL, 0);
	for(round = 0; round < 3; round++){
		const nmjson_token_t *root;
		int elems = 40 + round * 30;	// 40, 70, 100
		size_t n = 0;
		int i;
		n += snprintf(&src[n], sizeof(src) - n, "[");
		for(i = 0; i < elems; i++){
			n += snprintf(&src[n], sizeof(src) - n, "%s%d", i ? "," : "", i);
		}
		snprintf(&src[n], sizeof(src) - n, "]");
		nmjson_parser_reset(&p);
		nmjson_parser_parse(&p, src);
		root = nmjson_parser_get_root(&p);
		if(p.error.code != nmjson_error_complete || root == NULL || root->v.len != (uint32_t)elems){
			// 以前はリセット後にmallocの許可が外れ、前回より大きい入力で nmjson_error_tokens になっていた
			fprintf(stderr, "test28: NG: malloc mode round %d (%d elems) error=%d\n", round, elems, p.error.code);
			g_ng ++;
		}
	}
	// 解析途中(木 + 組み立て途中のトークンが残っている状態)で deinit しても全て解放されること
	snprintf(src, sizeof(src), "{\"a\":[1,2,{\"b\":\"x\"},[3,4");
	nmjson_parser_reset(&p);
	nmjson_parser_parse(&p, src);
	if(p.error.code != nmjson_error_incomplete){
		fprintf(stderr, "test28: NG: malloc mode partial error=%d\n", p.error.code);
		g_ng ++;
	}
	nmjson_parser_deinit(&p);	// valgrind でリークが無いこと
	nmjson_parser_deinit(&p);	// 2回呼んでも安全
	if(p.free_head != NULL){
		fprintf(stderr, "test28: NG: free list remains after deinit\n");
		g_ng ++;
	}
}

/// \brief	配列版: deinit は配列に触れず、再度 init すれば使えること
static void test_array_deinit_(void){
	static nmjson_token_t tokens[POOL_N];
	static char src[SRC_NUM];
	nmjson_parser_t p;
	nmjson_parser_init(&p, tokens, POOL_N);
	snprintf(src, sizeof(src), "%s", k_inputs[0]);
	nmjson_parser_parse(&p, src);
	nmjson_parser_deinit(&p);
	nmjson_parser_deinit(&p);
	// deinit後は(再initまで)トークンを取れない
	snprintf(src, sizeof(src), "[1]");
	nmjson_parser_parse(&p, src);
	if(p.error.code == nmjson_error_complete){
		fprintf(stderr, "test28: NG: parser usable after deinit without init\n");
		g_ng ++;
	}
	nmjson_parser_init(&p, tokens, POOL_N);
	snprintf(src, sizeof(src), "%s", k_inputs[0]);
	nmjson_parser_parse(&p, src);
	if(p.error.code != nmjson_error_complete){
		fprintf(stderr, "test28: NG: re-init after deinit error=%d\n", p.error.code);
		g_ng ++;
	}
	nmjson_parser_deinit(&p);
}

void test28(){
	printf("test28: sizeof(nmjson_token_t) = %zu\n", sizeof(nmjson_token_t));
#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
	if(sizeof(nmjson_token_t) > 64){
		fprintf(stderr, "test28: NG: sizeof(nmjson_token_t) = %zu > 64 on 64bit\n", sizeof(nmjson_token_t));
		g_ng ++;
	}
#endif
	test_prefixes_();
	test_bytewise_();
	test_values_after_reuse_();
	test_malloc_mode_();
	test_array_deinit_();
	if(g_ng != 0){
		fprintf(stderr, "test28: FAIL: %d case(s)\n", g_ng);
		exit(1);
	}
	printf("test28: token pool OK\n");
}
