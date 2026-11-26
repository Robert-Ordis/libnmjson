#ifndef	NMJSON_JSON_CONST_H_
#define	NMJSON_JSON_CONST_H_

#include "./polyfill.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 *	\brief		JSONトークンの型を定義する
 */
typedef enum {
	nmjson_type_null	= 0x00,
	nmjson_type_string	= 0x01,
	nmjson_type_integer	= 0x02,
	nmjson_type_float	= 0x04,
	nmjson_type_object	= 0x08,
	nmjson_type_array	= 0x10,
	nmjson_type_bool	= 0x20,
	
	//特殊処理を行うタイプ
	//「数」
	nmjson_type_number_	= (0x02 | 0x04),
	//「子を持つもの」かどうか。
	nmjson_type_parent_	= (0x08 | 0x10),
	//未使用
	nmjson_type_unused_ = -1,
	//「identifier」
	nmjson_type_rawstr_ = -2,
} nmjson_type_t;

/**
 *	\brief		JSONパーサのエラーの判別
 */
typedef enum {
	/// \brief 無事にパース成功
	nmjson_error_complete,
	
	/// \brief 不完全なJSON。バッファに文字を追加して継続できます
	nmjson_error_incomplete,
	
	//ここから「継続不可能なエラー」
	/// \brief 構文エラー
	nmjson_error_syntax,
	
	/// \brief トークン不足
	nmjson_error_tokens,
	
	/// \brief バッファメモリ枯渇(nmjson_buffer_tが通知)
	nmjson_error_buffer,
	
} nmjson_error_t;

/// \brief 継続可能なエラーかどうか
#define	nmjson_error_is_continuable(e) \
	(e == nmjson_error_complete || e == nmjson_error_incomplete)

/**
 *	\brief		受け入れるJSON拡張（機能ごとのビットフィールド）
 */
typedef struct nmjson_superset_s {
	/// \brief	コメント: // と /* */
	unsigned int	comment				: 1;
	/// \brief	配列/オブジェクトの末尾カンマ
	unsigned int	tailcomma			: 1;
	/// \brief	数値表現の拡張: 16進数(0x), 明示的な正符号(+), Infinity, NaN
	unsigned int	extnum				: 1;
	/// \brief	省略された小数: .123 / 123.
	unsigned int	abbrdecimal			: 1;
	/// \brief	エスケープシーケンスの拡張: \v, \0 など
	unsigned int	extescape			: 1;
	/// \brief	16進エスケープ: \xHH
	unsigned int	hexaescape			: 1;
	/// \brief	シングルクォートの文字列: 'string'
	unsigned int	literalsq			: 1;
	/// \brief	文字列中の行継続（改行の直前にエスケープが必要）
	unsigned int	linecontinuation	: 1;
	/// \brief	文字列中でエスケープなしの制御文字(改行など)を許可（独自拡張）
	unsigned int	literalcntl			: 1;
	/// \brief	オブジェクトのキーにクォートなしの識別子(ECMAScript 5.1 IdentifierName)
	unsigned int	identkey			: 1;
} nmjson_superset_t;

/*
 *	プリセット。C99の複合リテラルとして展開される。
 *	機能を足し引きしたい場合はコピーしてからフィールドを書き換える:
 *		nmjson_superset_t s = nmjson_superset_jsonc;
 *		s.literalsq = 1;
 *	パーサーでは「受け入れる」、writerでは「出力してよい」を意味する。
 *	writerにとって意味のない機能(tailcomma等)は無視される。
 */

/// \brief 拡張なし。プレーンのJSON。
#define	nmjson_superset_none \
	((nmjson_superset_t){ 0 })

/// \brief JSONC: コメントあり。配列/オブジェクトの末尾カンマを許可
#define	nmjson_superset_jsonc \
	((nmjson_superset_t){ \
		.comment = 1, .tailcomma = 1, \
	})

/// \brief JSON5: JSONCに加え、16進数の読み込みや小数の省略表記、\xHH, シングルクォート文字列等を追加。
#define	nmjson_superset_json5 \
	((nmjson_superset_t){ \
		.comment = 1, .tailcomma = 1, \
		.extnum = 1, .abbrdecimal = 1, .extescape = 1, .hexaescape = 1, \
		.literalsq = 1, .linecontinuation = 1, .identkey = 1, \
	})

/// \brief 雑な文字列でも受け入れるための独自拡張
#define	nmjson_superset_rough \
	((nmjson_superset_t){ \
		.extescape = 1, .literalcntl = 1, \
	})

/*
 *	以下、機能の判定マクロ。旧API互換のために残している。
 *	新しく書くコードではフィールドを直接参照してよい（例: superset.comment）。
 */

/// \brief		スーパーセット: コメントある？
#define nmjson_superset_has_comment(s)			((s).comment)

/// \brief		スーパーセット: もうちょっと数値表現広がる？（hexaとかいろいろ）
#define nmjson_superset_has_extnum(s)			((s).extnum)

/// \brief		スーパーセット: 省略された小数書ける？(.123とか0.とか)
#define nmjson_superset_has_abbrdecimal(s)		((s).abbrdecimal)

/// \brief		スーパーセット: 末尾カンマOK？
#define nmjson_superset_has_tailcomma(s)		((s).tailcomma)

/// \brief		スーパーセット: エスケープシーケンスもっとある？(\vとか\0)
#define nmjson_superset_has_extescape(s)		((s).extescape)

/// \brief		スーパーセット: \xXXできる？
#define nmjson_superset_has_hexaescape(s)		((s).hexaescape)

/// \brief		スーパーセット: シングルクォートのリテラルOK？
#define nmjson_superset_has_literalsq(s)		((s).literalsq)

/// \brief		スーパーセット: リテラル中の改行OK？（ただし、直前のエスケープ必要）
#define nmjson_superset_has_linecontinuation(s)	((s).linecontinuation)

/// \brief		リテラル中、エスケープなしでリテラル中の改行などを書いてもいいかどうか
#define nmjson_superset_allows_literalcntl(s)	((s).literalcntl)

/// \brief		スーパーセット: オブジェクトのキーにクォートなしの識別子OK？
#define nmjson_superset_has_identkey(s)			((s).identkey)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* !NMJSON_JSON_CONST_H_ */
