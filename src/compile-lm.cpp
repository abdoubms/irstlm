/******************************************************************************
 IrstLM: IRST Language Model Toolkit, compile LM
 Copyright (C) 2006 Marcello Federico, ITC-irst Trento, Italy

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA

******************************************************************************/

using namespace std;

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <stdlib.h>
#include "util.h"
#include "math.h"
#include "lmtable.h"


/* GLOBAL OPTIONS ***************/

std::string stxt = "no";
std::string sscore = "no";
std::string seval = "";
std::string sdebug = "0";
std::string smemmap = "0";
/********************************/

void usage(const char *msg = 0) {
  if (msg) { std::cerr << msg << std::endl; }
  std::cerr << "Usage: compile-lm [options] input-file.lm [output-file.blm]" << std::endl;
  if (!msg) std::cerr << std::endl
            << "  compile-lm reads a standard LM file in ARPA format and produces" << std::endl
            << "  a compiled representation that the IRST LM toolkit can quickly" << std::endl
            << "  read and process. LM file can be compressed with gzip." << std::endl << std::endl;
  std::cerr << "Options:\n"
    << "--text=[yes|no] -t=[yes|no] (output is again in text format)" << std::endl
            << "--eval=text-file -e=text-file (computes perplexity of text-file and returns)"<< std::endl
            << "--score=[yes|no] -s=[yes|no] (computes log-prob scores from standard input)"<< std::endl
            << "--debug=1 -d=1 (verbose output for --eval option)"<< std::endl
            << "--memmap=1 --mm=1 (uses memory map to read a binary LM)\n" ;
}

bool starts_with(const std::string &s, const std::string &pre) {
  if (pre.size() > s.size()) return false;

  if (pre == s) return true;
  std::string pre_equals(pre+'=');
  if (pre_equals.size() > s.size()) return false;
  return (s.substr(0,pre_equals.size()) == pre_equals);
}

std::string get_param(const std::string& opt, int argc, const char **argv, int& argi)
{
  std::string::size_type equals = opt.find_first_of('=');
  if (equals != std::string::npos && equals < opt.size()-1) {
    return opt.substr(equals+1);
  }
  std::string nexto;
  if (argi + 1 < argc) { 
    nexto = argv[++argi]; 
  } else {
    usage((opt + " requires a value!").c_str());
    exit(1);
  }
  return nexto;
}

void handle_option(const std::string& opt, int argc, const char **argv, int& argi)
{
  if (opt == "--help" || opt == "-h") { usage(); exit(1); }
  
  if (starts_with(opt, "--text") || starts_with(opt, "-t"))
    stxt = get_param(opt, argc, argv, argi);
  else
    if (starts_with(opt, "--eval") || starts_with(opt, "-e"))
      seval = get_param(opt, argc, argv, argi);
  else
    if (starts_with(opt, "--score") || starts_with(opt, "-s"))
      sscore = get_param(opt, argc, argv, argi);  
  else
    if (starts_with(opt, "--debug") || starts_with(opt, "-d"))
      sdebug = get_param(opt, argc, argv, argi);
  
  else
    if (starts_with(opt, "--memmap") || starts_with(opt, "-mm"))
      smemmap = get_param(opt, argc, argv, argi);     
  
  else {
    usage(("Don't understand option " + opt).c_str());
    exit(1);
  }
}

