/* 
 *   Creation Date: <1999/03/29 11:09:18 samuel>
 *   Time-stamp: <2004/03/27 01:48:11 samuel>
 *   
 *	<default_options.h>
 *	
 *	Default resources 
 *   
 *   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifdef HAVE_GETOPT_H
#include <getopt_compat.h>
#else
struct option {
	char	*optname;
	int 	has_arg;
	int	*unimplemented_flag;
	int	val;
};
#endif

static char helpstr[] = "Do not invoke mol directly, use startmol\n";

enum{ 
	optRamSize=1000, optVTNum, optSessNum, optTest, optKeyConfig,
	optElfImage, optDataDir, optLibDir, optAddRes, optKernel, 
	optLinux, optOldworld, optNewworld, optConfig, optLoadOnly,
	optDetachTTY, optCDBoot, optAltConfig, optZapNVRAM, optNoAutoBoot
};

static struct option opt[] = {
	{"session",	1, 0, optSessNum },
	{"alt",		0, 0, optAltConfig },
	{"config",	1, 0, optConfig },
	
	{"loadonly",	0, 0, optLoadOnly },
	{"exact",	0, 0, 'e' },

	{"help",	0, 0, 'h'},
	{"ram",		1, 0, optRamSize },
	{"debug",	0, 0, 'd' },
	{"vt",		1, 0, optVTNum },
	{"detach",	0, 0, optDetachTTY },
	{"cdboot",	0, 0, optCDBoot },

	{"zapnvram",	0, 0, optZapNVRAM },
	{"noautoboot",	0, 0, optNoAutoBoot },

	{"datadir",	1, 0, optDataDir },
	{"libdir",	1, 0, optLibDir },

	{"test",	0, 0, optTest },
	{"keyconfig",	0, 0, optKeyConfig },

	{"linux",	0, 0, optLinux },
	{"oldworld",	0, 0, optOldworld },
	{"newworld",	0, 0, optNewworld },
	{"classic",	0, 0, optNewworld },
	{"macos",	0, 0, optNewworld },
	{"osx",		0, 0, 'X' },

	{"elfimage",	1, 0, optElfImage },
	{"kernel",	1, 0, optKernel },

	{"res",		1, 0, optAddRes },
	{0,0,0,0}
};

struct opt_res_entry {
	int	opt_code;
	int	root_only;	/* true if allowed for root only */
	int	res_type;
	char	*str;
};

enum { 
	kResLine=1, kNumericRes, kParseRes, kStringRes
};

/* option code -> resource name */
/* IMPORTANT: don't forget security issues. */
static struct opt_res_entry or_table[] = {
	{optAddRes,	1,	kResLine,	NULL },
	{optLoadOnly,	0,	kParseRes,	"load_only: 1"},
	{optRamSize,	0,	kNumericRes,	"ram_size"},
	{optVTNum,	0,	kNumericRes,	"vt"},
	{optSessNum,	0,	kNumericRes,	"session" },
	{optAltConfig,	0,	kParseRes,	"altconfig: 1" },
	{optConfig,	0,	kStringRes,	"config" },
	{optTest,	0,	kParseRes,	"boot_method: elf; elf_image: ${lib}/bin/selftest; no_video: 1" },
	{optKeyConfig,	0,	kParseRes,	"boot_method: elf; elf_image: ${lib}/bin/keyremap" },
	{optLinux,	0,	kParseRes,	"boot_type: linux" },
	{optOldworld,	0,	kParseRes,	"boot_type: oldworld" },
	{optNewworld,	0,	kParseRes,	"boot_type: newworld" },
	{'X',		0,	kParseRes,	"boot_type: osx" },
	{optDataDir,	1,	kStringRes,	"data" },
	{optLibDir,	1,	kStringRes,	"lib" },
	{optElfImage,	1,	kParseRes,	"boot_type: elf" },
	{optElfImage,	1,	kStringRes,	"elf_image" },
	{optKernel,	1,	kStringRes,	"kernel" },
	{'x',		1,	kParseRes,	"no_moldeb: 1; debug: 1; debug_stop: 1" },
	{'d',		1,	kParseRes,	"debug: 1; debug_stop: 1" },
	{'a',		1,	kParseRes,	"allow_kver_mismatch: 1" },
	{'e',		0,	kParseRes,	"allow_kver_mismatch: 0" },
	{optDetachTTY,	0,	kParseRes,	"detach_tty: 1" },
	{optCDBoot,	0,	kParseRes,	"cdboot: -cdboot" },
	{optZapNVRAM,	0,	kParseRes,	"zap_nvram: true" },
	{optNoAutoBoot,	0,	kParseRes,	"autoboot: false" },
	{0,0,0,0}
};
