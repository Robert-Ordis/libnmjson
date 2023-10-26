/**
 *	\file	json_token.h
 *	\brief	JSONの値を示すトークンの構造体。
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
