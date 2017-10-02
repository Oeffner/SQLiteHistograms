/*
helpers.h, Robert Oeffner 2017

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


std::vector< std::vector<double> > GetColumns(sqlite3* db, std::string sqlxprs, int *rc);

std::vector<histobin> CalcHistogram(std::vector< std::vector<double> > Yvals,
  int bins, double minbin, double maxbin, int *rc);

std::vector<interpolatebin> CalcInterpolations(std::vector< std::vector<double> > XYvals, 
  int bins, double minbin, double maxbin, int *rc);



#pragma once
