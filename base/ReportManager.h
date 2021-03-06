
#ifndef REPORTMANAGER
#define REPORTMANAGER

#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <sstream>
#include <stdlib.h>

using namespace std;

class ReportMgr
{

public:

ReportMgr() {}


ReportMgr(string &file, ios::openmode mode = ios::trunc) 
{ 
  OpenOne(file, mode); 
}


void OpenFile(string &file, ios::openmode mode = ios::trunc)
{
  //  cout << "opening " << file << endl;
 OpenOne(file,mode);
}

void CloseFile(string &file)
{ 
  //cout << "closing " << file << endl; 
  CloseOne(file); 
}


void CloseAll()
{
  fmap_iter fiter = mFilesStrms.begin();
  while (fiter != mFilesStrms.end()) {
    string file = fiter->first;
    CloseFile(file);
    ++fiter;
  }

}


template <typename T>
void WritePairToFile(string &file, char* msg, T value)
{
  ostringstream o;

  o << msg << "\t" << value;

  string full(o.str());

  WriteToFile(file,full);
}

void Write(string &info)
{
  if ( mFilesStrms.size() != 1 )
  {
    cout << "need a file" << endl;
    exit(-1);
  }

  fmap_iter fiter = mFilesStrms.begin();
  (*fiter->second) << info << flush;

}

template <typename T>
void WriteToFile(string &file, T value)
{
 if ( !AlreadyOpen(file) )
  {
    cout << "Open the file: " << file << endl;
    exit(-1);
  }
  
  fmap_iter fiter = mFilesStrms.find(file);
  if ( fiter != mFilesStrms.end() )
  {
    (*fiter->second) << value << endl;
  }

}


void WriteToFile(string &file, string &info)
{
  if ( !AlreadyOpen(file) )
  {
    cout << "Open the file: " << file << endl;
    exit(-1);
  }
  
  fmap_iter fiter = mFilesStrms.find(file);
  if ( fiter != mFilesStrms.end() )
  {
    (*fiter->second) << info << endl;
  }

}


void WriteToFile(string &file, char* info)
{
  if ( !AlreadyOpen(file) )
  {
    cout << "Open the file: " << file << endl;
    exit(-1);
  }
  
  fmap_iter fiter = mFilesStrms.find(file);
  if ( fiter != mFilesStrms.end() )
  {
    (*fiter->second) << info <<endl;
  }

}

void AddReturnToFile(string &file)
{
  if ( !AlreadyOpen(file) )
  {
    cout << "Open the file: " << file << endl;
    exit(-1);
  }
  
  fmap_iter fiter = mFilesStrms.find(file);
  if ( fiter != mFilesStrms.end() )
  {
    (*fiter->second) <<"\n";
  }

}


private:
  
typedef map<string,ofstream *> fmap;
typedef fmap::iterator fmap_iter;

fmap mFilesStrms;

void OpenOne(string &file, ios::openmode mode)
{
  if ( AlreadyOpen(file) && mode == ios::app )
    return;
  
  ofstream *ostrm = new ofstream(file.c_str(), mode);
  if ( !ostrm )
  {
    OpenFailMsg(file);
    delete ostrm;
    
    exit(-1);
  }
  
  mFilesStrms.insert(make_pair(file,ostrm));
  
}

bool AlreadyOpen(string &file)
{
  fmap_iter f = mFilesStrms.find(file);
  return ( f != mFilesStrms.end() );
}

void CloseOne(string &file) 
{
  fmap_iter fiter = mFilesStrms.find(file);
  if ( fiter == mFilesStrms.end() )
  {
    cout << "Cannot close " << file << endl;
    return;
  }

  fiter->second->close();
  delete fiter->second;

  // remove all instances with key=file
  mFilesStrms.erase(file);

}


void OpenFailMsg(string &file)
{
  cout << "Cannot open " << file << endl;
}


};


#endif
