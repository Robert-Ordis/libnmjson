/**
 *	\file	22-token-as-int.c
 *	\brief	nmjson_token_as_int / nmjson_object_get_int で「文字列値」を整数として読む場合の検証
 *			（失敗で exit(1)）
 */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/nmjson/nmjson.h"

#define TOKENS_NUM	128
#define SRC_NUM		4096
#define DEF_		((int64_t)-334)

static const struct { const char *s; int64_t expected; } k_cases[] = {
	// 10進
	{ "0",                     0 },
	{ "42",                    42 },
	{ "-42",                   -42 },
	{ "+42",                   42 },
	{ "  42",                  42 },
	{ "42 ",                   42 },
	{ "9223372036854775807",   INT64_MAX },
	{ "-9223372036854775808",  INT64_MIN },
	{ "-9223372036854775809",  INT64_MIN },	// 負側は飽和
	{ "18446744073709551615",  -1 },			// 正側はビットそのまま(uint64として扱える)
	// 16進
	{ "0x1F",                  31 },
	{ "0X1f",                  31 },
	{ "-0x10",                 -16 },
	{ "0x7FFFFFFFFFFFFFFF",    INT64_MAX },
	// 8進
	{ "0o17",                  15 },
	{ "0O17",                  15 },
	{ "-0o17",                 -15 },
	// 2進
	{ "0b101",                 5 },
	{ "0B101",                 5 },
	{ "+0b11",                 3 },
	{ "-0b11",                 -3 },
	{ "0b0",                   0 },
	// 不正 → デフォルト値
	{ "",                      DEF_ },
	{ "-",                     DEF_ },
	{ "+",                     DEF_ },
	{ "   ",                   DEF_ },
	{ "0x",                    DEF_ },
	{ "0b",                    DEF_ },
	{ "0b2",                   DEF_ },
	{ "0o8",                   DEF_ },
	{ "0b-1",                  DEF_ },
	{ "0x 1",                  DEF_ },
	{ "0x+1",                  DEF_ },
	{ "--1",                   DEF_ },
	{ "abc",                   DEF_ },
	{ "12abc",                 DEF_ },
	{ "0b102",                 DEF_ },
};

#define ARRAY_LEN_(a) (sizeof(a) / sizeof((a)[0]))

void test22(){
	static char src[SRC_NUM];
	nmjson_parser_t parser;
	nmjson_token_t tokens[TOKENS_NUM];
	const nmjson_token_t *root;
	const nmjson_token_t *arr;
	size_t i;
	size_t n = 0;
	int ng = 0;

	// {"arr":["...",...],"k0":"...",...} を組み立てる（ケースに '"' や '\\' は含まない）
	n += snprintf(&src[n], SRC_NUM - n, "{\"arr\":[");
	for(i = 0; i < ARRAY_LEN_(k_cases); i++){
		n += snprintf(&src[n], SRC_NUM - n, "%s\"%s\"", (i == 0) ? "" : ",", k_cases[i].s);
	}
	n += snprintf(&src[n], SRC_NUM - n, "]");
	for(i = 0; i < ARRAY_LEN_(k_cases); i++){
		n += snprintf(&src[n], SRC_NUM - n, ",\"k%zu\":\"%s\"", i, k_cases[i].s);
	}
	n += snprintf(&src[n], SRC_NUM - n, "}");

	nmjson_parser_init(&parser, tokens, TOKENS_NUM);
	nmjson_parser_reset(&parser);
	nmjson_parser_parse(&parser, src);
	if(parser.error.code != nmjson_error_complete){
		fprintf(stderr, "test22: FAIL: parse error(%d)\n", parser.error.code);
		exit(1);
	}
	root = nmjson_parser_get_root(&parser);
	arr = nmjson_object_get_array(root, "arr");
	if(arr == NULL || arr->v.len != ARRAY_LEN_(k_cases)){
		fprintf(stderr, "test22: FAIL: array\n");
		exit(1);
	}

	for(i = 0; i < ARRAY_LEN_(k_cases); i++){
		char key[16];
		int64_t got_t = nmjson_token_as_int(nmjson_token_nth_child(arr, (int)i), DEF_);
		int64_t got_o;
		snprintf(key, sizeof(key), "k%zu", i);
		got_o = nmjson_object_get_int(root, key, DEF_);
		if(got_t != k_cases[i].expected || got_o != k_cases[i].expected){
			fprintf(stderr, "test22: NG: \"%s\" expected %" PRId64 " got token=%" PRId64 " object=%" PRId64 "\n",
				k_cases[i].s, k_cases[i].expected, got_t, got_o);
			ng ++;
		}
	}
	if(ng != 0){
		fprintf(stderr, "test22: FAIL: %d/%zu case(s)\n", ng, ARRAY_LEN_(k_cases));
		exit(1);
	}
	printf("test22: token_as_int(string): %zu cases OK\n", ARRAY_LEN_(k_cases));
}
