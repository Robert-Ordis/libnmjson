/**
 *	\file	27-space.c
 *	\brief	トークン間の空白(extspace)とコメント終端の検証（失敗で exit(1)）
 *			- extspace無効: RFC 8259 の4文字のみ。VT/FF/Unicodeの空白は構文エラー
 *			- extspace有効: JSON5の WhiteSpace / LineTerminator を全て受け入れる(値も正しい)
 *			- 1行コメントは extspace 有効時 U+2028/U+2029 でも終わる
 *			- 数値の直後のコメント(以前は構文エラーだった)
 *			- マルチバイトの空白が途中で途切れた場合は incomplete → 続きで complete
 *			- ロケールに依存しないこと(isspaceを使っていないこと)
 */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/nmjson/nmjson.h"

#define TOKENS_NUM	16
#define SRC_NUM		256

static int g_ng = 0;

/// \brief	各種空白(UTF-8)
static const struct { const char *name; const char *s; int ascii4; } k_spaces[] = {
	{ "SP",     " ",            1 },
	{ "TAB",    "\t",           1 },
	{ "LF",     "\n",           1 },
	{ "CR",     "\r",           1 },
	{ "VT",     "\v",           0 },
	{ "FF",     "\f",           0 },
	{ "U+00A0", "\xC2\xA0",     0 },
	{ "U+1680", "\xE1\x9A\x80", 0 },
	{ "U+2000", "\xE2\x80\x80", 0 },
	{ "U+2005", "\xE2\x80\x85", 0 },
	{ "U+200A", "\xE2\x80\x8A", 0 },
	{ "U+2028", "\xE2\x80\xA8", 0 },
	{ "U+2029", "\xE2\x80\xA9", 0 },
	{ "U+202F", "\xE2\x80\xAF", 0 },
	{ "U+205F", "\xE2\x81\x9F", 0 },
	{ "U+3000", "\xE3\x80\x80", 0 },
	{ "U+FEFF", "\xEF\xBB\xBF", 0 },
};

/// \brief	空白ではない(似ているが対象外の)文字
static const struct { const char *name; const char *s; } k_not_spaces[] = {
	{ "U+200B(ZWSP)", "\xE2\x80\x8B" },
	{ "U+0085(NEL)",  "\xC2\x85" },
	{ "U+180E",       "\xE1\xA0\x8E" },
	{ "U+3001",       "\xE3\x80\x81" },
};

#define ARRAY_LEN_(a) (sizeof(a) / sizeof((a)[0]))

/// \brief	パースして、成功時は配列 [1,2] / オブジェクト {"a":1} の値を確認する
static nmjson_error_t parse_(const char *in, nmjson_superset_t s, int *values_ok){
	static char src[SRC_NUM];
	nmjson_parser_t parser;
	nmjson_token_t tokens[TOKENS_NUM];
	const nmjson_token_t *root;
	snprintf(src, sizeof(src), "%s", in);
	nmjson_parser_init(&parser, tokens, TOKENS_NUM);
	nmjson_parser_reset_as_superset(&parser, s);
	nmjson_parser_parse(&parser, src);
	*values_ok = 0;
	if(parser.error.code == nmjson_error_complete){
		root = nmjson_parser_get_root(&parser);
		if(root->v.type == nmjson_type_array){
			*values_ok = root->v.len == 2
				&& nmjson_token_as_int(nmjson_token_nth_child(root, 0), -1) == 1
				&& nmjson_token_as_int(nmjson_token_nth_child(root, 1), -1) == 2;
		}
		else{
			*values_ok = root->v.len == 1 && nmjson_object_get_int(root, "a", -1) == 1;
		}
	}
	return parser.error.code;
}

