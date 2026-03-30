#ifndef	LOCAL_SWRITER_H_
#define	LOCAL_SWRITER_H_

#include <errno.h>
#include <stdarg.h>

typedef struct swriter_s {
	FILE *fp;
	size_t	written;
	int err;
} swriter_t;

static inline void swriter_init_(swriter_t *self, FILE *fp) {
	self->fp = fp;
	self->written = 0;
	self->err = 0;
}

static inline ssize_t swriter_write_(swriter_t *self, const void *ptr, size_t each_size, size_t block_num){
	errno = 0;
	size_t ret = fwrite(ptr, each_size, block_num, self->fp);
	self->written += (ret * each_size);
	if(ret == block_num){
		return ret;
	}
	self->err = errno;
	return -1;
}

static inline ssize_t swriter_putc_(swriter_t *self, int c) {
	errno = 0;
	int c_out = fputc(c, self->fp);
	if(c_out != EOF){
		self->written ++;
		return 1;
	}
	self->err = errno;
	return -1;
}

static ssize_t swriter_printf_(swriter_t *self, const char *fmt, ...){
	va_list argp;
	int ret = 0;
	errno = 0;
	do{
		va_start(argp, fmt);
		ret = vfprintf(self->fp, fmt, argp);
	}while(0);
	va_end(argp);
	if(ret >= 0){
		self->written += ret;
	}
	else{
		self->err = errno;
	}
	return ret;
}

#endif	/* !LOCAL_SWRITER_H_ */
