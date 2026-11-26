/**
 *	\file	21-roundtrip-num.c
 *	\brief	数値の書き出し→読み戻しの検証
 *
 *	- 整数: writer出力がprintfと完全一致すること、parser→token_fout→parserで値が保存されること（失敗で exit(1)）
 *	- 浮動小数: ビット単位で同一値に戻るか、ロケールに影響されないかを確認する。
 *	  現状は既知の問題(%gの6桁精度 / LC_NUMERIC依存)があるため KNOWN ISSUE として報告のみ。
 *	  環境変数 NMJSON_TEST_STRICT_FLOAT=1 で失敗扱いにする（float置き換え後に有効化する想定）。
 */
#include <inttypes.h>
#include <locale.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/nmjson/nmjson.h"
#include "include/nmjson/nmjson_writer.h"

#define TOKENS_NUM	256
#define MBUF_NUM	8192

/// \brief	メモリに書き込むためのwriterコンテキスト
typedef struct {
	char	buf[MBUF_NUM];
	size_t	len;
} membuf_t;

static ssize_t membuf_write_(nmjson_writer_ctx_t *pctx, const void *p, size_t l){
	membuf_t *self = pctx->p;
	if(self->len + l + 1 > sizeof(self->buf)){
		return -1;
	}
	memcpy(&self->buf[self->len], p, l);
	self->len += l;
	self->buf[self->len] = '\0';
	return (ssize_t)l;
}

static void membuf_reset_(membuf_t *self){
	self->len = 0;
	self->buf[0] = '\0';
}

static void fail_(const char *msg, const char *detail){
	fprintf(stderr, "test21: FAIL: %s [%s]\n", msg, detail ? detail : "");
	exit(1);
}

/// \brief	bufをパースしてrootを返す。失敗したらexit。
static const nmjson_token_t *parse_or_die_(nmjson_parser_t *parser, nmjson_token_t *tokens, char *buf){
	nmjson_parser_init(parser, tokens, TOKENS_NUM);
	nmjson_parser_reset(parser);
	nmjson_parser_parse(parser, buf);
	if(parser->error.code != nmjson_error_complete){
		fail_("parse error", buf);
	}
	return nmjson_parser_get_root(parser);
}

//---------------------------------------------------------------
// 整数
//---------------------------------------------------------------

static const int64_t k_i64_values[] = {
	0, 1, -1, 9, 10, -9, -10, 99, 100, -100,
	12345, -12345,
	INT32_MAX, INT32_MIN, (int64_t)INT32_MAX + 1, (int64_t)INT32_MIN - 1,
	999999999999999999LL, -999999999999999999LL,
	1000000000000000000LL, -1000000000000000000LL,
	INT64_MAX - 1, INT64_MAX, INT64_MIN + 1, INT64_MIN,
};

static const uint64_t k_u64_values[] = {
	0, 1, 9, 10, UINT32_MAX, (uint64_t)UINT32_MAX + 1,
	(uint64_t)INT64_MAX, (uint64_t)INT64_MAX + 1,
	10000000000000000000ULL, UINT64_MAX - 1, UINT64_MAX,
};

#define ARRAY_LEN_(a) (sizeof(a) / sizeof((a)[0]))

/// \brief	xorshift64: 再現性のある疑似乱数
static uint64_t xorshift64_(uint64_t *s){
	uint64_t x = *s;
	x ^= x << 13; x ^= x >> 7; x ^= x << 17;
	return *s = x;
}

