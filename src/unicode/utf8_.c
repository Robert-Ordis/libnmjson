#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>

#include "include/nmjson/utf8.h"


ssize_t	utf8_copy_decode(char *dst, const char *src, size_t src_len, uint32_t *p_hi_code){
	ssize_t ret;
	char codestr [17];
	
	uint32_t unicode;
	
	memcpy(codestr, src, src_len);
	codestr[src_len] = '\0';
	
	unicode = strtoul(codestr, NULL, 16);
	
	if(unicode < 0x80){
		//ASCII
		*dst = (char)unicode;
		ret = 1;
	}
	else if(unicode < 0x800){
		//2バイト文字
		dst[0] = (char)(0xC0 + unicode / 0x00000040		);
		dst[1] = (char)(0x80 + unicode					% 64);
		ret = 2;
	}
	else if(unicode < 0x0000FFFF){
		if(0xD800 <= unicode && unicode < 0xDC00){
			//サロゲートペア
			if(p_hi_code != NULL){
				*p_hi_code = unicode;
				ret = 0;
			}
			else{
				ret = -1;
			}
		}
		else{
			//３バイト文字
			dst[0] = (char)(0xE0 + unicode / 0x00001000		);
			dst[1] = (char)(0x80 + unicode / 0x00000040	% 64);
			dst[2] = (char)(0x80 + unicode					% 64);
			ret = 3;
		}
	}
	else{
		//４バイト文字を直接指定された場合。
		dst[0] = (char)(0xF0 + unicode / 0x00040000		);
		dst[1] = (char)(0x80 + unicode / 0x00001000	% 64);
		dst[2] = (char)(0x80 + unicode / 0x00000040	% 64);
		dst[3] = (char)(0x80 + unicode					% 64);
		ret = 4;
	}
	return ret;
}

ssize_t	utf8_copy_decode_surrogate(char *dst, const char *src, uint32_t hi_code){
	//ssize_t ret = 0;
	uint32_t lo_code;
	uint32_t unicode;
	char codestr[5];
	memcpy(codestr, src, 4);
	codestr[4] = '\0';
	
	lo_code = strtoul(codestr, NULL, 16);
	
	if(lo_code < 0xDC00 || 0xDFFF < lo_code){
		//下位サロゲートの範囲外
		return -1;
	}
	
	unicode = 0x10000 + (hi_code - 0xD800) * 0x400 + (lo_code - 0xDC00);
	
	//printf("%s: 0x%04x & 0x%04x -> 0x%06x\n", __func__, hi_code, lo_code, unicode);
	
	dst[0] = (char)(0xF0 + unicode / 0x00040000		);
	dst[1] = (char)(0x80 + unicode / 0x00001000	% 64);
	dst[2] = (char)(0x80 + unicode / 0x00000040	% 64);
	dst[3] = (char)(0x80 + unicode					% 64);
	
	//printf("%s: 0x%02x.0x%02x.0x%02x.0x%02x\n", __func__, (uint8_t)dst[0], (uint8_t)dst[1], (uint8_t)dst[2], (uint8_t)dst[3]);
	
	return 4;
}

ssize_t	utf8_str_to_unicode(const char *src, uint32_t *p_unicode){
	uint8_t head = (uint8_t)src[0];
	int i, len;
	
	static uint8_t head_masks[8] = {
		0x7F,
		0x3F,
		0x1F,
		0x0F,
		0x07,
		0x03,
		0x01,
		0x00
	};
	
	if(head < 0x80){
		*p_unicode = (uint32_t) head;
		return 1;
	}
	
	if((head & 0xE0) == 0xC0){
		len = 2;
	}
	else if((head & 0xF0) == 0xE0){
		len = 3;
	}
	else if((head & 0xF8) == 0xF0){
		len = 4;
	}
	else if((head & 0xFC) == 0xF8){
		len = 5;
	}
	else if((head & 0xFE) == 0xFC){
		len = 6;
	}
	else{
		return -1;
	}
	
	*p_unicode = head & head_masks[len];
	for(i = 1; i < len; i++){
		if(src[i] == '\0'){
			return (ssize_t) i;
		}
		*p_unicode = (*p_unicode << 6) + (src[i] & head_masks[1]);
	}
	
	return (ssize_t) len;
}
