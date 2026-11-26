/**
 *	\file	25-buffer-resize.c
 *	\brief	nmjson_buffer のバッファ入れ替え(grow/shrink)の検証（失敗で exit(1)）
 *
 *	- 貸し出し版: 予め用意したバッファを貸し借りする。入れ替え後に旧バッファを0xAAで塗りつぶし、
 *	  ポインタの付け替え漏れがあれば必ず結果がずれるようにする。
 *	  初期バッファ長と拡張幅を総当たりし、文字列/エスケープ/キー/数値/コメントの途中など様々な位置で入れ替える。
 *	- realloc版: malloc版を想定し、reallocで伸縮する。
 *	- 拡張拒否: 従来どおり nmjson_error_buffer になること。
 *	- 契約違反: 条件を満たさないバッファが返っても、メモリを壊さずエラーになること。
 *	- read_for_cmd 経由でも同様に動くこと。
 */
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "include/nmjson/nmjson.h"
#include "include/nmjson/nmjson_buffer.h"

#define TOKENS_NUM	128
#define POOL_LEN	4096
#define CANON_LEN	4096
#define MSG_MAX		16

//---------------------------------------------------------------
// 入力
//---------------------------------------------------------------

static char g_long_msg[1024];

static const char *k_msgs_fixed[] = {
	"{\"cmd\":\"set\",\"name\":\"\\u3042\\u3044\\u3046 long string crossing buffer boundaries\","
		"\"esc\":\"a\\\\b\\\"c\\n\\t/\",\"arr\":[1,2.5,-3e2,true,false,null,{\"k\":\"v\"}],"
		"\"emoji\":\"\\uD83D\\uDE42\"}",
	"{\"cmd\":\"deep\",\"x\":{\"deep\":{\"deeper\":{\"deepest\":\"value with spaces\"}}},"
		"\"big\":12345678901234567890, /* comment in between */ \"neg\":-0.000123,"
		"// line comment\n \"tail\":\"z\"}",
	"{\"cmd\":\"s\",\"a\":1}",
	NULL,	// g_long_msg
	"{\"cmd\":\"s\",\"b\":2}",
};
#define MSG_NUM	(sizeof(k_msgs_fixed) / sizeof(k_msgs_fixed[0]))

static const char *msg_(size_t i){
	return (k_msgs_fixed[i] != NULL) ? k_msgs_fixed[i] : g_long_msg;
}

static void build_long_msg_(void){
	size_t n = 0;
	int i;
	n += snprintf(&g_long_msg[n], sizeof(g_long_msg) - n, "{\"cmd\":\"long\",\"text\":\"");
	for(i = 0; i < 40; i++){
		n += snprintf(&g_long_msg[n], sizeof(g_long_msg) - n, "line%02d\\n\\u00e9\\\"", i);
	}
	n += snprintf(&g_long_msg[n], sizeof(g_long_msg) - n, "\",\"k2\":\"end\",\"n\":[0.1,1e-7,123456]}");
}

/// \brief	ストリーム全体(メッセージ間に空白とコメントを挟む)
static size_t build_stream_(char *dst, size_t cap){
	size_t n = 0;
	size_t i;
	for(i = 0; i < MSG_NUM; i++){
		n += snprintf(&dst[n], cap - n, "%s%s", msg_(i), (i == 2) ? " /* between */\n" : "\n ");
	}
	return n;
}

//---------------------------------------------------------------
// 結果の正規化（トークン木→文字列）
//---------------------------------------------------------------

static void canon_bytes_(char *dst, size_t *n, const char *s, size_t len){
	size_t i;
	for(i = 0; i < len; i++){
		uint8_t c = (uint8_t)s[i];
		if(c >= 0x20 && c < 0x7F && c != '%'){
			dst[(*n)++] = (char)c;
		}
		else{
			*n += snprintf(&dst[*n], CANON_LEN - *n, "%%%02X", c);
		}
	}
}

