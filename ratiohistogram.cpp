/*
ratiohistogram.cpp, Robert Oeffner 2017

SQLite extension for calculating ratiohistogram of column values as well as calculating ratios of 
two histograms deduced from the same table.

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

  /* histo_cursor is a subclass of sqlite3_vtab_cursor which will
** serve as the underlying representation of a cursor that scans
** over rows of the result
*/
typedef struct ratiohisto_cursor ratiohisto_cursor;
struct ratiohisto_cursor {
  sqlite3_vtab_cursor base;  /* Base class - must be first */
  int isDesc;                /* True to count down rather than up */
  sqlite3_int64 iRowid;      /* The rowid */
  double         bin;
  sqlite3_int64  count1;
  sqlite3_int64  count2;
  double         ratio;
  sqlite3_int64  totalcount;
  std::string    tblname;
  std::string    colid;
  int            nbins;
  double         minbin;
  double         maxbin;
  std::string    discrcolid;
  std::string    discrval;
  std::vector<histobin> ratiohistogram1;
  std::vector<histobin> ratiohistogram2;
};



enum ColNum
{ /* Column numbers. The order determines the order of columns in the table output
  and must match the order of columns in the CREATE TABLE statement below
  */
  RATIOHISTO_BIN = 0,
  RATIOHISTO_COUNT1,
  RATIOHISTO_COUNT2, 
  RATIOHISTO_RATIO,     
  RATIOHISTO_TOTALCOUNT,
  RATIOHISTO_TBLNAME,   
  RATIOHISTO_COLID,     
  RATIOHISTO_NBINS,     
  RATIOHISTO_MINBIN,    
  RATIOHISTO_MAXBIN,    
  RATIOHISTO_DISCRCOLID,
  RATIOHISTO_DISCRVAL  
};


