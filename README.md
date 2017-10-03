 
# SQLiteHistograms
An SQLite extension library for creating histogram tables, tables of ratio between histograms and  interpolation 
tables of scatter point tables.

This SQLite extension library is based on the 'ext/misc/series.c example from the SQLite source files. 
It uses the virtual table feature of SQLite to generate tables on the fly.

The library contains three extensions: HISTO for calculating histograms of data, RATIOHISTO for calculating 
ratios of two histograms and MEANHISTO for calculating interpolated values of 2D scatter data.

## HISTO function: 

The signature of the HISTO function is as follows:  
HISTO("tablename", "columnname", nbins, minbin, maxbin)  
The tablename and the columnname must be entered with quotes or double quotes. The arguments, nbins, minbin and maxbin are the
number of histogram bins, the minimum bin value and the maximum bin value respectively.
An example is an SQLite table, "AllProteins", that contains a column of integers labelled "NumberofResiduesInModel". Rows in the table are labelled with this
number which goes up to about 1500. A histogram can be computed using the HISTO function like this:  
  SELECT * FROM HISTO("AllProteins", "NumberofResiduesInModel", 15, 0, 1500);  
This produces a histogram with 15 bins between 0 and 1500. In the SQLitebrowser the result can be visualised graphically as below:  
![alt text](histo.jpg)

## RATIOHISTO function: 

The signature of the RATIOHISTO function is as follows: 
RATIOHISTO("tablename", "columnname", nbins, minbin, maxbin,  "discrcolid", discrval);  
The tablename and the columnname must be entered with quotes or double quotes. The arguments, nbins, minbin and maxbin are the
number of histogram bins, the minimum bin value and the maximum bin value respectively. The "discrcolid" is the name of a
column with values that may be a function of the columnname values.  
Given two columns where the values in one column is a function of the values in the other RATIOHISTO creates two histograms, count1 and count2. The bin count in count1 are those "columnname" values where the corresponding "discrcolid" is smaller than discrval. The bin count in count2 are those "columnname" values where the corresponding "discrcolid" is greater than discrval. RATIOHISTO then computes a column named ratio given as count2/(count1+count2) for each bin.  

An example is an SQLite table, "AllProteins", containing a column labelled "LLGvrms". It also has a column, "CCglobal", 
which is correlated to the values in "LLGvrms" in the sense that whenever values of CCglobal are below 0.2 the 
corresponding LLGvrms value is 
below 50. Two different histograms, "count1" and "count2", can be produced when issuing the statement:  
   SELECT * FROM RATIOHISTO("AllProteins", "LLGvrms", 20, 0, 90, "CCglobal", 0.2);  
as illustrated below:  
![alt text](ratio1.jpg)

Although the two histograms peak at different bin values this can be better illustrated by computing the ratio of the "count2" 
histogram divided by the total count. These values are stored in the "ratio" column which for this particular table 
happens to be a sigmoidal curve as illustrated below:  
![alt text](ratio2.jpg)

## MEANHISTO function  

The signature for the MEANHISTO function is as follows:  
MEANHISTO('tablename', 'xcolumnname', 'ycolumnname', nbins, minbin, maxbin);  
The "tablename", the "xcolumnname" and the "ycolumnname" must be entered in quotes. The arguments, nbins, minbin and maxbin are the
number of histogram bins, the minimum bin value and the maximum bin value respectively. This function takes a 2 dimensional table
and sorts the data into nbins number of bins witth respect to the xcolumnname values. For each bin it then computes the average of
the ycolumnname values. Thus creating an interpolated curve through the 2 dimensional scatter data.  

An example is an SQLite table, "AllProteins", containing two columns labelled "FracvarVRMS1" and "LLGrefl_vrms" 
respectively. These can be illustrated in the scatter plot below with the following statement:  
  SELECT FracvarVRMS1, LLGrefl_vrms FROM AllProteins WHERE FracvarVRMS1 <= 0.6;  
![alt text](scatter.jpg)

For the "AllProteins" table there is a visible nonlinear dependency of the LLGrefl_vrms values with respect to FracvarVRMS1. 
An interpolated curve through this scatter plot can be created with the following statement:  
  SELECT * FROM MEANHISTO("AllProteins", "FracvarVRMS1", "LLGrefl_vrms", 30, 0, 0.6);  
which produces the table that is visualised below:  
![alt text](mean.jpg)



## Compile on Windows with Visual Studio 2015

cl /Fohelpers.obj /c helpers.cpp /EHsc ^  
 && cl /Foratiohistogram.obj /c ratiohistogram.cpp /EHsc ^  
 && cl /Fohistogram.obj /c histogram.cpp /EHsc ^  
 && cl /Fomeanhistogram.obj /c meanhistogram.cpp /EHsc ^  
 && cl /FoRegistExt.obj /c RegistExt.cpp /EHsc ^  
 && link /DLL /OUT:histograms.dll helpers.obj RegistExt.obj meanhistogram.obj histogram.obj ratiohistogram.obj


## Compile on Linux with g++

 g++ -fPIC -lm -shared histogram.cpp helpers.cpp meanhistogram.cpp ratiohistogram.cpp RegistExt.cpp -o histograms.so


## From the sqlite commandline load the extension

### on Windows:
 
 sqlite> .load histograms.dll
 
### on Linux:
 
 sqlite> .load ./histograms.so

