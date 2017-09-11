/*

histogram.cpp, Robert Oeffner 2017
SQLite extension for calculating histogram of column values as well as calculating ratios of 
two histograms deduced from the same table.


Compiles with command line:

On Windows with MSVC++:

cl /Gd ..\MySqliteExtentions\histogram.cpp /I sqlite3 /DDLL /EHsc /LD /link /export:sqlite3_histogram_init /out:histogram.sqlext
For debugging:
cl /Gd ..\MySqliteExtentions\histogram.cpp /I sqlite3 /DDEBUG  /ZI /DDLL /EHsc /LD /link /debugtype:cv /export:sqlite3_histogram_init /out:histogram.sqlext

On Linux with g++:

g++ -fPIC -lm -shared histogram.cpp -o libhistogram.so

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



std::vector<histobin> myhistogram1;



/* histo_cursor is a subclass of sqlite3_vtab_cursor which will
** serve as the underlying representation of a cursor that scans
** over rows of the result
*/
typedef struct histo_cursor histo_cursor;
struct histo_cursor {
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
};



enum ColNum
{ // Column numbers. The order determines the order of columns in the table output
  HISTO_BIN = 0,
  HISTO_COUNT1,
  HISTO_COUNT2,
  HISTO_TBLNAME,   
  HISTO_COLID,     
  HISTO_NBINS,     
  HISTO_MINBIN,    
  HISTO_MAXBIN    
};


/*
** The histoConnect() method is invoked to create a new
** histo_vtab that describes the generate_histo virtual table.
** As the histoCreate method is set to NULL this virtual table is
** an Eponymous-only virtual table, i.e. useful as a table-valued function.
** The hidden columns are the arguments to the function and won't show up 
** in the SQL tables.
** Think of this routine as the constructor for histo_vtab objects.
**
** All this routine needs to do is:
**
**    (1) Allocate the histo_vtab object and initialize all fields.
**
**    (2) Tell SQLite (via the sqlite3_declare_vtab() interface) what the
**        result set of queries against generate_histo will look like.
*/
int histoConnect(
  sqlite3 *db,
  void *pAux,
  int argc, const char *const*argv,
  sqlite3_vtab **ppVtab,
  char **pzErr
){
  sqlite3_vtab *pNew;
  int rc;
  // The hidden columns serves as arguments to the HISTO function as in:
  // SELECT * FROM HISTO('tblname', 'colid', nbins, minbin, maxbin, 'discrcolid', discrval);
  // They won't show up in the SQL tables.
  rc = sqlite3_declare_vtab(db,
    // Order of columns MUST match the order of the above enum ColNum
    "CREATE TABLE x(bin REAL, bincount INTEGER, accumcount INTEGER, " \
  "tblname hidden, colid hidden, nbins hidden, minbin hidden, maxbin hidden)");
  if( rc==SQLITE_OK ){
    pNew = *ppVtab = (sqlite3_vtab *)sqlite3_malloc( sizeof(*pNew) );
    if( pNew==0 ) return SQLITE_NOMEM;
    memset(pNew, 0, sizeof(*pNew));
  }
  thisdb = db;
  return rc;
}

/*
** This method is the destructor for histo_cursor objects.
*/
int histoDisconnect(sqlite3_vtab *pVtab){
  sqlite3_free(pVtab);
  return SQLITE_OK;
}

/*
** Constructor for a new histo_cursor object.
*/
int histoOpen(sqlite3_vtab *p, sqlite3_vtab_cursor **ppCursor){
  histo_cursor *pCur;
  pCur = (histo_cursor *)sqlite3_malloc( sizeof(*pCur) );
  //pCur = sqlite3_malloc(sizeof(*pCur));
  if( pCur==0 ) return SQLITE_NOMEM;
  memset(pCur, 0, sizeof(*pCur));
  *ppCursor = &pCur->base;
  return SQLITE_OK;
}

/*
** Destructor for a histo_cursor.
*/
int histoClose(sqlite3_vtab_cursor *cur){
  sqlite3_free(cur);
  return SQLITE_OK;
}


/*
** Advance a histo_cursor to its next row of output.
*/
int histoNext(sqlite3_vtab_cursor *cur){
  histo_cursor *pCur = (histo_cursor*)cur;
  pCur->iRowid++;
  int i = pCur->iRowid - 1;
  pCur->bin = myhistogram1[i].binval; 
  pCur->count1 = myhistogram1[i].count;
  pCur->count2 = myhistogram1[i].accumcount;
  return SQLITE_OK;
}

/*
** Return values of columns for the row at which the histo_cursor
** is currently pointing.
*/
int histoColumn(
  sqlite3_vtab_cursor *cur,   /* The cursor */
  sqlite3_context *ctx,       /* First argument to sqlite3_result_...() */
  int i                       /* Which column to return */
){
  histo_cursor *pCur = (histo_cursor*)cur;
  sqlite3_int64 x = 123456;
  std::string c = "waffle";
  double d = -42.24;
  switch( i ){
    case HISTO_BIN:     d = pCur->bin; sqlite3_result_double(ctx, d); break;
    case HISTO_COUNT1:   x = pCur->count1; sqlite3_result_int64(ctx, x); break;
    case HISTO_COUNT2:   x = pCur->count2; sqlite3_result_int64(ctx, x); break;
    case HISTO_TBLNAME: c = pCur->tblname; sqlite3_result_text(ctx, c.c_str(), -1, NULL);  break;
    case HISTO_COLID:   c = pCur->colid; sqlite3_result_text(ctx, c.c_str(), -1, NULL); break;
    case HISTO_NBINS:    x = pCur->nbins; sqlite3_result_double(ctx, x); break;
    case HISTO_MINBIN:  d = pCur->minbin; sqlite3_result_double(ctx, d); break;
    case HISTO_MAXBIN:  d = pCur->maxbin; sqlite3_result_double(ctx, d); break;
    default:            x = pCur->count1; sqlite3_result_int64(ctx, x); break;
  }
  return SQLITE_OK;
}

