#include <string>
#include <vector>
#include <iostream>

#include <api/BamReader.h>
#include <api/BamAlignment.h>

#include "base/CommandLineParser.h"
#include "base/Types.h"
#include "io/BamIO.h"
#include "io/TrackIO.h"
#include "analysis/Nucleosome.h"

BamTools::RefVector refs;
TrackFile tio;
BamIO* bio;

void DoExtendPileup(const std::string& trackname,
		    const std::string& bamname, int extend) {
  bio = new BamIO(bamname);

  for (BamTools::RefVector::iterator it = refs.begin();
       it != refs.end(); ++it) {
      if (it->RefName.find("random") != std::string::npos
          || it->RefName.find("NT") != std::string::npos
          || it->RefName.find("chrM") != std::string::npos) {
	continue;
      } else {
	std::string chrname = it->RefName;
	std::cerr << chrname << std::endl;
	ExtendPileup pileup;
	pileup.set_fraglen(extend);
	pileup.set_out_track_name(trackname);
	pileup.set_out_subtrack_name(chrname);
	SingleReadFactory* reads = bio->LoadChrSingleReads(chrname);
	pileup.set_reads(reads);
	pileup.set_start(0);
	pileup.set_stop(it->RefLength);
	pileup.Compute();
	Track<int>::Ptr track = pileup.output();
	tio.WriteSubTrack<int>(*track);
	delete reads;
      }
  }
  delete bio;
}

void DoPileup(const std::string& trackname, 
	      const std::string& bamname) {
  bio = new BamIO(bamname);

  for (BamTools::RefVector::iterator it = refs.begin();
       it != refs.end(); ++it) {
    if (it->RefName.find("random") != std::string::npos
	|| it->RefName.find("NT") != std::string::npos
	|| it->RefName.find("chrM") != std::string::npos) {
      continue;
    } else {
      std::string chrname = it->RefName;
      std::cerr << chrname << std::endl;
      NucPileup pileup;
      pileup.set_out_track_name(trackname);
      pileup.set_out_subtrack_name(chrname);
      SingleReadFactory* reads = bio->LoadChrSingleReads(chrname);
      pileup.set_reads(reads);
      pileup.set_start(0);
      pileup.set_stop(it->RefLength);
      pileup.Compute();
      Track<int>::Ptr track = pileup.output();
      tio.WriteSubTrack<int>(*track);
      delete reads;
    }
  }
  delete bio;
}
void DoPlusMinus(const std::string& trackname, 
		  BamTools::BamReader& b) {

  PlusMinusDataInt data;


  data.plus = 0;
  data.minus = 0;

  for (BamTools::RefVector::iterator it = refs.begin();
     it != refs.end(); ++it) {
    BamTools::BamAlignment al;
    std::string chrname = it->RefName;
    data.plus = 0;
    data.minus = 0;
    std::cerr << "Loading chr " << chrname << std::endl;
    int refid = b.GetReferenceID(chrname);
    BamTools::BamRegion region(refid, 0, refid, it->RefLength);
    Track<PlusMinusDataInt>::Ptr t(new Track<PlusMinusDataInt>);
    t->set_trackname(trackname);
    t->set_subtrackname(chrname);
    t->set_extends(0, it->RefLength);
    t->set_resolution(1);
    for (size_t i = 0; i < t->size(); ++i) {
      t->set(i, data);
    }
    b.SetRegion(region);
    
    int num_reads = 0;
    while (b.GetNextAlignmentCore(al)) {
      data = t->get((size_t)al.Position);
      if (al.IsReverseStrand()) {
	data.minus += 1;	
      } else {
	data.plus += 1;
      }
      t->set((size_t)al.Position, data);
    }
    //    b.Rewind();

    tio.WriteSubTrack<PlusMinusDataInt>(*t);
    t.reset();
  }
}

int main(int argc, char** argv) {

  commandArg<std::string> bBam("-b", "In BAM file");
  commandArg<std::string> oDataPath("-o", "Trackfile name");
  commandArg<std::string> tTrackName("-t", "Out trackname");
  commandArg<int> pPileup("-p", "Do paired end pileup (p=1), or do plus minus start (p=0)", 1);
  commandArg<int> eExtend("-e", "Extend read by length", -1);

  commandLineParser P(argc, argv);
  P.SetDescription("Save pileup of read starts from BAM");

  P.registerArg(bBam);
  P.registerArg(oDataPath);
  P.registerArg(tTrackName);
  P.registerArg(pPileup);
  P.registerArg(eExtend);
  P.parse();


  std::string bamname = P.GetStringValueFor(bBam);
  std::string trackfilename = P.GetStringValueFor(oDataPath);
  std::string trackname = P.GetStringValueFor(tTrackName);
  int do_pileup = P.GetIntValueFor(pPileup);
  int extend = P.GetIntValueFor(eExtend);

  BamTools::BamReader b;
  if (!b.Open(bamname)) {
    std::cerr << "Couldn't open input BAM file." << std::endl;
    return -1;
  }
  
  refs = b.GetReferenceData();
  tio.Open(trackfilename);

  if (extend > 0) {
    b.Close();
    DoExtendPileup(trackname, bamname, extend);
    return -1;
  }

  if (do_pileup == 1) {
    b.Close();
    DoPileup(trackname, bamname);
    return -1;
  } else if (do_pileup == 0) {
    DoPlusMinus(trackname,b);
    return -1;
  }
}
