/**
 *	\file	24-writer-comment.c
 *	\brief	nmjson_writer_put_comment の検証（失敗で exit(1)）
 *			- 出力文字列の完全一致
 *			- 出力を jsonc / json5 でパースし直して値が保たれること
 *			- 無効条件では何も書かないこと / 不正引数
 *			- 改行を含むコメントの分割
 */
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/nmjson/nmjson.h"
#include "include/nmjson/nmjson_writer.h"

#define TOKENS_NUM	64
#define MBUF_NUM	4096

typedef struct {
	char	buf[MBUF_NUM];
	size_t	len;
} membuf24_t;

static ssize_t membuf24_write_(nmjson_writer_ctx_t *pctx, const void *p, size_t l){
	membuf24_t *self = pctx->p;
	if(self->len + l + 1 > sizeof(self->buf)){
		return -1;
	}
	memcpy(&self->buf[self->len], p, l);
	self->len += l;
	self->buf[self->len] = '\0';
	return (ssize_t)l;
}

static int g_ng = 0;

/// \brief	改行を見やすくして差分を出す
static void show_(const char *label, const char *s){
	fprintf(stderr, "%s:\n----\n%s\n----\n", label, s);
}

static void expect_text_(const char *label, const char *got, const char *expected){
	if(strcmp(got, expected) != 0){
		fprintf(stderr, "test24: NG: %s text mismatch\n", label);
		show_("expected", expected);
		show_("got", got);
		g_ng ++;
	}
}

static void writer_init_(nmjson_writer_t *w, membuf24_t *mb, nmjson_superset_t s, int pretty){
	mb->len = 0;
	mb->buf[0] = '\0';
	nmjson_writer_init_cb(w, mb, membuf24_write_);
	nmjson_writer_cfg_superset(w, s);
	nmjson_writer_cfg_pretty_print(w, pretty);
}

/// \brief	一通りの位置にコメントを置いた出力
static void build_sample_(nmjson_writer_t *w){
	nmjson_writer_put_comment(w, "header", nmjson_writer_comment_newline);
	nmjson_writer_begin(w, NULL, nmjson_type_object);
	nmjson_writer_put_comment(w, "about a", nmjson_writer_comment_newline);
	nmjson_writer_put_int(w, "a", 1);
	nmjson_writer_put_comment(w, "a inline", nmjson_writer_comment_inline);
	nmjson_writer_put_comment(w, "second", nmjson_writer_comment_inline);	// 連続→ニューライン扱い
	nmjson_writer_put_int(w, "b", 2);
	nmjson_writer_begin(w, "arr", nmjson_type_array);
	nmjson_writer_put_comment(w, "in arr", nmjson_writer_comment_inline);
	nmjson_writer_put_int(w, NULL, 1);
	nmjson_writer_put_comment(w, "multi\nline", nmjson_writer_comment_newline);
	nmjson_writer_end(w, nmjson_type_array);
	nmjson_writer_put_comment(w, "last", nmjson_writer_comment_inline);	// 直後に閉じる→末尾カンマ
	nmjson_writer_end(w, nmjson_type_object);
	nmjson_writer_put_comment(w, "trailer", nmjson_writer_comment_inline);	// トップレベル→カンマなし
}

static const char *k_sample_expected =
	"// header\n"
	"{\n"
	"  // about a\n"
	"  \"a\": 1, // a inline\n"
	"  // second\n"
	"  \"b\": 2, \n"
	"  \"arr\": [ // in arr\n"
	"    1,\n"
	"    // multi\n"
	"    // line\n"
	"  ], // last\n"
	"} // trailer";

static void test_sample_text_(void){
	static membuf24_t mb;
	nmjson_writer_t w;
	writer_init_(&w, &mb, nmjson_superset_jsonc, 1);
	build_sample_(&w);
	if(w.err.num != 0){
		fprintf(stderr, "test24: NG: writer error %d at %s\n", w.err.num, w.err.at ? w.err.at : "");
		g_ng ++;
	}
	expect_text_("sample", mb.buf, k_sample_expected);
}

/// \brief	出力をパースし直して値を確認する
static void test_sample_reparse_(nmjson_superset_t s, const char *label){
	static membuf24_t mb;
	nmjson_writer_t w;
	nmjson_parser_t parser;
	nmjson_token_t tokens[TOKENS_NUM];
	const nmjson_token_t *root;
	const nmjson_token_t *arr;

	writer_init_(&w, &mb, s, 1);
	build_sample_(&w);

	nmjson_parser_init(&parser, tokens, TOKENS_NUM);
	nmjson_parser_reset_as_superset(&parser, s);
	nmjson_parser_parse(&parser, mb.buf);
	if(parser.error.code != nmjson_error_complete){
		fprintf(stderr, "test24: NG: reparse(%s) error=%d detail=%s\n", label, parser.error.code,
			parser.error.detail ? parser.error.detail : "");
		show_("source", mb.buf);
		g_ng ++;
		return;
	}
	root = nmjson_parser_get_root(&parser);
	arr = nmjson_object_get_array(root, "arr");
	if(nmjson_object_get_int(root, "a", -1) != 1
		|| nmjson_object_get_int(root, "b", -1) != 2
		|| arr == NULL || arr->v.len != 1
		|| nmjson_token_as_int(nmjson_token_nth_child(arr, 0), -1) != 1
	){
		fprintf(stderr, "test24: NG: reparse(%s) values\n", label);
		g_ng ++;
	}
}

