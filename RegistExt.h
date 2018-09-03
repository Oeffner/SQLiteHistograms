/*
RegistExt.h, Robert Oeffner 2018

The MIT License (MIT)

Copyright (c) 2017 Robert Oeffner

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include "sqlite3ext.h"
#include <cstddef>


#ifdef __cplusplus
extern "C" {
#endif


// scalar functions

void sqrtFunc(sqlite3_context *context, int argc, sqlite3_value **argv);
void logFunc(sqlite3_context *context, int argc, sqlite3_value **argv);
void expFunc(sqlite3_context *context, int argc, sqlite3_value **argv);
void powFunc(sqlite3_context *context, int argc, sqlite3_value **argv);

// aggregate functions

void CoVarStep(sqlite3_context *context, int argc, sqlite3_value **argv);
void CoVarFinal(sqlite3_context *context);
void CorrelStep(sqlite3_context *context, int argc, sqlite3_value **argv);
void CorrelFinal(sqlite3_context *context);
void SpCorrelStep(sqlite3_context *context, int argc, sqlite3_value **argv);
void SpCorrelFinal(sqlite3_context *context);



extern const sqlite3_api_routines *sqlite3_api;
extern sqlite3 *thisdb;


// module functions for some virtual tables

extern sqlite3_module histoModule;

int histoConnect(
  sqlite3 *db,
  void *pAux,
  int argc, const char *const*argv,
  sqlite3_vtab **ppVtab,
  char **pzErr
  );
int histoDisconnect(sqlite3_vtab *pVtab);
int histoOpen(sqlite3_vtab *p, sqlite3_vtab_cursor **ppCursor);
int histoClose(sqlite3_vtab_cursor *cur);
int histoNext(sqlite3_vtab_cursor *cur);
int histoColumn(sqlite3_vtab_cursor *cur, sqlite3_context *ctx, int i);
int histoRowid(sqlite3_vtab_cursor *cur, sqlite_int64 *pRowid);
int histoEof(sqlite3_vtab_cursor *cur);
int histoFilter(
  sqlite3_vtab_cursor *pVtabCursor,
  int idxNum, const char *idxStr,
  int argc, sqlite3_value **argv
  );
int histoBestIndex(sqlite3_vtab *tab, sqlite3_index_info *pIdxInfo);




extern sqlite3_module ratiohistoModule;

int ratiohistoConnect(
  sqlite3 *db,
  void *pAux,
  int argc, const char *const*argv,
  sqlite3_vtab **ppVtab,
  char **pzErr
  );
int ratiohistoDisconnect(sqlite3_vtab *pVtab);
int ratiohistoOpen(sqlite3_vtab *p, sqlite3_vtab_cursor **ppCursor);
int ratiohistoClose(sqlite3_vtab_cursor *cur);
int ratiohistoNext(sqlite3_vtab_cursor *cur);
int ratiohistoColumn(sqlite3_vtab_cursor *cur, sqlite3_context *ctx, int i);
int ratiohistoRowid(sqlite3_vtab_cursor *cur, sqlite_int64 *pRowid);
int ratiohistoEof(sqlite3_vtab_cursor *cur);
int ratiohistoFilter(
  sqlite3_vtab_cursor *pVtabCursor,
  int idxNum, const char *idxStr,
  int argc, sqlite3_value **argv
  );
int ratiohistoBestIndex(sqlite3_vtab *tab, sqlite3_index_info *pIdxInfo);



extern sqlite3_module meanhistoModule;

int meanhistoConnect(
  sqlite3 *db,
  void *pAux,
  int argc, const char *const*argv,
  sqlite3_vtab **ppVtab,
  char **pzErr
  );
int meanhistoDisconnect(sqlite3_vtab *pVtab);
int meanhistoOpen(sqlite3_vtab *p, sqlite3_vtab_cursor **ppCursor);
int meanhistoClose(sqlite3_vtab_cursor *cur);
int meanhistoNext(sqlite3_vtab_cursor *cur);
int meanhistoColumn(sqlite3_vtab_cursor *cur, sqlite3_context *ctx, int i);
int meanhistoRowid(sqlite3_vtab_cursor *cur, sqlite_int64 *pRowid);
int meanhistoEof(sqlite3_vtab_cursor *cur);
int meanhistoFilter(
  sqlite3_vtab_cursor *pVtabCursor,
  int idxNum, const char *idxStr,
  int argc, sqlite3_value **argv
  );
int meanhistoBestIndex(sqlite3_vtab *tab, sqlite3_index_info *pIdxInfo);




#ifdef __cplusplus
}
#endif


#pragma once

