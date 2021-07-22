#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "parse.h"
#include "load_config.h"

#define UMAX ((unsigned short) -1)

unsigned short fdgetc(int stream) {
	unsigned char c;
	ssize_t len = read(stream, &c, 1);
	switch(len) {
		case  1: return c;
		case  0: return UMAX;
		default: warn("read"); return UMAX;
	}
}

unsigned short fdpeekc(int stream) {
	unsigned short c;
	if((c = fdgetc(stream)) != UMAX) {
		if(lseek(stream, -1, SEEK_CUR) < 0) {
			warn("fseek");
		}
	}
	return c;
}

ssize_t gotoNext(int stream, char end) {
	unsigned short c;
	ssize_t len = 0;
	for(;;) {
		if((c = fdgetc(stream)) == UMAX) {
			return -1;
		}
		++len;
		if(c == end) {
			break;
		}
	}
	return len;
}

ssize_t findNext(int stream, char end) {
	ssize_t len;
	if((len = gotoNext(stream, end)) < 0) {
		warn("gotoNext");
	} else {
		if(lseek(stream, -len, SEEK_CUR) < 0) {
			warn("lseek");
		}
	}
	return len;
}

void processCommand(int* command_ready, char* chain, char* command) {
	if(*command_ready) {
		process_hotkey(chain, command);
	}
	*command_ready = 0;
}

int safeRealloc(void** data, size_t size) {
	if(size == 0) {
		free(*data);
		return 0;
	}

	void* ptr;
	if((ptr = realloc(*data, size)) == NULL) {
		warn("realloc");
		return -1;
	} else {
		*data = ptr;
		return 0;
	}
}

void load_config(const char* config_file) {
	int cfg = -1;
	if((cfg = open(config_file, O_RDONLY)) < 0) {
		warn("open");
		goto end;
	}

	char* chain = NULL;
	char* command = NULL;
	int command_ready = 0;

	for(;;) {
		unsigned short here = fdpeekc(cfg);
		if(here == '\t') {
			// read command
			ssize_t len;
			size_t total = 0;

			for(;;) {
				if((len = gotoNext(cfg, '\n')) < 0) goto end;
				total += len;
				unsigned short c = fdpeekc(cfg);
				if(c != '\t') break;
			}

			if(lseek(cfg, -total, SEEK_CUR) < 0) {
				warn("lseek");
				goto end;
			}

			if(safeRealloc((void**) &command, total + 1) < 0) goto end;
			if(read(cfg, command, total) != (ssize_t) total) goto end;
			command[total] = 0;
			command_ready = 1;
		} else {
			processCommand(&command_ready, chain, command);
			if(here == UMAX) break; // EOF

			// read chain
			ssize_t len;
			if((len = findNext(cfg, '\n')) < 0) goto end;
			if(safeRealloc((void**) &chain, len) < 0) goto end;
			if(read(cfg, chain, len) != len) goto end;
			chain[len - 1] = 0;
		}
	}

end:
	if(cfg) close(cfg);
	if(chain) free(chain);
	if(command) free(command);
}
