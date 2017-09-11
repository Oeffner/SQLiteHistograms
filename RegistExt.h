
#include "sqlite3ext.h"



#ifdef __cplusplus
extern "C" {
#endif

extern const sqlite3_api_routines *sqlite3_api;

extern sqlite3 *thisdb;

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

