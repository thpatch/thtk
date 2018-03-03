/* This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <http://unlicense.org/>
 **/
#include <stdio.h> /* for fprintf, stderr, NULL */
#include <ctype.h> /* for isalnum */
#include <string.h> /* for strchr */
#define MYGETOPT_KEEPDEFINES
#include "mygetopt.h"

char *optarg = NULL;
int opterr = 1, optind = 1, optopt = 0;
int getopt(int argc, char *const argv[], const char *optstring) {
	static char* arg = NULL;
	char*p;
	int rv;
	if(optind >= argc || !argv || !optstring) return -1;
	if(arg == NULL) {
		char *temp = argv[optind];
		if(temp == NULL) return -1;
		if(temp[0] != '-') return -1;
		if(temp[1] == '\0') return -1;
		if(temp[1] == '-' && temp[2] == '\0') { optind++; return -1; }
		arg = temp+1;
	}
	
	optopt = *arg++;
	if(isalnum(optopt) && (p=strchr(optstring,optopt))) {
		rv = optopt;
		if(p[1] == ':') {
			if(*arg) {
				/* argument of form -ffilename */
				optarg = arg;
				arg = NULL;
				optind++;
			}
			else {
				/* argument of form -f filename */
				optarg = argv[++optind]; /* optind will be increased second time later */
				if(!optarg) {
					if(opterr && optstring[0] != ':')
						fprintf(stderr, "%s: Option -%c requires an operand\n", argv[0], optopt);
					rv = optstring[0] == ':' ? ':' : '?';
				}
			}
		}
	}
	else {
		if(opterr && optstring[0] != ':')
			fprintf(stderr, "%s: Unrecognized option: '-%c'\n", argv[0], optopt);
		rv = '?';
	}

	if(arg && *arg == '\0') {
		arg = NULL;
		optind++;
	}

	return rv;
}
