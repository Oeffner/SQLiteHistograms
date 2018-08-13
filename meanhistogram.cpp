/*
meanhistogram.cpp, Robert Oeffner 2017

SQLite extension for calculating interpolated curve of one dimensional scatterplot 
of column values as well as corresponding standard deviations.

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

#include <iostream>
#include <vector>
#include <cstdlib>

#include "RegistExt.h"
#include "helpers.h"
#include <assert.h>
#include <memory.h>


#ifndef SQLITE_OMIT_VIRTUALTABLE




#ifdef __cplusplus
extern "C" {
#endif



std::vector<interpolatebin> mymeanhistobins;



/* meanhisto_cursor is a subclass of sqlite3_vtab_cursor which will
** serve as the underlying representation of a cursor that scans
** over rows of the result
*/
typedef struct meanhisto_cursor meanhisto_cursor;
struct meanhisto_cursor {
  sqlite3_vtab_cursor base;  /* Base class - must be first */
  int isDesc;                /* True to count down rather than up */
  sqlite3_int64 iRowid;      /* The rowid */
  double         x;
  double         y;
  double         sigma;
  double         sem;
  sqlite3_int64  count;
  std::string    tblname;
  std::string    xcolid;
  std::string    ycolid;
  int            nbins;
  double         minbin;
  double         maxbin;
};



enum ColNum
{ /* Column numbers. The order determines the order of columns in the table output
  and must match the order of columns in the CREATE TABLE statement below
  */
  MEANHISTO_X = 0,
  MEANHISTO_Y,
  MEANHISTO_SIGMA,
  MEANHISTO_SEM,
  MEANHISTO_COUNT,
  MEANHISTO_TBLNAME,
  MEANHISTO_XCOLID,
  MEANHISTO_YCOLID,
  MEANHISTO_NBINS,
  MEANHISTO_MINBIN,    
  MEANHISTO_MAXBIN    
};


int meanhistoConnect(
  sqlite3 *db,
  void *pAux,
  int argc, const char *const*argv,
  sqlite3_vtab **ppVtab,
  char **pzErr
)
{
  sqlite3_vtab *pNew;
  int rc;
/* The hidden columns serves as arguments to the MEANHISTO function as in:
SELECT * FROM MEANHISTO('tblname', 'xcolid', 'ycolid', nbins, minbin, maxbin);
They won't show up in the SQL tables.
*/
  rc = sqlite3_declare_vtab(db,
// Order of columns MUST match the order of the above enum ColNum
  "CREATE TABLE x(xbin REAL, yval REAL, sigma REAL, sem REAL, bincount INTEGER, " \
  "tblname hidden, xcolid hidden, ycolid hidden, nbins hidden, minbin hidden, maxbin hidden)");
  if( rc==SQLITE_OK )
  {
    pNew = *ppVtab = (sqlite3_vtab *)sqlite3_malloc( sizeof(*pNew) );
    if( pNew==0 ) return SQLITE_NOMEM;
    memset(pNew, 0, sizeof(*pNew));
  }
  thisdb = db;
  return rc;
}

/*
** This method is the destructor for meanhisto_cursor objects.
*/
int meanhistoDisconnect(sqlite3_vtab *pVtab){
  sqlite3_free(pVtab);
  return SQLITE_OK;
}

/*
** Constructor for a new meanhisto_cursor object.
*/
int meanhistoOpen(sqlite3_vtab *p, sqlite3_vtab_cursor **ppCursor){
  meanhisto_cursor *pCur;
  // allocate c++ object with new rather than sqlite3_malloc which doesn't call constructors
  pCur = new meanhisto_cursor;
  if (pCur == NULL) return SQLITE_NOMEM;
  *ppCursor = &pCur->base;
  return SQLITE_OK;
}

/*
** Destructor for a meanhisto_cursor.
*/
int meanhistoClose(sqlite3_vtab_cursor *cur){
  delete cur;
  return SQLITE_OK;
}


/*
** Advance a meanhisto_cursor to its next row of output.
*/
int meanhistoNext(sqlite3_vtab_cursor *cur){
  meanhisto_cursor *pCur = (meanhisto_cursor*)cur;
  pCur->iRowid++;
  int i = pCur->iRowid - 1;
  pCur->x = mymeanhistobins[i].xval;
  pCur->y = mymeanhistobins[i].yval;
  pCur->sigma = mymeanhistobins[i].sigma;
  pCur->sem = mymeanhistobins[i].sem;
  pCur->count = mymeanhistobins[i].count;
  return SQLITE_OK;
}