/// \brief	無効条件では0を返し、何も書かない。他の出力にも影響しない
static void test_disabled_(void){
	static membuf24_t mb;
	static membuf24_t mb_ref;
	nmjson_writer_t w;
	nmjson_superset_t comment_only = nmjson_superset_none;
	const struct { const char *label; nmjson_superset_t s; int pretty; } k_cases[] = {
		{ "none+pretty",          nmjson_superset_none,  1 },
		{ "jsonc,no pretty",      nmjson_superset_jsonc, 0 },
		{ "comment only+pretty",  comment_only,          1 },
	};
	size_t i;
	comment_only.comment = 1;

	for(i = 0; i < sizeof(k_cases) / sizeof(k_cases[0]); i++){
		nmjson_superset_t s = k_cases[i].s;
		if(i == 2){ s = comment_only; }
		// コメントなしの参照出力
		writer_init_(&w, &mb_ref, s, k_cases[i].pretty);
		nmjson_writer_begin(&w, NULL, nmjson_type_object);
		nmjson_writer_put_int(&w, "a", 1);
		nmjson_writer_put_int(&w, "b", 2);
		nmjson_writer_end(&w, nmjson_type_object);
		// コメントを挟んだ出力
		writer_init_(&w, &mb, s, k_cases[i].pretty);
		nmjson_writer_begin(&w, NULL, nmjson_type_object);
		nmjson_writer_put_int(&w, "a", 1);
		if(nmjson_writer_put_comment(&w, "x", nmjson_writer_comment_inline) != 0
			|| nmjson_writer_put_comment(&w, "y", nmjson_writer_comment_newline) != 0){
			fprintf(stderr, "test24: NG: disabled(%s) returned non-zero\n", k_cases[i].label);
			g_ng ++;
		}
		nmjson_writer_put_int(&w, "b", 2);
		nmjson_writer_end(&w, nmjson_type_object);
		expect_text_(k_cases[i].label, mb.buf, mb_ref.buf);
	}
}

static void test_invalid_(void){
	static membuf24_t mb;
	nmjson_writer_t w;
	writer_init_(&w, &mb, nmjson_superset_jsonc, 1);
	if(nmjson_writer_put_comment(&w, NULL, nmjson_writer_comment_inline) >= 0 || w.err.num != EINVAL){
		fprintf(stderr, "test24: NG: NULL comment should fail with EINVAL\n");
		g_ng ++;
	}
	if(mb.len != 0){
		fprintf(stderr, "test24: NG: NULL comment wrote something\n");
		g_ng ++;
	}
}

/// \brief	改行の種類ごとの分割
static void test_lines_(void){
	static membuf24_t mb;
	nmjson_writer_t w;
	// 整形出力は先頭に改行が入る(既存仕様)ため、期待値は "\n[" から始まる
	const struct { const char *in; const char *expected; } k_cases[] = {
		{ "a\r\nb\rc\xE2\x80\xA8" "d\xE2\x80\xA9" "e",
		  "\n[\n  // a\n  // b\n  // c\n  // d\n  // e\n]" },
		{ "x\n",          "\n[\n  // x\n]" },				// 末尾の改行は空行にしない
		{ "x\n\ny",       "\n[\n  // x\n  //\n  // y\n]" },	// 途中の空行は残す
		{ "",             "\n[\n  //\n]" },
		{ "\xE2\x80\xA0ok","\n[\n  // \xE2\x80\xA0ok\n]" },	// U+2020(†)は改行ではない
	};
	size_t i;
	for(i = 0; i < sizeof(k_cases) / sizeof(k_cases[0]); i++){
		char label[32];
		writer_init_(&w, &mb, nmjson_superset_jsonc, 1);
		nmjson_writer_begin(&w, NULL, nmjson_type_array);
		nmjson_writer_put_comment(&w, k_cases[i].in, nmjson_writer_comment_newline);
		nmjson_writer_end(&w, nmjson_type_array);
		snprintf(label, sizeof(label), "lines[%zu]", i);
		expect_text_(label, mb.buf, k_cases[i].expected);
	}
}

void test24(){
	test_sample_text_();
	test_sample_reparse_(nmjson_superset_jsonc, "jsonc");
	test_sample_reparse_(nmjson_superset_json5, "json5");
	test_disabled_();
	test_invalid_();
	test_lines_();
	if(g_ng != 0){
		fprintf(stderr, "test24: FAIL: %d case(s)\n", g_ng);
		exit(1);
	}
	printf("test24: writer comment OK\n");
}
