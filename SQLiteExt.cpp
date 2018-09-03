/* SQLite extensions for providing useful math functions not present in vanilla SQLite
Assuming source files lives in MySqliteXtentions compile from different folder with command line:

cl /Gd ..\MySqliteXtentions\SQLiteXt.c /I sqlite3 /DDLL /LD /link /export:sqlite3_extension_init /out:myfuncs_64.sqlext

and then copy myfuncs.sqlext into same folder where sqlite3.exe lives.
Subsequently use it from the sqlite prompt as follows:

sqlite> .load myfuncs.sqlext
sqlite> SELECT LOG(2.71829);
LOG(2.71829)
1.0000030061374


with debug info:
cl /DDEBUG  /ZI /EHsc  SQLiteExt.cpp /I sqlite3 /DDLL /LD /link  /debugtype:cv /export:sqlite3_extension_init /out:myfuncs.sqlext

*/

#include "RegistExt.h"
#include "sqlite3ext.h"
#include <math.h>
#include <vector>


//SQLITE_EXTENSION_INIT1

void sqrtFunc(sqlite3_context *context, int argc, sqlite3_value **argv)
{
  sqlite3_result_double(context, sqrt(sqlite3_value_double(argv[0])));
}

void logFunc( sqlite3_context *context, int argc, sqlite3_value **argv)
{
  sqlite3_result_double(context, log(sqlite3_value_double(argv[0])));
}

void expFunc(sqlite3_context *context, int argc, sqlite3_value **argv)
{
  sqlite3_result_double(context, exp(sqlite3_value_double(argv[0])));
}

void powFunc(sqlite3_context *context, int argc, sqlite3_value **argv)
{
  sqlite3_result_double(context, pow(sqlite3_value_double(argv[0]), sqlite3_value_double(argv[1])));
}


struct StdevCtx 
{
  std::vector<double> X;
  std::vector<double> Y;
};


void CoVarStep(sqlite3_context *context, int argc, sqlite3_value **argv)
{
  StdevCtx *p = (StdevCtx*)sqlite3_aggregate_context(context, sizeof(*p));
  // only consider non-null values 
  if (SQLITE_NULL != sqlite3_value_numeric_type(argv[0])
    && SQLITE_NULL != sqlite3_value_numeric_type(argv[1]))
  {
    p->X.push_back(sqlite3_value_double(argv[0]));
    p->Y.push_back(sqlite3_value_double(argv[1]));
  }
}


void CoVarFinal(sqlite3_context *context) 
{
  StdevCtx *p = (StdevCtx*)sqlite3_aggregate_context(context, 0);
  if (p && p->X.size() > 0)
  {
    double Xsum = 0.0, Ysum = 0.0;
    for (unsigned j = 0; j < p->X.size(); j++)
    {
      Xsum += p->X[j];
      Ysum += p->Y[j];
    }
    double Xavg = Xsum / p->X.size();
    double Yavg = Ysum / p->Y.size();

    double covar = 0.0;
    for (unsigned j = 0; j < p->X.size(); j++)
    {
      covar += (p->X[j] - Xavg) * (p->Y[j] - Yavg);
    }
    covar /= p->X.size();

    sqlite3_result_double(context, covar);
  }
  else
  {
    sqlite3_result_double(context, 0.0);
  }
  p->X.clear();
  p->Y.clear();
}


void CorrelStep(sqlite3_context *context, int argc, sqlite3_value **argv)
{
  StdevCtx *p = (StdevCtx*)sqlite3_aggregate_context(context, sizeof(*p));
  // only consider non-null values 
  if (SQLITE_NULL != sqlite3_value_numeric_type(argv[0])
    && SQLITE_NULL != sqlite3_value_numeric_type(argv[1]))
  {
    p->X.push_back(sqlite3_value_double(argv[0]));
    p->Y.push_back(sqlite3_value_double(argv[1]));
  }
}


void CorrelFinal(sqlite3_context *context) 
{
  StdevCtx *p = (StdevCtx*)sqlite3_aggregate_context(context, 0);
  if (p && p->X.size() > 0)
  {
    double Xsum = 0.0, Ysum = 0.0;
    for (unsigned j = 0; j < p->X.size(); j++)
    {
      Xsum += p->X[j];
      Ysum += p->Y[j];
    }
    double Xavg = Xsum / p->X.size();
    double Yavg = Ysum / p->Y.size();

    double covar = 0.0, Xvar = 0.0, Yvar = 0.0;
    for (unsigned j = 0; j < p->X.size(); j++)
    {
      double xd = (p->X[j] - Xavg);
      double yd = (p->Y[j] - Yavg);
      Xvar += xd * xd;
      Yvar += yd * yd;
      covar += xd * yd;
    }
    covar /= p->X.size();
    Xvar /= p->X.size();
    Yvar /= p->X.size();
    double Xsig = sqrt(Xvar);
    double Ysig = sqrt(Yvar);
    double correlation = covar / (Xsig*Ysig);
    sqlite3_result_double(context, correlation);
  }
  else
  {
    sqlite3_result_double(context, 0.0);
  }
  p->X.clear();
  p->Y.clear();
}



