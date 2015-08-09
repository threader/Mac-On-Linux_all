/* Modified version of nvsetenv */

#include "mol_config.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define NVSTART	0x1800
#define NVSIZE	0x800

struct nvram {
	unsigned short magic;		/* 0x1275 */
	unsigned char  char2;
	unsigned char  char3;
	unsigned short cksum;
	unsigned short end_vals;
	unsigned short start_strs;
	unsigned short word5;
	unsigned long  bits;
	unsigned long  vals[1];
};

union {
	struct nvram nv;
	char c[NVSIZE];
	unsigned short s[NVSIZE/2];
} nvbuf;

int nvfd;

static int
nvcsum( void )
{
	int i;
	unsigned c;
	
	c=0;
	for( i=0; i < NVSIZE/2; ++i)
		c += nvbuf.s[i];
	c = (c & 0xffff) + (c >> 16);
	c += (c >> 16);
	return c & 0xffff;
}

static void
nvload( void )
{
	int s;

	if( lseek(nvfd, NVSTART, 0) < 0
	    || read(nvfd, &nvbuf, NVSIZE) != NVSIZE) {
		perror("Error reading file");
		exit(1);
	}
	s = nvcsum();
	if( s != 0xffff)
		fprintf(stderr, "Warning: checksum error (%x) on nvram\n", s ^ 0xffff);
}

static void
nvstore( void )
{
	if( lseek(nvfd, NVSTART, 0) < 0 || write(nvfd, &nvbuf, NVSIZE) != NVSIZE) {
		perror("Error writing file");
		exit(1);
	}
}

enum nvtype {
	boolean, word, string
};

struct nvvar {
	char *name;
	enum nvtype type;
} nvvars[] = {
	{ "little-endian?",	boolean	},
	{ "real-mode?",		boolean	},
	{ "auto-boot?",		boolean	},
	{ "diag-switch?",	boolean	},
	{ "fcode-debug?",	boolean	},
	{ "oem-banner?",	boolean	},
	{ "oem-logo?",		boolean	},
	{ "use-nvramrc?",	boolean	},
	{ "real-base",		word	},
	{ "real-size",		word	},
	{ "virt-base",		word	},
	{ "virt-size",		word	},
	{ "load-base",		word	},
	{ "pci-probe-list",	word	},
	{ "screen-#columns",	word	},
	{ "screen-#rows",	word	},
	{ "selftest-#megs",	word	},
	{ "boot-device",	string	},
	{ "boot-file",		string	},
	{ "diag-device",	string	},
	{ "diag-file",		string	},
	{ "input-device",	string	},
	{ "output-device",	string	},
	{ "oem-banner",		string	},
	{ "oem-logo",		string	},
	{ "nvramrc",		string	},
	{ "boot-command",	string	},
};

#define N_NVVARS	(sizeof(nvvars) / sizeof(nvvars[0]))

union nvval {
	unsigned int word_val;
	char *str_val;
} nvvals[32];

static char nvstrbuf[NVSIZE];
static int nvstr_used;

static void
nvunpack( void )
{
	int i;
	unsigned long bmask;
	int vi, off, len;
	
	nvstr_used = 0;	
	bmask = 0x80000000;
	vi = 0;
	for( i=0; i < N_NVVARS; ++i ) {
		switch( nvvars[i].type ) {
		case boolean:
			nvvals[i].word_val = (nvbuf.nv.bits & bmask)? 1: 0;
			bmask >>= 1;
			break;
		case word:
			nvvals[i].word_val = nvbuf.nv.vals[vi++];
			break;
		case string:
			off = nvbuf.nv.vals[vi] >> 16;
			len = nvbuf.nv.vals[vi++] & 0xffff;
			nvvals[i].str_val = nvstrbuf + nvstr_used;
			memcpy(nvvals[i].str_val, nvbuf.c + off - NVSTART, len);
			nvvals[i].str_val[len] = 0;
			nvstr_used += len + 1;
			break;
		}
	}
}

static void
nvpack( void )
{
	int i, off, len, vi;
	unsigned long bmask;
	
	bmask = 0x80000000;
	vi = 0;
	off = NVSIZE;
	nvbuf.nv.bits = 0;
	for( i = 0; i < N_NVVARS; ++i) {
	switch( nvvars[i].type ) {
	case boolean:
		if( nvvals[i].word_val)
		nvbuf.nv.bits |= bmask;
		bmask >>= 1;
		break;
	case word:
		nvbuf.nv.vals[vi++] = nvvals[i].word_val;
		break;
	case string:
		len = strlen(nvvals[i].str_val);
		off -= len;
		memcpy(nvbuf.c + off, nvvals[i].str_val, len);
		nvbuf.nv.vals[vi++] = ((off + NVSTART) << 16) + len;
		break;
	}
	}
	nvbuf.nv.magic = 0x1275;
	nvbuf.nv.cksum = 0;
	nvbuf.nv.end_vals = NVSTART + (unsigned) &nvbuf.nv.vals[vi]
	- (unsigned) &nvbuf;
	nvbuf.nv.start_strs = off + NVSTART;
	memset(&nvbuf.c[nvbuf.nv.end_vals - NVSTART], 0,
	   nvbuf.nv.start_strs - nvbuf.nv.end_vals);
	nvbuf.nv.cksum = ~nvcsum();
}

