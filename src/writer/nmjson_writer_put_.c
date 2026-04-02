#include <ctype.h>
#include <errno.h>
#include <string.h>

#include "include/nmjson/nmjson_str.h"
#include "include/nmjson/nmjson_writer.h"
#include "include/nmjson/utf8.h"

static inline void put_err_(nmjson_writer_t *self, int err, const char *err_at){
	self->err.num = err;
	if(err_at){
		self->err.at = err_at;
	}
}


static inline ssize_t write_raw_(nmjson_writer_t *self, ssize_t *acc_ret, const char *s, size_t l){
	ssize_t one_ret;
	if((one_ret = self->ctx.write_(&(self->ctx.inst_), s, l)) >= 0){
		*acc_ret += one_ret;
	}
	return one_ret;
}

/// \brief バイナリ列を「16進文字列」に書き換える。sprintfだとちょっと手間がかかりすぎるので。
static void inline hexstr_from_bin_(char *out, uint8_t in){
	const static char tbl[] = "0123456789ABCDEF";
	uint8_t half;
	
	half = (in >> 4) & 0x0F;
	out[0] = tbl[half];
	
	half = (in >> 0) & 0x0F;
	out[1] = tbl[half];
	
}

static inline size_t encode_u_(char *out, uint32_t unicode){
	int idx = 0;
	if(unicode < 0x010000){
		uint8_t hi = (unicode >> 8) & 0x00FF;
		uint8_t lo = (unicode     ) & 0x00FF;
		out[0] = '\\';
		out[1] = 'u';
		hexstr_from_bin_(&out[2], hi);
		hexstr_from_bin_(&out[4], lo);
		return 6;
	}
	else{
		uint32_t hi, lo;
		unicode -= 0x010000;
		encode_u_(&out[0], (unicode / 0x400 + 0xD800));
		encode_u_(&out[6], (unicode % 0x400 + 0xDC00));
		return 12;
	}
}

static ssize_t write_esc_(nmjson_writer_t *self, ssize_t *acc_ret, const char *s, size_t slen){
	//size_t slen = strlen(s);
	int i = 0;
	char encoded[32];
	int start_idx = 0;
	while(i < slen){
		const char *esc_ = NULL;
		size_t esc_len = 0;
		size_t seq_len = 1;	//utf-8読み込みで進めた数
		switch(s[i]){
		case '"':
			esc_ = "\\\""; esc_len = 2;
			break;
		case '\\':
			esc_ = "\\\\"; esc_len = 2;
			break;
		case '\b':
			esc_ = "\\b"; esc_len = 2;
			break;
		case '\f':
			esc_ = "\\f"; esc_len = 2;
			break;
		case '\n':
			esc_ = "\\n"; esc_len = 2;
			break;
		case '\r':
			esc_ = "\\r"; esc_len = 2;
			break;
		case '\t':
			esc_ = "\\t"; esc_len = 2;
			break;
		default:
			if(iscntrl((uint8_t)s[i])){
				/// \note 制御文字は必ずエスケープ
				esc_ = encoded; esc_len = encode_u_(encoded, (uint32_t)s[i]);
				break;
			}
			if(!(self->cfg.utf8_raw) && (s[i] & 0xE0) >= 0xC0){
				/// \note utf-8→unicodeへのエスケープ
				uint32_t unicode;
				ssize_t r = utf8_str_to_unicode(&s[i], &unicode);
				if(r < 0){
					//utf8不正シーケンス。
					encoded[0] = '#';
					hexstr_from_bin_(&encoded[1], (uint8_t)s[i]);
					esc_ = encoded; esc_len = 3;
					break;
				}
				seq_len = r;
				esc_len = encode_u_(encoded, unicode);
				esc_ = encoded;
				break;
			}
			//ただ読み込んだascii系の文字なら何もしないで１個進める。
			i++;
			continue;
		}
		/// \note 「これまでの文字列」をまとめて書く＆エスケープ後を書く
		//printf("%s(%d): write %zu - %zu of [%s]\n", __func__, s[i], start_idx, i, s);
		//printf("%s(%d): write %zu - %zu of [%s](%zd, %zd)\n", __func__, s[i], start_idx, i, esc_, esc_len, seq_len);
		if(write_raw_(self, acc_ret, &(s[start_idx]), i - start_idx) < 0){
			return -1;
		}
		if(esc_ != NULL){
			//エスケープしたほう。
			if(write_raw_(self, acc_ret, esc_, esc_len) < 0){
				return -1;
			}
		}
		start_idx = i + seq_len;
		i = (start_idx);
		//printf("%s: start_idx: %d\n", __func__, start_idx);
	}
	if(write_raw_(self, acc_ret, &(s[start_idx]), i - start_idx) < 0){
		return -1;
	}
	return 0;
}


