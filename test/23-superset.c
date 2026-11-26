/**
 *	\file	23-superset.c
 *	\brief	nmjson_superset_t の機能ビットごとに、許可した機能だけが受け入れられることの検証
 *			（失敗で exit(1)）
 */
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/nmjson/nmjson.h"

#define TOKENS_NUM	32
#define SRC_NUM		256

/// \brief	機能の識別子（テスト内でのみ使用）
typedef enum {
	F_COMMENT,
	F_TAILCOMMA,
	F_EXTNUM,
	F_ABBRDECIMAL,
	F_EXTESCAPE,
	F_HEXAESCAPE,
	F_LITERALSQ,
	F_LINECONT,
	F_LITERALCNTL,
	F_IDENTKEY,
	F_NUM_,
} feature_t;

static const char *k_feature_name[F_NUM_] = {
	"comment", "tailcomma", "extnum", "abbrdecimal", "extescape",
	"hexaescape", "literalsq", "linecontinuation", "literalcntl", "identkey",
};

/// \brief	機能を1つだけ立てたスーパーセットを作る
static nmjson_superset_t single_(feature_t f){
	nmjson_superset_t s = nmjson_superset_none;
	switch(f){
	case F_COMMENT:		s.comment = 1;			break;
	case F_TAILCOMMA:	s.tailcomma = 1;		break;
	case F_EXTNUM:		s.extnum = 1;			break;
	case F_ABBRDECIMAL:	s.abbrdecimal = 1;		break;
	case F_EXTESCAPE:	s.extescape = 1;		break;
	case F_HEXAESCAPE:	s.hexaescape = 1;		break;
	case F_LITERALSQ:	s.literalsq = 1;		break;
	case F_LINECONT:	s.linecontinuation = 1;	break;
	case F_LITERALCNTL:	s.literalcntl = 1;		break;
	case F_IDENTKEY:	s.identkey = 1;			break;
	default: break;
	}
	return s;
}

/// \brief	各機能を必要とする入力（その機能だけで受け入れられること）
///	\remarks	also: f 以外にこの入力を受け入れる機能(ビットマスク)。機能の意味が重なる場合に使う
static const struct { feature_t f; const char *src; unsigned also; } k_inputs[] = {
	{ F_COMMENT,     "[1, /* c */ 2]" },
	{ F_COMMENT,     "[1, // c\n 2]" },
	{ F_TAILCOMMA,   "[1, 2,]" },
	{ F_TAILCOMMA,   "{\"a\": 1,}" },
	{ F_EXTNUM,      "[0x1F]" },
	{ F_EXTNUM,      "[+1]" },
	{ F_EXTNUM,      "[Infinity]" },
	{ F_EXTNUM,      "[-Infinity]" },
	{ F_EXTNUM,      "[NaN]" },
	{ F_ABBRDECIMAL, "[.5]" },
	{ F_ABBRDECIMAL, "[5.]" },
	{ F_EXTESCAPE,   "[\"\\v\"]" },
	{ F_EXTESCAPE,   "[\"a\\0b\"]" },
	{ F_HEXAESCAPE,  "[\"\\x41\"]" },
	{ F_LITERALSQ,   "['a']" },
	{ F_LINECONT,    "[\"a\\\nb\"]", 1u << F_LITERALCNTL },	// 生の改行を許すなら、エスケープ付き改行も通る
	{ F_LITERALCNTL, "[\"a\nb\"]" },
	{ F_IDENTKEY,    "{a: 1}" },
	{ F_IDENTKEY,    "{Name: 1}" },	// extnumの先頭文字(N)と重なるキー
	{ F_IDENTKEY,    "{Infinite: 1}" },
};

#define ARRAY_LEN_(a) (sizeof(a) / sizeof((a)[0]))

/// \brief	パースして結果コードを返す
static nmjson_error_t parse_(nmjson_superset_t s, const char *src_in, nmjson_parser_t *parser, nmjson_token_t *tokens, char *src){
	snprintf(src, SRC_NUM, "%s", src_in);
	nmjson_parser_init(parser, tokens, TOKENS_NUM);
	nmjson_parser_reset_as_superset(parser, s);
	nmjson_parser_parse(parser, src);
	return parser->error.code;
}

static int g_ng = 0;

static void expect_(const char *label, nmjson_superset_t s, const char *src_in, int accept){
	static char src[SRC_NUM];
	nmjson_parser_t parser;
	nmjson_token_t tokens[TOKENS_NUM];
	nmjson_error_t e = parse_(s, src_in, &parser, tokens, src);
	int ok = accept ? (e == nmjson_error_complete) : (e == nmjson_error_syntax);
	if(!ok){
		char shown[SRC_NUM];
		size_t i;
		for(i = 0; src_in[i] != '\0' && i < sizeof(shown) - 1; i++){
			shown[i] = (src_in[i] == '\n') ? '~' : src_in[i];
		}
		shown[i] = '\0';
		fprintf(stderr, "test23: NG: [%s] %s -> error=%d (expected %s)\n",
			label, shown, e, accept ? "complete" : "syntax");
		g_ng ++;
	}
}

