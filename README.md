# libnmjson

## No-Malloc-JSON

### mallocなしでJSONの解析や組み立てを行うメモリにやさしいかもしれないライブラリ

* 通常のローカル変数の宣言のみでの運用を視野に入れたライブラリです  
→一応mallocありもサポートはする予定です

* JSONストリーム `{"test":123}{"test":456}`の連続読み込み対応

* 再帰処理を使用しない処理を心がけています。スタックフレームは大事です。

* 設定次第で、json-cやjson5（不完全）もいけます。  
→jsonファイルにコメント書きたいとか、末尾カンマを許したいとかの需要メインです

* json5未対応点  
→LineSeparator: `\n(<LF>)`とか`\r(<CR>)`、`\r\n(<CR><LF>)`しか想定していない  
→IdentifierName: 厳しくチェックする場合、アルファベット、ギリシャ、キリルのみ対応。これを定義したのは誰だぁっ！！  
→IdentifierName: ゆるくチェックする場合は最低限isspaceか:で終了する仕掛け。
→ほかにダメな点あったらissue送ってねマジで。  

### 移植性

* 下記の環境でのビルドを確認
    * Cygwin
    * 通常のLinux (Intel CPU)
    * bitbake
    
### 使い方
* 使うまで: 静的ライブラリなのでできれば`prefix`を付けることを推奨しますです。  
`./configure --prefix=/path/to/your/project && make && make install`
- 適当なパース
```
#include <inttypes.h>
#include <stdio.h>

#include <nmjson/nmjson.h>
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
```
- ストリームのパース→`test`ディレクトリを見てください。fdから１文字１文字読み取り、パースに成功したら扱う、という所作が書かれています。

### TODO
* nmjson_buffer_tについて、ヒープメモリ運用を可能にする。  
`nmjson_buffer_init(&jbuffer, NULL, 0, NULL, 0);//とすると完全ヒープメモリで行う、というイメージです`
* JSONPathの記法でわかりやすくJSONを組み立てられるようにする  
`nmjson_builder_set_int(&maker, "$.obj1.array[-1]", 334);	//といった具合のものです`

### あんちょこ
```$ autoreconf -vfi```
