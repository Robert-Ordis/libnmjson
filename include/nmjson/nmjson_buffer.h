/**
 *	\file	nmjson_buffer.h
 *	\brief	JSONを文字から読み込むためのバッファ構造
 */
#ifndef	NMJSON_NMJSON_BUFFER_H_
#define	NMJSON_NMJSON_BUFFER_H_

#include <inttypes.h>

#include "./json_const.h"
#include "./json_parser.h"
#include "./json_token.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 *	\brief	JSONを１個読み込めた時に呼び出すコールバック
 *	\arg	token_root	:rootのJSON。ここから値を探る。
 *	\arg	error		:解析結果。通常はnmjson_error_completeとなる。incompleteは出てこない。
 *	\arg	fd			:返事するためのファイルディスクリプタ
 *	\arg	arg			:呼び出し元から渡される任意のポインタ
 *	\return			:読み込みループがまだ必要な時に0。もうループを抜けて良いときにそれ以外。
 *	\return			:error != nmjson_error_completeの時は戻リ値に意味はない。
 *	\exception		:nmjson_error_syntax 構文エラー
 *	\exception		:nmjson_error_buffer 元バッファが足りなくなった(=JSONが長すぎるか、リテラルが閉じられていない)
 *	\exception		:nmjson_error_tokens トークンが枯渇した(=要素が多すぎた)
 */
typedef int (*nmjson_buffer_dispatch_cb)(const nmjson_token_t *token_root, nmjson_error_t error, int fd, void *arg);

/**
 *	\brief	特定のパラメータ名を以って受信した際、パラメータ名の値に従って処理を振り分けるためのコールバック
 */
typedef struct {
	const char					*name;
	nmjson_buffer_dispatch_cb	func;
} nmjson_cmd_elem_t;


/**
 *	\brief	バッファの入れ替え理由
 */
typedef enum nmjson_buffer_resize_e {
	/// \brief	解析途中でバッファが満杯になった。より大きいバッファが必要。
	nmjson_buffer_resize_grow,
	/// \brief	JSONを1個処理し終えた(または破棄した)。小さいバッファへ戻す機会。
	nmjson_buffer_resize_shrink,
} nmjson_buffer_resize_t;

/**
 *	\brief	読み込みバッファを入れ替えるためのコールバック
 *	\arg	reason		: 入れ替え理由
 *	\arg	cur_buf		: 現在のバッファ
 *	\arg	cur_len		: 現在のバッファ長
 *	\arg	used_len	: 使用中の長さ(終端'\0'を除く)
 *	\arg	pnew_len	: [out] 新しいバッファの長さ
 *	\arg	arg			: nmjson_buffer_set_resize_cb で渡した任意のポインタ
 *	\return	新しいバッファ。NULLなら入れ替えない(growの場合は従来どおり nmjson_error_buffer になる)
 *	\pre	新しいバッファの先頭 used_len + 1 バイトに、cur_buf の同じ範囲をそのまま写しておくこと
 *			(reallocはこれを満たす。自前のバッファを貸し出す場合は memcpy すること)。
 *			cur_buf はコールバックから戻った後ライブラリから参照されないため、コールバック内で解放・返却してよい。
 *	\pre	*pnew_len >= used_len + 2 であること(続きを1バイト以上読める大きさ)。
 *			growでは *pnew_len > cur_len であること。満たさない場合はバッファを採用した上で中身を破棄し、エラーとする。
 *	\remarks	cur_buf をそのまま返し、長さだけ変えてもよい(realloc が同じ場所で伸びた場合など)。
 *	\remarks	shrink は処理済みのJSONを捨てた直後に毎回呼ばれる。入れ替え不要ならNULLを返すこと。
 *	\warning	入れ替え後、dispatchコールバックで受け取ったトークンから得た文字列ポインタを保持していた場合、それは無効になる。
 */
typedef char *(*nmjson_buffer_resize_cb)(
	nmjson_buffer_resize_t reason,
	char *cur_buf, size_t cur_len, size_t used_len,
	size_t *pnew_len, void *arg
);

/**
 *	\brief	fdを通してストリームでjsonを扱うときの助け
 */
struct nmjson_buffer_s {
	/// \brief	パーサー本体
	nmjson_parser_t	parser;
	/// \brief	トータルで何バイトまでパースしたか=次ロードするときの地点。incompleteの時に使います。
	int				index;
	/// \brief	トークン
	nmjson_token_t	*tokens;
	/// \brief	トークンの上限数
	size_t			tokens_num;
	/// \brief	JSONをパースする元バッファ
	char			*cmdbuf;
	/// \brief	元バッファの長さ上限
	size_t			cmdbuf_len;
	