/// \brief	機能1つだけ: その機能の入力のみ通り、他の機能の入力は構文エラー
static void test_single_(void){
	int f;
	size_t i;
	for(f = 0; f < F_NUM_; f++){
		nmjson_superset_t s = single_((feature_t)f);
		for(i = 0; i < ARRAY_LEN_(k_inputs); i++){
			expect_(k_feature_name[f], s, k_inputs[i].src,
				k_inputs[i].f == (feature_t)f || (k_inputs[i].also & (1u << f)) != 0);
		}
	}
	// 機能なし: 全て構文エラー、プレーンJSONは通る
	for(i = 0; i < ARRAY_LEN_(k_inputs); i++){
		expect_("none", nmjson_superset_none, k_inputs[i].src, 0);
	}
	expect_("none", nmjson_superset_none, "{\"a\": [1, 2.5, \"x\\n\", true, null]}", 1);
}

/// \brief	プリセット: 定義どおりの機能集合であること
static void test_presets_(void){
	static const struct { const char *name; int accept[F_NUM_]; } k_presets[] = {
		//                comm tail extn abbr exte hexa sq   lcon lcnt idnt
		{ "jsonc", {      1,   1,   0,   0,   0,   0,   0,   0,   0,   0 } },
		{ "json5", {      1,   1,   1,   1,   1,   1,   1,   1,   0,   1 } },
		{ "rough", {      0,   0,   0,   0,   1,   0,   0,   0,   1,   0 } },
	};
	nmjson_superset_t sets[3];
	size_t p, i;
	sets[0] = nmjson_superset_jsonc;
	sets[1] = nmjson_superset_json5;
	sets[2] = nmjson_superset_rough;
	for(p = 0; p < ARRAY_LEN_(k_presets); p++){
		for(i = 0; i < ARRAY_LEN_(k_inputs); i++){
			int accept = k_presets[p].accept[k_inputs[i].f];
			int f;
			for(f = 0; f < F_NUM_; f++){
				if((k_inputs[i].also & (1u << f)) && k_presets[p].accept[f]){ accept = 1; }
			}
			expect_(k_presets[p].name, sets[p], k_inputs[i].src, accept);
		}
	}
}

/// \brief	組み合わせ: プリセットに機能を足す / 引く
static void test_combination_(void){
	nmjson_superset_t s;

	// JSONC + シングルクォート
	s = nmjson_superset_jsonc;
	s.literalsq = 1;
	expect_("jsonc+literalsq", s, "['a', /* c */ 'b',]", 1);
	expect_("jsonc+literalsq", s, "[0x1F]", 0);

	// JSON5 - 識別子キー: 値としてのInfinityは通るが、キーとしての識別子は通らない
	s = nmjson_superset_json5;
	s.identkey = 0;
	expect_("json5-identkey", s, "{\"a\": Infinity}", 1);
	expect_("json5-identkey", s, "{a: 1}", 0);
	expect_("json5-identkey", s, "{Name: 1}", 0);

	// JSON5 - extnum: キーとしての Name/Infinite は通るが、値のInfinityは通らない
	s = nmjson_superset_json5;
	s.extnum = 0;
	expect_("json5-extnum", s, "{Name: 1, Infinite: 2}", 1);
	expect_("json5-extnum", s, "[Infinity]", 0);
	expect_("json5-extnum", s, "[.5]", 1);

	// abbrdecimalのみでは Infinity は通らない（旧実装では extnum||abbrdecimal で通っていた）
	s = nmjson_superset_none;
	s.abbrdecimal = 1;
	expect_("abbrdecimal", s, "[-Infinity]", 0);
}

/// \brief	受け入れた場合の値が正しいこと
static void test_values_(void){
	static char src[SRC_NUM];
	nmjson_parser_t parser;
	nmjson_token_t tokens[TOKENS_NUM];
	const nmjson_token_t *root;
	nmjson_superset_t s = nmjson_superset_json5;

	if(parse_(s, "{a: 0x1F, b: .5, c: 'x', d: \"\\x41\", e: -Infinity,}", &parser, tokens, src) != nmjson_error_complete){
		fprintf(stderr, "test23: NG: values parse\n");
		g_ng ++;
		return;
	}
	root = nmjson_parser_get_root(&parser);
	if(nmjson_object_get_int(root, "a", -1) != 31){ fprintf(stderr, "test23: NG: value a\n"); g_ng ++; }
	if(nmjson_object_get_float(root, "b", -1) != 0.5){ fprintf(stderr, "test23: NG: value b\n"); g_ng ++; }
	if(strcmp(nmjson_object_get_string(root, "c", ""), "x") != 0){ fprintf(stderr, "test23: NG: value c\n"); g_ng ++; }
	if(strcmp(nmjson_object_get_string(root, "d", ""), "A") != 0){ fprintf(stderr, "test23: NG: value d\n"); g_ng ++; }
	if(!isinf(nmjson_object_get_float(root, "e", 0)) || nmjson_object_get_float(root, "e", 0) > 0){
		fprintf(stderr, "test23: NG: value e\n"); g_ng ++;
	}
}

void test23(){
	test_single_();
	test_presets_();
	test_combination_();
	test_values_();
	if(g_ng != 0){
		fprintf(stderr, "test23: FAIL: %d case(s)\n", g_ng);
		exit(1);
	}
	printf("test23: superset feature bits OK\n");
}
