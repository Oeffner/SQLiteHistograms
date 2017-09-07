
#include <iostream>
#include <vector>
#include <cstdlib>

#include "RegistExt.h"
#include <assert.h>
#include <string.h>


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


std::vector<double> GetColumn(sqlite3* db, std::string sqlxprs);

/* Caclulate a histogram from the col array with bins number of bins and values between
minbin and maxbin
*/
std::vector<histobin> CalcHistogram(std::vector<double> col, int bins,
  double minbin, double maxbin);



#pragma once
