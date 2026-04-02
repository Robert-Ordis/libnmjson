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
	nmjson_str_t nstr;
	
	nmjson_writer_init_fd(&writer, 1);
	//nmjson_writer_init_fp(&writer, stdout);
	nmjson_writer_cfg_pretty_print(&writer, 1);
	nmjson_writer_cfg_superset(&writer, nmjson_superset_json5);
	nmjson_writer_with_object(&writer, NULL, {
		
		nstr.s = "v_\0int"; nstr.len = 6;
		nmjson_writer_put_int_n(&writer, &nstr, 1);
		
		nmjson_writer_with_array(&writer, "v_aあrr", {
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
			nmjson_writer_put_float(&writer, NULL, NAN);
			nmjson_writer_put_float(&writer, NULL, INFINITY);
			nmjson_writer_put_float(&writer, NULL, -INFINITY);
		});
		nmjson_writer_with_array(&writer, "v_すｔr", {
			char buf[64];
			int i;
			for(i = 0; i < 16; i++){
				sprintf(buf, "\r%03dばんめstr\n", i);
				nmjson_writer_put_string(&writer, NULL, buf);
			}
		});
		nmjson_writer_put_string(&writer, "try_surrogate", "🙃←何ｺﾚ");
	});
	printf("\n");
}
