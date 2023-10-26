#include <fcntl.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "include/nmjson/nmjson.h"

#define TOKENS_NUM 200
#define CBUF_NUM 2048

void test03(){
	nmjson_parser_t parser;
	nmjson_token_t tokens[TOKENS_NUM];
	char cbuf[CBUF_NUM];
	int fd = -1;
	
	nmjson_parser_init(&parser, tokens, TOKENS_NUM);
	nmjson_parser_reset_as_superset(&parser, nmjson_superset_json5);
	
	do{
		int i = 0;
		if((fd = open("./example/03-kitchen-sink5.json5", O_RDONLY)) < 0){
			perror("test03");
			break;
		}
		
		while(parser.error.code == nmjson_error_incomplete && i < CBUF_NUM - 1){
			
			if(read(fd, &cbuf[i], 1) <= 0){
				perror("test01");
				break;
			}
			i++;
			cbuf[i] = '\0';
			
			nmjson_parser_parse(&parser, cbuf);
		}
		
	}while(0);
	
	if(fd >= 0){
		close(fd);
	}
	
	nmjson_parser_dbg_print(&parser, printf);
	nmjson_token_fout3(parser.token.root, stdout, 1, 1, nmjson_superset_json5);
	/*
	if(parser.error.code == nmjson_error_complete){
		
		const nmjson_token_t *ptoken = nmjson_parser_get_root(&parser);
		const nmjson_token_t *punicode = nmjson_object_token(ptoken, "unicode");
		const nmjson_token_t *pobj = nmjson_object_get_object(ptoken, "obj");
		const nmjson_token_t *pstrcast = nmjson_object_get_object(ptoken, "strcast");
		
		printf("root[%p], unicode[%p], obj[%p], strcast[%p]\n", ptoken, punicode, pobj, pstrcast);
		printf("unicode.ひらがな = %s\n", nmjson_object_get_string(punicode, "ひらがな", NULL));
		printf("unicode.escape = %s\n", nmjson_object_get_string(punicode, "escape", NULL));
		printf("unicode.uxxx = %s\n", nmjson_object_get_string(punicode, "uxxx", NULL));
		printf("obj.integer = %"PRId64"\n", nmjson_object_get_int(pobj, "integer", -1));
		printf("obj.float = %f\n", nmjson_object_get_float(pobj, "float", NAN));
		printf("obj.string = %s\n", nmjson_object_get_string(pobj, "string", NULL));
		printf("obj.unicode_str = %s\n", nmjson_object_get_string(pobj, "unicode_str", NULL));
		
		printf("strcast.hex_normal = 0x%016lX\n", nmjson_object_get_int(pstrcast, "hex_normal", -1));
		printf("strcast.ull        = 0x%016lX\n", nmjson_object_get_int(pstrcast, "ull", -1));
		printf("strcast.sll        = 0x%016lX\n", nmjson_object_get_int(pstrcast, "sll", -1));
		
		printf("strcast.bt         = %d\n", nmjson_object_get_bool(pstrcast, "bt"));
		printf("strcast.bf         = %d\n", nmjson_object_get_bool(pstrcast, "bf"));
		
	}
	*/
	
}
