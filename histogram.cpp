/*

histogram.cpp, Robert Oeffner 2017
SQLite extension for calculating histogram of column values as well as calculating ratios of 
two histograms deduced from the same table.


Compiles with command line:

On Windows with MSVC++:
cl /Gd ..\MySqliteExtentions\histogram.cpp /I sqlite3 /DDLL /EHsc /LD /link /export:sqlite3_histogram_init /out:histogram.sqlext

On Linux with g++:
g++ -fPIC -lm -shared histogram.cpp -o libhistogram.so

*/

#include <iostream>
#include <vector>
#include <cstdlib>


#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1
#include <assert.h>
#include <string.h>

#ifndef SQLITE_OMIT_VIRTUALTABLE




std::vector<double> GetColumn(sqlite3* db, std::string sqlxprs)
{
  // Access pCur->colid in pCur->tblname and create a histogram here. 
  // Then assign pCur->bin and pCur->count to the histogram.
  char *zErrMsg;
  char **result;
  int rc;
  int nrow, ncol;
  int db_open;

  rc = sqlite3_get_table(
    db,              /* An open database */
    sqlxprs.c_str(),       /* SQL to be executed */
    &result,       /* Result written to a char *[]  that this points to */
    &nrow,             /* Number of result rows written here */
    &ncol,          /* Number of result columns written here */
    &zErrMsg          /* Error msg written here */
    );

  std::vector<double> bins;
  bins.clear();
  if (rc == SQLITE_OK) {
    for (int i = 0; i < ncol*nrow; ++i)
    {
      if (result[ncol + i])
      {
        std::string val(result[ncol + i]);
        bins.push_back(atof(val.c_str()));
      }
    }
  }
  sqlite3_free_table(result);

  return bins;
}


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



sqlite3 *thisdb = NULL;
std::vector<histobin> myhistogram1;
std::vector<histobin> myhistogram2;

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
  std::string    discrcolid;
  std::string    discrval;
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
  //std::cout << "in histoConnect" << std::endl;

  sqlite3_vtab *pNew;
  int rc;

/* Column numbers */