static void canon_(const nmjson_token_t *t, char *dst, size_t *n){
	const nmjson_token_t *c;
	if(t->n.name != NULL){
		dst[(*n)++] = 'K';
		canon_bytes_(dst, n, t->n.name, t->n.len);
		dst[(*n)++] = '=';
	}
	switch(t->v.type){
	case nmjson_type_object:
	case nmjson_type_array:
		dst[(*n)++] = (t->v.type == nmjson_type_object) ? '{' : '[';
		for(c = nmjson_token_head_child(t); c != NULL; c = nmjson_token_next(c)){
			canon_(c, dst, n);
			dst[(*n)++] = ',';
		}
		dst[(*n)++] = (t->v.type == nmjson_type_object) ? '}' : ']';
		break;
	case nmjson_type_string:
		dst[(*n)++] = 'S';
		canon_bytes_(dst, n, t->v.value.s, t->v.len);
		break;
	case nmjson_type_integer:
		*n += snprintf(&dst[*n], CANON_LEN - *n, "I%" PRId64, t->v.value.i);
		break;
	case nmjson_type_float:
		*n += snprintf(&dst[*n], CANON_LEN - *n, "F%a", t->v.value.d);
		break;
	case nmjson_type_bool:
		*n += snprintf(&dst[*n], CANON_LEN - *n, "B%d", t->v.value.b);
		break;
	case nmjson_type_null:
		dst[(*n)++] = 'N';
		break;
	default:
		*n += snprintf(&dst[*n], CANON_LEN - *n, "?%d", (int)t->v.type);
		break;
	}
	dst[*n] = '\0';
}

static char g_expected[MSG_NUM][CANON_LEN];

static void build_expected_(void){
	static nmjson_token_t tokens[TOKENS_NUM];
	static char src[2048];
	nmjson_parser_t parser;
	size_t i;
	for(i = 0; i < MSG_NUM; i++){
		size_t n = 0;
		snprintf(src, sizeof(src), "%s", msg_(i));
		nmjson_parser_init(&parser, tokens, TOKENS_NUM);
		nmjson_parser_reset_as_superset(&parser, nmjson_superset_jsonc);
		nmjson_parser_parse(&parser, src);
		if(parser.error.code != nmjson_error_complete){
			fprintf(stderr, "test25: FAIL: expected msg %zu parse error %d\n", i, parser.error.code);
			exit(1);
		}
		canon_(nmjson_parser_get_root(&parser), g_expected[i], &n);
	}
}

//---------------------------------------------------------------
// 受信側
//---------------------------------------------------------------

typedef struct {
	const char	*label;
	size_t		received;
	int			errors[8];	// nmjson_error_t ごとの件数
	int			ng;
} recv_ctx_t;

static int on_json_(const nmjson_token_t *root, nmjson_error_t error, int fd, void *arg){
	recv_ctx_t *ctx = arg;
	static char got[CANON_LEN];
	size_t n = 0;
	(void)fd;
	if(error != nmjson_error_complete){
		ctx->errors[error] ++;
		return 0;
	}
	if(ctx->received >= MSG_NUM){
		fprintf(stderr, "test25: NG: [%s] too many messages\n", ctx->label);
		ctx->ng ++;
		return 0;
	}
	canon_(root, got, &n);
	if(strcmp(got, g_expected[ctx->received]) != 0){
		fprintf(stderr, "test25: NG: [%s] msg %zu mismatch\n  expected: %s\n  got     : %s\n",
			ctx->label, ctx->received, g_expected[ctx->received], got);
		ctx->ng ++;
	}
	ctx->received ++;
	return 0;
}

/// \brief	streamをパイプ経由で読み切る
static void run_stream_(nmjson_buffer_t *b, const char *stream, size_t len, recv_ctx_t *ctx, int use_cmd){
	static nmjson_cmd_elem_t cmds[] = {
		{ "set",  on_json_ },
		{ "deep", on_json_ },
		{ "long", on_json_ },
		{ NULL,   on_json_ },	// その他("s")
	};
	int fds[2];
	int ret;
	if(pipe(fds) != 0){ perror("pipe"); exit(1); }
	if(write(fds[1], stream, len) != (ssize_t)len){ perror("write"); exit(1); }
	close(fds[1]);
	do{
		ret = use_cmd
			? nmjson_buffer_read_for_cmd(b, cmds, "cmd", fds[0], ctx)
			: nmjson_buffer_read(b, on_json_, fds[0], ctx);
	}while(ret >= 0 || ret <= -3);
	close(fds[0]);
}

//---------------------------------------------------------------
// 貸し出し版
//---------------------------------------------------------------

typedef struct {
	char	*initial;
	size_t	initial_len;
	char	pool[2][POOL_LEN];
	size_t	step;
	int		grows;
	int		shrinks;
	int		violate;	// 1: 契約違反のバッファを返す
} lend_ctx_t;

