# 公開ヘッダ。ヘッダを追加したらここにも追記すること。
# ($(wildcard ...) はビルドディレクトリで評価されるため、ソースと別ディレクトリでビルドすると何も拾えない)
nobase_include_HEADERS = \
	nmjson/json_const.h \
	nmjson/json_parser.h \
	nmjson/json_token.h \
	nmjson/nmjson.h \
	nmjson/nmjson_buffer.h \
	nmjson/nmjson_str.h \
	nmjson/nmjson_writer.h \
	nmjson/polyfill.h \
	nmjson/unicode.h \
	nmjson/utf8.h

# configure が nmjson_version.h.in から生成する(配布物には .in のみを含める)
nobase_nodist_include_HEADERS = nmjson/nmjson_version.h
