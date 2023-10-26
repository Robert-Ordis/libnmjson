#include <stdio.h>

#define xstr(s) str(s)
#define str(s) #s

const char*	nmjson_get_build_ver(){
	static char verstr[128] = "";
	if(verstr[0] == '\0'){
		snprintf(verstr, sizeof(verstr), "%s %d.%d.%d%s", 
			PROJECT_NAME, VER_MAJOR, VER_MINOR, VER_PATCH, xstr(VER_TAILS));
	}
	
	return verstr;
}

const int	nmjson_get_major_ver(){
	return VER_MAJOR;
}

const int	nmjson_get_minor_ver(){
	return VER_MINOR;
}

const int	nmjson_get_patch_ver(){
	return VER_PATCH;
}

