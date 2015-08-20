#include <iomanip> 
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <sstream>
#include <vector>
#include <cstdint>

#include <TFile.h>
#include <TNtuple.h>
#include <TH2.h>
#include <TProfile.h>
#include <TCanvas.h>
#include <TFrame.h>
#include <TROOT.h>
#include <TSystem.h>
#include <TRandom3.h>
#include <TBenchmark.h>
#include <TInterpreter.h>
#include <TApplication.h>
#include <TString.h>
#if defined(__CINT__) && !defined(__MAKECINT__)
    #include "libEvent.so"
#else
    #include "Event.h"
#endif

#include "gem/readout/GEMDataAMCformat.h"

/**
* GEM Tree Composer (gtc) application provides translation of the GEM output HEX file to ROOT-based TTree with GEM Events
*/

/*! \file */
/*! 
  \mainpage GEM Tree Composer application.

  HEX data reading module for light DQM package. Converts HEX data to ROOT TTree.

  \section Installation

  Getting source code:

  git clone https://github.com/mexanick/gem-daq-code.git <br>
  cd gem-daq-code/
  git checkout lightDQM

  Make the executable:
  cd gem-light-dqm/gemtreewriter/
  make -j4

  Now you will have executables:
  - GEM Tree Writer bin/gtc
  - Example of GEMtree reader bin/reader
  Then run the executable providing input and output filenames:
  ./bin/gtc GEMDQMRawData.dat outputROOTtree.root

  You can download sample HEX-ASCII data file:
  wget https://baranov.web.cern.ch/baranov/xdaq/DataParker/GEM_DAQ_Tue_Jul_14_10-13-10_2015.dat

  After producing the outputROOTtree.root you can run the tree reader example, which should serve as basis for future analyzers:
  ./bin/reader outputROOTtree.root outputAnalyzer.root

  The source code for the reader you can find here:
  ./src/common/treeReaderExample.cxx

  All the compilation settings you can find in the Makefile

  \author Sergey.Baranov@cern.ch, Mykhailo.Dalchenko@cern.ch
*/

using namespace std;

 /*
  *  CRC ******************************************************************
  */
      uint16_t dataVFAT[11];

      uint16_t crc_calc(uint16_t crc_in, uint16_t dato){
      uint16_t v = 0x0001;
      uint16_t mask = 0x0001;    
      bool d=0;
      uint16_t crc_temp = crc_in;
      unsigned char datalen = 16;
       
      for (int i=0; i<datalen; i++){
        if (dato & v) d = 1;
        else d = 0;
        if ((crc_temp & mask)^d) crc_temp = crc_temp>>1 ^ 0x8408;
        else crc_temp = crc_temp>>1;
        v<<=1;
      }
      return(crc_temp);
    }

    // unsigned short int 
    uint16_t checkCRC(bool OKprint){
      uint16_t crc_fin = 0xffff;
      for (int i = 11; i >= 1; i--){
        crc_fin = crc_calc(crc_fin, dataVFAT[i]);
	/*
	if(OKprint){
          cout << " dataVFAT[" << std::setfill('0') << std::setw(2) << i << "] " << hex << std::setfill('0') << std::setw(4) << dataVFAT[i]
               << " crc_temp " << std::setfill('0') << std::setw(4) << crc_fin << dec << endl;
        }
	*/
      }
      return(crc_fin);
    }

//! GEM VFAT2 Data class.
/*!
  \brief GEMOnline
  contents VFAT2 GEM data format 
  \author Sergey.Baranov@cern.ch
*/

//! root function.
/*!
https://root.cern.ch/drupal/content/documentation
*/

// Ok printing
bool OKprint(int ievent, int iMaxPrint ){
  if( ievent <= iMaxPrint ){
    return (true);
  } else { 
    return (false);
  }
}

TROOT root("",""); // static TROOT object

//! main function.
/*!
C++ any documents
*/

