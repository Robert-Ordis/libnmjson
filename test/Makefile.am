noinst_PROGRAMS = testProgram

testProgram_SOURCES = \
	main.c \
	00-strbuf.c \
	01-kitchen-sink.c \
	02-cmd-stream.c \
	03-kitchen-sink5.c \
	04-kitchen-sink-rough.c 

testProgram_LDADD = ../src/libnmjson.a

testProgram_CFLAGS = \
	-I$(top_srcdir)/

EXTRA_DIST = example