static void expect_(const char *label, const char *sp_name, const char *in, nmjson_superset_t s, int accept){
	int values_ok;
	nmjson_error_t e = parse_(in, s, &values_ok);
	int ok = accept ? (e == nmjson_error_complete && values_ok) : (e == nmjson_error_syntax);
	if(!ok){
		fprintf(stderr, "test27: NG: [%s] %s: error=%d values_ok=%d (expected %s)\n",
			label, sp_name, e, values_ok, accept ? "complete" : "syntax");
		g_ng ++;
	}
}

/// \brief	空白を様々な位置に置いた入力を組み立てる
static void build_inputs_(const char *sp, char out[][SRC_NUM], size_t *n){
	*n = 0;
	snprintf(out[(*n)++], SRC_NUM, "%s[1,2]", sp);				// 先頭
	snprintf(out[(*n)++], SRC_NUM, "[%s1,2]", sp);				// [ の直後
	snprintf(out[(*n)++], SRC_NUM, "[1%s,2]", sp);				// 数値の直後
	snprintf(out[(*n)++], SRC_NUM, "[1,%s2]", sp);				// , の直後
	snprintf(out[(*n)++], SRC_NUM, "[1,2%s]", sp);				// ] の直前(数値の直後)
	snprintf(out[(*n)++], SRC_NUM, "{\"a\"%s:1}", sp);			// キーの直後
	snprintf(out[(*n)++], SRC_NUM, "{\"a\":%s1}", sp);			// : の直後
}

static void test_spaces_(void){
	nmjson_superset_t plain = nmjson_superset_none;
	nmjson_superset_t jsonc = nmjson_superset_jsonc;
	nmjson_superset_t json5 = nmjson_superset_json5;
	nmjson_superset_t only = nmjson_superset_none;
	static char inputs[8][SRC_NUM];
	size_t i, j, n;
	only.extspace = 1;
	for(i = 0; i < ARRAY_LEN_(k_spaces); i++){
		build_inputs_(k_spaces[i].s, inputs, &n);
		for(j = 0; j < n; j++){
			expect_("none",          k_spaces[i].name, inputs[j], plain, k_spaces[i].ascii4);
			expect_("jsonc",         k_spaces[i].name, inputs[j], jsonc, k_spaces[i].ascii4);
			expect_("json5",         k_spaces[i].name, inputs[j], json5, 1);
			expect_("none+extspace", k_spaces[i].name, inputs[j], only,  1);
		}
	}
	for(i = 0; i < ARRAY_LEN_(k_not_spaces); i++){
		build_inputs_(k_not_spaces[i].s, inputs, &n);
		for(j = 0; j < n; j++){
			expect_("json5(not space)", k_not_spaces[i].name, inputs[j], json5, 0);
		}
	}
}

/// \brief	コメントの終端と、数値の直後のコメント
static void test_comments_(void){
	nmjson_superset_t jsonc = nmjson_superset_jsonc;
	nmjson_superset_t json5 = nmjson_superset_json5;
	nmjson_superset_t plain = nmjson_superset_none;
	// 1行コメントは U+2028/2029 でも終わる(extspace有効時)
	expect_("json5", "comment-LS", "[1, // c\xE2\x80\xA8" "2]", json5, 1);
	expect_("json5", "comment-PS", "[1, // c\xE2\x80\xA9" "2]", json5, 1);
	expect_("json5", "comment-LF", "[1, // c\n2]", json5, 1);
	expect_("json5", "comment-CR", "[1, // c\r2]", json5, 1);
	// 数値の直後のコメント(以前は構文エラーだった)
	expect_("jsonc", "num/*c*/",  "[1/*c*/,2]", jsonc, 1);
	expect_("jsonc", "num//c",    "[1// c\n,2]", jsonc, 1);
	expect_("jsonc", "num/*c*/}", "{\"a\":1/*c*/}", jsonc, 1);
	expect_("json5", "num/*c*/",  "[1/*c*/,2]", json5, 1);
	// コメント不許可なら構文エラーのまま
	expect_("none",  "num/*c*/",  "[1/*c*/,2]", plain, 0);
	{
		// extspace無効のJSONCでは、U+2028はコメントを終わらせない(コメントが続く→途切れ扱い)
		int values_ok;
		nmjson_error_t e = parse_("[1, // c\xE2\x80\xA8" "2]", jsonc, &values_ok);
		if(e != nmjson_error_incomplete){
			fprintf(stderr, "test27: NG: [jsonc] comment-LS without extspace: error=%d (expected incomplete)\n", e);
			g_ng ++;
		}
	}
}

