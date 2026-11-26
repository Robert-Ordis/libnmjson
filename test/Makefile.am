noinst_PROGRAMS = testProgram

testProgram_SOURCES = \
	main.c \
	00-strbuf.c \
	01-kitchen-sink.c \
	02-cmd-stream.c \
	03-kitchen-sink5.c \
	04-kitchen-sink-rough.c \
	10-writer.c \
	20-roundtrip.c \
	21-roundtrip-num.c \
	22-token-as-int.c \
	23-superset.c \
	24-writer-comment.c \
	25-buffer-resize.c \
	26-e5ident.c \
	27-space.c \
	28-token-pool.c

testProgram_LDADD = ../src/libnmjson.a

testProgram_CFLAGS = \
	-I$(top_srcdir)/

EXTRA_DIST = example
