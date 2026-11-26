/**
 *	\file	21-roundtrip-num.c
 *	\brief	数値の書き出し→読み戻しの検証
 *
 *	- 整数: writer出力がprintfと完全一致すること、parser→token_fout→parserで値が保存されること（失敗で exit(1)）
 *	- 浮動小数: 出力文字列(ECMAScript準拠)・最短性・ビット一致round-trip・writerのロケール非依存（失敗で exit(1)）
 *	  parser側のロケール依存(strtod)は既知の問題として KNOWN ISSUE 報告のみ。
 *	  環境変数 NMJSON_TEST_STRICT_FLOAT=1 で失敗扱いにする（parser修正後に有効化する想定）。
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
// 浮動小数
//---------------------------------------------------------------

/// \brief	期待する出力文字列（ECMAScript Number::toString準拠。-0のみ独自に"-0.0"）
static const struct { double v; const char *s; } k_dbl_text[] = {
	{ 0.0,                       "0" },
	{ -0.0,                      "-0.0" },
	{ 1.0,                       "1" },
	{ -1.5,                      "-1.5" },
	{ 0.1,                       "0.1" },
	{ 0.5,                       "0.5" },
	{ 0.1 + 0.2,                 "0.30000000000000004" },
	{ 1.0 / 3.0,                 "0.3333333333333333" },
	{ 2.0 / 3.0,                 "0.6666666666666666" },
	{ 3.14159,                   "3.14159" },
	{ 0.1234567,                 "0.1234567" },
	{ 100.0,                     "100" },
	{ 123456789.0,               "123456789" },
	{ 9007199254740992.0,        "9007199254740992" },
	{ 1e20,                      "100000000000000000000" },
	{ 1e21,                      "1e+21" },
	{ 1.5e300,                   "1.5e+300" },
	{ 1e-6,                      "0.000001" },
	{ 1.5e-6,                    "0.0000015" },
	{ -1.2345678901234567e-6,    "-0.0000012345678901234567" },
	{ 1e-7,                      "1e-7" },
	{ 1.23e-18,                  "1.23e-18" },
	{ 1.7976931348623157e308,    "1.7976931348623157e+308" },
	{ 2.2250738585072014e-308,   "2.2250738585072014e-308" },
	{ 4.9406564584124654e-324,   "5e-324" },
};

static int strict_float_(void){
	const char *e = getenv("NMJSON_TEST_STRICT_FLOAT");
	return e != NULL && strcmp(e, "1") == 0;
}

static void write_float_(membuf_t *mb, double v){
	nmjson_writer_t writer;
	membuf_reset_(mb);
	nmjson_writer_init_cb(&writer, mb, membuf_write_);
	if(nmjson_writer_put_float(&writer, NULL, v) < 0){ fail_("put_float", NULL); }
}

/// \brief	出力文字列が期待値と完全一致すること
static void test_float_text_(void){
	static membuf_t mb;
	size_t i;
	for(i = 0; i < ARRAY_LEN_(k_dbl_text); i++){
		write_float_(&mb, k_dbl_text[i].v);
		if(strcmp(mb.buf, k_dbl_text[i].s) != 0){
			fprintf(stderr, "test21: expected [%s] got [%s]\n", k_dbl_text[i].s, mb.buf);
			fail_("float text mismatch", k_dbl_text[i].s);
		}
	}
	printf("test21: float text: %zu values OK\n", ARRAY_LEN_(k_dbl_text));
}

/// \brief	文字列中の有効桁数（先頭/末尾の0を除いた仮数部の桁数）
static int significant_digits_(const char *s){
	const char *first = NULL;
	const char *last = NULL;
	int n = 0;
	const char *p;
	for(p = s; *p != '\0' && *p != 'e'; p++){
		if(*p >= '1' && *p <= '9'){
			if(first == NULL){ first = p; }
			last = p;
		}
	}
	if(first == NULL){ return 1; }
	for(p = first; p <= last; p++){
		if(*p >= '0' && *p <= '9'){ n++; }
	}
	return n;
}

/**
 *	\brief	vを読み戻せる最小の有効桁数（glibcのprintf/strtodを正解として使う）
 *	\remarks	p桁で正しく丸めた値が戻らなくても、末尾±1の隣の値なら戻る場合がある。
 *				p桁の候補はこの3つ以外に存在しえないため、3つとも試す。
 */
