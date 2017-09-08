/*

scatinterpol.cpp, Robert Oeffner 2017
SQLite extension for calculating interpolated curve of one dimensional scatterplot 
of column values as well as corresponding standard deviations.


*/

#include <iostream>
#include <vector>
#include <cstdlib>

#include "RegistExt.h"
#include "helpers.h"
#include <assert.h>
#include <string.h>


#ifndef SQLITE_OMIT_VIRTUALTABLE




#ifdef __cplusplus
extern "C" {
#endif



std::vector<interpolatebin> myscatinterpolatebins;



/* scatinterpolate_cursor is a subclass of sqlite3_vtab_cursor which will
** serve as the underlying representation of a cursor that scans
** over rows of the result
*/
typedef struct scatinterpolate_cursor scatinterpolate_cursor;
struct scatinterpolate_cursor {
  sqlite3_vtab_cursor base;  /* Base class - must be first */
  int isDesc;                /* True to count down rather than up */
  sqlite3_int64 iRowid;      /* The rowid */
  double         x;
  double         y;
  double         sigma;
  sqlite3_int64  count;
  std::string    tblname;
  std::string    xcolid;
  std::string    ycolid;
  int            nbins;
  double         minbin;
  double         maxbin;
};



enum ColNum
{ // Column numbers. The order determines the order of columns in the table output
  SCATINTERPOL_X = 0,
  SCATINTERPOL_Y,
  SCATINTERPOL_SIGMA,
  SCATINTERPOL_COUNT,
  SCATINTERPOL_TBLNAME,
  SCATINTERPOL_XCOLID,
  SCATINTERPOL_YCOLID,
  SCATINTERPOL_NBINS,
  SCATINTERPOL_MINBIN,    
  SCATINTERPOL_MAXBIN    
};


int scatinterpolateConnect(
  sqlite3 *db,
  void *pAux,
  int argc, const char *const*argv,
  sqlite3_vtab **ppVtab,
  char **pzErr
){
  sqlite3_vtab *pNew;
  int rc;
/* The hidden columns serves as arguments to the SCATINTERPOL function as in:
SELECT * FROM SCATINTERPOLATE('tblname', 'xcolid', 'ycolid', nbins, minbin, maxbin);
They won't show up in the SQL tables.
*/
  rc = sqlite3_declare_vtab(db,
  "CREATE TABLE x(x REAL, y REAL, sigma REAL, count INTEGER, " \
  "tblname hidden, xcolid hidden, ycolid hidden, nbins hidden, minbin hidden, maxbin hidden)");
  if( rc==SQLITE_OK ){
    pNew = *ppVtab = (sqlite3_vtab *)sqlite3_malloc( sizeof(*pNew) );
    if( pNew==0 ) return SQLITE_NOMEM;
    memset(pNew, 0, sizeof(*pNew));
  }
  thisdb = db;
  return rc;
}

/*
** This method is the destructor for scatinterpolate_cursor objects.
*/
int scatinterpolateDisconnect(sqlite3_vtab *pVtab){
  sqlite3_free(pVtab);
  return SQLITE_OK;
}

/*
** Constructor for a new scatinterpolate_cursor object.
*/
int scatinterpolateOpen(sqlite3_vtab *p, sqlite3_vtab_cursor **ppCursor){
  scatinterpolate_cursor *pCur;
  pCur = (scatinterpolate_cursor *)sqlite3_malloc( sizeof(*pCur) );
  //pCur = sqlite3_malloc(sizeof(*pCur));
  if( pCur==0 ) return SQLITE_NOMEM;
  memset(pCur, 0, sizeof(*pCur));
  *ppCursor = &pCur->base;
  return SQLITE_OK;
}

/*
** Destructor for a scatinterpolate_cursor.
*/
int scatinterpolateClose(sqlite3_vtab_cursor *cur){
  sqlite3_free(cur);
  return SQLITE_OK;
}


/*
** Advance a scatinterpolate_cursor to its next row of output.
*/
int scatinterpolateNext(sqlite3_vtab_cursor *cur){
  scatinterpolate_cursor *pCur = (scatinterpolate_cursor*)cur;
  pCur->iRowid++;
  int i = pCur->iRowid - 1;
  pCur->x = myscatinterpolatebins[0].xval;
  pCur->y = myscatinterpolatebins[0].yval;
  pCur->sigma = myscatinterpolatebins[0].sigma;
  pCur->count = myscatinterpolatebins[0].count;
  return SQLITE_OK;
}

/*
** Return values of columns for the row at which the scatinterpolate_cursor
** is currently pointing.
*/
int scatinterpolateColumn(
  sqlite3_vtab_cursor *cur,   /* The cursor */
  sqlite3_context *ctx,       /* First argument to sqlite3_result_...() */
  int i                       /* Which column to return */
){
  scatinterpolate_cursor *pCur = (scatinterpolate_cursor*)cur;
  sqlite3_int64 x = 123456;
  std::string c = "waffle";
  double d = -42.24;
  switch( i ){
    case SCATINTERPOL_X:     d = pCur->x; sqlite3_result_double(ctx, d); break;
    case SCATINTERPOL_Y:     d = pCur->y; sqlite3_result_double(ctx, d); break;
    case SCATINTERPOL_SIGMA:     d = pCur->sigma; sqlite3_result_double(ctx, d); break;
    case SCATINTERPOL_COUNT:   x = pCur->count; sqlite3_result_int64(ctx, x); break;
    case SCATINTERPOL_TBLNAME: c = pCur->tblname; sqlite3_result_text(ctx, c.c_str(), -1, NULL);  break;
    case SCATINTERPOL_XCOLID:   c = pCur->xcolid; sqlite3_result_text(ctx, c.c_str(), -1, NULL); break;
    case SCATINTERPOL_YCOLID:   c = pCur->ycolid; sqlite3_result_text(ctx, c.c_str(), -1, NULL); break;
    case SCATINTERPOL_NBINS:    x = pCur->nbins; sqlite3_result_double(ctx, x); break;
    case SCATINTERPOL_MINBIN:  d = pCur->minbin; sqlite3_result_double(ctx, d); break;
    case SCATINTERPOL_MAXBIN:  d = pCur->maxbin; sqlite3_result_double(ctx, d); break;
    default:           sqlite3_result_double(ctx, 0); break;
  }
  return SQLITE_OK;
}

/*
** Return the rowid for the current row.  In this implementation, the
** rowid is the same as the output value.
*/
int scatinterpolateRowid(sqlite3_vtab_cursor *cur, sqlite_int64 *pRowid){
  scatinterpolate_cursor *pCur = (scatinterpolate_cursor*)cur;
  *pRowid = pCur->iRowid;
  return SQLITE_OK;
}

/*
** Return TRUE if the cursor has been moved off of the last
** row of output.
*/

int scatinterpolateEof(sqlite3_vtab_cursor *cur) {
  scatinterpolate_cursor *pCur = (scatinterpolate_cursor*)cur;
  if (pCur->isDesc) {
    return pCur->iRowid < 1;
  }
  else {
    return pCur->iRowid > myscatinterpolatebins.size();
  }
}



int scatinterpolateFilter(
  sqlite3_vtab_cursor *pVtabCursor, 
  int idxNum, const char *idxStr,
  int argc, sqlite3_value **argv
){
  scatinterpolate_cursor *pCur = (scatinterpolate_cursor *)pVtabCursor;
  int i = 0;
  pCur->tblname = "";
  pCur->xcolid = "";
  pCur->ycolid = "";
  pCur->nbins = 1.0;
  pCur->minbin = 1.0;
  pCur->maxbin = 1.0;

  if( idxNum >= SCATINTERPOL_MAXBIN)
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
    std::cerr << "Incorrect number of arguments for function SCATINTERPOL.\n" \
     "SCATINTERPOL must be called as:\n SCATINTERPOL('tablename', 'xcolumnname', 'ycolumnname', nbins, minbin, maxbin)" << std::endl;
    return SQLITE_ERROR;
  }
  
  std::string s_exe("SELECT ");
  s_exe += pCur->xcolid + ", " + pCur->ycolid + " FROM " + pCur->tblname;
  std::vector< std::vector<double> > myXYs = GetColumns(thisdb, s_exe);
  //myscatinterpolatebins = CalcHistogram(mybins, pCur->nbins, pCur->minbin, pCur->maxbin);
  pCur->x = myscatinterpolatebins[0].xval;
  pCur->y = myscatinterpolatebins[0].yval;
  pCur->sigma = myscatinterpolatebins[0].sigma;
  pCur->count = myscatinterpolatebins[0].count;
  pCur->isDesc = 0;
  pCur->iRowid = 1;

  return SQLITE_OK;
}


