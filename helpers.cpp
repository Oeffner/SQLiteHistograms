/*
helpers.cpp, Robert Oeffner 2018

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


#include "helpers.h"



std::vector< std::vector<double> > GetColumns(sqlite3* db, std::string sqlxprs, int *rc)
{
  // get column from sql expression and return it 
  char *zErrMsg;
  char **result;
  int nrow, ncol;
  int db_open;
  *rc = sqlite3_get_table(
    db,              /* An open database */
    sqlxprs.c_str(),       /* SQL to be executed */
    &result,       /* Result written to a char *[]  that this points to */
    &nrow,             /* Number of result rows written here */
    &ncol,          /* Number of result columns written here */
    &zErrMsg          /* Error msg written here */
    );
/* If SQL table is structured as:
  Name        | Age
  -----------------------
  Alice       | 43
  Bob         | 28
  Cindy       | 21

  then &result will look like:

  result[0] = "Name";
  result[1] = "Age";
  result[2] = "Alice";
  result[3] = "43";
  result[4] = "Bob";
  result[5] = "28";
  result[6] = "Cindy";
  result[7] = "21";
*/
  std::vector< std::vector<double> > columns;
  columns.clear();
  columns.resize(ncol);

  for (int n = 0; n < ncol; n++)
  {
    columns[n].resize(nrow);
  }
  if (*rc == SQLITE_OK) {
    for (int i = 0; i < nrow*ncol; i++)
    {
      int col = i % ncol;
      int row = floor(i / ncol);
      if (result[ncol + i]) // skip column names
      {
        std::string val(result[ncol + i]);
        columns[col][row] = atof(val.c_str());
      }
    }
  }
  sqlite3_free_table(result);

  return columns;
}



/* Caclulate a histogram from the col array with bins number of bins and values between
minbin and maxbin
*/
std::vector<histobin> CalcHistogram(std::vector< std::vector<double> > Yvals, 
                                     int bins, double minbin, double maxbin, int *rc)
{
  std::vector<histobin> histo;

	if (bins < 1 || minbin >= maxbin)
	{
		std::cerr << "Nonsensical value for either bins, minbin or maxbin" << std::endl;
		*rc = SQLITE_ERROR;
		return histo;
	}

  histo.resize(bins);
  double bindomain = maxbin - minbin;
  double binwidth = bindomain / bins;
  int accumcount = 0;
  for (unsigned i = 0; i < histo.size(); i++)
  {
    double upper = binwidth * (i + 1) + minbin;
    double middle = binwidth * (i + 0.5) + minbin;
    double lower = binwidth * i + minbin;
    histo[i].binval = middle;
    histo[i].count = 0;
    histo[i].accumcount = 0;
    histo[i].accumcount = accumcount;
  }

  if (Yvals.size() > 0)
  {
    for (unsigned j = 0; j < Yvals[0].size(); j++)
    {
      int ibin = (Yvals[0][j] - minbin) / binwidth;
      if (ibin < 0 || ibin >(bins - 1))
        continue;
      histo[ibin].count++;
      accumcount++;
      histo[ibin].accumcount = accumcount;
    }
  }

  return histo;
};


/* Calculate interpolation values for scatter data within bin values between
minbin and maxbin
*/
std::vector<interpolatebin> CalcInterpolations(std::vector< std::vector<double> > XYvals, 
                                          int bins, double minbin, double maxbin, int *rc)
{
	std::vector<interpolatebin> interpol;
	std::vector< std::vector<double> > shiftval;

	if (bins < 1 || minbin >= maxbin)
	{
		std::cerr << "Nonsensical value for either bins, minbin or maxbin" << std::endl;
		*rc = SQLITE_ERROR;
		return interpol;
	}

  shiftval.resize(2);
  shiftval[0].resize(bins);
  shiftval[1].resize(bins);

  interpol.resize(bins);
  double bindomain = maxbin - minbin;
  double binwidth = bindomain / bins;
  if (XYvals.size() < 1)
    return interpol;

  for (unsigned i = 0; i < interpol.size(); i++)
  {
    double upper = binwidth * (i + 1) + minbin;
    double middle = binwidth * (i + 0.5) + minbin;
    double lower = binwidth * i + minbin;
    interpol[i].xval = middle;
    interpol[i].count = 0;
  }

  for (unsigned j = 0; j < XYvals[0].size(); j++)
  {
    int ibin = (XYvals[0][j] - minbin) / binwidth;
    if (ibin < 0 || ibin >(bins - 1))
      continue;
    interpol[ibin].count++;
    shiftval[0][ibin] = shiftval[1][ibin] = 0.0;
  }

  for (unsigned j = 0; j < XYvals[0].size(); j++)
  {
    int ibin = (XYvals[0][j] - minbin) / binwidth;
    if (ibin < 0 || ibin >(bins - 1))
      continue;
    // First calculate shiftconst used for avoiding numerical instability of sigma
    // by assigning it to the mean value of values in the bin.
    // See https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Algorithm_II
    double shiftconst = 0.0;
    if (interpol[ibin].count)
      shiftconst = XYvals[1][j]/interpol[ibin].count;

    double shifted = XYvals[1][j] - shiftconst;
    shiftval[0][ibin] += shifted; // avoid numerical instability close to zero
    shiftval[1][ibin] += shifted * shifted;
    interpol[ibin].yval += XYvals[1][j];
  }

  for (unsigned i = 0; i < interpol.size(); i++)
  {
    interpol[i].yval /= interpol[i].count;
    interpol[i].sigma = sqrt((shiftval[1][i]
      - (shiftval[0][i] * shiftval[0][i]) / interpol[i].count) / interpol[i].count);
    /*
    Margin of Error (MOE) of a mean value is based on Z*sigma/sqrt(N) Z=1.96 corresponds to 95% confidence
    Z-Score Confidence Limit (%)
    3       99.73
    2.58    99
    2.33    98
    2.17    97
    2.05    96
    2.0     95.45
    1.96    95
    1.64    90
    */
    interpol[i].sem = interpol[i].sigma / sqrt(interpol[i].count);
  }

  return interpol;
}