#define HISTO_BIN        0
#define HISTO_COUNT1     1
#define HISTO_COUNT2     2
#define HISTO_RATIO      3
#define HISTO_TOTALCOUNT 4
#define HISTO_TBLNAME    5
#define HISTO_COLID      6
#define HISTO_NBINS      7
#define HISTO_MINBIN     8
#define HISTO_MAXBIN     9
#define HISTO_DISCRCOLID 10
#define HISTO_DISCRVAL   11

  rc = sqlite3_declare_vtab(db,
  "CREATE TABLE x(bin REAL, count1 INTEGER, count2 INTEGER, ratio REAL, totalcount INTEGER, " \
  "tblname hidden, colid hidden, nbins hidden, minbin hidden, maxbin hidden, discrcolid hidden, discrval hidden)");
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
  //std::cout << "in histoOpen" << std::endl;
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

  pCur->bin = myhistogram1[i].binval; 
  pCur->count1 = myhistogram1[i].count;
  pCur->count2 = myhistogram2[i].count;
  pCur->totalcount = myhistogram1[i].count + myhistogram2[i].count;
  if (pCur->totalcount > 0)
    pCur->ratio = ((double) pCur->count1 ) / pCur->totalcount;
  else
    pCur->ratio = 0.0;

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
    case HISTO_COUNT1:   x = pCur->count1; sqlite3_result_int64(ctx, x); break;
    case HISTO_COUNT2:   x = pCur->count2; sqlite3_result_int64(ctx, x); break;
    case HISTO_RATIO:   d = pCur->ratio; sqlite3_result_double(ctx, d); break;
    case HISTO_TOTALCOUNT:  x = pCur->totalcount; sqlite3_result_int64(ctx, x); break;
    case HISTO_TBLNAME: c = pCur->tblname; sqlite3_result_text(ctx, c.c_str(), -1, NULL);  break;
    case HISTO_COLID:   c = pCur->colid; sqlite3_result_text(ctx, c.c_str(), -1, NULL); break;
    case HISTO_NBINS:    x = pCur->nbins; sqlite3_result_double(ctx, x); break;
    case HISTO_MINBIN:  d = pCur->minbin; sqlite3_result_double(ctx, d); break;
    case HISTO_MAXBIN:  d = pCur->minbin; sqlite3_result_double(ctx, d); break;
    case HISTO_DISCRCOLID:  c = pCur->discrcolid; sqlite3_result_text(ctx, c.c_str(), -1, NULL);  break;
    case HISTO_DISCRVAL:  c = pCur->discrval; sqlite3_result_text(ctx, c.c_str(), -1, NULL);  break;
    default:            x = pCur->count1; sqlite3_result_int64(ctx, x); break;
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
    return pCur->iRowid > myhistogram1.size();
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
  //std::cout << "in histoFilter" << std::endl;
  histo_cursor *pCur = (histo_cursor *)pVtabCursor;
  int i = 0;
  
  if( idxNum >= HISTO_TBLNAME){
    pCur->tblname = (const char*)sqlite3_value_text(argv[i++]);
    //pCur->tblname = sqlite3_value_int64(argv[i++]);
  }else{
    pCur->tblname = "";
  }
  if( idxNum >= HISTO_COLID){
    pCur->colid = (const char*)sqlite3_value_text(argv[i++]);
  }else{
    pCur->colid = "";
  }
  if (idxNum >= HISTO_NBINS) {
    pCur->nbins = sqlite3_value_double(argv[i++]);
  }
  else {
    pCur->nbins = 1.0;
  }
  if (idxNum >= HISTO_MINBIN) {
    pCur->minbin = sqlite3_value_double(argv[i++]);
  }
  else {
    pCur->minbin = 1.0;
  }
  if (idxNum >= HISTO_MAXBIN) {
    pCur->maxbin = sqlite3_value_double(argv[i++]);
  }
  else {
    pCur->maxbin = 1.0;
  }
  if (idxNum >= HISTO_DISCRCOLID) {
    pCur->discrcolid = (const char*)sqlite3_value_text(argv[i++]);
  }
  else {
    pCur->discrcolid = "";
  }
  if (idxNum >= HISTO_DISCRVAL) {
    pCur->discrval = (const char*)sqlite3_value_text(argv[i++]);
  }
  else {
    pCur->discrval = "0.0";
  }

  std::vector<double> mybins;
  std::string s_exe("SELECT ");
  s_exe += pCur->colid + " FROM " + pCur->tblname;
  mybins.clear();
  mybins = GetColumn(thisdb, s_exe);
  myhistogram1 = Myhistogram(mybins, pCur->nbins, pCur->minbin, pCur->maxbin);
  myhistogram2.resize(pCur->nbins);
  pCur->totalcount = myhistogram1[0].count;
  pCur->ratio = 0.0;

  if (pCur->discrcolid != "") // make two histograms for values above and below discrval
  {
    std::string s_exe("SELECT "); // get first histogram where values are above discrval
    s_exe += pCur->colid + " FROM " + pCur->tblname
      + " WHERE " + pCur->discrcolid + " >= " + pCur->discrval;
    mybins.clear();
    mybins = GetColumn(thisdb, s_exe);
    myhistogram1 = Myhistogram(mybins, pCur->nbins, pCur->minbin, pCur->maxbin);
    
    // get second histogram where values are below discrval
    s_exe = "SELECT " + pCur->colid + " FROM " + pCur->tblname
      + " WHERE " + pCur->discrcolid + " < " + pCur->discrval;
    mybins.clear();
    mybins = GetColumn(thisdb, s_exe);
    myhistogram2 = Myhistogram(mybins, pCur->nbins, pCur->minbin, pCur->maxbin);

    pCur->bin = myhistogram1[0].binval;
    pCur->count1 = myhistogram1[0].count;
    pCur->count2 = myhistogram2[0].count;
    pCur->totalcount = myhistogram1[0].count + myhistogram2[0].count;
    if (pCur->totalcount > 0)
      pCur->ratio = pCur->count1 / pCur->totalcount;
  }

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
static int histoBestIndex(
  sqlite3_vtab *tab,
  sqlite3_index_info *pIdxInfo
){
  //std::cout << "in histoBestIndex" << std::endl;
  int i;                 /* Loop over constraints */
  int idxNum = 0;        /* The query plan bitmask */
  int tblnameidx = -1;     /* Index of the start= constraint, or -1 if none */
  int colididx = -1;      /* Index of the stop= constraint, or -1 if none */
  int binsidx = -1;      /* Index of the step= constraint, or -1 if none */
  int minbinidx = -1;
  int maxbinidx = -1;
  int discrcolididx = -1;
  int discrvalidx = -1;
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
      case HISTO_DISCRCOLID:
        discrcolididx = i;
        idxNum = HISTO_DISCRCOLID;
        break;
      case HISTO_DISCRVAL:
        discrvalidx = i;
        idxNum = HISTO_DISCRVAL;
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
  if (discrcolididx >= 0) {
    pIdxInfo->aConstraintUsage[discrcolididx].argvIndex = ++nArg;
    pIdxInfo->aConstraintUsage[discrcolididx].omit = !SQLITE_SERIES_CONSTRAINT_VERIFY;
  }
  if (discrvalidx >= 0) {
    pIdxInfo->aConstraintUsage[discrvalidx].argvIndex = ++nArg;
    pIdxInfo->aConstraintUsage[discrvalidx].omit = !SQLITE_SERIES_CONSTRAINT_VERIFY;
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