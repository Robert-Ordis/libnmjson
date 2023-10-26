/**
 *	\file		nmjson_buffer_.c
 *	\brief		fdからのJSON読み込みおよびハンドリングをサポートする
 *	\remarks	
 */

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "include/nmjson/nmjson_buffer.h"

static void buffer_flush_(nmjson_buffer_t *self){
	self->index = 0;
	self->cmdbuf[0] = '\0';
	nmjson_parser_reset_as_superset(&(self->parser), self->private.superset);
}

static void buffer_dequeue_(nmjson_buffer_t *self, size_t parsed_len){
	memmove(self->cmdbuf, &(self->cmdbuf[parsed_len]), (self->index + 1) - parsed_len);
	self->index -= parsed_len;
	nmjson_parser_reset_as_superset(&(self->parser), self->private.superset);
}

static inline int buffer_loadable_(nmjson_buffer_t *self){
	return self->index < (self->cmdbuf_len - 1);
}

static ssize_t buffer_load_(nmjson_buffer_t *self, int fd){
	ssize_t read_len;
	
	int err;
	
	if(!buffer_loadable_(self)){
		//バッファにため込んだ長さが想定を追い越した場合。
		buffer_flush_(self);
		return -3;
	}
	
	errno = 0;
	//読み込みバッファからはみ出ないように読む。
	//最後の1文字を必ず'\0'にするため、「バッファ長の-1の部分まで」を読むようにする。
	read_len = read(fd, &(self->cmdbuf[self->index]), (self->cmdbuf_len - 1) - self->index);
	err = errno;
	
	if(read_len > 0){
		//普通に読み込めた→末尾に終端を必ずつける。
		self->index += read_len;
		self->cmdbuf[self->index] = '\0';
		//printf("%s: read(%zd),%s\n", __func__, read_len, &self->cmdbuf[self->index - read_len]);
		//printf("%s: index is %zd\n", __func__, self->index);
	}
	else if(read_len < 0){
		if(0
			|| err == EPIPE
			|| err == ECONNRESET){
			read_len = 0;
		}
	}
	
	errno = err;
	return read_len;
}


static int nmjson_buffer_dispatch_(nmjson_buffer_t *self, nmjson_buffer_dispatch_cb dispatch, int fd, void *arg){
	
	int ret;
	size_t parsed_len = nmjson_parser_parse(&(self->parser), self->cmdbuf);
	self->private.last_result = self->parser.error.code;
	switch(self->private.last_result){
	case nmjson_error_incomplete:	//JSONの読み込み途中で終わった
		if(buffer_loadable_(self)){
			//まだ受信バッファが存在する→続けて読む必要があるので、イベント発火はしない
			return 0;
		}
		//受信バッファがもうない→//そのことを通知して強制終了
		self->private.last_result = nmjson_error_buffer;
	case nmjson_error_buffer:
	case nmjson_error_syntax:		//JSONの体をなしていない
	case nmjson_error_tokens:	//JSONトークンが足りない
		dispatch(self->parser.token.root, self->private.last_result, fd, arg);
		buffer_flush_(self);
		return 1;
	
	default:
		break;
	}
	
	//JSONとして成立→ディスパッチャを呼び出しし、本命処理を行ったらJSONに使った分をデキューする
	ret = dispatch(self->parser.token.root, self->private.last_result, fd, arg);
	
	buffer_dequeue_(self, parsed_len);
	
	return ret;
	
}

static int nmjson_buffer_proc_cmd_(nmjson_buffer_t *self, nmjson_cmd_elem_t cmds[], const char *cmd_name, int fd, void *arg){
	int ret = 1;
	size_t parsed_len = nmjson_parser_parse(&(self->parser), self->cmdbuf);
	self->private.last_result = self->parser.error.code;
	const char *name = NULL;
	nmjson_cmd_elem_t *pcmd;
	
	//printf("%s: parsed %zu, result is %d\n", __func__, parsed_len, self->private.last_result);
	
	switch(self->private.last_result){
	case nmjson_error_incomplete:	//JSONの読み込み途中で終わった
		if(buffer_loadable_(self)){
			//まだ受信バッファが存在する→続けて読む必要があるので、イベント発火はしない
			return 0;
		}
		//受信バッファがもうない→//そのことを通知して強制終了
		self->private.last_result = nmjson_error_buffer;
	case nmjson_error_buffer:
	case nmjson_error_syntax:		//JSONの体をなしていない
	case nmjson_error_tokens:	//JSONトークンが足りない
		break;
	case nmjson_error_complete:
		name = nmjson_object_get_string(self->parser.token.root, cmd_name, NULL);
		break;
	default:
		break;
	}
	
	if(name == NULL){
		for(pcmd = &cmds[0]; pcmd->name != NULL; pcmd ++){}
	}
	else{
		for(pcmd = &cmds[0]; pcmd->name != NULL; pcmd ++){
			if(strcmp(pcmd->name, name) == 0){
				break;
			}
		}
	}
	
	if(pcmd->func != NULL){
		ret = pcmd->func(self->parser.token.root, self->private.last_result, fd, arg);
	}
	
	if(self->private.last_result == nmjson_error_complete){
		//成功時。単純にデキューを試みる。
		buffer_dequeue_(self, parsed_len);
	}
	else{
		//ダメなときは戻り値を考慮しない。
		switch(self->private.last_result){
		case nmjson_error_syntax:
			buffer_dequeue_(self, parsed_len);
			break;
		case nmjson_error_buffer:
		case nmjson_error_tokens:
			buffer_flush_(self);
			break;
		default:
			break;
		}
		ret = 1;
	}
	
	return ret;
}

