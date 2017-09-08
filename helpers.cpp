

#include "helpers.h"


std::vector<double> GetColumn(sqlite3* db, std::string sqlxprs)
{
  // get column from sql expression and return it 
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

  std::vector<double> column;
  column.clear();
  if (rc == SQLITE_OK) {
    for (int i = 0; i < ncol*nrow; ++i)
    {
      if (result[ncol + i])
      {
        std::string val(result[ncol + i]);
        column.push_back(atof(val.c_str()));
      }
    }
  }
  sqlite3_free_table(result);

  return column;
}


std::vector< std::vector<double> > GetColumns(sqlite3* db, std::string sqlxprs)
{
  // get column from sql expression and return it 
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
/* If table is structured as:
  Name        | Age
  -----------------------
  Alice       | 43
  Bob         | 28
  Cindy       | 21

  then **result will look like:

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
  if (rc == SQLITE_OK) {
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
std::vector<histobin> CalcHistogram(std::vector<double> col, int bins, double minbin, double maxbin)
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
      if (col[j] >= lower && col[j] < upper)
        histo[i].count++;
    }
  }

  return histo;
};


std::vector<interpolatebin> CalcInterpolations(std::vector< std::vector<double> > XYs, int bins, 
                                               double minbin, double maxbin)
{
  std::vector< std::vector<double> > shiftval;
  shiftval.resize(2);
  shiftval[0].resize(bins);
  shiftval[1].resize(bins);
  double shiftconst = 123.321;

  std::vector<interpolatebin> interpol;
  interpol.resize(bins);
  double bindomain = maxbin - minbin;
  double binwidth = bindomain / bins;
  for (unsigned i = 0; i < interpol.size(); i++)
  {
    double upper = binwidth * (i + 1) + minbin;
    double middle = binwidth * (i + 0.5) + minbin;
    double lower = binwidth * i + minbin;
    interpol[i].xval = middle;
    interpol[i].count = 0;
    shiftval[0][i] = shiftval[1][i] = 0.0;
    for (unsigned j = 0; j < XYs[0].size(); j++)
    {
      if (XYs[0][j] >= lower && XYs[0][j] < upper)
      {
        double shifted = XYs[1][j] + shiftconst;
        shiftval[0][i] += shifted; // avoid numerical instability close to zero
        shiftval[1][i] += shifted*shifted;

        interpol[i].yval += XYs[1][j];
        interpol[i].count++;
      }
    }
    interpol[i].yval /= interpol[i].count;
    interpol[i].sigma = sqrt( ( shiftval[1][i] - (shiftval[0][i] * shiftval[0][i])/ interpol[i].count)/ interpol[i].count);
  }

  return interpol;
}