static inline ssize_t write_str_(nmjson_writer_t *self, ssize_t *acc_ret, const char *s, size_t l){
	ssize_t one_ret;
	if(write_raw_(self, acc_ret, "\"", 1) < 0){
		return -1;
	}
	if(write_esc_(self, acc_ret, s, l) < 0){	//エスケープしつつ文字列を書く
		return -1;
	}
	if(write_raw_(self, acc_ret, "\"", 1) < 0){
		return -1;
	}
	return 0;
}

static inline ssize_t write_newline_(nmjson_writer_t *self, ssize_t *acc_ret){
	ssize_t one_ret;
	if(!self->cfg.pretty_print){
		return 0;
	}
	return write_raw_(self, acc_ret, "\n", 1);
}

static inline ssize_t write_comma_(nmjson_writer_t *self, ssize_t *acc_ret){
	ssize_t one_ret;
	const char *comma = ",";
	size_t slen = 1;
	if(!self->state.need_comma){
		return 0;
	}
	if(self->cfg.pretty_print){
		return write_raw_(self, acc_ret, ", ", 2);
	}
	return write_raw_(self, acc_ret, ",", 1);
}

static inline ssize_t write_indent_(nmjson_writer_t *self, ssize_t *acc_ret){
	int depth = (self->cfg.pretty_print) ? self->state.depth : 0;
	ssize_t ret = 0;
	while(depth > 0){
		if(write_raw_(self, acc_ret, "  ", 2) < 0){
			ret = -1;
			break;
		}
		depth --;
	}
	return ret;
}

static ssize_t write_before_token_(nmjson_writer_t *self, ssize_t *acc_ret){
	ssize_t one_ret;
	if(write_comma_(self, acc_ret) < 0){
		return -1;
	}
	if(write_newline_(self, acc_ret) < 0){
		return -1;
	}
	if(write_indent_(self, acc_ret) < 0){
		return -1;
	}
	return 0;
}

static inline ssize_t write_key_(nmjson_writer_t *self, ssize_t *acc_ret, const nmjson_str_t *key){
	if(key != NULL && key->s != NULL){
		if(write_str_(self, acc_ret, key->s, key->len) < 0){
			return -1;
		}
		return (self->cfg.pretty_print)
			? write_raw_(self, acc_ret, ": ", 2)
			: write_raw_(self, acc_ret, ":",  1)
		;
	}
	return 0;
}

//大体「インデントとカンマ」→「あればキー名」→「自分たちの値」→「改行」の順でモノを書く。
//書いたら次の値に備えてneed_commaを立てる。
#define	write_frame_token_(self, pret, perr, perr_at, key, name, ...) \
	do{\
		ssize_t acc_ret = 0;\
		int i;\
		\
		if(write_before_token_((self), &acc_ret) < 0){\
			*(perr) = errno; *(perr_at) = "write_before_token_("name")";\
			break;\
		}\
		if(write_key_((self), &acc_ret, (key)) < 0){\
			*(perr) = errno; *(perr_at) = "write_key_("name")";\
			break;\
		}\
		__VA_ARGS__\
		\
		(self)->state.need_comma = 1;\
		\
		*(pret) = acc_ret;\
	}while(0);\