static int oracle_shortest_(double v){
	char tmp[64];
	char cand[64];
	int p;
	double a = fabs(v);
	for(p = 1; p < 17; p++){
		uint64_t m = 0;
		int e;
		int d;
		const char *c;
		snprintf(tmp, sizeof(tmp), "%.*e", p - 1, a);
		// "d.ddde±XX" → 整数仮数mと指数e
		for(c = tmp; *c != 'e'; c++){
			if(*c >= '0' && *c <= '9'){ m = m * 10 + (uint64_t)(*c - '0'); }
		}
		e = atoi(c + 1) - (p - 1);
		for(d = -1; d <= 1; d++){
			double back;
			if(m + d == 0){ continue; }
			snprintf(cand, sizeof(cand), "%" PRIu64 "e%d", m + d, e);
			back = strtod(cand, NULL);
			if(back == a){ return p; }
		}
	}
	return 17;
}

/**
 *	\brief		ランダムなビット列のdoubleで、round-trip（ビット一致）と最短性を検証する
 *	\remarks	件数は環境変数 NMJSON_TEST_FLOAT_RANDOM_N で変更可（デフォルト20000）
 */
static void test_float_random_(void){
	static membuf_t mb;
	uint64_t seed = 0xD1B54A32D192ED03ULL;
	int i;
	int count = 0;
	int total = 20000;
	const char *env_n = getenv("NMJSON_TEST_FLOAT_RANDOM_N");
	if(env_n != NULL && atoi(env_n) > 0){
		total = atoi(env_n);
	}
	for(i = 0; i < total; i++){
		uint64_t bits = xorshift64_(&seed);
		double v, back;
		char *endp;
		if(i & 1){
			// 半分は「キリのいい値」が出やすいよう、仮数の下位ビットを落とす
			bits &= ~((1ULL << (xorshift64_(&seed) % 53)) - 1);
		}
		memcpy(&v, &bits, sizeof(v));
		if(!isfinite(v)){ continue; }
		write_float_(&mb, v);
		back = strtod(mb.buf, &endp);
		if(*endp != '\0' || memcmp(&back, &v, sizeof(double)) != 0){
			fprintf(stderr, "test21: %.17g -> [%s] -> %.17g\n", v, mb.buf, back);
			fail_("float random roundtrip", mb.buf);
		}
		if(v != 0.0 && significant_digits_(mb.buf) != oracle_shortest_(v)){
			fprintf(stderr, "test21: %.17g -> [%s] digits=%d shortest=%d\n",
				v, mb.buf, significant_digits_(mb.buf), oracle_shortest_(v));
			fail_("float not shortest", mb.buf);
		}
		count ++;
	}
	printf("test21: float random: %d values roundtrip & shortest OK\n", count);
}

/// \brief	writer → parser → token_fout → parser でビット一致すること
static void test_float_roundtrip_(void){
	static membuf_t mb;
	static char fout_buf[MBUF_NUM];
	nmjson_writer_t writer;
	nmjson_parser_t parser;
	nmjson_token_t tokens[TOKENS_NUM];
	const nmjson_token_t *arr;
	size_t i;
	int pass;

	membuf_reset_(&mb);
	nmjson_writer_init_cb(&writer, &mb, membuf_write_);
	nmjson_writer_with_array(&writer, NULL, {
		for(i = 0; i < ARRAY_LEN_(k_dbl_text); i++){
			nmjson_writer_put_float(&writer, NULL, k_dbl_text[i].v);
		}
	});
	if(writer.err.num != 0){ fail_("writer error", writer.err.at); }

	for(pass = 0; pass < 2; pass++){
		char *src = (pass == 0) ? mb.buf : fout_buf;
		arr = parse_or_die_(&parser, tokens, src);
		if(arr == NULL || arr->v.len != ARRAY_LEN_(k_dbl_text)){ fail_("float array", src); }
		for(i = 0; i < ARRAY_LEN_(k_dbl_text); i++){
			double got = nmjson_token_as_float(nmjson_token_nth_child(arr, (int)i), NAN);
			if(memcmp(&got, &k_dbl_text[i].v, sizeof(double)) != 0){
				fprintf(stderr, "test21: pass%d [%zu] expected %.17g got %.17g\n", pass, i, k_dbl_text[i].v, got);
				fail_("float roundtrip mismatch", src);
			}
		}
		if(pass == 0){
			FILE *fp = tmpfile();
			size_t rlen;
			if(fp == NULL){ fail_("tmpfile", NULL); }
			if(nmjson_token_fout2(arr, fp, 0, 1) < 0){ fail_("token_fout", NULL); }
			rewind(fp);
			rlen = fread(fout_buf, 1, sizeof(fout_buf) - 1, fp);
			fout_buf[rlen] = '\0';
			fclose(fp);
		}
	}
	printf("test21: float roundtrip (writer/fout) OK\n");
}

