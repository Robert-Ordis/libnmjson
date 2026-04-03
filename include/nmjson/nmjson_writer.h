/**
 *	\file		nmjson_writer.h
 *	\brief		簡易的にJSON文字列を出力するためのもの
 *	\warning	値→安全な文字列は担当するが、「正しいJSON構造」の構築はユーザーの責務
 */

#ifndef	NMJSON_NMJSON_WRITER_H_
#define	NMJSON_NMJSON_WRITER_H_

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

#include <sys/types.h>

#include "./nmjson_str.h"
#include "./json_const.h"

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

/// \brief	コールバックのアダプタ。
typedef union {
	/// \brief	fdを持たせるときの互換用
	struct {
		int fd;
	}i32;
	/// \brief	普通はこちらを参照
	void *p;
} nmjson_writer_ctx_t;

/**
 *	\brief	自前のコンテキストに文字列を渡すためのコールバック
 *	\arg		ctx		: ctx->pで書き込み対象コンテキストとなる
 *	\arg		p		: 書き込み対象
 *	\arg		l		: 書き込み長[bytes]
 *	\return	>= 0	: 成功。書き込めた長さ。
 *	\return	 < 0	: 失敗。errnoが来ることが前提。
 *	\exception		: できればwriteに準拠したerrnoが分かりやすい。それ以外でも特に制限はしないが
 *	ssize_t sample_write_(nmjson_writer_ctx_t *pctx, const void *p, size_t l){
 *		sample_t *self = pctx->p;
 *		return sample_sprintf(self, "%s", (const char *)p);
 *	}
 *
 */
typedef ssize_t (*nmjson_writer_ctx_write_cb) \
	(nmjson_writer_ctx_t *pctx, const void *p, size_t l);

/**
 *	\brief	コメントを書き込むときの動作指定
 */
typedef enum nmjson_writer_comment_e {
	/**
	 *	\brief		コメント書き込み指定: 新しい行に出す。
	 */
	nmjson_writer_comment_newline,
	/**
	 *	\brief		コメント書き込み指定: トークンと同じ行に出す
	 *	\remarks	連続で書き込んだ後はnewlineと同じ動作になる
	 */
	nmjson_writer_comment_inline,
} nmjson_writer_comment_t;

/// \brief		
/**
 *	\brief		JSONを「比較的安全に」書くための軽量構造体
 *	\remarks	構造体の中身は極力触れない方向でお願いします。
 */
typedef struct nmjson_writer_s {
	struct {
		nmjson_writer_ctx_t			inst_;
		nmjson_writer_ctx_write_cb	write_;
	} ctx;
	
	struct {
		int							need_comma;
		int							depth;
	} state;
	
	struct {
		nmjson_superset_t			superset;
		int							pretty_print;
		int							utf8_raw;
	} cfg;
	
	struct {
		int							num;
		const char					*at;
	} err;
	
} nmjson_writer_t;

/**
 *	\brief		ユーザー定義の出力で初期化
 *	\arg		self		: 初期化対象
 *	\arg		user_data	: 書き込み用オブジェクト
 *	\arg		user_write	: nmjson_writer用書き込みアダプタ
 */
void	nmjson_writer_init_cb(nmjson_writer_t *self, void *user_data, nmjson_writer_ctx_write_cb user_write);

/**
 *	\brief		FILEポインターに向けて初期化
 *	\arg		self		: 初期化対象
 *	\arg		fp			: FILEポインタ。fwriteで書いていくイメージ
 *	\remarks	fwrite(3)での動作が基準となる。
 */
void	nmjson_writer_init_fp(nmjson_writer_t *self, FILE *fp);

/**
 *	\brief		ファイルディスクリプタに合わせた初期化
 *	\arg		self		: 初期化対象
 *	\arg		fd			: 書き込み先のファイルディスクリプタ
 *	\remarks	write(2)での動作が基準となる。
 */
void	nmjson_writer_init_fd(nmjson_writer_t *self, int fd);


//-----------設定系----------------
/**
 *	\brief		json5用の出力を許可する場合にこれを使用する。
 *	\arg		self		: writerオブジェクト
 *	\arg		superset	: 大体json5かnormalの二択。
 *	\remarks	デフォルトはnmjson_superset_normal
 *	\remarks	json5ならNaN, Infinity/-Infinity, \xXX出力を許可する。
 */