static char *lend_cb_(nmjson_buffer_resize_t reason, char *cur, size_t cur_len, size_t used_len, size_t *pnew_len, void *arg){
	lend_ctx_t *ctx = arg;
	char *next;
	size_t next_len;
	if(reason == nmjson_buffer_resize_grow){
		next = (cur == ctx->pool[0]) ? ctx->pool[1] : ctx->pool[0];
		next_len = cur_len + ctx->step;
		if(next_len > POOL_LEN){
			return NULL;
		}
		if(ctx->violate){
			next_len = cur_len;	// 伸びていない
		}
		ctx->grows ++;
	}
	else{
		if(cur == ctx->initial || used_len + 2 > ctx->initial_len){
			return NULL;
		}
		next = ctx->initial;
		next_len = ctx->initial_len;
		ctx->shrinks ++;
	}
	memcpy(next, cur, used_len + 1);
	memset(cur, 0xAA, cur_len);	// 旧バッファを壊す: 付け替え漏れの検出用
	*pnew_len = next_len;
	return next;
}

static void test_lend_(const char *stream, size_t len, int use_cmd){
	static lend_ctx_t lend;
	static char initial[64];
	static nmjson_token_t tokens[TOKENS_NUM];
	nmjson_buffer_t b;
	size_t init_len;
	static const size_t k_steps[] = { 1, 3, 7, 16, 61 };
	size_t si;
	int runs = 0;
	int total_grows = 0;
	int total_shrinks = 0;
	char label[64];

	for(init_len = 4; init_len <= sizeof(initial); init_len++){
		for(si = 0; si < sizeof(k_steps) / sizeof(k_steps[0]); si++){
			recv_ctx_t ctx;
			memset(&ctx, 0, sizeof(ctx));
			memset(&lend, 0, sizeof(lend));
			snprintf(label, sizeof(label), "lend%s init=%zu step=%zu", use_cmd ? "/cmd" : "", init_len, k_steps[si]);
			ctx.label = label;
			lend.initial = initial;
			lend.initial_len = init_len;
			lend.step = k_steps[si];
			nmjson_buffer_init_superset(&b, tokens, TOKENS_NUM, initial, init_len, nmjson_superset_jsonc);
			nmjson_buffer_set_resize_cb(&b, lend_cb_, &lend);
			run_stream_(&b, stream, len, &ctx, use_cmd);
			if(ctx.ng != 0 || ctx.received != MSG_NUM){
				fprintf(stderr, "test25: FAIL: [%s] received %zu/%zu, ng=%d, errors(syntax=%d,tokens=%d,buffer=%d)\n",
					label, ctx.received, MSG_NUM, ctx.ng,
					ctx.errors[nmjson_error_syntax], ctx.errors[nmjson_error_tokens], ctx.errors[nmjson_error_buffer]);
				exit(1);
			}
			total_grows += lend.grows;
			total_shrinks += lend.shrinks;
			runs ++;
		}
	}
	if(total_grows == 0 || total_shrinks == 0){
		fprintf(stderr, "test25: FAIL: lend: no resize happened (grows=%d shrinks=%d)\n", total_grows, total_shrinks);
		exit(1);
	}
	printf("test25: lend%s: %d runs OK (grows=%d, shrinks=%d)\n", use_cmd ? "/cmd" : "", runs, total_grows, total_shrinks);
}

//---------------------------------------------------------------
// realloc版
//---------------------------------------------------------------

typedef struct {
	size_t	min_len;
	size_t	max_len;
	int		grows;
	int		shrinks;
} realloc_ctx_t;

static char *realloc_cb_(nmjson_buffer_resize_t reason, char *cur, size_t cur_len, size_t used_len, size_t *pnew_len, void *arg){
	realloc_ctx_t *ctx = arg;
	size_t next_len;
	char *next;
	if(reason == nmjson_buffer_resize_grow){
		next_len = cur_len * 2;
		if(next_len > ctx->max_len){ next_len = ctx->max_len; }
		if(next_len <= cur_len){ return NULL; }
		ctx->grows ++;
	}
	else{
		if(cur_len <= ctx->min_len || used_len + 2 > ctx->min_len){ return NULL; }
		next_len = ctx->min_len;
		ctx->shrinks ++;
	}
	if((next = realloc(cur, next_len)) == NULL){
		return NULL;
	}
	*pnew_len = next_len;
	return next;
}