/// \brief	マルチバイトの空白が途中で途切れた場合
static void test_incomplete_(void){
	static const struct { const char *first; const char *rest; } k_cases[] = {
		{ "[1,\xE3\x80",       "\x80" "2]" },		// トークン間
		{ "[1,\xE3",           "\x80\x80" "2]" },
		{ "[1\xE3\x80",        "\x80,2]" },			// 数値の直後
		{ "[1,\xC2",           "\xA0" "2]" },
		{ "\xEF\xBB",          "\xBF[1,2]" },		// 先頭のBOM
		{ "[1, // c\xE2",      "\x80\xA8" "2]" },	// 1行コメント中のU+2028が途切れている
		{ "[1, // c\xE2\x80",  "\xA8" "2]" },
	};
	static char src[SRC_NUM];
	size_t i;
	for(i = 0; i < ARRAY_LEN_(k_cases); i++){
		nmjson_parser_t parser;
		nmjson_token_t tokens[TOKENS_NUM];
		const nmjson_token_t *root;
		snprintf(src, sizeof(src), "%s", k_cases[i].first);
		nmjson_parser_init(&parser, tokens, TOKENS_NUM);
		nmjson_parser_reset_as_superset(&parser, nmjson_superset_json5);
		nmjson_parser_parse(&parser, src);
		if(parser.error.code != nmjson_error_incomplete){
			fprintf(stderr, "test27: NG: incomplete case %zu: first error=%d\n", i, parser.error.code);
			g_ng ++;
			continue;
		}
		strcat(src, k_cases[i].rest);
		nmjson_parser_parse(&parser, src);
		root = nmjson_parser_get_root(&parser);
		if(parser.error.code != nmjson_error_complete || root == NULL || root->v.len != 2){
			fprintf(stderr, "test27: NG: incomplete case %zu: second error=%d\n", i, parser.error.code);
			g_ng ++;
		}
	}
}

/// \brief	ロケールを変えても結果が変わらないこと(isspaceを使っていない)
static void test_locale_(void){
	static const char *k_locales[] = { "de_DE.ISO-8859-1", "de_DE.iso88591", "de_DE", "en_US.ISO-8859-1", "C.UTF-8", NULL };
	const char *saved = setlocale(LC_CTYPE, NULL);
	char saved_copy[64];
	size_t i;
	int tried = 0;
	snprintf(saved_copy, sizeof(saved_copy), "%s", saved ? saved : "C");
	for(i = 0; k_locales[i] != NULL; i++){
		if(setlocale(LC_CTYPE, k_locales[i]) == NULL){
			continue;
		}
		tried ++;
		// U+00A0 の2バイト目(0xA0)は、単一バイトのロケールではisspaceが真になり得る
		expect_(k_locales[i], "U+00A0 in none", "[1,\xC2\xA0" "2]", nmjson_superset_none, 0);
		expect_(k_locales[i], "VT in none",     "[1,\v2]",           nmjson_superset_none, 0);
		expect_(k_locales[i], "U+00A0 in json5","[1,\xC2\xA0" "2]", nmjson_superset_json5, 1);
	}
	setlocale(LC_CTYPE, saved_copy);
	printf("test27: locale check with %d locale(s)\n", tried);
}

void test27(){
	test_spaces_();
	test_comments_();
	test_incomplete_();
	test_locale_();
	if(g_ng != 0){
		fprintf(stderr, "test27: FAIL: %d case(s)\n", g_ng);
		exit(1);
	}
	printf("test27: space OK\n");
}