void	nmjson_writer_cfg_superset(nmjson_writer_t *self, nmjson_superset_t superset);

/**
 *	\brief		出力文字列を「人間に見やすい形」に整えるフラグ
 *	\arg		self		: writerオブジェクト
 *	\arg		flag		: (bool) 1で「インデント/改行」をサポートする
 *	\remarks	デフォルトは0になっている。
 */
void	nmjson_writer_cfg_pretty_print(nmjson_writer_t *self, int flag);

/**
 *	\brief		出力文字列を「unicode変換をしない」方向で処理するフラグ
 *	\arg		self		: writerオブジェクト
 *	\arg		flag		: (bool) 1でunicode変換を行わない。
 *	\remarks	デフォルトは0になっている。
 */
void	nmjson_writer_cfg_utf8_raw(nmjson_writer_t *self, int flag);

//-----------指定長のキー名書き込み------------
/**
 *	\brief		オブジェクト"{"か配列"["の開始を記述する。
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		container_type	: nmjson_type_objectかnmjson_type_arrayのどちらか。
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t	nmjson_writer_begin_n(nmjson_writer_t *self, const nmjson_str_t *key, nmjson_type_t container_type);

/**
 *	\brief		null の書き出し
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 親がオブジェクトの場合のキー名。配列要素の場合は NULL。
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_nullobj_n(nmjson_writer_t *self, const nmjson_str_t *key);

/**
 *	\brief		bool の書き出し
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		val				: _n(bool)0でfalse, それ以外でtrue
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_bool_n(nmjson_writer_t *self, const nmjson_str_t *key, int val);

/**
 *	\brief		整数の書き出し
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		val				: 整数値
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_int_n(nmjson_writer_t *self, const nmjson_str_t *key, int64_t val);

/**
 *	\brief		整数の書き出し
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		val				: 整数値
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_uint_n(nmjson_writer_t *self, const nmjson_str_t *key, uint64_t val);

/**
 *	\brief		実数の書き出し（%g フォーマット）
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		val				: 浮動小数値
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_float_n(nmjson_writer_t *self, const nmjson_str_t *key, double val);

/**
 *	\brief		文字列の書き出し（エスケープ処理込み）
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		val				: 文字列。NULL の場合は null として出力する
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_string_n(nmjson_writer_t *self, const nmjson_str_t *key, const char *val);


//-----------ここ以降書き込み系--------------
/**
 *	\brief		オブジェクト"{"か配列"["の開始を記述する。
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		container_type	: nmjson_type_objectかnmjson_type_arrayのどちらか。
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t	nmjson_writer_begin(nmjson_writer_t *self, const char *key, nmjson_type_t container_type);

/**
 *	\brief		オブジェクト"}"か配列"]"の終了を記述する。
 *	\arg		self			: writeオブジェクト
 *	\arg		container_type	: nmjson_type_objectかnmjson_type_arrayのどちらか。
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t	nmjson_writer_end(nmjson_writer_t *self, nmjson_type_t container_type);

/**
 *	\brief		null の書き出し
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 親がオブジェクトの場合のキー名。配列要素の場合は NULL。
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_nullobj(nmjson_writer_t *self, const char *key);

/**
 *	\brief		bool の書き出し
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		val				: (bool)0でfalse, それ以外でtrue
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_bool(nmjson_writer_t *self, const char *key, int val);

/**
 *	\brief		整数の書き出し
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		val				: 整数値
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_int(nmjson_writer_t *self, const char *key, int64_t val);

/**
 *	\brief		整数の書き出し
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		val				: 整数値
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_uint(nmjson_writer_t *self, const char *key, uint64_t val);

/**
 *	\brief		実数の書き出し（%g フォーマット）
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		val				: 浮動小数値
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_float(nmjson_writer_t *self, const char *key, double val);

/**
 *	\brief		文字列の書き出し（エスケープ処理込み）
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		val				: 文字列。NULL の場合は null として出力する
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_string(nmjson_writer_t *self, const char *key, const char *val);

/**
 *	\brief		1行コメントを押し込む
 *	\arg		self			: writeオブジェクト
 *	\arg		line_mode		: 「どの行に書き込むか」の指定
 *	\arg		comment			: コメント。
 *	\return	>= 0			: 成功。書き込んだ文字数。無効の時は0。
 *	\return	 < 0			: 失敗
 *	\remarks	(pretty_print == 1 && (superset == nmjson_superset_jsonc || superset == nmjson_superset_json5))の時のみ有効
 *	\todo		未実装
 */
