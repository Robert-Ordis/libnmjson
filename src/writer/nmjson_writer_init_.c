#include <string.h>
#include <unistd.h>

#include "include/nmjson/nmjson_writer.h"

/**
 *	\brief		ユーザー定義の出力で初期化
 *	\arg		self		: 初期化対象
 *	\arg		user_data	: 書き込み用オブジェクト
 *	\arg		user_write	: nmjson_writer用書き込みアダプタ
 */
void	nmjson_writer_init_cb(nmjson_writer_t *self, void *user_data, nmjson_writer_ctx_write_cb user_write){
	memset(self, 0, sizeof(nmjson_writer_t));
	self->ctx.inst_.p = user_data;
	self->ctx.write_ = user_write;
	self->cfg.superset = nmjson_superset_none;
}


static ssize_t write_fp_(nmjson_writer_ctx_t *ctx_inst, const void *p, size_t l){
	FILE *fp = ctx_inst->p;
	size_t r = fwrite(p, 1, l, fp);
	return (r == l) ? r : -1;
}
/**
 *	\brief		FILEポインターに向けて初期化
 *	\arg		self		: 初期化対象
 *	\arg		fp			: FILEポインタ。fwriteで書いていくイメージ
 *	\remarks	fwrite(3)での動作が基準となる。
 */
void	nmjson_writer_init_fp(nmjson_writer_t *self, FILE *fp){
	nmjson_writer_init_cb(self, fp, write_fp_);
}

static ssize_t write_fd_(nmjson_writer_ctx_t *ctx_inst, const void *p, size_t l){
	return write(ctx_inst->i32.fd, p, l);
}
/**
 *	\brief		ファイルディスクリプタに合わせた初期化
 *	\arg		self		: 初期化対象
 *	\arg		fd			: 書き込み先のファイルディスクリプタ
 *	\remarks	write(2)での動作が基準となる。
 */
void	nmjson_writer_init_fd(nmjson_writer_t *self, int fd){
	nmjson_writer_init_cb(self, NULL, write_fd_);
	self->ctx.inst_.i32.fd = fd;
}