/*
** Return values of columns for the row at which the meanhisto_cursor
** is currently pointing.
*/
int meanhistoColumn(
  sqlite3_vtab_cursor *cur,   /* The cursor */
  sqlite3_context *ctx,       /* First argument to sqlite3_result_...() */
  int i                       /* Which column to return */
){
  meanhisto_cursor *pCur = (meanhisto_cursor*)cur;
  sqlite3_int64 x = 123456;
  std::string c = "waffle";
  double d = -42.24;
  switch( i ){
    case MEANHISTO_X:     d = pCur->x; sqlite3_result_double(ctx, d); break;
    case MEANHISTO_Y:     d = pCur->y; sqlite3_result_double(ctx, d); break;
    case MEANHISTO_SIGMA:     d = pCur->sigma; sqlite3_result_double(ctx, d); break;
    case MEANHISTO_SEM:     d = pCur->sem; sqlite3_result_double(ctx, d); break;
    case MEANHISTO_COUNT:   x = pCur->count; sqlite3_result_int64(ctx, x); break;
    case MEANHISTO_TBLNAME: c = pCur->tblname; sqlite3_result_text(ctx, c.c_str(), -1, NULL);  break;
    case MEANHISTO_XCOLID:   c = pCur->xcolid; sqlite3_result_text(ctx, c.c_str(), -1, NULL); break;
    case MEANHISTO_YCOLID:   c = pCur->ycolid; sqlite3_result_text(ctx, c.c_str(), -1, NULL); break;
    case MEANHISTO_NBINS:    x = pCur->nbins; sqlite3_result_double(ctx, x); break;
    case MEANHISTO_MINBIN:  d = pCur->minbin; sqlite3_result_double(ctx, d); break;
    case MEANHISTO_MAXBIN:  d = pCur->maxbin; sqlite3_result_double(ctx, d); break;
    default:           sqlite3_result_double(ctx, 0); break;
  }
  return SQLITE_OK;
}

/*
** Return the rowid for the current row.  In this implementation, the
** rowid is the same as the output value.
*/
int meanhistoRowid(sqlite3_vtab_cursor *cur, sqlite_int64 *pRowid){
  meanhisto_cursor *pCur = (meanhisto_cursor*)cur;
  *pRowid = pCur->iRowid;
  return SQLITE_OK;
}

/*
** Return TRUE if the cursor has been moved off of the last
** row of output.
*/

int meanhistoEof(sqlite3_vtab_cursor *cur) {
  meanhisto_cursor *pCur = (meanhisto_cursor*)cur;
  if (pCur->isDesc) {
    return pCur->iRowid < 1;
  }
  else {
    return pCur->iRowid > mymeanhistobins.size();
  }
}



int meanhistoFilter(
  sqlite3_vtab_cursor *pVtabCursor, 
  int idxNum, const char *idxStr,
  int argc, sqlite3_value **argv
){
  meanhisto_cursor *pCur = (meanhisto_cursor *)pVtabCursor;
  int i = 0, rc = SQLITE_OK;
  pCur->tblname = "";
  pCur->xcolid = "";
  pCur->ycolid = "";
  pCur->nbins = 1.0;
  pCur->minbin = 1.0;
  pCur->maxbin = 1.0;

  if( idxNum >= MEANHISTO_MAXBIN)
  {
    pCur->tblname = (const char*)sqlite3_value_text(argv[i++]);
    pCur->xcolid = (const char*)sqlite3_value_text(argv[i++]);
    pCur->ycolid = (const char*)sqlite3_value_text(argv[i++]);
    pCur->nbins = sqlite3_value_double(argv[i++]);
    pCur->minbin = sqlite3_value_double(argv[i++]);
    pCur->maxbin = sqlite3_value_double(argv[i++]);
  }
  else 
  {
    const char *zText = "Incorrect arguments for function MEANHISTO which must be called as:\n" \
     " MEANHISTO('tablename', 'xcolumnname', 'ycolumnname', nbins, minbin, maxbin)\n";
    pCur->base.pVtab->zErrMsg = sqlite3_mprintf(zText);
    return SQLITE_ERROR;
  }
  
  std::string s_exe("SELECT ");
  s_exe += pCur->xcolid + ", " + pCur->ycolid + " FROM " + pCur->tblname;
  std::vector< std::vector<double> > myXYs = GetColumns(thisdb, s_exe, &rc);
	if (rc != SQLITE_OK)
	{
    pCur->base.pVtab->zErrMsg = sqlite3_mprintf(sqlite3_errmsg(thisdb));
		return rc;
	}
	mymeanhistobins = CalcInterpolations(myXYs, pCur->nbins, pCur->minbin, pCur->maxbin, &rc);
	if (rc != SQLITE_OK)
		return rc;

  pCur->x = mymeanhistobins[0].xval;
  pCur->y = mymeanhistobins[0].yval;
  pCur->sigma = mymeanhistobins[0].sigma;
  pCur->sem = mymeanhistobins[0].sem;
  pCur->count = mymeanhistobins[0].count;
  pCur->isDesc = 0;
  pCur->iRowid = 1;

  return SQLITE_OK;
}