/// \brief	小数点がカンマのロケールでも結果が変わらないか
static void test_float_locale_(void){
	static const char *k_locales[] = { "de_DE.UTF-8", "de_DE.utf8", "fr_FR.UTF-8", "fr_FR.utf8", NULL };
	static membuf_t mb_c;
	static membuf_t mb_l;
	const char *used = NULL;
	char *saved;
	size_t i;
	int parse_ng = 0;

	// まずCロケールでの出力を取っておく
	membuf_reset_(&mb_c);
	for(i = 0; i < ARRAY_LEN_(k_dbl_text); i++){
		static membuf_t one;
		write_float_(&one, k_dbl_text[i].v);
		membuf_write_(&(nmjson_writer_ctx_t){ .p = &mb_c }, one.buf, one.len);
		membuf_write_(&(nmjson_writer_ctx_t){ .p = &mb_c }, " ", 1);
	}

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

	// writer: ロケールに依存しないこと（失敗で exit）
	membuf_reset_(&mb_l);
	for(i = 0; i < ARRAY_LEN_(k_dbl_text); i++){
		static membuf_t one;
		write_float_(&one, k_dbl_text[i].v);
		membuf_write_(&(nmjson_writer_ctx_t){ .p = &mb_l }, one.buf, one.len);
		membuf_write_(&(nmjson_writer_ctx_t){ .p = &mb_l }, " ", 1);
	}

	// parser: strtod依存のため現状は既知の問題
	for(i = 0; i < ARRAY_LEN_(k_dbl_text); i++){
		static char src[64];
		nmjson_parser_t parser;
		nmjson_token_t tokens[4];
		double got;
		snprintf(src, sizeof(src), "[%s]", k_dbl_text[i].s);
		nmjson_parser_init(&parser, tokens, 4);
		nmjson_parser_reset(&parser);
		nmjson_parser_parse(&parser, src);
		if(parser.error.code != nmjson_error_complete){ parse_ng++; continue; }
		got = nmjson_token_as_float(nmjson_token_nth_child(nmjson_parser_get_root(&parser), 0), NAN);
		if(memcmp(&got, &k_dbl_text[i].v, sizeof(double)) != 0){ parse_ng++; }
	}

	setlocale(LC_NUMERIC, saved != NULL ? saved : "C");
	free(saved);

	if(strcmp(mb_c.buf, mb_l.buf) != 0){
		fprintf(stderr, "test21: C    : %s\ntest21: %s: %s\n", mb_c.buf, used, mb_l.buf);
		fail_("writer output depends on locale", used);
	}
	printf("test21: float writer under LC_NUMERIC=%s OK\n", used);

	if(parse_ng == 0){
		printf("test21: float parser under LC_NUMERIC=%s OK\n", used);
	}
	else if(strict_float_()){
		fprintf(stderr, "test21: FAIL: float parser under LC_NUMERIC=%s: %d mismatch(es)\n", used, parse_ng);
		exit(1);
	}
	else{
		printf("test21: KNOWN ISSUE: float parser under LC_NUMERIC=%s: %d/%zu mismatch(es) (strtod depends on locale; set NMJSON_TEST_STRICT_FLOAT=1 to fail)\n",
			used, parse_ng, ARRAY_LEN_(k_dbl_text));
	}
}

void test21(){
	test_int_text_();
	test_int_roundtrip_();
	test_float_text_();
	test_float_random_();
	test_float_roundtrip_();
	test_float_locale_();
	printf("test21: done\n");
}