/*
** Return the rowid for the current row.  In this implementation, the
** rowid is the same as the output value.
*/
int histoRowid(sqlite3_vtab_cursor *cur, sqlite_int64 *pRowid){
  histo_cursor *pCur = (histo_cursor*)cur;
  *pRowid = pCur->iRowid;
  return SQLITE_OK;
}

/*
** Return TRUE if the cursor has been moved off of the last
** row of output.
*/

int histoEof(sqlite3_vtab_cursor *cur) {
  histo_cursor *pCur = (histo_cursor*)cur;
  if (pCur->isDesc) {
    return pCur->iRowid < 1;
  }
  else {
    return pCur->iRowid > myhistogram1.size();
  }
}


/*
** This method is called to "rewind" the histo_cursor object back
** to the first row of output.  This method is always called at least
** once prior to any call to histoColumn() or histoRowid() or 
** histoEof().
**
** The query plan selected by histoBestIndex is passed in the idxNum
** parameter.  (idxStr is not used in this implementation.)  idxNum
** is one of the enum ColNum values above.
** This routine should initialize the cursor and position it so that it
** is pointing at the first row, or pointing off the end of the table
** (so that histoEof() will return true) if the table is empty.
*/
int histoFilter(
  sqlite3_vtab_cursor *pVtabCursor, 
  int idxNum, const char *idxStr,
  int argc, sqlite3_value **argv
){
  histo_cursor *pCur = (histo_cursor *)pVtabCursor;
  int i = 0;
  pCur->tblname = "";
  pCur->colid = "";
  pCur->nbins = 1.0;
  pCur->minbin = 1.0;
  pCur->maxbin = 1.0;

  if( idxNum >= HISTO_MAXBIN)
  {
    pCur->tblname = (const char*)sqlite3_value_text(argv[i++]);
    pCur->colid = (const char*)sqlite3_value_text(argv[i++]);
    pCur->nbins = sqlite3_value_double(argv[i++]);
    pCur->minbin = sqlite3_value_double(argv[i++]);
    pCur->maxbin = sqlite3_value_double(argv[i++]);
  }
  else 
  {
    std::cerr << "Incorrect number of arguments for function HISTO.\n" \
     "HISTO must be called as:\n HISTO('tablename', 'columnname', nbins, minbin, maxbin)" << std::endl;
    return SQLITE_ERROR;
  }
  
  std::vector< std::vector<double> > mybins;
  std::string s_exe("SELECT ");
  s_exe += pCur->colid + " FROM " + pCur->tblname;
  mybins.clear();
  mybins = GetColumns(thisdb, s_exe);
  myhistogram1 = CalcHistogram(mybins, pCur->nbins, pCur->minbin, pCur->maxbin);
  pCur->bin = myhistogram1[0].binval;
  pCur->count1 = myhistogram1[0].count;
  pCur->count2 = myhistogram1[0].accumcount;
  pCur->isDesc = 0;
  pCur->iRowid = 1;

  return SQLITE_OK;
}


/*
** SQLite will invoke this method one or more times while planning a query
** that uses the generate_histo virtual table.  This routine needs to create
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
int histoBestIndex(
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
  int nArg = 0;          /* Number of arguments that histoFilter() expects */

  sqlite3_index_info::sqlite3_index_constraint *pConstraint;
  pConstraint = pIdxInfo->aConstraint;
  for(i=0; i<pIdxInfo->nConstraint; i++, pConstraint++){
    if( pConstraint->usable==0 ) continue;
    if( pConstraint->op!=SQLITE_INDEX_CONSTRAINT_EQ ) continue;
    switch( pConstraint->iColumn ){
      case HISTO_TBLNAME:
        tblnameidx = i;
        idxNum = HISTO_TBLNAME;
        break;
      case HISTO_COLID:
        colididx = i;
        idxNum = HISTO_COLID;
        break;
      case HISTO_NBINS:
        binsidx = i;
        idxNum = HISTO_NBINS;
        break;
      case HISTO_MINBIN:
        minbinidx = i;
        idxNum = HISTO_MINBIN;
        break;
      case HISTO_MAXBIN:
        maxbinidx = i;
        idxNum = HISTO_MAXBIN;
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
** generate_histo virtual table.
*/
sqlite3_module histoModule = {
  0,                         /* iVersion */
  0,                         /* xCreate */
  histoConnect,             /* xConnect */
  histoBestIndex,           /* xBestIndex */
  histoDisconnect,          /* xDisconnect */
  0,                         /* xDestroy */
  histoOpen,                /* xOpen - open a cursor */
  histoClose,               /* xClose - close a cursor */
  histoFilter,              /* xFilter - configure scan constraints */
  histoNext,                /* xNext - advance a cursor */
  histoEof,                 /* xEof - check for end of scan */
  histoColumn,              /* xColumn - read data */
  histoRowid,               /* xRowid - read data */
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
