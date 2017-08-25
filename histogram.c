/*
** 2015-08-18
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
**
** This file demonstrates how to create a table-valued-function using
** a virtual table.  This demo implements the generate_series() function
** which gives similar results to the eponymous function in PostgreSQL.
** Examples:
**
**      SELECT * FROM generate_series(0,100,5);
**
** The query above returns integers from 0 through 100 counting by steps
** of 5.
**
**      SELECT * FROM generate_series(0,100);
**
** Integers from 0 through 100 with a step size of 1.
**
**      SELECT * FROM generate_series(20) LIMIT 10;
**
** Integers 20 through 29.
**
** HOW IT WORKS
**
** The generate_series "function" is really a virtual table with the
** following schema:
**
**     CREATE TABLE generate_series(
**       value,
**       start HIDDEN,
**       stop HIDDEN,
**       step HIDDEN
**     );
**
** Function arguments in queries against this virtual table are translated
** into equality constraints against successive hidden columns.  In other
** words, the following pairs of queries are equivalent to each other:
**
**    SELECT * FROM generate_series(0,100,5);
**    SELECT * FROM generate_series WHERE start=0 AND stop=100 AND step=5;
**
**    SELECT * FROM generate_series(0,100);
**    SELECT * FROM generate_series WHERE start=0 AND stop=100;
**
**    SELECT * FROM generate_series(20) LIMIT 10;
**    SELECT * FROM generate_series WHERE start=20 LIMIT 10;
**
** The generate_series virtual table implementation leaves the xCreate method
** set to NULL.  This means that it is not possible to do a CREATE VIRTUAL
** TABLE command with "generate_series" as the USING argument.  Instead, there
** is a single generate_series virtual table that is always available without
** having to be created first.
**
** The xBestIndex method looks for equality constraints against the hidden
** start, stop, and step columns, and if present, it uses those constraints
** to bound the sequence of generated values.  If the equality constraints
** are missing, it uses 0 for start, 4294967295 for stop, and 1 for step.
** xBestIndex returns a small cost when both start and stop are available,
** and a very large cost if either start or stop are unavailable.  This
** encourages the query planner to order joins such that the bounds of the
** series are well-defined.
*/


#include "sqlite3ext.h"
#include "sqlite3.h"
SQLITE_EXTENSION_INIT1
#include <assert.h>
#include <string.h>
#include <stdio.h>

#ifndef SQLITE_OMIT_VIRTUALTABLE

