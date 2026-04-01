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

#include "include/nmjson/nmjson.h"
#include "include/nmjson/nmjson_writer.h"

#define TOKENS_NUM 200
#define CBUF_NUM 1024



void test10(){
	nmjson_writer_t writer;
	//nmjson_writer_init_fd(&writer, 1);
	nmjson_writer_init_fp(&writer, stdout);
	nmjson_writer_cfg_pretty_print(&writer, 1);
	nmjson_writer_with_object(&writer, NULL, {
		nmjson_writer_put_int(&writer, "v_int", 1);
		nmjson_writer_with_array(&writer, "v_arr", {
			int i;
			for(i = 0; i < 32; i++){
				nmjson_writer_put_int(&writer, NULL, i);
			}
		});
		nmjson_writer_with_array(&writer, "v_\ndbl", {
			int i;
			for(i = 0; i < 32; i++){
				nmjson_writer_put_float(&writer, NULL, (double)i);
			}
		});
	});
	printf("\n");
}
