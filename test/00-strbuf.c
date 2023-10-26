#include <inttypes.h>
#include <stdio.h>

#include "include/nmjson/nmjson.h"
//HTTPレスポンスの読み込みがこういう形になるんじゃないかしら

#define TOKENS_NUM 64
#define BUF_LEN 1024
void test00(){
	//全部スタックフレーム上の自動変数で運用
	char buf[BUF_LEN];
	nmjson_parser_t parser;
	nmjson_token_t tokens[TOKENS_NUM];
	nmjson_parser_init(&parser, tokens, TOKENS_NUM);
	nmjson_parser_reset(&parser);
	
	printf("%s: \n", __func__);
	
	//JSONを文字列バッファに書きます
	sprintf(buf, 
		"{"\
			"\"args\": \"%s\","\
			"\"argi\": %d,"\
			"\"argf\": %f,"\
			"\"argo\": {"\
				"\"beast\": \"0x114514\""\
			"}"\
		"}",
		"TestString",
		114514,
		0.114514
	);
	printf("%s: parse %s\n", __func__, buf);
	//パースします。今回はJSONを書ききっているので問題ありません。
	//文字列リテラルを直接パースしようとしたらセグフォります。
	nmjson_parser_parse(&parser, buf);
	if(parser.error.code != nmjson_error_complete){
		printf("%s: failure to parse[%d]\n", __func__, parser.error.code);
		exit(1);
	}
	else{
		const nmjson_token_t *token_r = nmjson_parser_get_root(&parser);
		const nmjson_token_t *token_c = nmjson_object_get_object(token_r, "argo");
		printf("%s: root[%p], child[%p]\n", __func__, token_r, token_c);
		printf("%s: args[%s], argi[%"PRId64"], argf[%f]\n", __func__,
			nmjson_object_get_string(token_r, "args", "##NOT FOUND##"),
			nmjson_object_get_int(token_r, "argi", -334),
			nmjson_object_get_float(token_r, "argf", -3.34)
		);
		if(token_c != NULL){
			printf("%s: argo.beast[0x%"PRIx64"]\n", __func__,
				nmjson_object_get_int(token_c, "beast", -1919)
			);
		}
	}
}