int meanhistoBestIndex(
  sqlite3_vtab *tab,
  sqlite3_index_info *pIdxInfo
){
  int i;                 /* Loop over constraints */
  int idxNum = 0;        /* The query plan bitmask */
  int tblnameidx = -1;     /* Index of the start= constraint, or -1 if none */
  int xcolididx = -1;      /* Index of the stop= constraint, or -1 if none */
  int ycolididx = -1;      /* Index of the stop= constraint, or -1 if none */
  int binsidx = -1;      /* Index of the step= constraint, or -1 if none */
  int minbinidx = -1;
  int maxbinidx = -1;
  int nArg = 0;          /* Number of arguments that meanhistoFilter() expects */

  sqlite3_index_info::sqlite3_index_constraint *pConstraint;
  pConstraint = pIdxInfo->aConstraint;
  for(i=0; i<pIdxInfo->nConstraint; i++, pConstraint++){
    if( pConstraint->usable==0 ) continue;
    if( pConstraint->op!=SQLITE_INDEX_CONSTRAINT_EQ ) continue;
    switch( pConstraint->iColumn ){
      case MEANHISTO_TBLNAME:
        tblnameidx = i;
        idxNum = MEANHISTO_TBLNAME;
        break;
      case MEANHISTO_XCOLID:
        xcolididx = i;
        idxNum = MEANHISTO_XCOLID;
        break;
      case MEANHISTO_YCOLID:
        ycolididx = i;
        idxNum = MEANHISTO_YCOLID;
        break;
      case MEANHISTO_NBINS:
        binsidx = i;
        idxNum = MEANHISTO_NBINS;
        break;
      case MEANHISTO_MINBIN:
        minbinidx = i;
        idxNum = MEANHISTO_MINBIN;
        break;
      case MEANHISTO_MAXBIN:
        maxbinidx = i;
        idxNum = MEANHISTO_MAXBIN;
        break;
    }
  }
  if(tblnameidx >=0 ){
    pIdxInfo->aConstraintUsage[tblnameidx].argvIndex = ++nArg;
    pIdxInfo->aConstraintUsage[tblnameidx].omit= 1;
  }
  if (xcolididx >= 0) {
    pIdxInfo->aConstraintUsage[xcolididx].argvIndex = ++nArg;
    pIdxInfo->aConstraintUsage[xcolididx].omit = 1;
  }
  if (ycolididx >= 0) {
    pIdxInfo->aConstraintUsage[ycolididx].argvIndex = ++nArg;
    pIdxInfo->aConstraintUsage[ycolididx].omit = 1;
  }
  if (binsidx >= 0) {
    pIdxInfo->aConstraintUsage[binsidx].argvIndex = ++nArg;
    pIdxInfo->aConstraintUsage[binsidx].omit = 1;
  }
  if (minbinidx >= 0) {
    pIdxInfo->aConstraintUsage[minbinidx].argvIndex = ++nArg;
    pIdxInfo->aConstraintUsage[minbinidx].omit = 1;
  }
  if (maxbinidx >= 0) {
    pIdxInfo->aConstraintUsage[maxbinidx].argvIndex = ++nArg;
    pIdxInfo->aConstraintUsage[maxbinidx].omit = 1;
  }
  pIdxInfo->estimatedCost = 2.0;
  pIdxInfo->estimatedRows = 500;
  if( pIdxInfo->nOrderBy==1 )
  {
    pIdxInfo->orderByConsumed = 1;
  }
  pIdxInfo->idxNum = idxNum;
  return SQLITE_OK;
}




/*
** This following structure defines all the methods for the
** generate_meanhisto virtual table.
*/
sqlite3_module meanhistoModule = {
  0,                         /* iVersion */
  0,                         /* xCreate */
  meanhistoConnect,             /* xConnect */
  meanhistoBestIndex,           /* xBestIndex */
  meanhistoDisconnect,          /* xDisconnect */
  0,                         /* xDestroy */
  meanhistoOpen,                /* xOpen - open a cursor */
  meanhistoClose,               /* xClose - close a cursor */
  meanhistoFilter,              /* xFilter - configure scan constraints */
  meanhistoNext,                /* xNext - advance a cursor */
  meanhistoEof,                 /* xEof - check for end of scan */
  meanhistoColumn,              /* xColumn - read data */
  meanhistoRowid,               /* xRowid - read data */
  0,                         /* xUpdate */
  0,                         /* xBegin */
  0,                         /* xSync */
  0,                         /* xCommit */
  0,                         /* xRollback */
  0,                         /* xFindMethod */
  0,                         /* xRename */
};



#endif /* SQLITE_OMIT_VIRTUALTABLE */




#ifdef __cplusplus
}
#endif