int		nmjson_buffer_init(
	nmjson_buffer_t *self, 
	nmjson_token_t tokens[], 
	size_t tokens_num, 
	char cmdbuf[], 
	size_t cmdbuf_len
){
	return nmjson_buffer_init_superset(self, tokens, tokens_num, cmdbuf, cmdbuf_len, nmjson_superset_none);
}

int		nmjson_buffer_init_superset(
	nmjson_buffer_t *self, 
	nmjson_token_t tokens[], 
	size_t tokens_num, 
	char cmdbuf[], 
	size_t cmdbuf_len,
	nmjson_superset_t superset
){
	memset(self, 0, sizeof(nmjson_buffer_t));
	
	memset(tokens, 0, sizeof(nmjson_token_t) * tokens_num);
	self->tokens = tokens;
	self->tokens_num = tokens_num;
	
	nmjson_parser_init(&(self->parser), tokens, tokens_num);
	
	memset(cmdbuf, 0, cmdbuf_len);
	self->cmdbuf = cmdbuf;
	self->cmdbuf_len = cmdbuf_len;
	self->private.superset = superset;
	
	buffer_flush_(self);
	return 0;
}

int	nmjson_buffer_read(nmjson_buffer_t *self, nmjson_buffer_dispatch_cb dispatch, int fd, void *arg){
	
	int err = 0;
	ssize_t read_len;
	int prev_index = self->index;
	
	read_len = buffer_load_(self, fd);
	
	if(read_len > 0){
		
		//readが終わった分だけパースと実行を行う。今できる分を全部行う。
		int tmp, ret = 0;
		for(;;){
			//実行の結果。「続行不能」なら-3、コールバックのエラーなら-4以降を返す。
			//「まだ読んでほしい」のなら0を、「上位ループはもう離脱してもいい」のなら1以上を返す。
			tmp = nmjson_buffer_dispatch_(self, dispatch, fd, arg);
			if(!nmjson_error_is_continuable(self->private.last_result)){
				ret = -3;
			}
			else if(tmp < 0){
				ret = -3 + tmp;
			}
			if(ret >= 0){
				//エラーが確定しているわけじゃないので、コールバックの結果を重ねる。
				ret |= tmp;
			}
			
			if(prev_index == self->index){
				//パースしてもデキューされなかった→もうこれ以上読んでも何もない。
				break;
			}
			prev_index = self->index;
		}
		return ret;
	}
	else if(read_len == 0){
		//切断、ファイル終端相当の出来事
		errno = 0;
		return -1;
	}
	else{
		err = errno;
		if(0
			|| err == EWOULDBLOCK
			|| err == EAGAIN
			|| err == EINTR
		){
			//また次読んでほしいということなので0を返す。
			return 0;
		}
		errno = err;
		return -2;
	}
}

int	nmjson_buffer_read_for_cmd(nmjson_buffer_t *self, nmjson_cmd_elem_t cmds[], const char *cmd_name, int fd, void *arg){
	int err = 0;
	ssize_t read_len;
	int prev_index = self->index;
	
	read_len = buffer_load_(self, fd);
	
	if(read_len > 0){
		//readが終わった分だけパースと実行を行う。今できる分を全部行う。
		int tmp, ret = 0;
		for(;;){
			//実行の結果。「続行不能」なら-3、コールバックのエラーなら-4以降を返す。
			//「まだ読んでほしい」のなら0を、「上位ループはもう離脱してもいい」のなら1以上を返す。
			//printf("%s: prev index is %d\n", __func__, prev_index);
			tmp = nmjson_buffer_proc_cmd_(self, cmds, cmd_name, fd, arg);
			if(!nmjson_error_is_continuable(self->private.last_result)){
				ret = -3;
			}
			else if(tmp < 0){
				ret = -3 + tmp;
			}
			if(ret >= 0){
				//エラーが確定しているわけじゃないので、コールバックの結果を重ねる。
				ret |= tmp;
			}
			
			if(prev_index == self->index){
				//パースしてもデキューされなかった→もうこれ以上読んでも何もない。
				break;
			}
			prev_index = self->index;
		}
		return ret;
	}
	else if(read_len == 0){
		//切断、ファイル終端相当の出来事
		errno = 0;
		return -1;
	}
	else{
		err = errno;
		if(0
			|| err == EWOULDBLOCK
			|| err == EAGAIN
			|| err == EINTR
		){
			//また次読んでほしいということなので0を返す。
			return 0;
		}
		errno = err;
		return -2;
	}
}
