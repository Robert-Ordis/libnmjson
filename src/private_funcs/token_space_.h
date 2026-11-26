/**
 *	\file		token_space_.h
 *	\brief		トークン間の空白の判定（isspaceの代わり）
 *	\remarks	isspaceはJSONには広すぎ(VT/FFを含む)、JSON5には狭すぎ(Unicodeの空白を含まない)、
 *				かつロケール依存(0x80以上のバイトを空白とみなすロケールがあり得る)なため使わない。
 *				- extspace == 0 : RFC 8259 の4文字(SP, TAB, LF, CR)
 *				- extspace == 1 : JSON5(ECMAScript 5.1)の WhiteSpace と LineTerminator
 *				                  上記 + VT, FF, U+00A0, U+FEFF, Zs(U+1680, U+2000-200A, U+202F, U+205F, U+3000),
 *				                  U+2028, U+2029
 */
#ifndef	NMJSON_PRIVATE_TOKEN_SPACE_H_
#define	NMJSON_PRIVATE_TOKEN_SPACE_H_

#include <inttypes.h>
#include <sys/types.h>

/// \brief	JSON5の空白・行終端に当たるコードポイントか
static inline int json5_is_space_cp_(uint32_t u){
	switch(u){
	case 0x0009: case 0x000A: case 0x000B: case 0x000C: case 0x000D: case 0x0020:
	case 0x00A0: case 0xFEFF: case 0x1680: case 0x202F: case 0x205F: case 0x3000:
	case 0x2028: case 0x2029:
		return 1;
	default:
		break;
	}
	return (u >= 0x2000 && u <= 0x200A);
}

/// \brief	JSON5の行終端(LineTerminator)に当たるコードポイントか(1行コメントの終わり)
static inline int json5_is_line_terminator_cp_(uint32_t u){
	return u == 0x000A || u == 0x000D || u == 0x2028 || u == 0x2029;
}

/**
 *	\brief		p の位置にあるのが空白なら、そのバイト長を返す
 *	\return		 > 0	: 空白のバイト長
 *	\return		== 0	: 空白ではない
 *	\return		 < 0	: 空白になり得るマルチバイト文字の途中で入力が途切れている(incomplete扱いにすること)
 */
static inline ssize_t json_space_len_(const char *p, nmjson_superset_t superset){
	const uint8_t *u = (const uint8_t *)p;
	size_t need;
	size_t i;
	uint32_t cp;

	switch(u[0]){
	case ' ': case '\t': case '\n': case '\r':
		return 1;
	case 0x0B: case 0x0C:
		return superset.extspace ? 1 : 0;
	default:
		break;
	}
	if(!superset.extspace){
		return 0;
	}
	// マルチバイトの空白の先頭バイトは C2(U+00A0), E1(U+1680), E2(U+2000台), E3(U+3000), EF(U+FEFF) のみ
	switch(u[0]){
	case 0xC2:
		need = 2; cp = u[0] & 0x1F;
		break;
	case 0xE1: case 0xE2: case 0xE3: case 0xEF:
		need = 3; cp = u[0] & 0x0F;
		break;
	default:
		return 0;
	}
	for(i = 1; i < need; i++){
		if(u[i] == '\0'){
			return -1;	//途中で途切れている
		}
		if((u[i] & 0xC0) != 0x80){
			return 0;	//不正なUTF-8: 空白ではない(後段で構文エラーになる)
		}
		cp = (cp << 6) | (u[i] & 0x3F);
	}
	return json5_is_space_cp_(cp) ? (ssize_t)need : 0;
}

#endif	/* !NMJSON_PRIVATE_TOKEN_SPACE_H_ */
