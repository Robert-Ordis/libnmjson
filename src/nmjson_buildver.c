#include <stdio.h>

// configure が生成する(バージョンの定義元は configure.ac)
#include "include/nmjson/nmjson_version.h"

const char*	nmjson_get_build_ver(void){
	static char verstr[128] = "";
	if(verstr[0] == '\0'){
		snprintf(verstr, sizeof(verstr), "%s %s(%s)",
			NMJSON_PACKAGE_NAME, NMJSON_VERSION_STRING, BUILD_DATE);
	}

	return verstr;
}

int			nmjson_get_major_ver(void){
	return NMJSON_VERSION_MAJOR;
}

int			nmjson_get_minor_ver(void){
	return NMJSON_VERSION_MINOR;
}

int			nmjson_get_patch_ver(void){
	return NMJSON_VERSION_PATCH;
}