int main(int argc, char** argv)
{  
    cout<<"---> Main()"<<endl;
    if (argc<3) 
    {
        cout << "Please provide input and output filenames" << endl;
        cout << "Usage: <path>/gtc inputFile.dat outputFile.root" << endl;
        return 0;
    }
    string ifile=argv[1];
    const TString ofile = argv[2];
    ifstream inpf(ifile.c_str());
    if(!inpf.is_open()) {
      cout << "\nThe file: " << ifile.c_str() << " is missing.\n" << endl;
      return 0;
    };

    TFile *hfile = new TFile(ofile,"RECREATE","Threshold Scan ROOT file with histograms");
    TTree GEMtree("GEMtree","A Tree with GEM Events");
    Event *ev = new Event(); 
    GEMtree.Branch("GEMEvents", &ev);

    gem::readout::GEMData   gem;
    gem::readout::GEBData   geb;
    gem::readout::VFATData vfat;

    const Int_t ieventPrint = 3;
    const Int_t ieventMax   = 9000000;
    const Int_t kUPDATE     = 10;
    bool OKpri = false;

    for(int ievent=0; ievent<ieventMax; ievent++)
    {
        OKpri = OKprint(ievent,ieventPrint);
        if(inpf.eof()) break;
        if(!inpf.good()) break;

        cout << "Processing event " << ievent << endl;

       /*
        *  GEM Headers Data level
        */
      
        if(!gem::readout::readGEMhd1(inpf, gem)) break;
        if(!gem::readout::readGEMhd2(inpf, gem)) break;
        if(!gem::readout::readGEMhd3(inpf, gem)) break;

       /*
        *  GEB Headers Data level
        */

        gem::readout::readGEBheader(inpf, geb);
        if(OKpri) gem::readout::printGEBheader(ievent,geb);

        uint32_t ZSFlag  = (0xffffff0000000000 & geb.header) >> 40; 
        uint16_t ChamID  = (0x000000fff0000000 & geb.header) >> 28; 
        uint32_t sumVFAT = (0x000000000fffffff & geb.header);

        gem::readout::readGEBrunhed(inpf, geb);

       /*
        *  GEB PayLoad Data
        */
        GEBdata *GEBdata_ = new GEBdata(ZSFlag, ChamID, sumVFAT);

        for(int ivfat=0; ivfat<sumVFAT; ivfat++){

            if(!gem::readout::readVFATdata(inpf, ievent, vfat)) break;

            uint8_t   b1010  = (0xf000 & vfat.BC) >> 12;
            uint16_t  BC     = (0x0fff & vfat.BC);
            uint8_t   b1100  = (0xf000 & vfat.EC) >> 12;
            uint8_t   EC     = (0x0ff0 & vfat.EC) >> 4;
            uint8_t   Flag   = (0x000f & vfat.EC);
            uint8_t   b1110  = (0xf000 & vfat.ChipID) >> 12;
            uint16_t  ChipID = (0x0fff & vfat.ChipID);
            uint16_t  CRC    = vfat.crc;
            uint16_t  BX     = vfat.BXfrOH;  
            uint64_t  lsData = vfat.lsData;
            uint64_t  msData = vfat.msData;

            // CRC check
            dataVFAT[11] = vfat.BC;
            dataVFAT[10] = vfat.EC;
            dataVFAT[9]  = vfat.ChipID;
            dataVFAT[8]  = (0xffff000000000000 & vfat.msData) >> 48;
            dataVFAT[7]  = (0x0000ffff00000000 & vfat.msData) >> 32;
            dataVFAT[6]  = (0x00000000ffff0000 & vfat.msData) >> 16;
            dataVFAT[5]  = (0x000000000000ffff & vfat.msData);
            dataVFAT[4]  = (0xffff000000000000 & vfat.lsData) >> 48;
            dataVFAT[3]  = (0x0000ffff00000000 & vfat.lsData) >> 32;
            dataVFAT[2]  = (0x00000000ffff0000 & vfat.lsData) >> 16;
            dataVFAT[1]  = (0x000000000000ffff & vfat.lsData);
    
            uint16_t checkedCRC = checkCRC(OKpri);
    
            VFATdata *VFATdata_ = new VFATdata(b1010, BC, b1100, EC, Flag, b1110, ChipID, lsData, msData, CRC);
            GEBdata_->addVFATData(*VFATdata_);
            delete VFATdata_;

            if(OKpri) gem::readout::printVFATdataBits(ievent, vfat);

        }//end of GEB PayLoad Data

       /*
        *  GEB Trailers Data level
        */
        if(!gem::readout::readGEBtrailer(inpf, geb)) break;

        uint16_t OHcrc      = (0xffff000000000000 & geb.trailer) >> 48; 
        uint16_t OHwCount   = (0x0000ffff00000000 & geb.trailer) >> 32; 
        uint16_t ChamStatus = (0x00000000ffff0000 & geb.trailer) >> 16;
        uint16_t GEBres     = (0x000000000000ffff & geb.trailer);

        GEBdata_->setTrailer(OHcrc, OHwCount, ChamStatus, GEBres);

       /*
        *  GEM Trailers Data level
        */
        if(!gem::readout::readGEMtr2(inpf, gem)) break;
        if(!gem::readout::readGEMtr1(inpf, gem)) break;
    
        ev->Build(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
        ev->addGEBdata(*GEBdata_);
        GEMtree.Fill();
        ev->Clear();

        if(OKpri)
        {
            cout << "GEM Chamber Trailer: OHcrc " << hex << OHcrc 
            << " OHwCount " << OHwCount << " ChamStatus " << ChamStatus << dec 
            << " ievent " << ievent << endl;
        }
    }// End loop on events
    inpf.close();
    hfile->Write();// Save file with tree
    cout<<"=== hfile->Write()"<<endl;
    return 0;
}