static void
print_var( int i, int indent )
{
	char *p;

	switch( nvvars[i].type ) {
	case boolean:
		printf("%s", nvvals[i].word_val? "true": "false");
		break;
	case word:
		printf("0x%x", nvvals[i].word_val);
		break;
	case string:
		for( p = nvvals[i].str_val; *p != 0; ++p)
			if( *p != '\r')
				putchar(*p);
			else
				printf("\n%*s", indent, "");
		break;
	}
	printf("\n");
}

static int
parse_val( int i, char *str )
{
	char *endp;

	switch( nvvars[i].type ) {
	case boolean:
		if( strcmp(str, "true") == 0)
			nvvals[i].word_val = 1;
		else if( strcmp(str, "false") == 0)
			nvvals[i].word_val = 0;
		else {
			fprintf(stderr, "bad boolean value '%s' for %s\n", str,
				nvvars[i].name);
			return 0;
		}
		break;
	case word:
		nvvals[i].word_val = strtoul(str, &endp, 16);
		if( str == endp) {
			fprintf(stderr, "bad hexadecimal value '%s' for %s\n", str,
				nvvars[i].name);
			return 0;
		}
		break;
	case string:
		nvvals[i].str_val = str;
		break;
	}
	return 1;
}

static char *
get_image_name( char *molrc_name )
{
	FILE *file;
	char *nvram_name = NULL;
	char buf[1024];
	
	if( !molrc_name )
		molrc_name = "molrc";
	file = fopen( molrc_name, "r" );
	if( !file ) {
		fprintf(stderr,"Could not find '%s'\n",molrc_name );
		return NULL;
	}

	while( fgets(buf,1024,file) ) {
		char *ptr = buf;
		strsep( &ptr, "#");
		ptr = strstr(buf, "nvram_image:" );
		strsep( &ptr, ":");
		while( ptr && isblank(*ptr) )
			ptr++;
		ptr = strsep( &ptr, " \t\n\r");
		if( !ptr || !*ptr )
			continue;
		nvram_name = strdup(ptr);
	}
	fclose( file );

	return nvram_name;
}


int
main( int ac, char **av )
{
	int ch, i=0, print;
	char **args, *file_name, *res_name;

	file_name = NULL;
	res_name = NULL;
	while( (ch=getopt(ac, av, "f:r:F:hH")) != EOF ) {
		switch( ch ) {
		case 'f':
		case 'F':
			file_name = optarg;
			break;
		case 'r':
			res_name = optarg;
			break;
		case 'h':
		case 'H':
			fprintf(stderr, "Usage: %s [-f nvram_image] [-r molrc_file] [variable [value]]\n", av[0]);
			exit(0);
			break;
		case '?':
			break;
		}
	}
	if( !file_name )
		file_name = get_image_name( res_name );
	if( !file_name )
		file_name = "nvram.image";

	ac -= optind;
	args = av + optind;

	print = ac <= 1;
	if( print && ac > 1) {
		fprintf(stderr, "Usage: %s [-f image] [variable]\n", av[0]);
		exit(1);
	}
	if( ac > 2) {
		fprintf(stderr, "Usage: %s [-f image] [variable [value]]\n", av[0]);
		exit(1);
	}

	if( ac >= 1 ) {
		for( i=0; i < N_NVVARS; ++i )
			if( strcmp(args[0], nvvars[i].name) == 0 )
				break;
		if( i >= N_NVVARS ) {
			fprintf(stderr, "%s: no variable called '%s'\n", av[0], args[0]);
			exit(1);
		}
	}

	nvfd = open( file_name, ac <= 1? O_RDONLY: O_RDWR );
	if( nvfd < 0 ) {
		fprintf(stderr, "Opening '%s': ", file_name);
		perror("");
		exit(1);
	}
	nvload();
	nvunpack();

	switch( ac ) {
	case 0:
		for( i=0; i < N_NVVARS; ++i ) {
			printf("%-16s", nvvars[i].name);
			print_var(i, 16);
		}
		break;

	case 1:
		print_var(i, 0);
		break;
	
	case 2:
		if( !parse_val(i, args[1]) )
			exit(1);
		nvpack();
		nvstore();
		break;
	}

	close(nvfd);
	exit(0);
}
