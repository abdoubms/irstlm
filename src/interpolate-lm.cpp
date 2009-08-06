// $Id: compile-lm.cpp 252 2009-05-08 08:58:20Z mfederico $

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
#include <sstream>
#include <vector>
#include <string>
#include <stdlib.h>
#include "util.h"
#include "math.h"
//#include "dictionary.h"
#include "lmtable.h"

/* GLOBAL OPTIONS ***************/

std::string slearn = "";
std::string seval = "";
std::string sorder = "";
std::string sscore = "no";
std::string sdebug = "0";
std::string smemmap = "0";
std::string sdub = "10000000";


/********************************/

lmtable *load_lm(std::string file,int dub,int memmap);

void usage(const char *msg = 0) {
  if (msg) { std::cerr << msg << std::endl; }
  std::cerr << "Usage: interpolate-lm [options] lm-list-file [lm-list-file.out]" << std::endl;
  if (!msg) std::cerr << std::endl
            << "  interpolate-lm reads a LM list file including interpolation weights " << std::endl
            << "  with the format: N\\n w1 lm1 \\n w2 lm2 ...\\n wN lmN\n" << std::endl
            << "  It estimates new weights on a development text, " << std::endl
			<< "  computes the perplexity on an evaluation text, " << std::endl
			<< "  computes probabilities of n-grams read from stdin." << std::endl
			<< "  It reads LMs in ARPA and IRSTLM binary format." << std::endl  << std::endl;
			
  std::cerr << "Options:\n"
            << "--learn text-file -l=text-file (learns new weights and creates a new lm-list-file)"<< std::endl
            << "--order n -o=n (specify order of n-grams to use during learning: default is max order of LMs)"<< std::endl
            << "--eval text-file -e=text-file (computes perplexity of interpolation on text-file)"<< std::endl
            << "--dub dict-size (dictionary upperbound to compute OOV word penalty: default 10^7)"<< std::endl
            << "--score [yes|no] -s=[yes|no] (compute log-probs of n-grams read from stdin) "<< std::endl
            << "--debug 1 -d 1 (verbose output for --eval option)"<< std::endl
            << "--memmap 1 -mm 1 (uses memory map to read a binary LM)\n" ;
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
  
  if (starts_with(opt, "--learn") || starts_with(opt, "-l"))
    slearn = get_param(opt, argc, argv, argi);
  else
    if (starts_with(opt, "--order") || starts_with(opt, "-o"))
      sorder = get_param(opt, argc, argv, argi);
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
  
  else
    if (starts_with(opt, "--dub") || starts_with(opt, "-dub"))
      sdub = get_param(opt, argc, argv, argi);     
  
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
	
	bool learn = (slearn != ""? true : false);
	bool score = (sscore != ""? true : false);
	int order=(sorder!=""?atoi(sorder.c_str()):0);
	int debug = atoi(sdebug.c_str()); 
	int memmap = atoi(smemmap.c_str());
	int dub = atoi(sdub.c_str()); //dictionary upper bound

	if (sorder != "" && order < 1) {usage("Order must be a positive integer"); exit(1);} 

	if (files.size() > 2) { usage("Too many arguments"); exit(1); }
	if (files.size() < 1) { usage("Please specify a LM list file to read from"); exit(1); }
			
	std::string infile = files[0];
	std::string outfile="";
	
	if (files.size() == 1) {  
		outfile=infile;    
		//remove path information
		std::string::size_type p = outfile.rfind('/');
		if (p != std::string::npos && ((p+1) < outfile.size()))           
			outfile.erase(0,p+1);
		outfile+=".out";
	}
	else
		outfile = files[1];
	
	std::cerr << "inpfile: " << infile << std::endl;

	if (learn) std::cerr << "outfile: " << outfile << std::endl;
	if (score) std::cerr << "interactive: " << sscore << std::endl;
	std::cerr << "order: " << order << std::endl;
	if (memmap) std::cerr << "memory mapping: " << memmap << std::endl;

	std::cerr << "dub: " << dub<< std::endl;

	
	lmtable *lmt[100], *start_lmt[100]; //interpolated language models
	std::string lmf[100]; //lm filenames
		
	float w[100]; //interpolation weights
	int N;
	
	
	//Loading Language Models
	std::cerr << "Reading " << infile << "..." << std::endl;  
	std::fstream inptxt(infile.c_str(),std::ios::in);
	inptxt >> N; std::cerr << "Number of LMs: " << N << "..." << std::endl;   
	
	if(N > 100) {
		std::cerr << "Can't interpolate more than 100 language models." << std::endl;
		exit(1);
	}

	for (int i=0;i<N;i++){
		inptxt >> w[i] >> lmf[i];
		start_lmt[i] = lmt[i] = load_lm(lmf[i],dub,memmap);
	}
	inptxt.close();
	
	//Learning mixture weights
	if (learn){
	
		std::vector<float> p[N]; //LM probabilities
		float c[N]; //expected counts
		float den,norm; //inner denominator, normalization term
		float variation=1.0; // global variation between new old params
		
		dictionary* dict;dict=new dictionary((char*)slearn.c_str(),1000000,(char*)NULL,(char*)NULL);
		ngram ng(dict); 
		int bos=ng.dict->encode(ng.dict->BoS());
		std::ifstream dev(slearn.c_str(),std::ios::in);

		for(;;) {
			std::string line;
			getline(dev, line);
			if(dev.eof())
				break;
			if(dev.fail()) {
				std::cerr << "Problem reading input file " << seval << std::endl;
				return 1;
			}
			std::istringstream lstream(line);
			if(line.substr(0, 29) == "###interpolate-lm:replace-lm ") {
				std::string token, newlm;
				int id;
				lstream >> token >> id >> newlm;
				if(id <= 0 || id > N) {
					std::cerr << "LM id out of range." << std::endl;
					return 1;
				}
				id--; // count from 0 now
				if(lmt[id] != start_lmt[id])
					delete lmt[id];
				lmt[id] = load_lm(newlm,dub,memmap);
				continue;
			}
			while(lstream >> ng){     
				
				// reset ngram at begin of sentence
				if (*ng.wordp(1)==bos) {ng.size=1;continue;}
				if (order > 0 && ng.size > order) ng.size=order;				
				for (int i=0;i<N;i++){
					ngram ong(lmt[i]->dict);ong.trans(ng);
					p[i].push_back(pow(10.0,lmt[i]->clprob(ong))); //LM log-prob						
				}	
			}	
		
			for (int i=0;i<N;i++) lmt[i]->check_cache_levels();
		}
		dev.close();

		while( variation > 0.01 ){ 
			
			for (int i=0;i<N;i++) c[i]=0;	//reset counters
			
			for(unsigned i = 0; i < p[0].size(); i++) {
				den=0.0;	
				for(int j = 0; j < N; j++)
					den += w[j] * p[j][i]; //denominator of EM formula
				//update expected counts
				for(int j = 0; j < N; j++)
					c[j] += w[j] * p[j][i] / den;
			}

			norm=0.0; 
			for (int i=0;i<N;i++) norm+=c[i];
			
			//update weights and compute distance 			
			variation=0.0;
			for (int i=0;i<N;i++){
				c[i]/=norm; //c[i] is now the new weight
				variation+=(w[i]>c[i]?(w[i]-c[i]):(c[i]-w[i]));
				w[i]=c[i]; //update weights
			}
			std::cerr << "Variation " << variation << std::endl;  
		}
		
		//Saving results
		std::cerr << "Saving in " << outfile << "..." << std::endl; 
		//saving result
		std::fstream outtxt(outfile.c_str(),std::ios::out);
		outtxt << N << "\n";
		for (int i=0;i<N;i++) outtxt << w[i] << " " << lmf[i] << "\n";
		outtxt.close();
	}
	
	for(int i = 0; i < N; i++)
		if(lmt[i] != start_lmt[i]) {
			delete lmt[i];
			lmt[i] = start_lmt[i];
		}

	if (seval != ""){
		std::cerr << "Start Eval" << std::endl;
		
		std::cout.setf(ios::fixed);
		std::cout.precision(2);
		int i,Nw=0,Noov=0, Nbo=0;
		double logPr=0,PP=0,Pr;
		
		//normalize weights
		for (i=0,Pr=0;i<N;i++) Pr+=w[i];
		for (i=0;i<N;i++) w[i]/=Pr;
		
		dictionary* dict;dict=new dictionary((char*)seval.c_str(),1000000,(char*)NULL,(char*)NULL);
		ngram ng(dict); 
		int bos=ng.dict->encode(ng.dict->BoS()); 
		int eos=ng.dict->encode(ng.dict->EoS());
   
		std::fstream inptxt(seval.c_str(),std::ios::in);
		
		for(;;) {
			std::string line;
			getline(inptxt, line);
			if(inptxt.eof())
				break;
			if(inptxt.fail()) {
				std::cerr << "Problem reading input file " << seval << std::endl;
				return 1;
			}
			std::istringstream lstream(line);
			if(line.substr(0, 26) == "###interpolate-lm:weights ") {
				std::string token;
				lstream >> token;
				for(int i = 0; i < N; i++) {
					if(lstream.eof()) {
						std::cerr << "Not enough weights!" << std::endl;
						return 1;
					}
					lstream >> w[i];
				}
				continue;
			}
			if(line.substr(0, 29) == "###interpolate-lm:replace-lm ") {
				std::string token, newlm;
				int id;
				lstream >> token >> id >> newlm;
				if(id <= 0 || id > N) {
					std::cerr << "LM id out of range." << std::endl;
					return 1;
				}
				id--; // count from 0 now
				delete lmt[id];
				lmt[id] = load_lm(newlm,dub,memmap);
				continue;
			}

			double bow; int bol=0;
			
					
			while(lstream >> ng){      
				
				// reset ngram at begin of sentence
				if (*ng.wordp(1)==bos) {ng.size=1;continue;}
				if (order > 0 && ng.size > order) ng.size=order;	
				
				if (ng.size>=1){	
				
					int  minbol=MAX_NGRAM; //minimum backoff level of the mixture			
					bool OOVflag=true;	  //OOV flag
								
					for (i=0,Pr=0;i<N;i++){
						ngram ong(lmt[i]->dict);ong.trans(ng);
						Pr+=w[i] * pow(10.0,lmt[i]->lprob(ong,&bow,&bol)); //LM log-prob	
						if (bol < minbol) minbol=bol; //backoff of LM[i]						
			        	if (*ong.wordp(1) != lmt[i]->dict->oovcode()) OOVflag=false;  //OOV wrt to LM[i]						
					}
					
					logPr+=(log(Pr)/M_LN10);
					
					if (debug==1){
						std::cout << ng.dict->decode(*ng.wordp(1)) << "[" << ng.size-minbol << "]" << " "; 
						if (*ng.wordp(1)==eos) std::cout << std::endl;
					}
					if (debug==2)
						std::cout << ng << "[" << ng.size-minbol << "-gram]" << " " << logPr << std::endl; 
					
					if (debug==3)
						std::cout << ng << "[" << ng.size-minbol << "-gram]" << " " << logPr << " bow:" << bow << std::endl; 
					
					
					if (minbol) Nbo++; //all LMs have back-offed by at least one
					
					if (OOVflag) Noov++;
				
					Nw++;  
					
					if ((Nw % 10000)==0) std::cerr << ".";
				}
			}
		}

		PP=exp((-logPr * M_LN10) /Nw);
		
		std::cout << "%% Nw=" << Nw << " PP=" << PP
			<< " Nbo=" << Nbo << " Noov=" << Noov
			<< " OOV=" << (float)Noov/Nw * 100.0 << "%" << std::endl;
		
	};
	
	
	if (sscore == "yes"){
		
		
		dictionary* dict;dict=new dictionary(NULL,1000000,(char*)NULL,(char*)NULL);
		dict->incflag(1); // start generating the dictionary;
		ngram ng(dict); 
		double Pr,logPr;
		
		//use caches to save time
		//lmt.init_probcache();
		//lmt.init_lmtcaches(lmt.maxlevel());
		
     	unsigned int maxstatesize, statesize;
		int i,n=0;
		std::cout << "> ";	
		while(std::cin >> ng){
			n++;
			maxstatesize=0;
			for (i=0,Pr=0;i<N;i++){
				Pr+=w[i] * pow(10.0,lmt[i]->clprob(ng)); //LM log-prob	
				lmt[i]->maxsuffptr(ng,&statesize);
				if (maxstatesize<statesize) maxstatesize=statesize;	
			};
			logPr=log(Pr);
			
			ng.size=maxstatesize;
			std::cout << "recombine= " << maxstatesize << " " << ng << " p= " << logPr  << std::endl;
			
			if ((n % 10000000)==0){ 
				std::cerr << "check cache levels" << std::endl;
				for (i=0;i<N;i++) lmt[i]->check_cache_levels();   
			}
			
			
			std::cout << "> ";                 
		}
		

	}

	for (int i=0;i<N;i++) delete lmt[i];
	
	return 0;
}

lmtable *load_lm(std::string file,int dub,int memmap) {
	inputfilestream inplm(file.c_str());
	std::cerr << "Reading " << file.c_str() << "..." << std::endl;  
	lmtable *lmt=new lmtable;
	if (file.compare(file.size()-3,3,".mm")==0)
		lmt->load(inplm,file.c_str(),NULL,1,NONE);   		
	else 
		lmt->load(inplm,file.c_str(),NULL,memmap,NONE);   		
	if (dub) lmt->setlogOOVpenalty(dub);	//set OOV Penalty for each LM
	lmt->init_probcache();
	return lmt;
}
