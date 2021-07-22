#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "parse.h"
#include "load_config.h"

char fdgetc(int stream) {
	char c = -1;
	ssize_t len = read(stream, &c, 1);
	if(len != 1) {
		if(len < 0) {
			warn("fdgetc");
			return len;
		}
		return -2;
	}
	return c;
}

char fdpeekc(int stream) {
	char c = -1;
	if((c = fdgetc(stream)) > -1) {
		if(lseek(stream, -1, SEEK_CUR) < 0) {
			warn("fseek");
		}
	}
	return c;
}

ssize_t gotoNext(int stream, char end) {
	char c;
	ssize_t len = 0;
	for(;;) {
		if((c = fdgetc(stream)) < 0) {
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
		printf("%s :: %s", chain, command);
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
		char here = fdpeekc(cfg);
		if(here == '\t') {
			// read command
			ssize_t len;
			size_t total = 0;

			for(;;) {
				if((len = gotoNext(cfg, '\n')) < 0) goto end;
				total += len;
				char c = fdpeekc(cfg);
				if(c == -1) goto end;
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
			if(here < 0) break; // EOF

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
