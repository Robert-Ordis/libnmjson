/**
 *	\file	26-e5ident.c
 *	\brief	JSON5のクォートなしキー(IdentifierName)の検証（失敗で exit(1)）
 *			- 終端: ':' / コメント / ASCII・Unicodeの空白と行終端で正しく終わること
 *			- 構造上の記号・制御文字・不正なUTF-8は構文エラーになること
 *			- \uXXXX(サロゲートペア含む)の解除
 *			- 途中で途切れた入力は incomplete になり、続きを足すと完了すること
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/nmjson/nmjson.h"

#define TOKENS_NUM	16
#define SRC_NUM		256

static int g_ng = 0;

/// \brief	表示用: 制御文字と非ASCIIを\xHHにする
static const char *show_(const char *s){
	static char outs[3][SRC_NUM * 4];	// 1つのprintfで複数回呼べるよう循環させる
	static int idx = 0;
	char *out = outs[idx = (idx + 1) % 3];
	size_t n = 0;
	for(; *s != '\0' && n < sizeof(outs[0]) - 5; s++){
		unsigned char c = (unsigned char)*s;
		if(c < 0x20 || c >= 0x7F){
			n += snprintf(&out[n], sizeof(outs[0]) - n, "\\x%02X", c);
		}
		else{
			out[n++] = (char)c;
		}
	}
	out[n] = '\0';
	return out;
}

/// \brief	パースして、成功時は先頭2つのキー名を "k1|k2" 形式で返す
static nmjson_error_t parse_keys_(const char *in, char *keys, size_t keys_len){
	static char src[SRC_NUM];
	nmjson_parser_t parser;
	nmjson_token_t tokens[TOKENS_NUM];
	const nmjson_token_t *c;
	size_t n = 0;
	snprintf(src, sizeof(src), "%s", in);
	nmjson_parser_init(&parser, tokens, TOKENS_NUM);
	nmjson_parser_reset_as_superset(&parser, nmjson_superset_json5);
	nmjson_parser_parse(&parser, src);
	keys[0] = '\0';
	if(parser.error.code != nmjson_error_complete){
		return parser.error.code;
	}
	for(c = nmjson_token_head_child(nmjson_parser_get_root(&parser)); c != NULL; c = nmjson_token_next(c)){
		n += snprintf(&keys[n], keys_len - n, "%s%.*s", (n == 0) ? "" : "|", (int)c->n.len, c->n.name);
	}
	return nmjson_error_complete;
}

static void expect_keys_(const char *in, const char *expected){
	char keys[SRC_NUM];
	nmjson_error_t e = parse_keys_(in, keys, sizeof(keys));
	if(e != nmjson_error_complete || strcmp(keys, expected) != 0){
		fprintf(stderr, "test26: NG: %s -> error=%d keys=[%s] (expected [%s])\n", show_(in), e, show_(keys), show_(expected));
		g_ng ++;
	}
}

static void expect_error_(const char *in, nmjson_error_t expected){
	char keys[SRC_NUM];
	nmjson_error_t e = parse_keys_(in, keys, sizeof(keys));
	if(e != expected){
		fprintf(stderr, "test26: NG: %s -> error=%d (expected %d)\n", show_(in), e, expected);
		g_ng ++;
	}
}

static void test_terminators_(void){
	// 以前は終端を誤ってキー名に取り込んでいたもの
	expect_keys_("{key/*c*/:1}",                 "key");
	expect_keys_("{key//c\n:1}",                 "key");
	// Unicodeの空白・行終端(json5はextspaceを含む)
	expect_keys_("{key\xC2\xA0:1}",              "key");	// U+00A0 NBSP
	expect_keys_("{key\xE3\x80\x80:1}",          "key");	// U+3000
	expect_keys_("{key\xE2\x80\xA8:1}",          "key");	// U+2028
	expect_keys_("{key\xE2\x80\xA9:1}",          "key");	// U+2029
	expect_keys_("{key\xEF\xBB\xBF:1}",          "key");	// U+FEFF
	expect_keys_("{key\xE2\x80\x8A:1}",          "key");	// U+200A
	expect_keys_("{key\xE1\x9A\x80:1}",          "key");	// U+1680
	// ASCIIの空白・行終端
	expect_keys_("{key :1}",                     "key");
	expect_keys_("{key\t:1}",                    "key");
	expect_keys_("{key\r\n:1}",                  "key");
	expect_keys_("{key\v:1}",                    "key");
	// 複数キー・コメントを挟む
	expect_keys_("{a:1, b /*x*/ : 2}",           "a|b");
	// 終端に近いが終端ではない文字
	expect_keys_("{a\xE2\x80\x8B" "b:1}",        "a\xE2\x80\x8B" "b");	// U+200B(ZWSP, Cf)は空白ではない
}