/*
** The ratiohistoConnect() method is invoked to create a new
** ratiohisto_vtab that describes the generate_ratiohisto virtual table.
** As the ratiohistoCreate method is set to NULL this virtual table is
** an Eponymous-only virtual table, i.e. useful as a table-valued function.
** The hidden columns are the arguments to the function and won't show up 
** in the SQL tables.
** Think of this routine as the constructor for ratiohisto_vtab objects.
**
** All this routine needs to do is:
**
**    (1) Allocate the ratiohisto_vtab object and initialize all fields.
**
**    (2) Tell SQLite (via the sqlite3_declare_vtab() interface) what the
**        result set of queries against generate_ratiohisto will look like.
*/
int ratiohistoConnect(
  sqlite3 *db,
  void *pAux,
  int argc, const char *const*argv,
  sqlite3_vtab **ppVtab,
  char **pzErr
)
{
  sqlite3_vtab *pNew;
  int rc;
  // The hidden columns serves as arguments to the RATIOHISTO function as in:
  // SELECT * FROM RATIOHISTO('tblname', 'colid', nbins, minbin, maxbin, 'discrcolid', discrval);
  // They won't show up in the SQL tables.
  rc = sqlite3_declare_vtab(db,
    // Order of columns MUST match the order of the above enum ColNum
    "CREATE TABLE x(bin REAL, count1 INTEGER, count2 INTEGER, ratio REAL, totalcount INTEGER, " \
  "tblname hidden, colid hidden, nbins hidden, minbin hidden, maxbin hidden, discrcolid hidden, discrval hidden)");
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
** This method is the destructor for ratiohisto_cursor objects.
*/
int ratiohistoDisconnect(sqlite3_vtab *pVtab){
  sqlite3_free(pVtab);
  return SQLITE_OK;
}

/*
** Constructor for a new ratiohisto_cursor object.
*/
int ratiohistoOpen(sqlite3_vtab *p, sqlite3_vtab_cursor **ppCursor){
  ratiohisto_cursor *pCur;
  // allocate c++ object with new rather than sqlite3_malloc which doesn't call constructors
  pCur = new ratiohisto_cursor;
  if (pCur == NULL) return SQLITE_NOMEM;
  *ppCursor = &pCur->base;
  return SQLITE_OK;
}

/*
** Destructor for a ratiohisto_cursor.
*/
int ratiohistoClose(sqlite3_vtab_cursor *cur){
  delete cur;
  return SQLITE_OK;
}


/*
** Advance a ratiohisto_cursor to its next row of output.
*/
int ratiohistoNext(sqlite3_vtab_cursor *cur){
  ratiohisto_cursor *pCur = (ratiohisto_cursor*)cur;
  pCur->iRowid++;
  int i = pCur->iRowid - 1;
  pCur->bin = pCur->ratiohistogram1[i].binval;
  pCur->count1 = pCur->ratiohistogram1[i].count;
  pCur->count2 = pCur->ratiohistogram2[i].count;
  pCur->totalcount = pCur->ratiohistogram1[i].count + pCur->ratiohistogram2[i].count;
  if (pCur->totalcount > 0)
    pCur->ratio = ((double) pCur->count1 ) / pCur->totalcount;
  else
    pCur->ratio = 0.0;
  return SQLITE_OK;
}

/*
** Return values of columns for the row at which the ratiohisto_cursor
** is currently pointing.
*/
int ratiohistoColumn(
  sqlite3_vtab_cursor *cur,   /* The cursor */
  sqlite3_context *ctx,       /* First argument to sqlite3_result_...() */
  int i                       /* Which column to return */
){
  ratiohisto_cursor *pCur = (ratiohisto_cursor*)cur;
  sqlite3_int64 x = 123456;
  std::string c = "waffle";
  double d = -42.24;
  switch( i ){
    case RATIOHISTO_BIN:     d = pCur->bin; sqlite3_result_double(ctx, d); break;
    case RATIOHISTO_COUNT1:   x = pCur->count1; sqlite3_result_int64(ctx, x); break;
    case RATIOHISTO_COUNT2: {
      if (pCur->discrcolid == "") 
      { // if not computing ratio ratiohistogram then return empty column values
        sqlite3_result_null(ctx);
      }
      else
      {
        x = pCur->count2; sqlite3_result_int64(ctx, x);
      } 
    } break;

    case RATIOHISTO_RATIO: {
      if (pCur->discrcolid == "")
      { // if not computing ratio ratiohistogram then return empty column values
        sqlite3_result_null(ctx);
      }
      else
      {
        d = pCur->ratio; sqlite3_result_double(ctx, d);
      }
    } break;

    case RATIOHISTO_TOTALCOUNT: {
      if (pCur->discrcolid == "")
      { // if not computing ratio ratiohistogram then return empty column values
        sqlite3_result_null(ctx);
      }
      else
      {
        x = pCur->totalcount; sqlite3_result_int64(ctx, x);
      }
    } break;

    case RATIOHISTO_TBLNAME: c = pCur->tblname; sqlite3_result_text(ctx, c.c_str(), -1, NULL);  break;
    case RATIOHISTO_COLID:   c = pCur->colid; sqlite3_result_text(ctx, c.c_str(), -1, NULL); break;
    case RATIOHISTO_NBINS:    x = pCur->nbins; sqlite3_result_double(ctx, x); break;
    case RATIOHISTO_MINBIN:  d = pCur->minbin; sqlite3_result_double(ctx, d); break;
    case RATIOHISTO_MAXBIN:  d = pCur->maxbin; sqlite3_result_double(ctx, d); break;
    case RATIOHISTO_DISCRCOLID:  c = pCur->discrcolid; sqlite3_result_text(ctx, c.c_str(), -1, NULL);  break;
    case RATIOHISTO_DISCRVAL:  c = pCur->discrval; sqlite3_result_text(ctx, c.c_str(), -1, NULL);  break;
    default:            x = pCur->count1; sqlite3_result_int64(ctx, x); break;
  }
  return SQLITE_OK;
}

/*
** Return the rowid for the current row.  In this implementation, the
** rowid is the same as the output value.
*/
int ratiohistoRowid(sqlite3_vtab_cursor *cur, sqlite_int64 *pRowid){
  ratiohisto_cursor *pCur = (ratiohisto_cursor*)cur;
  *pRowid = pCur->iRowid;
  return SQLITE_OK;
}

/*
** Return TRUE if the cursor has been moved off of the last
** row of output.
*/

int ratiohistoEof(sqlite3_vtab_cursor *cur) {
  ratiohisto_cursor *pCur = (ratiohisto_cursor*)cur;
  if (pCur->isDesc) {
    return pCur->iRowid < 1;
  }
  else {
    return pCur->iRowid > pCur->ratiohistogram1.size();
  }
}


/*
** This method is called to "rewind" the ratiohisto_cursor object back
** to the first row of output.  This method is always called at least
** once prior to any call to ratiohistoColumn() or ratiohistoRowid() or 
** ratiohistoEof().
**
** The query plan selected by ratiohistoBestIndex is passed in the idxNum
** parameter.  (idxStr is not used in this implementation.)  idxNum
** is one of the enum ColNum values above.
** This routine should initialize the cursor and position it so that it
** is pointing at the first row, or pointing off the end of the table
** (so that ratiohistoEof() will return true) if the table is empty.
*/
int ratiohistoFilter(
  sqlite3_vtab_cursor *pVtabCursor, 
  int idxNum, const char *idxStr,
  int argc, sqlite3_value **argv
){
  ratiohisto_cursor *pCur = (ratiohisto_cursor *)pVtabCursor;
  int i = 0, rc = SQLITE_OK;
  pCur->tblname = "";
  pCur->colid = "";
  pCur->nbins = 1.0;
  pCur->minbin = 1.0;
  pCur->maxbin = 1.0;
  pCur->discrcolid = "";
  pCur->discrval = "0.0";

  if (idxNum >= RATIOHISTO_DISCRVAL)
  {
    pCur->tblname = (const char*)sqlite3_value_text(argv[i++]);
    pCur->colid = (const char*)sqlite3_value_text(argv[i++]);
    pCur->nbins = sqlite3_value_double(argv[i++]);
    pCur->minbin = sqlite3_value_double(argv[i++]);
    pCur->maxbin = sqlite3_value_double(argv[i++]);
    pCur->discrcolid = (const char*)sqlite3_value_text(argv[i++]);
    pCur->discrval = (const char*)sqlite3_value_text(argv[i++]);
  }
  else 
  {
    const char *zText = "Incorrect arguments for function RATIOHISTO which must be called as:\n" \
      "RATIOHISTO('tablename', 'columnname', nbins, minbin, maxbin, 'discrcolid', discrval)\n";
    pCur->base.pVtab->zErrMsg = sqlite3_mprintf(zText);
    return SQLITE_ERROR;
  }
  
  std::vector< std::vector<double> > mybins;
  std::string s_exe("SELECT ");
  s_exe += pCur->colid + " FROM " + pCur->tblname;
  mybins.clear();
	mybins = GetColumns(thisdb, s_exe, &rc);
	if (rc != SQLITE_OK)
	{
    pCur->base.pVtab->zErrMsg = sqlite3_mprintf(sqlite3_errmsg(thisdb));
		return rc;
	}
  pCur->ratiohistogram1 = CalcHistogram(mybins, pCur->nbins, pCur->minbin, pCur->maxbin, &rc);
	if (rc != SQLITE_OK)
		return rc;
  pCur->ratiohistogram2.resize(pCur->nbins);
  pCur->totalcount = pCur->ratiohistogram1[0].count;
  pCur->ratio = 0.0;

  if (pCur->discrcolid != "") // make two ratiohistograms for values above and below discrval
  {
    std::string s_exe("SELECT "); // get first ratiohistogram where values are above discrval
    s_exe += pCur->colid + " FROM " + pCur->tblname
      + " WHERE " + pCur->discrcolid + " >= " + pCur->discrval;
    mybins.clear();
		mybins = GetColumns(thisdb, s_exe, &rc);
		if (rc != SQLITE_OK)
		{
      pCur->base.pVtab->zErrMsg = sqlite3_mprintf(sqlite3_errmsg(thisdb));
			return rc;
		}
    pCur->ratiohistogram1 = CalcHistogram(mybins, pCur->nbins, pCur->minbin, pCur->maxbin, &rc);
		if (rc != SQLITE_OK)
			return rc;

    // get second ratiohistogram where values are below discrval
    s_exe = "SELECT " + pCur->colid + " FROM " + pCur->tblname
      + " WHERE " + pCur->discrcolid + " < " + pCur->discrval;
    mybins.clear();
		mybins = GetColumns(thisdb, s_exe, &rc);
		if (rc != SQLITE_OK)
		{
      pCur->base.pVtab->zErrMsg = sqlite3_mprintf(sqlite3_errmsg(thisdb));
			return rc;
		}
    pCur->ratiohistogram2 = CalcHistogram(mybins, pCur->nbins, pCur->minbin, pCur->maxbin, &rc);
		if (rc != SQLITE_OK)
			return rc;
  }
  pCur->bin = pCur->ratiohistogram1[0].binval;
  pCur->count1 = pCur->ratiohistogram1[0].count;
  pCur->count2 = pCur->ratiohistogram2[0].count;
  pCur->totalcount = pCur->ratiohistogram1[0].count + pCur->ratiohistogram2[0].count;
  if (pCur->totalcount > 0)
    pCur->ratio = pCur->count1 / pCur->totalcount;

  pCur->isDesc = 0;
  pCur->iRowid = 1;

  return rc;
}


/*
** SQLite will invoke this method one or more times while planning a query
** that uses the generate_ratiohisto virtual table.  This routine needs to create
** a query plan for each invocation and compute an estimated cost for that
** plan.
**
** In this implementation idxNum is used to represent the
** query plan.  idxStr is unused.
**
** The query plan is represented by bits in idxNum:
**
**  (1)  start = $value  -- constraint exists
**  (2)  stop = $value   -- constraint exists
**  (4)  step = $value   -- constraint exists
**  (8)  output in descending order
*/
int ratiohistoBestIndex(
  sqlite3_vtab *tab,
  sqlite3_index_info *pIdxInfo
){
  int i;                 /* Loop over constraints */
  int idxNum = 0;        /* The query plan bitmask */
  int tblnameidx = -1;     /* Index of the start= constraint, or -1 if none */
  int colididx = -1;      /* Index of the stop= constraint, or -1 if none */
  int binsidx = -1;      /* Index of the step= constraint, or -1 if none */
  int minbinidx = -1;
  int maxbinidx = -1;
  int discrcolididx = -1;
  int discrvalidx = -1;
  int nArg = 0;          /* Number of arguments that ratiohistoFilter() expects */

  sqlite3_index_info::sqlite3_index_constraint *pConstraint;
  pConstraint = pIdxInfo->aConstraint;
  for(i=0; i<pIdxInfo->nConstraint; i++, pConstraint++){
    if( pConstraint->usable==0 ) continue;
    if( pConstraint->op!=SQLITE_INDEX_CONSTRAINT_EQ ) continue;
    switch( pConstraint->iColumn ){
      case RATIOHISTO_TBLNAME:
        tblnameidx = i;
        idxNum = RATIOHISTO_TBLNAME;
        break;
      case RATIOHISTO_COLID:
        colididx = i;
        idxNum = RATIOHISTO_COLID;
        break;
      case RATIOHISTO_NBINS:
        binsidx = i;
        idxNum = RATIOHISTO_NBINS;
        break;
      case RATIOHISTO_MINBIN:
        minbinidx = i;
        idxNum = RATIOHISTO_MINBIN;
        break;
      case RATIOHISTO_MAXBIN:
        maxbinidx = i;
        idxNum = RATIOHISTO_MAXBIN;
        break;
      case RATIOHISTO_DISCRCOLID:
        discrcolididx = i;
        idxNum = RATIOHISTO_DISCRCOLID;
        break;
      case RATIOHISTO_DISCRVAL:
        discrvalidx = i;
        idxNum = RATIOHISTO_DISCRVAL;
        break;
    }
  }
  if(tblnameidx >=0 ){
    pIdxInfo->aConstraintUsage[tblnameidx].argvIndex = ++nArg;
    pIdxInfo->aConstraintUsage[tblnameidx].omit= 1;
  }
  if(colididx >=0 ){
    pIdxInfo->aConstraintUsage[colididx].argvIndex = ++nArg;
    pIdxInfo->aConstraintUsage[colididx].omit = 1;
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
  if (discrcolididx >= 0) {
    pIdxInfo->aConstraintUsage[discrcolididx].argvIndex = ++nArg;
    pIdxInfo->aConstraintUsage[discrcolididx].omit = 1;
  }
  if (discrvalidx >= 0) {
    pIdxInfo->aConstraintUsage[discrvalidx].argvIndex = ++nArg;
    pIdxInfo->aConstraintUsage[discrvalidx].omit = 1;
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
** generate_ratiohisto virtual table.
*/
sqlite3_module ratiohistoModule = {
  0,                         /* iVersion */
  0,                         /* xCreate */
  ratiohistoConnect,             /* xConnect */
  ratiohistoBestIndex,           /* xBestIndex */
  ratiohistoDisconnect,          /* xDisconnect */
  0,                         /* xDestroy */
  ratiohistoOpen,                /* xOpen - open a cursor */
  ratiohistoClose,               /* xClose - close a cursor */
  ratiohistoFilter,              /* xFilter - configure scan constraints */
  ratiohistoNext,                /* xNext - advance a cursor */
  ratiohistoEof,                 /* xEof - check for end of scan */
  ratiohistoColumn,              /* xColumn - read data */
  ratiohistoRowid,               /* xRowid - read data */
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
