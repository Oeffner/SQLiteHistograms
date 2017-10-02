/*
RegistExt.cpp, Robert Oeffner 2017

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




Compile on Windows:

cl /Fohelpers.obj /c helpers.cpp /EHsc ^
 && cl /Foratiohistogram.obj /c ratiohistogram.cpp /EHsc ^
 && cl /Fohistogram.obj /c histogram.cpp /EHsc ^
 && cl /Fomeanhistogram.obj /c meanhistogram.cpp /EHsc ^
 && cl /FoRegistExt.obj /c RegistExt.cpp /EHsc ^
 && link /DLL /OUT:histograms.dll helpers.obj RegistExt.obj meanhistogram.obj histogram.obj ratiohistogram.obj

With debug info:

cl /Fohelpers.obj /c helpers.cpp /DDEBUG  /ZI /EHsc ^
 && cl /Foratiohistogram.obj /c ratiohistogram.cpp /DDEBUG  /ZI /EHsc ^
 && cl /Fomeanhistogram.obj /c meanhistogram.cpp /DDEBUG  /ZI /EHsc ^
 && cl /Fohistogram.obj /c histogram.cpp /DDEBUG  /ZI /EHsc ^
 && cl /FoRegistExt.obj /c RegistExt.cpp  /DDEBUG  /ZI /EHsc ^
 && link /DLL /DEBUG /debugtype:cv /OUT:histograms.dll helpers.obj meanhistogram.obj RegistExt.obj histogram.obj ratiohistogram.obj

 
Compile on Linux:

 g++ -fPIC -lm -shared histogram.cpp helpers.cpp meanhistogram.cpp ratiohistogram.cpp RegistExt.cpp -o libhistograms.so

*/

#include "RegistExt.h"



#ifdef __cplusplus
extern "C" {
#endif

SQLITE_EXTENSION_INIT1



sqlite3 *thisdb = NULL;

#ifdef _WIN32
__declspec(dllexport)
#endif
/* The built library file name excluding its file extension must be part of the 
 function name below as documented on http://www.sqlite.org/loadext.html
*/
int sqlite3_histograms_init( // always use lower case
  sqlite3 *db,
  char **pzErrMsg,
  const sqlite3_api_routines *pApi
  )
{
  int rc = SQLITE_OK;
  SQLITE_EXTENSION_INIT2(pApi);
#ifndef SQLITE_OMIT_VIRTUALTABLE
  if (sqlite3_libversion_number()<3008012)
  {
    *pzErrMsg = sqlite3_mprintf("Histogram extension requires SQLite 3.8.12 or later");
    return SQLITE_ERROR;
  }
  rc = sqlite3_create_module(db, "HISTO", &histoModule, 0);
  rc = sqlite3_create_module(db, "RATIOHISTO", &ratiohistoModule, 0);
  rc = sqlite3_create_module(db, "MEANHISTO", &meanhistoModule, 0);

#endif
  return rc;
}



#ifdef __cplusplus
}
#endif