#ifdef __cplusplus
extern "C" {
#endif

/* histo_cursor is a subclass of sqlite3_vtab_cursor which will
** serve as the underlying representation of a cursor that scans
** over rows of the result
*/
typedef struct histo_cursor histo_cursor;
struct histo_cursor {
  sqlite3_vtab_cursor base;  /* Base class - must be first */
  int isDesc;                /* True to count down rather than up */
  sqlite3_int64 iRowid;      /* The rowid */
  double        iu;          /* Current value ("value") */
  double        iv;          /* the result, some function of iu */
  sqlite3_int64 mnValue;     /* Mimimum value ("start") */
  sqlite3_int64 mxValue;     /* Maximum value ("stop") */
  sqlite3_int64 iStep;       /* Increment ("step") */
};

/*
** The histoConnect() method is invoked to create a new
** histo_vtab that describes the generate_histo virtual table.
**
** Think of this routine as the constructor for histo_vtab objects.
**
** All this routine needs to do is:
**
**    (1) Allocate the histo_vtab object and initialize all fields.
**
**    (2) Tell SQLite (via the sqlite3_declare_vtab() interface) what the
**        result set of queries against generate_histo will look like.
*/
static int histoConnect(
  sqlite3 *db,
  void *pAux,
  int argc, const char *const*argv,
  sqlite3_vtab **ppVtab,
  char **pzErr
){
  sqlite3_vtab *pNew;
  int rc;

/* Column numbers */
#define SERIES_COLUMN_VALUE 0
#define SERIES_COLUMN_FUNCVAL 1
#define SERIES_COLUMN_START 2
#define SERIES_COLUMN_STOP  3
#define SERIES_COLUMN_STEP  4

  rc = sqlite3_declare_vtab(db,
//     "CREATE TABLE x(value,start hidden,stop hidden,step hidden)");
      "CREATE TABLE x(u REAL, v REAL, start hidden, stop hidden, step hidden)");
  if( rc==SQLITE_OK ){
    pNew = *ppVtab = (sqlite3_vtab *)sqlite3_malloc( sizeof(*pNew) );
    if( pNew==0 ) return SQLITE_NOMEM;
    memset(pNew, 0, sizeof(*pNew));
  }
  return rc;
}

/*
** This method is the destructor for histo_cursor objects.
*/
static int histoDisconnect(sqlite3_vtab *pVtab){
  sqlite3_free(pVtab);
  return SQLITE_OK;
}

/*
** Constructor for a new histo_cursor object.
*/
static int histoOpen(sqlite3_vtab *p, sqlite3_vtab_cursor **ppCursor){
  histo_cursor *pCur;
  //pCur = (histo_cursor *)sqlite3_malloc( sizeof(*pCur) );
  pCur = sqlite3_malloc(sizeof(*pCur));
  if( pCur==0 ) return SQLITE_NOMEM;
  memset(pCur, 0, sizeof(*pCur));
  *ppCursor = &pCur->base;
  return SQLITE_OK;
}

/*
** Destructor for a histo_cursor.
*/
static int histoClose(sqlite3_vtab_cursor *cur){
  sqlite3_free(cur);
  return SQLITE_OK;
}


/*
** Advance a histo_cursor to its next row of output.
*/
static int histoNext(sqlite3_vtab_cursor *cur){
  histo_cursor *pCur = (histo_cursor*)cur;
  pCur->iu += pCur->iStep;
  pCur->iv = pCur->iu * pCur->iu;
  pCur->iRowid++;
  return SQLITE_OK;
}

/*
** Return values of columns for the row at which the histo_cursor
** is currently pointing.
*/
static int histoColumn(
  sqlite3_vtab_cursor *cur,   /* The cursor */
  sqlite3_context *ctx,       /* First argument to sqlite3_result_...() */
  int i                       /* Which column to return */
){
  histo_cursor *pCur = (histo_cursor*)cur;
  sqlite3_int64 x = 0;
  switch( i ){
    case SERIES_COLUMN_START:  x = pCur->mnValue; break;
    case SERIES_COLUMN_STOP:   x = pCur->mxValue; break;
    case SERIES_COLUMN_STEP:   x = pCur->iStep;   break;
    case SERIES_COLUMN_FUNCVAL: x = pCur->iv; break;
    default:                   x = pCur->iu;  break;
  }
  sqlite3_result_int64(ctx, x);
  return SQLITE_OK;
}

/*
** Return the rowid for the current row.  In this implementation, the
** rowid is the same as the output value.
*/
static int histoRowid(sqlite3_vtab_cursor *cur, sqlite_int64 *pRowid){
  histo_cursor *pCur = (histo_cursor*)cur;
  *pRowid = pCur->iRowid;
  return SQLITE_OK;
}

/*
** Return TRUE if the cursor has been moved off of the last
** row of output.
*/

static int histoEof(sqlite3_vtab_cursor *cur) {
  histo_cursor *pCur = (histo_cursor*)cur;
  if (pCur->isDesc) {
    return pCur->iu < pCur->mnValue;
  }
  else {
    return pCur->iu > pCur->mxValue;
  }
}

/* True to cause run-time checking of the start=, stop=, and/or step= 
** parameters.  The only reason to do this is for testing the
** constraint checking logic for virtual tables in the SQLite core.
*/
#ifndef SQLITE_SERIES_CONSTRAINT_VERIFY
# define SQLITE_SERIES_CONSTRAINT_VERIFY 0
#endif

/*
** This method is called to "rewind" the histo_cursor object back
** to the first row of output.  This method is always called at least
** once prior to any call to histoColumn() or histoRowid() or 
** histoEof().
**
** The query plan selected by histoBestIndex is passed in the idxNum
** parameter.  (idxStr is not used in this implementation.)  idxNum
** is a bitmask showing which constraints are available:
**
**    1:    start=VALUE
**    2:    stop=VALUE
**    4:    step=VALUE
**
** Also, if bit 8 is set, that means that the histo should be output
** in descending order rather than in ascending order.
**
** This routine should initialize the cursor and position it so that it
** is pointing at the first row, or pointing off the end of the table
** (so that histoEof() will return true) if the table is empty.
*/
static int histoFilter(
  sqlite3_vtab_cursor *pVtabCursor, 
  int idxNum, const char *idxStr,
  int argc, sqlite3_value **argv
){
  histo_cursor *pCur = (histo_cursor *)pVtabCursor;
  int i = 0;
  if( idxNum & 1 ){
    pCur->mnValue = sqlite3_value_int64(argv[i++]);
  }else{
    pCur->mnValue = 0;
  }
  if( idxNum & 2 ){
    pCur->mxValue = sqlite3_value_int64(argv[i++]);
  }else{
    pCur->mxValue = 0xffffffff;
  }
  if( idxNum & 4 ){
    pCur->iStep = sqlite3_value_int64(argv[i++]);
    if( pCur->iStep<1 ) pCur->iStep = 1;
  }else{
    pCur->iStep = 1;
  }
  if( idxNum & 8 ){
    pCur->isDesc = 1;
    pCur->iu = pCur->mxValue;
    if( pCur->iStep>0 ){
      pCur->iu -= (pCur->mxValue - pCur->mnValue) % pCur->iStep;
    }
  }else{
    pCur->isDesc = 0;
    pCur->iu = pCur->mnValue;
  }
  pCur->iv = pCur->iu * pCur->iu;
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
static int histoBestIndex(
  sqlite3_vtab *tab,
  sqlite3_index_info *pIdxInfo
){
  int i;                 /* Loop over constraints */
  int idxNum = 0;        /* The query plan bitmask */
  int startIdx = -1;     /* Index of the start= constraint, or -1 if none */
  int stopIdx = -1;      /* Index of the stop= constraint, or -1 if none */
  int stepIdx = -1;      /* Index of the step= constraint, or -1 if none */
  int nArg = 0;          /* Number of arguments that histoFilter() expects */

  const struct sqlite3_index_constraint *pConstraint;
  //sqlite3_index_info::sqlite3_index_constraint *pConstraint;
  pConstraint = pIdxInfo->aConstraint;
  for(i=0; i<pIdxInfo->nConstraint; i++, pConstraint++){
    if( pConstraint->usable==0 ) continue;
    if( pConstraint->op!=SQLITE_INDEX_CONSTRAINT_EQ ) continue;
    switch( pConstraint->iColumn ){
      case SERIES_COLUMN_START:
        startIdx = i;
        idxNum |= 1;
        break;
      case SERIES_COLUMN_STOP:
        stopIdx = i;
        idxNum |= 2;
        break;
      case SERIES_COLUMN_STEP:
        stepIdx = i;
        idxNum |= 4;
        break;
    }
  }
  if( startIdx>=0 ){
    pIdxInfo->aConstraintUsage[startIdx].argvIndex = ++nArg;
    pIdxInfo->aConstraintUsage[startIdx].omit= !SQLITE_SERIES_CONSTRAINT_VERIFY;
  }
  if( stopIdx>=0 ){
    pIdxInfo->aConstraintUsage[stopIdx].argvIndex = ++nArg;
    pIdxInfo->aConstraintUsage[stopIdx].omit = !SQLITE_SERIES_CONSTRAINT_VERIFY;
  }
  if( stepIdx>=0 ){
    pIdxInfo->aConstraintUsage[stepIdx].argvIndex = ++nArg;
    pIdxInfo->aConstraintUsage[stepIdx].omit = !SQLITE_SERIES_CONSTRAINT_VERIFY;
  }
  if( (idxNum & 3)==3 ){
    /* Both start= and stop= boundaries are available.  This is the 
    ** the preferred case */
    pIdxInfo->estimatedCost = (double)(2 - ((idxNum&4)!=0));
    pIdxInfo->estimatedRows = 1000;
    if( pIdxInfo->nOrderBy==1 ){
      if( pIdxInfo->aOrderBy[0].desc ) idxNum |= 8;
      pIdxInfo->orderByConsumed = 1;
    }
  }else{
    /* If either boundary is missing, we have to generate a huge span
    ** of numbers.  Make this case very expensive so that the query
    ** planner will work hard to avoid it. */
    pIdxInfo->estimatedCost = (double)2147483647;
    pIdxInfo->estimatedRows = 2147483647;
  }
  pIdxInfo->idxNum = idxNum;
  return SQLITE_OK;
}

/*
** This following structure defines all the methods for the 
** generate_histo virtual table.
*/
static sqlite3_module histoModule = {
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





#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_histo_init( // always use lower case
  sqlite3 *db, 
  char **pzErrMsg, 
  const sqlite3_api_routines *pApi
)
{
  int rc = SQLITE_OK;
  SQLITE_EXTENSION_INIT2(pApi);
#ifndef SQLITE_OMIT_VIRTUALTABLE
  if( sqlite3_libversion_number()<3008012 )
  {
    *pzErrMsg = sqlite3_mprintf(
        "histo() requires SQLite 3.8.12 or later");
    return SQLITE_ERROR;
  }
  rc = sqlite3_create_module(db, "histo", &histoModule, 0);

#endif
  return rc;
}


#ifdef __cplusplus
}
#endif