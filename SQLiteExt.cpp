/*
SQLiteExt.cpp, Robert Oeffner 2018

SQLite extension for providing useful math functions not present in vanilla SQLite.

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


#include "RegistExt.h"
#include "sqlite3ext.h"
#include <math.h>
#include <vector>



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




