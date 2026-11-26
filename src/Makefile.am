AUTOMAKE_OPTIONS = subdir-objects

EXTRA_DIST = \
	third_party/ryu/LICENSE-Apache2 \
	third_party/ryu/LICENSE-Boost \
	third_party/ryu/README.nmjson.txt


lib_LIBRARIES = libnmjson.a

libnmjson_a_SOURCES = \
	nmjson_buildver.c \
	buffer/nmjson_buffer_.c \
	parser/nmjson_parser_.c \
	parser/nmjson_parser_dbg_print.c \
	parser/nmjson_parser_parse.c \
	local_header/linear_linker.h \
	local_header/numfmt.h \
	local_header/swriter.h \
	local_header/token_tree.h \
	numfmt/nmjson_numfmt_f64_.c \
	numfmt/nmjson_numfmt_strtod_.c \
	third_party/ryu/common.h \
	third_party/ryu/d2s_intrinsics.h \
	third_party/ryu/d2s_small_table.h \
	private_funcs/token_input_e5ident_.h \
	private_funcs/token_input_literal_.h \
	private_funcs/token_parse_.h \
	private_funcs/token_parse_comment_.h \
	private_funcs/token_space_.h \
	token/nmjson_token_fout.c \
	writer/nmjson_writer_init_.c \
	writer/nmjson_writer_cfg_.c \
	writer/nmjson_writer_put_.c \
	unicode/utf8_.c 
	

libnmjson_a_CFLAGS = $(AM_CFLAGS) \
	-DVER_MAJOR=0 \
	-DVER_MINOR=10 \
	-DVER_PATCH=0 \
	-DVER_TAILS= \
	-DPROJECT_NAME=\"libnmjson\" \
	-DBUILD_DATE=\"$(shell date +'%Y-%m-%d_%H:%M:%S')\" \
	-I$(top_srcdir)/ \
	-I$(top_srcdir)/src/third_party

libnmjson_a-nmjson_buildver.$(OBJEXT) : FORCE
	
FORCE:
