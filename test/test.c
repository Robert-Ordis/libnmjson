#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]){
	const char s[] = "123.";
	char *endptr;
	printf("s[%s] -> d[%f]\n", s, strtod(s, &endptr));
	printf("endptr[%p/%s]\n", endptr, endptr);
	return 0;
}

