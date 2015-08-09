/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison interface for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2011 Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     MODE = 258,
     GEOMETRY = 259,
     TIMINGS = 260,
     HSYNC = 261,
     VSYNC = 262,
     CSYNC = 263,
     GSYNC = 264,
     EXTSYNC = 265,
     BCAST = 266,
     LACED = 267,
     DOUBLE = 268,
     NONSTD = 269,
     GRAYSCALE = 270,
     ENDMODE = 271,
     ACCEL = 272,
     RGBA = 273,
     FRACTION = 274,
     POLARITY = 275,
     BOOLEAN = 276,
     STRING = 277,
     NUMBER = 278
   };
#endif
/* Tokens.  */
#define MODE 258
#define GEOMETRY 259
#define TIMINGS 260
#define HSYNC 261
#define VSYNC 262
#define CSYNC 263
#define GSYNC 264
#define EXTSYNC 265
#define BCAST 266
#define LACED 267
#define DOUBLE 268
#define NONSTD 269
#define GRAYSCALE 270
#define ENDMODE 271
#define ACCEL 272
#define RGBA 273
#define FRACTION 274
#define POLARITY 275
#define BOOLEAN 276
#define STRING 277
#define NUMBER 278




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE yylval;