/// \brief	writerの単一値出力がprintfの結果と完全一致すること
static void test_int_text_(void){
	static membuf_t mb;
	nmjson_writer_t writer;
	char ref[64];
	size_t i;
	uint64_t seed = 0x9E3779B97F4A7C15ULL;
	int count = 0;

	for(i = 0; i < ARRAY_LEN_(k_i64_values) + 10000; i++){
		int64_t v;
		uint64_t u;
		if(i < ARRAY_LEN_(k_i64_values)){
			v = k_i64_values[i];
		}
		else{
			// 桁数がばらけるようにシフト量もランダムにする
			uint64_t r = xorshift64_(&seed);
			v = (int64_t)(r >> (xorshift64_(&seed) % 64));
			if(r & 1){ v = -v; }
		}
		membuf_reset_(&mb);
		nmjson_writer_init_cb(&writer, &mb, membuf_write_);
		if(nmjson_writer_put_int(&writer, NULL, v) < 0){ fail_("put_int", NULL); }
		snprintf(ref, sizeof(ref), "%" PRId64, v);
		if(strcmp(mb.buf, ref) != 0){ fail_("put_int text mismatch", ref); }
		count ++;

		u = (i < ARRAY_LEN_(k_u64_values)) ? k_u64_values[i] : (uint64_t)v;
		membuf_reset_(&mb);
		nmjson_writer_init_cb(&writer, &mb, membuf_write_);
		if(nmjson_writer_put_uint(&writer, NULL, u) < 0){ fail_("put_uint", NULL); }
		snprintf(ref, sizeof(ref), "%" PRIu64, u);
		if(strcmp(mb.buf, ref) != 0){ fail_("put_uint text mismatch", ref); }
		count ++;
	}
	printf("test21: int text: %d values OK\n", count);
}

/// \brief	writer → parser → token_fout → parser で値が保存されること
static void test_int_roundtrip_(void){
	static membuf_t mb;
	static char fout_buf[MBUF_NUM];
	nmjson_writer_t writer;
	nmjson_parser_t parser;
	nmjson_token_t tokens[TOKENS_NUM];
	const nmjson_token_t *root;
	const nmjson_token_t *arr;
	int pass;
	size_t i;

	membuf_reset_(&mb);
	nmjson_writer_init_cb(&writer, &mb, membuf_write_);
	nmjson_writer_with_object(&writer, NULL, {
		nmjson_writer_with_array(&writer, "i", {
			for(i = 0; i < ARRAY_LEN_(k_i64_values); i++){
				nmjson_writer_put_int(&writer, NULL, k_i64_values[i]);
			}
		});
		nmjson_writer_with_array(&writer, "u", {
			for(i = 0; i < ARRAY_LEN_(k_u64_values); i++){
				nmjson_writer_put_uint(&writer, NULL, k_u64_values[i]);
			}
		});
	});
	if(writer.err.num != 0){ fail_("writer error", writer.err.at); }

	// pass 0: writer出力をパース / pass 1: token_foutの出力を再パース
	for(pass = 0; pass < 2; pass ++){
		char *src = (pass == 0) ? mb.buf : fout_buf;
		root = parse_or_die_(&parser, tokens, src);

		arr = nmjson_object_get_array(root, "i");
		if(arr == NULL || arr->v.len != ARRAY_LEN_(k_i64_values)){ fail_("array i", src); }
		for(i = 0; i < ARRAY_LEN_(k_i64_values); i++){
			const nmjson_token_t *e = nmjson_token_nth_child(arr, (int)i);
			if(e == NULL || e->v.type != nmjson_type_integer || e->v.value.i != k_i64_values[i]){
				fail_("int roundtrip mismatch", src);
			}
		}
		arr = nmjson_object_get_array(root, "u");
		if(arr == NULL || arr->v.len != ARRAY_LEN_(k_u64_values)){ fail_("array u", src); }
		for(i = 0; i < ARRAY_LEN_(k_u64_values); i++){
			const nmjson_token_t *e = nmjson_token_nth_child(arr, (int)i);
			// INT64_MAX超はint64_tへビットそのままで格納される仕様
			if(e == NULL || e->v.type != nmjson_type_integer || (uint64_t)e->v.value.i != k_u64_values[i]){
				fail_("uint roundtrip mismatch", src);
			}
		}

		if(pass == 0){
			FILE *fp = tmpfile();
			size_t rlen;
			if(fp == NULL){ fail_("tmpfile", NULL); }
			if(nmjson_token_fout2(root, fp, 0, 1) < 0){ fail_("token_fout", NULL); }
			rewind(fp);
			rlen = fread(fout_buf, 1, sizeof(fout_buf) - 1, fp);
			fout_buf[rlen] = '\0';
			fclose(fp);
		}
	}
	printf("test21: int roundtrip (writer/fout) OK\n");
}

//---------------------------------------------------------------
// 浮動小数（現状は既知の問題を報告のみ）
//---------------------------------------------------------------

