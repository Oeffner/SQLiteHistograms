/*
helpers.h, Robert Oeffner 2017

*/

#include <iostream>
#include <vector>
#include <cstdlib>
#include <math.h>

#include "RegistExt.h"
#include <assert.h>
#include <memory.h>


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
  double xval, yval, sigma, sem;
  int count;
  interpolatebin(double x = 0, double y = 0, double s = 0, int c = 0)
  {
    xval = x;
    yval = y;
    count = c;
    sem = sigma = 0.0; // Standard Error of Mean (SEM) is sigma/sqrt(N)
    // Margin of error is then Z*sigma/sqrt(N) for a given Z score
  }
};


std::vector< std::vector<double> > GetColumns(sqlite3* db, std::string sqlxprs);

std::vector<histobin> CalcHistogram(std::vector< std::vector<double> > Yvals,
  int bins, double minbin, double maxbin);

std::vector<interpolatebin> CalcInterpolations(std::vector< std::vector<double> > XYvals, 
  int bins, double minbin, double maxbin);



#pragma once