/**
 *	\brief		オブジェクト"{"か配列"["の開始を記述する。
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		container_type	: nmjson_type_objectかnmjson_type_arrayのどちらか。
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t	nmjson_writer_begin_n(nmjson_writer_t *self, const nmjson_str_t *key, nmjson_type_t container_type){
	int ret = -1;
	int err = 0;
	ssize_t val_ret = 0;
	const char *err_at = NULL;
	write_frame_token_(self, &ret, &err, &err_at, key, "begin container", {
		const char *prefix_ = "{";
		if(!(container_type & nmjson_type_parent_)) {
			err = EINVAL; err_at = "### SPECIFY OBJECT|ARRAY FOR container_type ###";
			break;
		}
		if(container_type == nmjson_type_array){
			prefix_ = "[";
		}
		if(write_raw_(self, &val_ret, prefix_, 1) < 0){
			err = errno; err_at = "write_raw_(begin container)";
			break;
		}
	});
	if(ret >= 0){
		ret += val_ret;
		self->state.depth ++;
		self->state.need_comma = 0;
	}
	else{
		put_err_(self, err, err_at);
	}
	return ret;
}

/**
 *	\brief		オブジェクト"{"か配列"["の開始を記述する。
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		container_type	: nmjson_type_objectかnmjson_type_arrayのどちらか。
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_begin(nmjson_writer_t *self, const char *key, nmjson_type_t container_type){
	nmjson_str_t nstr;
	nmjson_str_init(&nstr, key);
	return nmjson_writer_begin_n(self, &nstr, container_type);
}

/**
 *	\brief		オブジェクト"}"か配列"]"の終了を記述する。
 *	\arg		self			: writeオブジェクト
 *	\arg		container_type	: nmjson_type_objectかnmjson_type_arrayのどちらか。
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t	nmjson_writer_end(nmjson_writer_t *self, nmjson_type_t container_type){
	int ret = -1;
	int err = 0;
	const char *err_at = NULL;
	do{
		const char *s = "}";
		ssize_t one_ret;
		ssize_t acc_ret = 0;
		int i;
		
		if(!(container_type & nmjson_type_parent_)) {
			err = EINVAL; err_at = "### SPECIFY OBJECT|ARRAY FOR container_type ###";
			break;
		}
		if(container_type == nmjson_type_array){
			s = "]";
		}
		self->state.depth --;
		if(write_newline_(self, &acc_ret) < 0){
			err = errno; err_at = "write_newline_(end container)";
			break;
		}
		if(write_indent_(self, &acc_ret) < 0){
			err = errno; err_at = "write_indent_(end container)";
			break;
		}
		{
			if(write_raw_(self, &acc_ret, s, 1) < 0){
				err = errno; err_at = "write_raw_(end container)";
				break;
			}
		}
		
		self->state.need_comma = 1;
		
		ret = acc_ret;
	}while(0);
	if(ret < 0){
		put_err_(self, err, err_at);
	}
	return ret;
}

/**
 *	\brief		null の書き出し
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 親がオブジェクトの場合のキー名。配列要素の場合は NULL。
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_nullobj_n(nmjson_writer_t *self, const nmjson_str_t *key){
	int ret = -1;
	int err = 0;
	ssize_t val_ret = 0;
	const char *err_at = NULL;
	write_frame_token_(self, &ret, &err, &err_at, key, "null", {
		if(write_raw_(self, &acc_ret, "null", 4) < 0){
			err = errno; err_at = "write_raw_(null)";
			break;
		}
	});
	if(ret < 0){
		put_err_(self, err, err_at);
	}
	return ret;
}

/**
 *	\brief		null の書き出し
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 親がオブジェクトの場合のキー名。配列要素の場合は NULL。
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_nullobj(nmjson_writer_t *self, const char *key){
	nmjson_str_t nstr;
	nmjson_str_init(&nstr, key);
	return nmjson_writer_put_nullobj_n(self, &nstr);
}


/**
 *	\brief		bool の書き出し
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		val				: (bool)0でfalse, それ以外でtrue
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_bool_n(nmjson_writer_t *self, const nmjson_str_t *key, int val){
	int ret = -1;
	int err = 0;
	ssize_t val_ret = 0;
	const char *err_at = NULL;
	write_frame_token_(self, &ret, &err, &err_at, key, "bool", {
		if(val){
			if(write_raw_(self, &acc_ret, "true", 4) < 0){
				err = errno; err_at = "write_raw_(true)";
				break;
			}
		}
		else{
			if(write_raw_(self, &acc_ret, "false", 5) < 0){
				err = errno; err_at = "write_raw_(false)";
				break;
			}
		}
	});
	if(ret < 0){
		put_err_(self, err, err_at);
	}
	return ret;
}

/**
 *	\brief		bool の書き出し
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		val				: (bool)0でfalse, それ以外でtrue
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_bool(nmjson_writer_t *self, const char *key, int val){
	nmjson_str_t nstr;
	nmjson_str_init(&nstr, key);
	return nmjson_writer_put_bool_n(self, &nstr, val);
}

/**
 *	\brief		符号付き整数の書き出し
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		val				: 整数値
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_int_n(nmjson_writer_t *self, const nmjson_str_t *key, int64_t val){
	int ret = -1;
	int err = 0;
	ssize_t val_ret = 0;
	const char *err_at = NULL;
	write_frame_token_(self, &ret, &err, &err_at, key, "int", {
		char buf[32];
		ssize_t slen;
		if((slen = snprintf(buf, 32, "%"PRId64"", val)) < 0){
			err = errno; err_at = "snprintf(int)";
			break;
		}
		
		if(write_raw_(self, &acc_ret, buf, slen) < 0){
			err = errno; err_at = "write_raw_(int)";
			break;
		}
	});
	if(ret < 0){
		put_err_(self, err, err_at);
	}
	return ret;
}

/**
 *	\brief		符号付き整数の書き出し
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		val				: 整数値
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_int(nmjson_writer_t *self, const char *key, int64_t val){
	nmjson_str_t nstr;
	nmjson_str_init(&nstr, key);
	return nmjson_writer_put_int_n(self, &nstr, val);
}

/**
 *	\brief		符号なし整数の書き出し
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		val				: 整数値
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_uint_n(nmjson_writer_t *self, const nmjson_str_t *key, uint64_t val){
	int ret = -1;
	int err = 0;
	ssize_t val_ret = 0;
	const char *err_at = NULL;
	write_frame_token_(self, &ret, &err, &err_at, key, "uint", {
		char buf[32];
		ssize_t slen;
		if((slen = snprintf(buf, 32, "%"PRIu64"", val)) < 0){
			err = errno; err_at = "snprintf(uint)";
			break;
		}
		
		if(write_raw_(self, &acc_ret, buf, slen) < 0){
			err = errno; err_at = "write_raw_(uint)";
			break;
		}
	});
	if(ret < 0){
		put_err_(self, err, err_at);
	}
	return ret;
}

/**
 *	\brief		符号なし整数の書き出し
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		val				: 整数値
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_uint(nmjson_writer_t *self, const char *key, uint64_t val){
	nmjson_str_t nstr;
	nmjson_str_init(&nstr, key);
	return nmjson_writer_put_uint_n(self, &nstr, val);
}

/**
 *	\brief		実数の書き出し（%g フォーマット）
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		val				: 浮動小数値
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_float_n(nmjson_writer_t *self, const nmjson_str_t *key, double val){
	int ret = -1;
	int err = 0;
	ssize_t val_ret = 0;
	const char *err_at = NULL;
	write_frame_token_(self, &ret, &err, &err_at, key, "float", {
		/// \note infinity/nanの場合の対応。json5の場合にraw, その他の場合strで書く。
		ssize_t slen;
		nmjson_str_t nstr;
		if(isnan(val)){
			nmjson_str_init(&nstr, nmjson_superset_has_extnum(self->cfg.superset) ?
				"NaN":"\"NaN\""
			);
			if(write_raw_(self, &acc_ret, nstr.s, nstr.len) < 0){
				err = errno; err_at = "write_raw_(nan)";
			}
		}
		else if(isinf(val)){
			if(val < 0.0){
				nmjson_str_init(&nstr, nmjson_superset_has_extnum(self->cfg.superset) ?
					"-Infinity":"\"-Infinity\""
				);
			}
			else{
				nmjson_str_init(&nstr, nmjson_superset_has_extnum(self->cfg.superset) ?
					"Infinity":"\"Infinity\""
				);
			}
			if(write_raw_(self, &acc_ret, nstr.s, nstr.len) < 0){
				err = errno; err_at = "write_raw_(infinity)";
			}
		}
		else{
			char buf[32];
			if((slen = snprintf(buf, 32, "%g", val)) < 0){
				err = errno; err_at = "snprintf(float)";
				break;
			}
			
			if(write_raw_(self, &acc_ret, buf, slen) < 0){
				err = errno; err_at = "write_raw_(float)";
				break;
			}
		}
	});
	if(ret < 0){
		put_err_(self, err, err_at);
	}
	return ret;
}

/**
 *	\brief		実数の書き出し（%g フォーマット）
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		val				: 浮動小数値
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_float(nmjson_writer_t *self, const char *key, double val){
	nmjson_str_t nstr;
	nmjson_str_init(&nstr, key);
	return nmjson_writer_put_float_n(self, &nstr, val);
}

/**
 *	\brief		文字列の書き出し（エスケープ処理込み）
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		val				: 文字列。NULL の場合は null として出力する
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_string_n(nmjson_writer_t *self, const nmjson_str_t *key, const char *val){
	int ret = -1;
	int err = 0;
	ssize_t val_ret = 0;
	const char *err_at = NULL;
	if(val == NULL){
		return nmjson_writer_put_nullobj_n(self, key);
	}
	write_frame_token_(self, &ret, &err, &err_at, key, "string", {
		if(write_str_(self, &acc_ret, val, strlen(val)) < 0){
			err = errno; err_at = "write_raw_(string)";
			break;
		}
	});
	if(ret < 0){
		put_err_(self, err, err_at);
	}
	return ret;
}

/**
 *	\brief		文字列の書き出し（エスケープ処理込み）
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		val				: 文字列。NULL の場合は null として出力する
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_string(nmjson_writer_t *self, const char *key, const char *val){
	nmjson_str_t nstr;
	nmjson_str_init(&nstr, key);
	return nmjson_writer_put_string_n(self, &nstr, val);
}

/**
 *	\brief		指定長文字列の書き出し（エスケープ処理込み）
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		val				: 文字列。NULL の場合は null として出力する
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_nstring_n(nmjson_writer_t *self, const nmjson_str_t *key, const nmjson_str_t *val){
	int ret = -1;
	int err = 0;
	ssize_t val_ret = 0;
	const char *err_at = NULL;
	if(val == NULL || val->s == NULL){
		return nmjson_writer_put_nullobj_n(self, key);
	}
	write_frame_token_(self, &ret, &err, &err_at, key, "string", {
		if(write_str_(self, &acc_ret, val->s, val->len) < 0){
			err = errno; err_at = "write_raw_(string)";
			break;
		}
	});
	if(ret < 0){
		put_err_(self, err, err_at);
	}
	return ret;
}

/**
 *	\brief		指定長文字列の書き出し（エスケープ処理込み）
 *	\arg		self			: writeオブジェクト
 *	\arg		key				: 追加するトークンの名前。配列下ならNULL。
 *	\arg		val				: 文字列。NULL の場合は null として出力する
 *	\return	>= 0			: 成功。書き込んだ文字数
 *	\return	 < 0			: 失敗
 */
ssize_t nmjson_writer_put_nstring(nmjson_writer_t *self, const char *key, const nmjson_str_t *val){
	nmjson_str_t nstr;
	nmjson_str_init(&nstr, key);
	return nmjson_writer_put_nstring_n(self, &nstr, val);
}


