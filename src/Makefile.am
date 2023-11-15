AUTOMAKE_OPTIONS = subdir-objects

lib_LIBRARIES = libnmjson.a

libnmjson_a_SOURCES = \
	nmjson_buildver.c \
	buffer/nmjson_buffer_.c \
	parser/nmjson_parser_.c \
	parser/nmjson_parser_dbg_print.c \
	parser/nmjson_parser_parse.c \
	local_header/linear_linker.h \
	private_funcs/token_input_e5ident_.h \
	private_funcs/token_input_literal_.h \
	private_funcs/token_parse_.h \
	private_funcs/token_parse_comment_.h \
	token/nmjson_token_fout.c \
	unicode/utf8_.c 
	

libnmjson_a_CFLAGS = $(AM_CFLAGS) \
	-DVER_MAJOR=0 \
	-DVER_MINOR=7 \
	-DVER_PATCH=1 \
	-DVER_TAILS= \
	-DPROJECT_NAME=\"libnmjson\" \
	-DBUILD_DATE=\"$(shell date +'%Y-%m-%d_%H:%M:%S')\" \
	-I$(top_srcdir)/

libnmjson_a-nmjson_buildver.$(OBJEXT) : FORCE
	
FORCE:
