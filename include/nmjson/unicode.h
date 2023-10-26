#ifndef	NMJSON_UNICODE_H_
#define	NMJSON_UNICODE_H_

#include <inttypes.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define	IN_RANGE_(x, min, max) ((min <= x) && (x <= max))

typedef enum {
	unicode_category_UNTREATED,
	unicode_category_ll,		//Lowercase letter
	unicode_category_lu,		//Uppercase letter
	unicode_category_lt,		//Titlecase letter
	unicode_category_lm,		//Modifier letter
	unicode_category_lo,		//Other letter
	unicode_category_mc,		//Non-spacing mark
	unicode_category_mn,		//Combining spacing mark
	unicode_category_nd,		//Decimal number
	unicode_category_nl,		//Number letter
	unicode_category_pc,		//Connector punctuation
} unicode_category_t;


static unicode_category_t unicode_to_category(uint32_t unicode){
	unicode_category_t ret;
	do{
		ret = unicode_category_ll;	//小文字カテゴリ
		if(IN_RANGE_(unicode, 0x0061, 0x007A)){break;}	//普通の小文字ラテン
		//if(IN_RANGE_(unicode, 0x00B5, 0x00B5)){break;}	//micro-sign
		//if(IN_RANGE_(unicode, 0x00DF, 0x00FF)){break;}	//飾り付きラテン1その他２文字
		//if(IN_RANGE_(unicode, 0x0101, 0x0137) && (unicode & 0x01)){break;}	//飾り付きラテン2
		//if(IN_RANGE_(unicode, 0x0138, 0x0148) && !(unicode & 0x01)){break;}	//飾り付きラテン3
		//if(IN_RANGE_(unicode, 0x0149, 0x0177) && (unicode & 0x01)){break;}	//飾り付きラテン4
		//if(IN_RANGE_(unicode, 0x017A, 0x017E) && (unicode & 0x01)){break;}	//飾り付きラテン5
		//if(IN_RANGE_(unicode, 0x017F, 0x0180)){break;}	//Long s to [b] stroke
		//
		//if(IN_RANGE_(unicode, 0x0250, 0x02AF)){break;}
		if(IN_RANGE_(unicode, 0x03B1, 0x03CE)){break;}	//多分現代ギリシャ小文字（一部）
		if(IN_RANGE_(unicode, 0x0430, 0x045F)){break;}	//キリル小文字1
		//if(IN_RANGE_(unicode, 0x0461, 0x0481) && (unicode & 0x01)){break;}	//キリル小文字2
		//if(IN_RANGE_(unicode, 0x048B, 0x04BF) && (unicode & 0x01)){break;}	//キリル小文字3
		//if(IN_RANGE_(unicode, 0x04C2, 0x04CE) && !(unicode & 0x01)){break;}	//キリル小文字4
		//if(IN_RANGE_(unicode, 0x04CF, 0x052F) && (unicode & 0x01)){break;}	//キリル小文字5
		//以降もう嫌です
		
		ret = unicode_category_lu;	//大文字カテゴリ
		if(IN_RANGE_(unicode, 0x0041, 0x005A)){break;}	//普通の大文字ラテン
		//if(IN_RANGE_(unicode, 0x00C0, 0x00DE)){break;}	//ラテン2
		//if(IN_RANGE_(unicode, 0x0100, 0x0136) && !(unicode & 0x01)){break;}	//大文字ラテン3
		//if(IN_RANGE_(unicode, 0x0139, 0x0147) && (unicode & 0x01)){break;}	//大文字ラテン4
		//if(IN_RANGE_(unicode, 0x014A, 0x0178) && !(unicode & 0x01)){break;}	//大文字ラテン5
		//if(IN_RANGE_(unicode, 0x0179, 0x017D) && (unicode & 0x01)){break;}	//大文字ラテン(- to Z with caron)
		//めっちゃ飛ばす。
		if(IN_RANGE_(unicode, 0x0391, 0x03A1)){break;}	//ギリシャ大文字(α-ρまで)
		if(IN_RANGE_(unicode, 0x03A3, 0x03A9)){break;}	//ギリシャ大文字(ωまで)
		//ギリシャはバラバラすぎて対応しきれない
		if(IN_RANGE_(unicode, 0x0400, 0x042F)){break;}	//キリル大文字
		
		
		//タイトルケースは手を出しません。
		
		//modifier letters
		ret = unicode_category_lm;
		if(IN_RANGE_(unicode, 0x02B0, 0x02FF)){break;}
		
		//Other letterは手を出しません。無理です
		
		//spacing markも手を出しません。
		
		//数字は普通のやつだけです。
		if(IN_RANGE_(unicode, 0x0030, 0x0039)){break;}
		
		//nlはローマ数字
		if(IN_RANGE_(unicode, 0x2160, 0x217F)){break;}
		
		//pcは珍しくカバーできそうです。
		ret = unicode_category_pc;
		if(0
			|| unicode == 0x005F
			|| unicode == 0x203F || unicode == 0x2040 || unicode == 0x2054
			|| unicode == 0xFE33 || unicode == 0xFE34
			|| unicode == 0xFE4D || unicode == 0xFE4E || unicode == 0xFE4F || unicode == 0xFF3F
		){
			break;
		}
		
		ret = unicode_category_UNTREATED;
	}while(0);
	return ret;
}

static inline int unicode_is_letter(uint32_t unicode){
	unicode_category_t category = unicode_to_category(unicode);
	switch(category){
	case unicode_category_ll:
	case unicode_category_lu:
	case unicode_category_lt:
	case unicode_category_lm:
	case unicode_category_lo:
	case unicode_category_nl:
		return 1;
	default:
		break;
	}
	return 0;
}


#undef IN_RANGE_

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* !NMJSON_UNICODE_H_ */
