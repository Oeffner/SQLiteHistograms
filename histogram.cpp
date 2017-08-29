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


#include <iostream>
#include <vector>

#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1
#include <assert.h>
#include <string.h>

#ifndef SQLITE_OMIT_VIRTUALTABLE



struct histobin
{
  double binval;
  int count;
  histobin(double b = 0, int c = 0)
  {
    binval = b;
    count = c;
  }
};


std::vector<histobin> Myhistogram(std::vector<double> col, int bins, double minbin, double maxbin)
{
  std::vector<histobin> histo;

  histo.resize(bins);
  double bindomain = maxbin - minbin;
  double binwidth = bindomain / bins;

  for (unsigned i = 0; i < histo.size(); i++)
  {
    double upper = binwidth * (i + 1) + minbin;
    double middle = binwidth * (i + 0.5) + minbin;
    double lower = binwidth * i + minbin;
    histo[i].binval = middle;
    histo[i].count = 0;

    for (unsigned j = 0; j < col.size(); j++)
    {
      if (col[j] > lower && col[j] < upper)
        histo[i].count++;
    }
  }

  return histo;
};




#ifdef __cplusplus
extern "C" {
#endif



static double myvalues[10][2];
sqlite3 *thisdb = NULL;

std::vector<double> mybins;
std::vector<int> mycounts;
std::vector<histobin> myhistogram;

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

  double         bin;
  sqlite3_int64  count;
  std::string     tblname;
  std::string     colid;
  int            bins;
  double         minbin;
  double         maxbin;
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
  std::cout << "in histoConnect" << std::endl;

  sqlite3_vtab *pNew;
  int rc;

/* Column numbers */

#define HISTO_BIN 0
#define HISTO_COUNT 1
#define HISTO_TBLNAME 2
#define HISTO_COLID 3
#define HISTO_BINS 4
#define HISTO_MINBIN 5
#define HISTO_MAXBIN 6

  rc = sqlite3_declare_vtab(db,
//      "CREATE TABLE x(u REAL, v REAL, start hidden, stop hidden, step hidden)");
//   "CREATE TABLE x(bin REAL, count INTEGER, tblname hidden, colid hidden, width hidden)");
  "CREATE TABLE x(bin REAL, count INTEGER, tblname hidden, colid hidden, bins hidden, minbin hidden, maxbin hidden)");
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
static int histoDisconnect(sqlite3_vtab *pVtab){
  sqlite3_free(pVtab);
  return SQLITE_OK;
}

/*
** Constructor for a new histo_cursor object.
*/
static int histoOpen(sqlite3_vtab *p, sqlite3_vtab_cursor **ppCursor){
  std::cout << "in histoOpen" << std::endl;
  histo_cursor *pCur;
  pCur = (histo_cursor *)sqlite3_malloc( sizeof(*pCur) );
  //pCur = sqlite3_malloc(sizeof(*pCur));
  if( pCur==0 ) return SQLITE_NOMEM;
  memset(pCur, 0, sizeof(*pCur));
  *ppCursor = &pCur->base;

  for (int i = 0; i < 10; i++)
  {
    myvalues[i][0] = 42.42 + (double)i;
    myvalues[i][1] = 123 - (double)i;
  }

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
  pCur->iRowid++;
  int i = pCur->iRowid - 1;
  //pCur->bin = myvalues[i][0];
  //pCur->count = pCur->count = mybins[i];  // myvalues[i][1];

  pCur->bin = myhistogram[i].binval;  // myvalues[0][0];
  pCur->count = myhistogram[i].count;    // mybins[0];    // myvalues[j][1];

  //std::cout << "wibble" << std::endl;
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
  sqlite3_int64 x = 123456;
  std::string c = "waffle";
  double d = -42.24;
  switch( i ){
    case HISTO_BIN:     d = pCur->bin; sqlite3_result_double(ctx, d); break;
    case HISTO_COUNT:   x = pCur->count; sqlite3_result_int64(ctx, x); break;
    case HISTO_TBLNAME: c = pCur->tblname; sqlite3_result_text(ctx, c.c_str(), -1, NULL);  break;
    case HISTO_COLID:   c = pCur->colid; sqlite3_result_text(ctx, c.c_str(), -1, NULL); break;
    case HISTO_BINS:    d = pCur->bins; sqlite3_result_double(ctx, x); break;
    case HISTO_MINBIN:  d = pCur->minbin; sqlite3_result_double(ctx, d); break;
    case HISTO_MAXBIN:  d = pCur->minbin; sqlite3_result_double(ctx, d); break;
    default:            x = pCur->count; sqlite3_result_int64(ctx, x); break;
  }
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
  //std::cout << "in histoEof" << std::endl;
  histo_cursor *pCur = (histo_cursor*)cur;
  if (pCur->isDesc) {
    return pCur->iRowid < 1;
  }
  else {
    return pCur->iRowid > myhistogram.size();
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
  std::cout << "in histoFilter" << std::endl;

  histo_cursor *pCur = (histo_cursor *)pVtabCursor;
  int i = 0;
  
  if( idxNum & 1 ){
    pCur->tblname = (const char*)sqlite3_value_text(argv[i++]);
    //pCur->tblname = sqlite3_value_int64(argv[i++]);
  }else{
    pCur->tblname = "";
  }
  if( idxNum & 2 ){
    pCur->colid = (const char*)sqlite3_value_text(argv[i++]);
  }else{
    pCur->colid = "";
  }
  if (idxNum & 4) {
    pCur->bins = sqlite3_value_double(argv[i++]);
  }
  else {
    pCur->bins = 1.0;
  }
  if (idxNum & 8) {
    pCur->minbin = sqlite3_value_double(argv[i++]);
  }
  else {
    pCur->minbin = 1.0;
  }
  if (idxNum & 16) {
    pCur->maxbin = sqlite3_value_double(argv[i++]);
  }
  else {
    pCur->maxbin = 1.0;
  }

  // Access pCur->colid in pCur->tblname and create a histogram here. 
  // Then assign pCur->bin and pCur->count to the histogram.
  char *zErrMsg;
  char **result;
  int rc;
  int nrow, ncol;
  int db_open;
  std::string s_exe("SELECT ");
  s_exe += pCur->colid + " FROM " + pCur->tblname + " LIMIT 20;";

  rc = sqlite3_get_table(
    thisdb,              /* An open database */
    s_exe.c_str(),       /* SQL to be executed */
    &result,       /* Result written to a char *[]  that this points to */
    &nrow,             /* Number of result rows written here */
    &ncol,          /* Number of result columns written here */
    &zErrMsg          /* Error msg written here */
    );

  mybins.clear();
  mycounts.clear();
  if (rc == SQLITE_OK) {
    for (int i = 0; i < ncol*nrow; ++i)
      mybins.push_back(atof(result[ncol + i]));
  }
  sqlite3_free_table(result);

  myhistogram = Myhistogram(mybins, pCur->bins, pCur->minbin, pCur->maxbin);

  pCur->isDesc = 0;
  pCur->iRowid = 1;

  pCur->bin = myhistogram[0].binval;  // myvalues[0][0];
  pCur->count = myhistogram[0].count;    // mybins[0];    // myvalues[j][1];

  return rc;
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
  std::cout << "in histoBestIndex" << std::endl;
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
        idxNum |= 1;
        break;
      case HISTO_COLID:
        colididx = i;
        idxNum |= 2;
        break;
      case HISTO_BINS:
        binsidx = i;
        idxNum |= 4;
        break;
      case HISTO_MINBIN:
        minbinidx = i;
        idxNum |= 8;
        break;
      case HISTO_MAXBIN:
        maxbinidx = i;
        idxNum |= 16;
        break;
    }
  }
  if(tblnameidx >=0 ){
    pIdxInfo->aConstraintUsage[tblnameidx].argvIndex = ++nArg;
    pIdxInfo->aConstraintUsage[tblnameidx].omit= !SQLITE_SERIES_CONSTRAINT_VERIFY;
  }
  if(colididx >=0 ){
    pIdxInfo->aConstraintUsage[colididx].argvIndex = ++nArg;
    pIdxInfo->aConstraintUsage[colididx].omit = !SQLITE_SERIES_CONSTRAINT_VERIFY;
  }
  if (binsidx >= 0) {
    pIdxInfo->aConstraintUsage[binsidx].argvIndex = ++nArg;
    pIdxInfo->aConstraintUsage[binsidx].omit = !SQLITE_SERIES_CONSTRAINT_VERIFY;
  }
  if (minbinidx >= 0) {
    pIdxInfo->aConstraintUsage[minbinidx].argvIndex = ++nArg;
    pIdxInfo->aConstraintUsage[minbinidx].omit = !SQLITE_SERIES_CONSTRAINT_VERIFY;
  }
  if (maxbinidx >= 0) {
    pIdxInfo->aConstraintUsage[maxbinidx].argvIndex = ++nArg;
    pIdxInfo->aConstraintUsage[maxbinidx].omit = !SQLITE_SERIES_CONSTRAINT_VERIFY;
  }
  if( (idxNum & 3)==3 ){
    /* Both start= and stop= boundaries are available.  This is the 
    ** the preferred case */
    pIdxInfo->estimatedCost = (double)(2 - ((idxNum&4)!=0));
    pIdxInfo->estimatedRows = 1000;
    if( pIdxInfo->nOrderBy==1 ){
      if( pIdxInfo->aOrderBy[0].desc ) idxNum |= 32;
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
int sqlite3_histogram_init( // always use lower case
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