	struct {
		nmjson_buffer_resize_cb	resize_cb;
		void				*resize_arg;
		nmjson_cmd_elem_t	*cmd_array;
		nmjson_superset_t	superset;
		nmjson_error_t		last_result;
	}private;
};

typedef struct nmjson_buffer_s nmjson_buffer_t;


/**
 *	\brief	読み込み機の初期化
 *	\arg	self		:読み込みインスタンス
 *	\arg	tokens		:JSONトークン。
 *	\arg	tokens_num	:mallocなしでパースする場合の最大トークン数。
 *	\arg	cmdbuf		:読み込み元バッファ
 *	\arg	cmdbuf_len	:読み込み元バッファ長
 */
int		nmjson_buffer_init(
	nmjson_buffer_t *self, 
	nmjson_token_t tokens[],
	size_t tokens_num,
	char cmdbuf[],
	size_t cmdbuf_len
);

int		nmjson_buffer_init_superset(
	nmjson_buffer_t *self, 
	nmjson_token_t tokens[],
	size_t tokens_num,
	char cmdbuf[],
	size_t cmdbuf_len,
	nmjson_superset_t superset
);



/**
 *	\brief	バッファ入れ替え用のコールバックを設定する
 *	\arg	self		: nmjson_buffer_tのインスタンス(init済みであること)
 *	\arg	cb			: コールバック。NULLで入れ替え無効(従来動作)
 *	\arg	arg			: コールバックに渡す任意のポインタ
 *	\remarks	malloc版では realloc を使うコールバックを、mallocなし版では予め用意した大小のバッファを
 *				貸し借りするコールバックを渡す想定。上限サイズはコールバック側で決める。
 */
void	nmjson_buffer_set_resize_cb(nmjson_buffer_t *self, nmjson_buffer_resize_cb cb, void *arg);

/**
 *	\brief	fdからJSONを読みこみ、解析した結果を扱う
 *	\arg	self		:nmjson_buffer_tのインスタンス
 *	\arg	dispatch	:読み込めたJSON一つを扱うためのコールバック
 *	\arg	fd			:読み込み元のfd。大抵はソケットやファイル
 *	\arg	*arg		:コールバックに渡したい任意のデータ
 *	\return			:0→まだループを回す必要があると言われた（例えばワンショット用に読み込んでいる）
 *	\return			:1→もうこれ以上の読み込みは必要ないと言われた
 *	\return			-1→EOFを検出した（ファイルの終わりやソケットの切断）
 *	\return			-2→システムエラー。バッファは自動的にクリーンされます
 *	\return			-3→ライブラリ側のエラー。バッファは自動的にクリーンされます。
 *	\remarks			イベントループ内では、これを１個雑に読んで負の値が帰ったら切断ぐらいの扱い。
 *	\remarks			クライアント用では、while((ret = ...) == 0)としておくと「読めるまで待つ」を実行できる。
 */
int		nmjson_buffer_read(nmjson_buffer_t *self, nmjson_buffer_dispatch_cb dispatch, int fd, void *arg);

/**
 *	\brief	fdからJSONを読みこみ、解析した結果をコマンドエントリー形式で扱う。
 *	\arg	self		:nmjson_buffer_tのインスタンス
 *	\arg	cmds		:実行したいコマンドのエントリー配列。最後はname == NULLとしなければならない。
 *	\arg	cmd_name	:分岐の材料にしたいパラメータ名(string)。これを読むため、JSONはオブジェクトであるべき。
 *	\arg	fd			:読み込み元のfd。大抵はソケットやファイル。
 *	\arg	*arg		:コールバックに渡したい任意のデータ
 *	\return			:0→まだループを回す必要があると言われた（例えばワンショット用に読み込んでいる）
 *	\return			:1→もうこれ以上の読み込みは必要ないと言われた
 *	\return			-1→EOFを検出した（ファイルの終わりやソケットの切断）
 *	\return			-2→システムエラー。バッファは自動的にクリーンされます
 *	\return			-3→ライブラリ側のエラー。バッファは自動的にクリーンされます。
 */
int		nmjson_buffer_read_for_cmd(
	nmjson_buffer_t *self, 
	nmjson_cmd_elem_t cmds[], 
	const char *cmd_name, 
	int fd, void *arg
);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* !NMJSON_JSON_PARSER_H_ */
