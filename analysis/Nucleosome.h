#ifndef NUCLEOSOME_H_
#define NUCLEOSOME_H_

#include <algorithm>
#include <math.h>
#include <vector>
#include <list>

#include <tbb/parallel_for.h>
#include <omp.h>

#include <Eigen/Dense>

#include "base/StringUtil.h"
#include "base/BEDelement.h"
#include "io/BamIO.h"
#include "io/TrackIO.h"
#include "common/Track.h"
#include "base/Types.h"
#include "analysis/AnalysisBase.h"
#include "analysis/Dist.h"
#include "base/Log.h"


struct Event
{
    int pos;
    float prob;
    float weight;
};

struct NucFrag
{  
    int start;
    int stop;
    std::vector<Event*> nucs;
    std::vector<float> nuc_probs;
};

class NucPosFinder : public Processor<float>
{
public:
    NucPosFinder()
    : reads_(NULL)
    , width_(75) 
    , alpha_(2.0)
    , num_frags_(0)
    , bin_size_(1000)
    {}
    
    virtual ~NucPosFinder() {
        while (!events_.empty()) {
            delete events_.front();
            events_.pop_front();
        }
        delete reads_;
    }
    
    void set_reads(SingleReadFactory* reads)
    { reads_ = reads; }

    SingleReadFactory* reads() 
    { return reads_; }    
    
    std::vector<NucFrag>& frags()
    { return frags_; }
    
    std::list<Event*>& events()
    { return events_; }
    
    void set_pileup(Track<int>::Ptr pileup)
    { pileup_ = pileup; }
    
    void set_nuc_width(int w)
    { width_ = w; }
    
    void set_prior(double alpha)
    { alpha_ = alpha; }
    
    const double prior() const
    { return alpha_; }
    
private:    
    void InitializeEvents();    
    void FitEM();
    void ReadsToFrags();
    
    void UpdateEmpiricalDist();
    
    void AssignEventsToFrags();
    
    int PosToBinIdx(int pos)
    {
        return floor((float)pos / (float)bin_size_);
    }

    SingleReadFactory* reads_;
    int width_;
    std::vector<NucFrag> frags_;
    std::list<Event*> events_;
    Track<int>::Ptr pileup_;
    MultiDist dist_;
    double alpha_;
    int num_frags_;
    int bin_size_;

};

class NucPileup : public Processor<int>
{
public:
    NucPileup()
    : mean_(-1.0)
    , var_(0.0)
    , start_(0)
    , stop_(0)
    , reads_(NULL)
    { 
        set_out_track_name(std::string("DEFAULT"));
        set_out_subtrack_name(std::string("DEFAULT"));
    }
    
    virtual ~NucPileup() {}
    
    void set_extends(int start, int stop)
    { start_ = start; stop_ = stop; }
    
    const float fraglen_mean() const
    { return mean_; }
    
    const float fraglen_var() const
    { return var_; }
    
    void set_reads(SingleReadFactory* reads)
    { reads_ = reads; }

    SingleReadFactory* reads() 
    { return reads_; }

    void set_start(int start)
    { start_ = start; }

    int start() 
    { return start_; }

    void set_stop(int stop)
    { stop_ = stop; }
    
    int stop()
    { return stop_; }

private:
    virtual void ComputeProcess();
    
    float mean_;
    float var_;
    int start_;
    int stop_;
    SingleReadFactory* reads_;
};

class ExtendPileup : public Processor<int>
{
public:
    ExtendPileup()
      : fraglen_(-1)
    , start_(0)
    , stop_(0)
    , reads_(NULL)
    { 
        set_out_track_name(std::string("DEFAULT"));
        set_out_subtrack_name(std::string("DEFAULT"));
    }
    
    virtual ~ExtendPileup() {}
    
    void set_extends(int start, int stop)
    { start_ = start; stop_ = stop; }
    
    void set_reads(SingleReadFactory* reads)
    { reads_ = reads; }

    SingleReadFactory* reads() 
    { return reads_; }

    void set_start(int start)
    { start_ = start; }

    int start() 
    { return start_; }

    void set_stop(int stop)
    { stop_ = stop; }
    
    int stop()
    { return stop_; }

    void set_fraglen(int f) 
    { fraglen_ = f; }
    
    int fraglen()
    { return fraglen_; }

private:
    virtual void ComputeProcess();
    int fraglen_;
    int start_;
    int stop_;
    SingleReadFactory* reads_;
};

class NucKDE : public Analysis<int, float>
{
public:
    NucKDE()
    : w_(30)
    {}
    
    virtual ~NucKDE() {}

    void set_bandwidth(int w)
    { w_ = w; }
    
    
private:
    virtual void ComputeAnalysis();
    int w_;  
};

class TBB_NucKernel
{
public: 
  TBB_NucKernel(const Track<int>& track,
                  Track<float>& output,
                float* dists,
		int w)
    : track_(track)
    , output_(output)
    , dists_(dists)
    , w_(w)
    { }
    
    void operator()(const tbb::blocked_range<size_t>& r) const; 
    float D(int pos) const;

private:
  const Track<int>& track_;
  Track<float>& output_;
  float* dists_;
  int w_;
};

class TBB_NucPositioning
{
public:
    TBB_NucPositioning(const Track<float>& temp,
                       Track<float>& output,
                       int w)
    : temp_(temp)
    , output_(output)
    , w_(w)
    {}

    void operator()(const tbb::blocked_range<size_t>& r) const; 
   
private:
    const Track<float>& temp_;
    Track<float>& output_;
    int w_;
};


struct Nuc
{
  int pos;
  float weight;
};

class NucPositioner 
{
public:
  NucPositioner() 
    : period_(200)
    , nuc_size_(147)
  {}

  virtual ~NucPositioner() {}
  
  
  const std::vector<Nuc>& nucs() const
  { return nucs_; }
  
  std::vector<BEDelement> NucPosAsBED();

  void FindNucs() 
  {
    FindMaxima();
    SelectNucs(); 
  }

  void set_chrname(const std::string& chrname) 
  {
    chrname_  = chrname;
  }

  const std::string& chrname() const
  { return chrname_; }

  void set_input(Track<float>::Ptr p)
  { input_ = p; }

  Track<float>::Ptr input()
    { return input_; }

private:
  Track<float>::Ptr input_;
  void FindMaxima();
  void SelectNucs();
  std::string chrname_;
  int period_;
  int nuc_size_;
  std::vector<Nuc> nucs_;
  std::vector<Nuc> nuc_maxima_;
};

class Histogram
{
public:
    // defaults to 10 bins
    Histogram() 
    : n_(10) 
    , counts_(10) {}
    
    Histogram(int n) 
    : n_(n)
    , counts_(n) {}
    
    virtual ~Histogram() {}
    
    void set_num_bins(int n)
    { n_ = n; }
    
    const int num_bins() const
    { return n_; }
    
    void add_to_bin(int i, int val) 
    { counts_[i] += val; }
    
    void set_bin(int i, int val) 
    { counts_[i] = val; }
    
    const int get_bin(int i) const
    { return counts_[i]; }
    
private:
    int n_; // num bins
    Eigen::ArrayXi counts_;
};
#endif
