/**
 * @(#) $Id$
 * Gwyddion plug-in proxy dump file handling.
 * Written by Yeti <yeti@gwyddion.net>.  Public domain.
 **/
#include <dump.hh>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <cstring>

using namespace std;

/***************** DataField **********************/
DataField::DataField(unsigned long int xres_, unsigned long int yres_)
{
    xres = xres_;
    yres = yres_;
    data = new double [xres*yres];
    xreal = 0.0;
    yreal = 0.0;
    xyunits = string("");
    zunits = string("");
}

DataField::DataField(const DataField &dfield)
{
    xres = dfield.xres;
    yres = dfield.yres;
    data = new double [xres*yres];
    memcpy(data, dfield.data, xres*yres*sizeof(double));
    xreal = dfield.xreal;
    yreal = dfield.yreal;
    xyunits = string(dfield.xyunits);
    zunits = string(dfield.zunits);
}

DataField::~DataField()
{
    delete [] data;
}

/***************** Dump **********************/
bool
Dump::read(const char *filename)
{
    typedef pair<string,string>    MetaValue;
    typedef pair<string,DataField> DataValue;

    meta.clear();
    data.clear();

    ifstream fh(filename, ifstream::in | ifstream::binary);
    if (!fh)
        return false;

    int lineno = 0;
    const unsigned long int buf_len = 4096;
    char line_buf[buf_len];
    while (fh.good() && !fh.eof()) {
        string line("");

        lineno++;
        /* read a line, no matter how long */
        unsigned long int appended;
        do {
            if (!fh.getline(line_buf, buf_len)) {
                if (fh.eof() && !line.size())
                    return true;
                cerr << "Cannot read line #" << lineno << endl;
                meta.clear();
                data.clear();
                return false;
            }
            appended = line.size();
            line.append(line_buf);
            appended = line.size() - appended;
        } while (appended == buf_len - 1);

        unsigned long int eqpos = line.find('=');
        if (line.empty())
            continue;
        if (line[0] != '/' || eqpos == string::npos) {
            cerr << "Invalid line #" << lineno << endl;
            meta.clear();
            data.clear();
            return false;
        }

        MetaValue value = MetaValue(string(line, 0, eqpos),
                                    string(line, eqpos + 1));
        /* metadata */
        if (value.second != "[") {
            meta.insert(value);
            continue;
        }

        /* datafield */
        if (fh.get() != '[') {
            cerr << "Expected `[' marker at start of data field" << endl;
            meta.clear();
            data.clear();
            return false;
        }
        {
            map<string,string>::iterator iter;

            if ((iter = meta.find(value.first + "/xres")) == meta.end()) {
                cerr << "No data field " << value.first << " x-resolution" 
                     << endl;
                meta.clear();
                data.clear();
                return false;
            }
            unsigned long int xres = atoi(iter->second.c_str());
            meta.erase(iter);

            if ((iter = meta.find(value.first + "/yres")) == meta.end()) {
                cerr << "No data field " << value.first << " y-resolution" 
                     << endl;
                meta.clear();
                data.clear();
                return false;
            }
            unsigned long int yres = atoi(iter->second.c_str());
            meta.erase(iter);

            unsigned long int size = xres*yres*sizeof(double);
            DataField dfield = DataField(xres, yres);

            unsigned long int n = 0;
            do {
                n += fh.readsome(((char*)dfield.data) + n, size - n);
            } while (n < size && !fh.eof() && fh.good());
            if (n < size) {
                cerr << "Truncated data field" << endl;
                meta.clear();
                data.clear();
                return false;
            }
            if (fh.get() != ']' || fh.get() != ']' || fh.get() != '\n') {
                cerr << "Missed end of data field" << endl;
                meta.clear();
                data.clear();
                return false;
            }

            if ((iter = meta.find(value.first + "/xreal")) != meta.end()) {
                dfield.xreal = strtod(iter->second.c_str(), NULL);
                meta.erase(iter);
            }
            if ((iter = meta.find(value.first + "/yreal")) != meta.end()) {
                dfield.yreal = strtod(iter->second.c_str(), NULL);
                meta.erase(iter);
            }
            if ((iter = meta.find(value.first + "/xyunits")) != meta.end()) {
                dfield.xyunits = iter->second;
                meta.erase(iter);
            }
            if ((iter = meta.find(value.first + "/zunits")) != meta.end()) {
                dfield.zunits = iter->second;
                meta.erase(iter);
            }

            data.insert(DataValue(value.first, dfield));
        }
    }
    {
        bool ok = fh.eof();
        fh.close();
        return ok;
    }
}

bool
Dump::write(const char *filename)
{
    ofstream fh(filename, ifstream::out | ifstream::binary);
    if (!fh)
        return false;
    {
        for (map<string,string>::iterator iter = meta.begin();
             iter != meta.end();
             iter++)
            fh << iter->first << "=" << iter->second << endl;
    }

    for (map<string,DataField>::iterator iter = data.begin();
         iter != data.end();
         iter++) {
        fh << iter->first << "/xres=" << iter->second.xres << endl;
        fh << iter->first << "/yres=" << iter->second.yres << endl;
        if (iter->second.xreal > 0.0)
            fh << iter->first << "/xreal=" << iter->second.xreal << endl;
        if (iter->second.yreal > 0.0)
            fh << iter->first << "/yreal=" << iter->second.yreal << endl;
        if (iter->second.xyunits.size())
            fh << iter->first << "/xyunits=" << iter->second.xyunits << endl;
        if (iter->second.zunits.size())
            fh << iter->first << "/zunits=" << iter->second.zunits << endl;

        fh << iter->first << "=[" << endl;
        fh.put('[');
        unsigned long int size = iter->second.xres * iter->second.yres
                                 * sizeof(double);
        fh.write(((char*)iter->second.data), size);
        fh << "]]" << endl;
    }
    fh.close();

    return true;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