static void test_chars_(void){
	expect_keys_("{\xE3\x81\x82\xE3\x81\x84:1}", "\xE3\x81\x82\xE3\x81\x84");	// あい
	expect_keys_("{caf\xC3\xA9:1}",              "caf\xC3\xA9");				// café
	expect_keys_("{$_a1:1}",                     "$_a1");
	expect_keys_("{\\u0041bc:1}",                "Abc");
	expect_keys_("{a\\u3042:1}",                 "a\xE3\x81\x82");
	expect_keys_("{\\uD83D\\uDE42:1}",           "\xF0\x9F\x99\x82");			// サロゲートペア
	// 緩い判定のため受け入れる(仕様: 不正な識別子も受け入れる)
	expect_keys_("{a-b:1}",                      "a-b");
}

static void test_errors_(void){
	// 構造上の記号・制御文字はキー名に吸収しない
	expect_error_("{a,b:1}",          nmjson_error_syntax);
	expect_error_("{a{:1}",           nmjson_error_syntax);
	expect_error_("{a}:1}",           nmjson_error_syntax);
	expect_error_("{a[:1}",           nmjson_error_syntax);
	expect_error_("{a]:1}",           nmjson_error_syntax);
	expect_error_("{a\":1}",          nmjson_error_syntax);
	expect_error_("{a':1}",           nmjson_error_syntax);
	expect_error_("{a\x01:1}",        nmjson_error_syntax);
	expect_error_("{a\x7F:1}",        nmjson_error_syntax);
	// キー名の後に続けて識別子(以前はキー名が上書きされて成功していた)
	expect_error_("{\"a\" b:1}",      nmjson_error_syntax);
	expect_error_("{a b:1}",          nmjson_error_syntax);
	expect_error_("{a:1, b c:2}",     nmjson_error_syntax);
	// エスケープ
	expect_error_("{ab\\x41:1}",      nmjson_error_syntax);	// 識別子では\xは不可
	expect_error_("{ab\\u12:1}",      nmjson_error_syntax);	// 桁不足
	expect_error_("{\\uD83D:1}",      nmjson_error_syntax);	// 単独の上位サロゲート
	expect_error_("{a\\uD83Db:1}",    nmjson_error_syntax);	// 上位サロゲートの後に通常文字
	// 不正なUTF-8(以前は先頭に来ると incomplete のまま進まなかった)
	expect_error_("{\xFF:1}",         nmjson_error_syntax);
	expect_error_("{a\xC3\x28:1}",    nmjson_error_syntax);
}

/// \brief	途中で途切れた入力: incomplete → 続きを足して complete
static void test_incomplete_(void){
	static const struct { const char *first; const char *rest; const char *key; } k_cases[] = {
		{ "{abc",                  ":1}",              "abc" },
		{ "{ab\xE3\x81",           "\x82:1}",          "ab\xE3\x81\x82" },	// マルチバイトの途中
		{ "{ab\xE3\x80",           "\x80:1}",          "ab" },				// U+3000(終端)の途中
		{ "{ab\\u30",              "42:1}",            "ab\xE3\x81\x82" },	// \uの途中
		{ "{ab",                   "/*c*/:1}",         "ab" },
	};
	static char src[SRC_NUM];
	size_t i;
	for(i = 0; i < sizeof(k_cases) / sizeof(k_cases[0]); i++){
		nmjson_parser_t parser;
		nmjson_token_t tokens[TOKENS_NUM];
		const nmjson_token_t *c;
		snprintf(src, sizeof(src), "%s", k_cases[i].first);
		nmjson_parser_init(&parser, tokens, TOKENS_NUM);
		nmjson_parser_reset_as_superset(&parser, nmjson_superset_json5);
		nmjson_parser_parse(&parser, src);
		if(parser.error.code != nmjson_error_incomplete){
			fprintf(stderr, "test26: NG: %s should be incomplete (error=%d)\n", show_(k_cases[i].first), parser.error.code);
			g_ng ++;
			continue;
		}
		strcat(src, k_cases[i].rest);
		nmjson_parser_parse(&parser, src);
		if(k_cases[i].key == NULL){
			if(parser.error.code != nmjson_error_syntax){
				fprintf(stderr, "test26: NG: %s + %s -> error=%d (expected syntax)\n", show_(k_cases[i].first), show_(k_cases[i].rest), parser.error.code);
				g_ng ++;
			}
			continue;
		}
		c = (parser.error.code == nmjson_error_complete) ? nmjson_token_head_child(nmjson_parser_get_root(&parser)) : NULL;
		if(c == NULL || c->n.len != strlen(k_cases[i].key) || memcmp(c->n.name, k_cases[i].key, c->n.len) != 0){
			fprintf(stderr, "test26: NG: %s + %s -> error=%d\n", show_(k_cases[i].first), show_(k_cases[i].rest), parser.error.code);
			g_ng ++;
		}
	}
}

void test26(){
	test_terminators_();
	test_chars_();
	test_errors_();
	test_incomplete_();
	if(g_ng != 0){
		fprintf(stderr, "test26: FAIL: %d case(s)\n", g_ng);
		exit(1);
	}
	printf("test26: e5ident OK\n");
}