static const double k_dbl_values[] = {
	0.0, 1.0, -1.0, 0.5, 3.14159, 100.0, 1e21, 1e-7,
	0.1, 1.0 / 3.0, 0.1234567, 123456789.0, 2.718281828459045,
	1.7976931348623157e308, 2.2250738585072014e-308, 4.9406564584124654e-324,
	-0.0,
};

static int strict_float_(void){
	const char *e = getenv("NMJSON_TEST_STRICT_FLOAT");
	return e != NULL && strcmp(e, "1") == 0;
}

static void float_report_(int ng, const char *what){
	if(ng == 0){
		printf("test21: float %s OK\n", what);
		return;
	}
	if(strict_float_()){
		fprintf(stderr, "test21: FAIL: float %s: %d mismatch(es)\n", what, ng);
		exit(1);
	}
	printf("test21: KNOWN ISSUE: float %s: %d mismatch(es) (set NMJSON_TEST_STRICT_FLOAT=1 to fail)\n", what, ng);
}

/// \brief	writer → parser でビット単位で同じ値に戻るか
static int float_roundtrip_ng_(int verbose){
	static membuf_t mb;
	nmjson_writer_t writer;
	nmjson_parser_t parser;
	nmjson_token_t tokens[TOKENS_NUM];
	const nmjson_token_t *root;
	const nmjson_token_t *arr;
	size_t i;
	int ng = 0;

	membuf_reset_(&mb);
	nmjson_writer_init_cb(&writer, &mb, membuf_write_);
	nmjson_writer_with_array(&writer, NULL, {
		for(i = 0; i < ARRAY_LEN_(k_dbl_values); i++){
			nmjson_writer_put_float(&writer, NULL, k_dbl_values[i]);
		}
	});
	if(writer.err.num != 0){ fail_("writer error", writer.err.at); }
	if(verbose){ printf("test21: float written: %s\n", mb.buf); }

	nmjson_parser_init(&parser, tokens, TOKENS_NUM);
	nmjson_parser_reset(&parser);
	nmjson_parser_parse(&parser, mb.buf);
	if(parser.error.code != nmjson_error_complete){
		if(verbose){ printf("test21:   parse error(%d)\n", parser.error.code); }
		return (int)ARRAY_LEN_(k_dbl_values);
	}
	root = arr = nmjson_parser_get_root(&parser);
	(void)root;
	if(arr == NULL || arr->v.len != ARRAY_LEN_(k_dbl_values)){
		if(verbose){ printf("test21:   element count mismatch\n"); }
		return (int)ARRAY_LEN_(k_dbl_values);
	}
	for(i = 0; i < ARRAY_LEN_(k_dbl_values); i++){
		const nmjson_token_t *e = nmjson_token_nth_child(arr, (int)i);
		double got = nmjson_token_as_float(e, NAN);
		if(memcmp(&got, &k_dbl_values[i], sizeof(double)) != 0){
			if(verbose){ printf("test21:   [%zu] expected %.17g got %.17g\n", i, k_dbl_values[i], got); }
			ng ++;
		}
	}
	return ng;
}

/// \brief	小数点がカンマのロケールでも結果が変わらないか
static void test_float_locale_(void){
	static const char *k_locales[] = { "de_DE.UTF-8", "de_DE.utf8", "fr_FR.UTF-8", "fr_FR.utf8", NULL };
	const char *used = NULL;
	char *saved;
	size_t i;
	int ng;

	saved = setlocale(LC_NUMERIC, NULL);
	saved = (saved != NULL) ? strdup(saved) : NULL;
	for(i = 0; k_locales[i] != NULL; i++){
		if(setlocale(LC_NUMERIC, k_locales[i]) != NULL){
			used = k_locales[i];
			break;
		}
	}
	if(used == NULL){
		printf("test21: SKIP: float locale (no comma-decimal locale installed)\n");
		free(saved);
		return;
	}
	printf("test21: float locale check with LC_NUMERIC=%s\n", used);
	ng = float_roundtrip_ng_(1);
	setlocale(LC_NUMERIC, saved != NULL ? saved : "C");
	free(saved);
	float_report_(ng, "roundtrip under comma-decimal locale");
}

void test21(){
	test_int_text_();
	test_int_roundtrip_();
	float_report_(float_roundtrip_ng_(1), "roundtrip (bit exact)");
	test_float_locale_();
	printf("test21: done\n");
}