int main(int argc, const char **argv)
{
  
  if (argc < 2) { usage(); exit(1); }
  std::vector<std::string> files;
  for (int i=1; i < argc; i++) {
    std::string opt = argv[i];
    if (opt[0] == '-') { handle_option(opt, argc, argv, i); }
      else files.push_back(opt);
  }
  
  if (files.size() > 2) { usage("Too many arguments"); exit(1); }
  if (files.size() < 1) { usage("Please specify a LM file to read from"); exit(1); }

  bool textoutput = (stxt == "yes"? true : false);
  OUTFILE_TYPE outtype=(stxt == "yes"? TEXT : BINARY);

  int debug = atoi(sdebug.c_str()); 
  int memmap = atoi(smemmap.c_str());
  
  std::string infile = files[0];
  std::string outfile="";
  
  if (files.size() == 1) {  
    outfile=infile;
    
    //remove path information
    std::string::size_type p = outfile.rfind('/');
    if (p != std::string::npos && ((p+1) < outfile.size()))           
      outfile.erase(0,p+1);
          
    //eventually strip .gz 
    if (outfile.compare(outfile.size()-3,3,".gz")==0)
       outfile.erase(outfile.size()-3,3);
   
    outfile+=(textoutput?".lm":".blm");
  }
  else
     outfile = files[1];
 
  std::cerr << "inpfile: " << infile << std::endl;
  std::cerr << "outfile: " << outfile << std::endl;
  std::cerr << "interactive: " << sscore << std::endl;
  
  lmtable lmt; 
  
  std::cerr << "Reading " << infile << "..." << std::endl;
  inputfilestream inp(infile.c_str());
  if (!inp.good()) {
    std::cerr << "Failed to open " << infile << "!" << std::endl;
    exit(1);
  }
  
  lmt.load(inp,infile.c_str(),outfile.c_str(),memmap,outtype);       

  if (seval != ""){
    std::cerr << "Start Eval" << std::endl;
    std::cerr << "OOV code: " << lmt.dict->oovcode() << std::endl;
    ngram ng(lmt.dict);    
    std::cout.setf(ios::fixed);
    std::cout.precision(2);
   
    if (debug>0) std::cout.precision(8);
    std::fstream inptxt(seval.c_str(),std::ios::in);
    
    int Nbo=0,Nw=0,Noov=0;
    double logPr=0,PP=0,PPwp=0,Pr;
    
    int bos=ng.dict->encode(ng.dict->BoS());

#ifdef TRACE_CACHE
    lmt.init_probcache();
#endif
    
    while(inptxt >> ng){      
      
      if (ng.size>lmt.maxlevel()) ng.size=lmt.maxlevel();
      
      // reset ngram at begin of sentence
      if (*ng.wordp(1)==bos) continue;
     
      lmt.bo_state(0);
      if (ng.size>=1){ 
        logPr+=(Pr=lmt.clprob(ng));
        if (debug>0)
          std::cout << ng << "[" << ng.size << "-gram]" << " " << Pr << std::endl; 
        
        if (*ng.wordp(1) == lmt.dict->oovcode()) Noov++;        
        Nw++; if (lmt.bo_state()) Nbo++;                   
      }
    } 
    PP=exp((-logPr * log(10.0)) /Nw);
    PPwp= PP * exp(Noov * log(10000000.0-lmt.dict->size())/Nw);
    
    std::cout << "%% Nw=" << Nw << " PP=" << PP << " PPwp=" << PPwp
      << " Nbo=" << Nbo << " Noov=" << Noov 
      << " OOV=" << (float)Noov/Nw * 100.0 << "%" << std::endl;
    
      return 0;    
  };
  
  
  if (sscore == "yes"){    
    
    ngram ng(lmt.dict);    
    std::cout.setf(ios::scientific);
    std::cout << "> ";
    
    //use caches to save time
    //lmt.init_probcache();
    //lmt.init_lmtcaches(lmt.maxlevel());
    float log10=log(10.0);

    unsigned int n=0;
    while(std::cin >> ng){
      lmt.bo_state(0);
      if (ng.size>=lmt.maxlevel()){
        ++n;
        std::cout << ng << " p= " << lmt.clprob(ng) * log10;
        std::cout << " bo= " << lmt.bo_state() << std::endl;
        if ((n % 10000000)==0){ 
          std::cerr << "check cache levels" << std::endl;
          lmt.check_cache_levels();   
          //lmt.reset_mmap();
        }        
      }
      else
        std::cout << ng << " p= NULL" << std::endl;      
      std::cout << "> ";                 
    }
    
    return 0;
  }
  
  
  if (textoutput) {
    std::cout << "Saving in txt format to " << outfile << std::endl;
    lmt.savetxt(outfile.c_str());    
  } else if (!memmap) {
    std::cout << "Saving in bin format to " << outfile << std::endl;
    lmt.savebin(outfile.c_str());
  }
  return 0;
}