ssize_t	nmjson_writer_put_comment(nmjson_writer_t *self, nmjson_writer_comment_t mode, const char *val);


//-----------おまけ。Cの文法から外れていいなら------------
/**
 *	\brief		オブジェクトをちょっと便利に書くマクロ
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: トークン名。配列下ならNULL。
 *	\arg		...				: 中に記述する文。{...}の形式で必ず書く。
 *	\warning	最後の引数は{...}形式で記述する。
 *	\warning	内部では、トップレベルでbreakすることで中断する。
 *	\warning	内部でfor/while/switchを行っている場合、breakはその中で適用されてしまいます
 *	\warning	内部ではgoto/returnは使用しないでください
 *	\pre		下記のように書きます。
 *	nmjson_writer_with_object(pwriter, "name", {
 *		int i;
 *		char name[32];
 *		for(i = 0; i < 32; i++){
 *			sprintf(name, "value%02d", i);
 *			if(nmjson_writer_put_int(pwriter, name, i) < 0){
 *				break;
 *			}
 *		}
 *		if(i != 32){ break; }
 *		nmjson_writer_with_array(pwriter, "arr", {
 *			for(i = 0; i < 32; i++){
 *				if(nmjson_writer_put_int(pwriter, NULL, i) < 0){
 *					break;
 *				}
 *			}
 *		});
 *	});
 */
#define	nmjson_writer_with_object(self, key, ...)\
	do{\
		if(nmjson_writer_begin((self), (key), nmjson_type_object) < 0) { break; }\
		do\
			__VA_ARGS__\
		while(0);\
		nmjson_writer_end((self), nmjson_type_object);\
	}while(0)

/**
 *	\brief		配列をちょっと便利に書くマクロ
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: トークン名。配列下ならNULL。
 *	\arg		...				: 中に記述する文。{...}の形式で必ず書く。
 *	\warning	最後の引数は{...}形式で記述する。
 *	\warning	内部では、トップレベルでbreakすることで中断する。
 *	\warning	内部でfor/while/switchを行っている場合、breakはその中で適用されてしまいます
 *	\warning	内部ではgoto/returnは使用しないでください
 */
#define	nmjson_writer_with_array(self, key, ...)\
	do{\
		if(nmjson_writer_begin((self), (key), nmjson_type_array) < 0) { break; }\
		do\
			__VA_ARGS__\
		while(0);\
		nmjson_writer_end((self), nmjson_type_array);\
	}while(0)

/**
 *	\brief		オブジェクトをちょっと便利に書くマクロ
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: トークン名。配列下ならNULL。
 *	\arg		...				: 中に記述する文。{...}の形式で必ず書く。
 *	\warning	最後の引数は{...}形式で記述する。
 *	\warning	内部では、トップレベルでbreakすることで中断する。
 *	\warning	内部でfor/while/switchを行っている場合、breakはその中で適用されてしまいます
 *	\warning	内部ではgoto/returnは使用しないでください
 */
#define	nmjson_writer_with_object_n(self, key, ...)\
	do{\
		if(nmjson_writer_begin_n((self), (key), nmjson_type_object) < 0) { break; }\
		do\
			__VA_ARGS__\
		while(0);\
		nmjson_writer_end((self), nmjson_type_object);\
	}while(0)

/**
 *	\brief		配列をちょっと便利に書くマクロ
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: トークン名。配列下ならNULL。
 *	\arg		...				: 中に記述する文。{...}の形式で必ず書く。
 *	\warning	最後の引数は{...}形式で記述する。
 *	\warning	内部では、トップレベルでbreakすることで中断する。
 *	\warning	内部でfor/while/switchを行っている場合、breakはその中で適用されてしまいます
 *	\warning	内部ではgoto/returnは使用しないでください
 */
#define	nmjson_writer_with_array_n(self, key, ...)\
	do{\
		if(nmjson_writer_begin_n((self), (key), nmjson_type_array) < 0) { break; }\
		do\
			__VA_ARGS__\
		while(0);\
		nmjson_writer_end((self), nmjson_type_array);\
	}while(0)


#ifdef	__cplusplus
}
#endif	/* __cplusplus */


#endif	/* !NMJSON_NMJSON_WRITER_H_ */
