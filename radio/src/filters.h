#pragma once

#define MAX_PARAMS 5
#define PARAM_CHAR_LEN 15

// Filter Testing
class Filter
{
 public:
  Filter()
  {
    for (int i = 0; i < MAX_PARAMS; i++) {
      params[i] = 0;
      paramstr[i][0] = '\0';
    }
  }

  virtual float doFilter(int channel, float value) = 0;
  virtual const char *getFilterName() = 0;
  virtual void setParameter(int p, float val)
  {
    if (p < MAX_PARAMS) params[p] = val;
  }
  float getParameter(int p) { return params[p]; }
  const char *getParameterStr(int p) { return paramstr[p]; }
  int getParameterCount() { return paramcount; }

 protected:
  void addFilter(std::string filt) {}

  void addParameter(const char *str, float initialval)
  {
    if (paramcount < MAX_PARAMS) {
      strncpy(paramstr[paramcount], str, PARAM_CHAR_LEN);
      paramstr[paramcount][PARAM_CHAR_LEN - 1] = '\0';
      paramcount++;
    }
  };

  float getPeriod()
  {
    return 1.0f / 143.0f;  // *** fixme
  }

  int paramcount = 0;
  float params[MAX_PARAMS];
  char paramstr[MAX_PARAMS][PARAM_CHAR_LEN];
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// 1 Euro Filter

typedef struct {
  float minCutoffFrequency;
  float cutoffSlope;
  float derivativeCutoffFrequency;
} SF1eFilterConfiguration;

typedef struct {
  float hatxprev;
  float xprev;
  char usedBefore;
} SFLowPassFilter;

typedef struct {
  SF1eFilterConfiguration config;
  SFLowPassFilter xfilt;
  SFLowPassFilter dxfilt;
  double lastTime;
} SF1eFilter;

class OneEuroFilter : public Filter
{
 public:
  OneEuroFilter() : Filter()
  {
    addParameter("Min Cut Off Freq", 0.06);
    addParameter("Cut Off Slope", 0.1);
    addParameter("Derv C/O Freq", 1.0);

    for (int i = 0; i < NUM_ANALOGS; i++) {
      oneeufilters[i].xfilt.usedBefore = 0;
      oneeufilters[i].xfilt.hatxprev = 0;
      oneeufilters[i].xfilt.xprev = 0;
      oneeufilters[i].dxfilt.usedBefore = 0;
      oneeufilters[i].dxfilt.hatxprev = 0;
      oneeufilters[i].dxfilt.xprev = 0;
      oneeufilters[i].config = sf1econf;
    }
  }

  float doFilter(int channel, float value) override
  {
    curperiod = getPeriod();
    oneeufilters[channel].config = sf1econf;  // Update configuration on the fly
    return SF1eFilterDo(&oneeufilters[channel], value);
  }

  const char *getFilterName() override { return "One Euro"; }

  void setParameter(int p, float val) override
  {
    Filter::setParameter(p, val);
    sf1econf.minCutoffFrequency =
        params[0];  // addParameter("Min Cut Off Freq", 0.06);
    sf1econf.cutoffSlope = params[1];  // addParameter("Cut Off Slope", 0.1);
    sf1econf.derivativeCutoffFrequency =
        params[2];  // addParameter("Derv C/O Freq", 1.0);
  }

 private:
  SF1eFilter oneeufilters[NUM_ANALOGS];
  SF1eFilterConfiguration sf1econf;
  float curperiod = 1.0f / 100.0;

  float SFLowPassFilterDo(SFLowPassFilter *filter, float x, float alpha)
  {
    if (!filter->usedBefore) {
      filter->usedBefore = 1;
      filter->hatxprev = x;
    }
    float hatx = alpha * x + (1.f - alpha) * filter->hatxprev;
    filter->xprev = x;
    filter->hatxprev = hatx;
    return hatx;
  }

  float SF1eFilterAlpha(SF1eFilter *filter, float cutoff)
  {
    float tau = 1.0f / (2.f * 3.14159265359 * cutoff);
    return 1.0f / (1.0f + tau / curperiod);
  }

  float SF1eFilterDo(SF1eFilter *filter, float x)
  {
    float dx = 0.f;

    if (filter->xfilt.usedBefore) {
      dx = (x - filter->xfilt.xprev) * (1.0f / curperiod);
    }

    float edx = SFLowPassFilterDo(
        &(filter->dxfilt), dx,
        SF1eFilterAlpha(filter, filter->config.derivativeCutoffFrequency));
    float cutoff = filter->config.minCutoffFrequency +
                   filter->config.cutoffSlope * fabsf(edx);
    return SFLowPassFilterDo(&(filter->xfilt), x,
                             SF1eFilterAlpha(filter, cutoff));
  }
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Jitter Filter (Modified Moving Average)

class JitterFilter : public Filter
{
 public:
  JitterFilter() : Filter()
  {
    addParameter("Jitter Alpha", 4);  // Can't add to more than 5
  }

  float doFilter(int channel, float value) override {
    float previous = anaFilt[channel];
    float diff = fabs(value - previous);
    if (diff < 10 * params[1]) {
      // apply jitter filter
      anaFilt[channel] = (((1-params[0]) * anaFilt[channel]) + value) / params[0];
    }
    else {
      // use unfiltered value
      anaFilt[channel] = value;
    }
    return anaFilt[channel];
  }

  void setParameter(int p, float val) override
  {
    Filter::setParameter(p, val);
    if(params[0] == 0) // prevent div/zero
        params[0] = 1; // JITTER_ALPHA
    
  }

  const char *getFilterName() override { return "Jitter"; }

 private:
    float anaFilt[NUM_ANALOGS];
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// 1 AUD Filter



// All filters and data.
typedef struct {
    float outputs[NUM_ANALOGS];
    Filter *filter;
} FilterList;

std::list<FilterList> filters;
int filterchoice=0;




