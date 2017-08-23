/* SQLite extensions for providing useful math functions not present in vanilla SQLite
Assuming source files lives in MySqliteExtentions compile from different folder with command line:

cl /Gd ..\MySqliteExtentions\SQLiteExt.c /I sqlite3 /DDLL /LD /link /export:sqlite3_extension_init /out:myfuncs_64.sqlext

and then copy myfuncs.sqlext into same folder where sqlite3.exe lives.
Subsequently use it from the sqlite prompt as follows:

sqlite> .load myfuncs.sqlext
sqlite> SELECT LOG(2.71829);
LOG(2.71829)
1.0000030061374

*/
#include "sqlite3ext.h"
#include <math.h>

SQLITE_EXTENSION_INIT1
/*
** The SQRT() SQL function returns the square root
*/
static void sqrtFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  sqlite3_result_double(context, sqrt(sqlite3_value_double(argv[0])));
}


/*
** The LOG() SQL function returns the logarithmt
*/
static void logFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  sqlite3_result_double(context, log(sqlite3_value_double(argv[0])));
}


/*
** The EXP() SQL function returns the exponential
*/
static void expFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  sqlite3_result_double(context, exp(sqlite3_value_double(argv[0])));
}


/*
** The POW() SQL function returns the power function
*/
static void powFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  sqlite3_result_double(context, pow(sqlite3_value_double(argv[0]), sqlite3_value_double(argv[1])));
}



static void TestFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
  ) {
  sqlite3_result_text(context, "foo bar\nwibble waffle\n", -1, SQLITE_TRANSIENT);
}





/* SQLite invokes this routine once when it loads the extension.
** Create new functions, collating sequences, and virtual table
** modules here.  This is usually the only exported symbol in
** the shared library.
*/
int sqlite3_extension_init(
  sqlite3 *db,
  char **pzErrMsg,
  const sqlite3_api_routines *pApi
){
  SQLITE_EXTENSION_INIT2(pApi)
/* 3. parameter is the number of arguments the functions take */
  sqlite3_create_function(db, "SQRT", 1, SQLITE_ANY, 0, sqrtFunc, 0, 0);
  sqlite3_create_function(db, "LOG", 1, SQLITE_ANY, 0, logFunc, 0, 0);
  sqlite3_create_function(db, "EXP", 1, SQLITE_ANY, 0, expFunc, 0, 0);
  sqlite3_create_function(db, "POW", 2, SQLITE_ANY, 0, powFunc, 0, 0);
  sqlite3_create_function(db, "TEST", 1, SQLITE_ANY, 0, TestFunc, 0, 0);

  return 0;
}