static void test_realloc_(const char *stream, size_t len){
	static nmjson_token_t tokens[TOKENS_NUM];
	nmjson_buffer_t b;
	realloc_ctx_t rc;
	recv_ctx_t ctx;
	size_t init_len;
	int grows = 0, shrinks = 0;
	for(init_len = 4; init_len <= 32; init_len++){
		char *buf = malloc(init_len);
		memset(&ctx, 0, sizeof(ctx));
		memset(&rc, 0, sizeof(rc));
		ctx.label = "realloc";
		rc.min_len = init_len;
		rc.max_len = POOL_LEN;
		nmjson_buffer_init_superset(&b, tokens, TOKENS_NUM, buf, init_len, nmjson_superset_jsonc);
		nmjson_buffer_set_resize_cb(&b, realloc_cb_, &rc);
		run_stream_(&b, stream, len, &ctx, 0);
		free(b.cmdbuf);
		if(ctx.ng != 0 || ctx.received != MSG_NUM){
			fprintf(stderr, "test25: FAIL: [realloc init=%zu] received %zu/%zu ng=%d\n", init_len, ctx.received, MSG_NUM, ctx.ng);
			exit(1);
		}
		grows += rc.grows;
		shrinks += rc.shrinks;
	}
	if(grows == 0 || shrinks == 0){
		fprintf(stderr, "test25: FAIL: realloc: no resize happened\n");
		exit(1);
	}
	printf("test25: realloc: OK (grows=%d, shrinks=%d)\n", grows, shrinks);
}

//---------------------------------------------------------------
// 拒否・契約違反
//---------------------------------------------------------------

static void test_refuse_and_violation_(void){
	static lend_ctx_t lend;
	static char initial[32];
	static nmjson_token_t tokens[TOKENS_NUM];
	nmjson_buffer_t b;
	recv_ctx_t ctx;
	const char *one = msg_(3);	// 長いメッセージ

	// 拡張拒否(コールバック未設定と同じ): nmjson_error_buffer
	memset(&ctx, 0, sizeof(ctx));
	ctx.label = "no-cb";
	nmjson_buffer_init_superset(&b, tokens, TOKENS_NUM, initial, sizeof(initial), nmjson_superset_jsonc);
	run_stream_(&b, one, strlen(one), &ctx, 0);
	if(ctx.errors[nmjson_error_buffer] == 0){
		fprintf(stderr, "test25: FAIL: no-cb should report nmjson_error_buffer\n");
		exit(1);
	}

	// 上限で拒否: 上限までは伸び、その後 nmjson_error_buffer
	memset(&ctx, 0, sizeof(ctx));
	memset(&lend, 0, sizeof(lend));
	ctx.label = "refuse";
	lend.initial = initial;
	lend.initial_len = sizeof(initial);
	lend.step = POOL_LEN;	// 1回目でPOOL_LENを超える→拒否
	nmjson_buffer_init_superset(&b, tokens, TOKENS_NUM, initial, sizeof(initial), nmjson_superset_jsonc);
	nmjson_buffer_set_resize_cb(&b, lend_cb_, &lend);
	run_stream_(&b, one, strlen(one), &ctx, 0);
	if(ctx.errors[nmjson_error_buffer] == 0){
		fprintf(stderr, "test25: FAIL: refuse should report nmjson_error_buffer\n");
		exit(1);
	}

	// 契約違反(伸びていないバッファ): エラーになり、メモリを壊さない
	memset(&ctx, 0, sizeof(ctx));
	memset(&lend, 0, sizeof(lend));
	ctx.label = "violation";
	lend.initial = initial;
	lend.initial_len = sizeof(initial);
	lend.step = 8;
	lend.violate = 1;
	nmjson_buffer_init_superset(&b, tokens, TOKENS_NUM, initial, sizeof(initial), nmjson_superset_jsonc);
	nmjson_buffer_set_resize_cb(&b, lend_cb_, &lend);
	run_stream_(&b, one, strlen(one), &ctx, 0);
	// (破棄後に残りの断片がたまたまJSONとして成立する可能性はあるため、受信件数は問わない)
	if(ctx.errors[nmjson_error_buffer] == 0){
		fprintf(stderr, "test25: FAIL: violation should report nmjson_error_buffer\n");
		exit(1);
	}
	printf("test25: refuse/violation OK\n");
}

void test25(){
	static char stream[8192];
	size_t len;
	build_long_msg_();
	build_expected_();
	len = build_stream_(stream, sizeof(stream));
	test_lend_(stream, len, 0);
	test_lend_(stream, len, 1);
	test_realloc_(stream, len);
	test_refuse_and_violation_();
	printf("test25: done\n");
}
