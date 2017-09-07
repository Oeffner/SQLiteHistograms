

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


