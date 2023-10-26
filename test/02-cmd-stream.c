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

#include "include/nmjson/nmjson_buffer.h"

#define TOKENS_NUM 200
#define CBUF_NUM 1024

#define	JSON_FILE "./example/02-cmd-stream.json"



static int dispatcher_(const nmjson_token_t *token_root, nmjson_error_t error, int fd, void *arg){
	printf("%s[%p]: %d\n", __func__, token_root, error);
	nmjson_token_fout2(token_root, stdout, 1, 1);
	if(error != nmjson_error_complete){
		exit(1);
	}
	return 0;
}

void test02_1(){
	printf("%s\n", __func__);
	nmjson_buffer_t jbuffer;
	nmjson_token_t tokens[TOKENS_NUM];
	char cbuf[CBUF_NUM];
	int fd = -1;
	
	nmjson_buffer_init(&jbuffer, tokens, TOKENS_NUM, cbuf, CBUF_NUM);
	
	do{
		int exret = 0;
		if((fd = open(JSON_FILE, O_RDONLY)) < 0){
			perror(__func__);
			break;
		}
		
		while((exret = nmjson_buffer_read(&jbuffer, dispatcher_, fd, NULL)) == 0){}
		
	}while(0);
	
	if(fd >= 0){
		close(fd);
	}
	
}

static int on_order1(const nmjson_token_t *token_root, nmjson_error_t error, int fd, void *arg){
	printf("%s[%p]: %d\n", __func__, token_root, error);
	nmjson_token_fout2(token_root, stdout, 1, 1);
	return 0;
}

static int on_search(const nmjson_token_t *token_root, nmjson_error_t error, int fd, void *arg){
	printf("%s[%p]: %d\n", __func__, token_root, error);
	nmjson_token_fout2(token_root, stdout, 1, 1);
	return 0;
}

static int on_destroy(const nmjson_token_t *token_root, nmjson_error_t error, int fd, void *arg){
	printf("%s[%p]: %d\n", __func__, token_root, error);
	nmjson_token_fout2(token_root, stdout, 1, 1);
	return 0;
}

static int on_STRAY(const nmjson_token_t *token_root, nmjson_error_t error, int fd, void *arg){
	printf("%s[%p]: %d\n", __func__, token_root, error);
	nmjson_token_fout2(token_root, stdout, 1, 1);
	if(error != nmjson_error_complete){
		exit(1);
	}
	return 0;
}

void test02_2(){
	nmjson_buffer_t jbuffer;
	nmjson_token_t tokens[TOKENS_NUM];
	char cbuf[CBUF_NUM];
	int fd = -1;
	
	static nmjson_cmd_elem_t elems[] = {
		{"order1"	, on_order1},
		{"search"	, on_search},
		{"destroy"	, on_destroy},
		{NULL			, on_STRAY}
	};
	printf("%s\n", __func__);
	nmjson_buffer_init(&jbuffer, tokens, TOKENS_NUM, cbuf, CBUF_NUM);
	
	
	
	do{
		int exret = 0;
		if((fd = open(JSON_FILE, O_RDONLY)) < 0){
			perror(__func__);
			break;
		}
		
		while((exret = nmjson_buffer_read_for_cmd(&jbuffer, elems, "request", fd, NULL)) == 0){}
		
	}while(0);
	
	if(fd >= 0){
		close(fd);
	}
	
}