int scatinterpolateBestIndex(
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
  int nArg = 0;          /* Number of arguments that scatinterpolateFilter() expects */

  sqlite3_index_info::sqlite3_index_constraint *pConstraint;
  pConstraint = pIdxInfo->aConstraint;
  for(i=0; i<pIdxInfo->nConstraint; i++, pConstraint++){
    if( pConstraint->usable==0 ) continue;
    if( pConstraint->op!=SQLITE_INDEX_CONSTRAINT_EQ ) continue;
    switch( pConstraint->iColumn ){
      case SCATINTERPOL_TBLNAME:
        tblnameidx = i;
        idxNum = SCATINTERPOL_TBLNAME;
        break;
      case SCATINTERPOL_XCOLID:
        xcolididx = i;
        idxNum = SCATINTERPOL_XCOLID;
        break;
      case SCATINTERPOL_YCOLID:
        ycolididx = i;
        idxNum = SCATINTERPOL_YCOLID;
        break;
      case SCATINTERPOL_NBINS:
        binsidx = i;
        idxNum = SCATINTERPOL_NBINS;
        break;
      case SCATINTERPOL_MINBIN:
        minbinidx = i;
        idxNum = SCATINTERPOL_MINBIN;
        break;
      case SCATINTERPOL_MAXBIN:
        maxbinidx = i;
        idxNum = SCATINTERPOL_MAXBIN;
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
** generate_scatinterpolate virtual table.
*/
sqlite3_module scatinterpolateModule = {
  0,                         /* iVersion */
  0,                         /* xCreate */
  scatinterpolateConnect,             /* xConnect */
  scatinterpolateBestIndex,           /* xBestIndex */
  scatinterpolateDisconnect,          /* xDisconnect */
  0,                         /* xDestroy */
  scatinterpolateOpen,                /* xOpen - open a cursor */
  scatinterpolateClose,               /* xClose - close a cursor */
  scatinterpolateFilter,              /* xFilter - configure scan constraints */
  scatinterpolateNext,                /* xNext - advance a cursor */
  scatinterpolateEof,                 /* xEof - check for end of scan */
  scatinterpolateColumn,              /* xColumn - read data */
  scatinterpolateRowid,               /* xRowid - read data */
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
