#ifndef	NMJSON_JSON_CONST_H_
#define	NMJSON_JSON_CONST_H_

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
 *	\brief		受け入れるJSON拡張
 */
typedef enum nmjson_superset_e {
	
	/// \brief 拡張なし。プレーンのJSON。
	nmjson_superset_none,
	
	/// \brief JSONC: コメントあり。配列/オブジェクトの末尾カンマを許可
	nmjson_superset_jsonc,
	
	/// \brief JSON5: JSONCに加え、16進数の読み込みや小数の省略表記、\xHH, シングルクォート文字列等を追加。
	nmjson_superset_json5,
	
} nmjson_superset_t;

/// \brief		スーパーセット: コメントある？
#define nmjson_superset_has_comment(s) \
	(s == nmjson_superset_jsonc || s == nmjson_superset_json5)

/// \brief		スーパーセット: もうちょっと数値表現広がる？（hexaとかいろいろ）
#define nmjson_superset_has_extnum(s) \
	(s == nmjson_superset_json5)

/// \brief		スーパーセット: 省略された小数書ける？(.123とか0.とか)
#define nmjson_superset_has_abbrdecimal(s) \
	(s == nmjson_superset_json5)

/// \brief		スーパーセット: 末尾カンマOK？
#define nmjson_superset_has_tailcomma(s) \
	(s == nmjson_superset_jsonc || s == nmjson_superset_json5)
	
/// \brief		スーパーセット: エスケープシーケンスもっとある？(\vとか\0)
#define nmjson_superset_has_extescape(s) \
	(s == nmjson_superset_json5)
	
/// \brief		スーパーセット: \xXXできる？
#define nmjson_superset_has_hexaescape(s) \
	(s == nmjson_superset_json5)

/// \brief		スーパーセット: シングルクォートのリテラルOK？
#define nmjson_superset_has_literalsq(s) \
	(s == nmjson_superset_json5)

/// \brief		スーパーセット: リテラル中の改行OK？
#define nmjson_superset_has_linecontinuation(s) \
	(s == nmjson_superset_json5)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* !NMJSON_JSON_CONST_H_ */
