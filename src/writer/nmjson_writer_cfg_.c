#include "include/nmjson/nmjson_writer.h"

/**
 *	\brief		json5用の出力を許可する場合にこれを使用する。
 *	\arg		self		: writerオブジェクト
 *	\arg		superset	: 大体json5かnormalの二択。
 *	\remarks	デフォルトはnmjson_superset_normal
 *	\remarks	json5ならNaN, Infinity/-Infinity, \xXX出力を許可する。
 */
void	nmjson_writer_cfg_superset(nmjson_writer_t *self, nmjson_superset_t superset){
	self->cfg.superset = superset;
}

/**
 *	\brief		出力文字列を「人間に見やすい形」に整えるフラグ
 *	\arg		self		: writerオブジェクト
 *	\arg		flag		: (bool) 1で「インデント/改行」をサポートする
 *	\remarks	デフォルトは0になっている。
 */
void	nmjson_writer_cfg_pretty_print(nmjson_writer_t *self, int flag){
	self->cfg.pretty_print = flag;
}

/**
 *	\brief		出力文字列を「unicode変換をしない」方向で処理するフラグ
 *	\arg		self		: writerオブジェクト
 *	\arg		flag		: (bool) 1でunicode変換を行わない。
 *	\remarks	デフォルトは0になっている。
 */
void	nmjson_writer_cfg_utf8_raw(nmjson_writer_t *self, int flag){
	self->cfg.utf8_raw = flag;
}