struct spcorval
{
  double val, rank;
};


struct v2Ctx
{
  std::vector<spcorval> X;
  std::vector<spcorval> Y;
};



// Function to find ranks in array of spcorval elements
void Rankify(std::vector<spcorval> &A) 
{
	//std::vector<spcorval>;
	// Sweep through all elements in A for each
	// element count the number of less than and
	// equal elements separately in r and s.
	for (int i = 0; i < A.size(); i++) 
  {
	  int r = 1, s = 1;
	  for (int j = 0; j < A.size(); j++)
    {
	    if (j != i && A[j].val < A[i].val)
	      r += 1;
	    if (j != i && A[j].val == A[i].val)
	      s += 1;
	  }
	  // Use formula to obtain rank
	  A[i].rank = r + (float)(s - 1)/2.0;
	}
}


void SpCorrelStep(sqlite3_context *context, int argc, sqlite3_value **argv)
{
  v2Ctx *p = (v2Ctx*)sqlite3_aggregate_context(context, sizeof(*p));
  // only consider non-null values 
  if (SQLITE_NULL != sqlite3_value_numeric_type(argv[0])
    && SQLITE_NULL != sqlite3_value_numeric_type(argv[1]))
  {
    spcorval x, y;
    x.val = sqlite3_value_double(argv[0]);
    y.val = sqlite3_value_double(argv[1]);
    p->X.push_back(x);
    p->Y.push_back(y);
  }
}


void SpCorrelFinal(sqlite3_context *context) {
  v2Ctx *p = (v2Ctx*)sqlite3_aggregate_context(context, 0);
  if (p && p->X.size() > 0)
  {
    // now rank the arrays first
    Rankify(p->X);
    Rankify(p->Y);

    double Xsum = 0.0, Ysum = 0.0;
    for (unsigned j = 0; j < p->X.size(); j++)
    {
      Xsum += p->X[j].rank;
      Ysum += p->Y[j].rank;
    }
    double Xavg = Xsum / p->X.size();
    double Yavg = Ysum / p->Y.size();
    double covar = 0.0, Xvar = 0.0, Yvar = 0.0;
    for (unsigned j = 0; j < p->X.size(); j++)
    {
      double xd = (p->X[j].rank - Xavg);
      double yd = (p->Y[j].rank - Yavg);
      Xvar += xd * xd;
      Yvar += yd * yd;
      covar += xd * yd;
    }
    covar /= p->X.size();
    Xvar /= p->X.size();
    Yvar /= p->X.size();
    double Xsig = sqrt(Xvar);
    double Ysig = sqrt(Yvar);
    double correlation = covar/(Xsig*Ysig);
    sqlite3_result_double(context, correlation);
  }
  else
  {
    sqlite3_result_double(context, 0.0);
  }
  p->X.clear();
  p->Y.clear();
}


/*
#ifdef __cplusplus
extern "C" {
#endif

int sqlite3_extension_init(
  sqlite3 *db,
  char **pzErrMsg,
  const sqlite3_api_routines *pApi
){
  SQLITE_EXTENSION_INIT2(pApi)

  sqlite3_create_function(db, "SQRT", 1, SQLITE_ANY, 0, sqrtFunc, 0, 0);
  sqlite3_create_function(db, "LOG", 1, SQLITE_ANY, 0, logFunc, 0, 0);
  sqlite3_create_function(db, "EXP", 1, SQLITE_ANY, 0, expFunc, 0, 0);
  sqlite3_create_function(db, "POW", 2, SQLITE_ANY, 0, powFunc, 0, 0);
  
  sqlite3_create_function(db, "COVAR", 2, SQLITE_ANY, db, NULL, CoVarStep, CoVarFinal);
  sqlite3_create_function(db, "COREL", 2, SQLITE_ANY, db, NULL, CorelStep, CorelFinal);
  sqlite3_create_function(db, "SPEARMANCOREL", 2, SQLITE_ANY, db, NULL, SpCorelStep, SpCorelFinal);

  return 0;
}


#ifdef __cplusplus
}
#endif
*/



