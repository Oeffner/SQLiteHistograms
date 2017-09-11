/*
helpers.h, Robert Oeffner 2017

*/

#include <iostream>
#include <vector>
#include <cstdlib>

#include "RegistExt.h"
#include <assert.h>
#include <string.h>


struct histobin
{
  double binval;
  int count, accumcount;
  histobin(double b = 0, int c = 0, int ac = 0)
  {
    binval = b;
    count = c;
    accumcount = ac;
  }
};


struct interpolatebin
{
  double xval, yval, sigma, moe;
  int count;
  interpolatebin(double x = 0, double y = 0, double s = 0, int c = 0)
  {
    xval = x;
    yval = y;
    count = c;
    moe = 0.0; // Margin of Error (MOE) of a mean value is based on Z*sigma/sqrt(N)
  }
};


std::vector< std::vector<double> > GetColumns(sqlite3* db, std::string sqlxprs);

std::vector<histobin> CalcHistogram(std::vector< std::vector<double> > Yvals,
  int bins, double minbin, double maxbin);

std::vector<interpolatebin> CalcInterpolations(std::vector< std::vector<double> > XYvals, 
  int bins, double minbin, double maxbin);



#pragma once
