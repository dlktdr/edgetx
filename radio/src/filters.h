#pragma once

#include <math.h>
#include "opentx.h"

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
      paramstep[i] = 0.005;
    }
  }

  bool plot=true;

  virtual float doFilter(int channel, float value) = 0;
  virtual const char *getFilterName() = 0;
  virtual void setParameter(int p, float val)
  {
    if (p < MAX_PARAMS) {
      params[p] = val;
    }
  }
  float getParameter(int p) { return params[p]; }
  const char *getParameterStr(int p) { return paramstr[p]; }
  int getParameterCount() { return paramcount; }
  float getParameterStep(int p) { return paramstep[p]; }
  void calcPeriod() {    
    uint16_t curtime = getTmr2MHz();
    uint16_t elapsed = curtime - lasttime;
    period = (float)elapsed / 2000000.0f;
    lasttime = curtime;
  }

  float getPeriod()
  {
    return period;
  }

 protected:
  void addFilter(std::string filt) {}

  void addParameter(const char *str, float initialval, float step = 0.02)
  {
    if (paramcount < MAX_PARAMS) {
      strncpy(paramstr[paramcount], str, PARAM_CHAR_LEN);
      paramstr[paramcount][PARAM_CHAR_LEN - 1] = '\0';
      params[paramcount] = initialval;
      paramstep[paramcount] = step;
      paramcount++;
    }
  };


  uint16_t lasttime = getTmr2MHz(); // First run will be wrong (*** FIXme)
  float period =0.001; // Non-zero starting value
  int paramcount = 0;
  float params[MAX_PARAMS];
  float paramstep[MAX_PARAMS];
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
    addParameter("Min Cut Off Freq", 0.02, 0.002);
    addParameter("Cut Off Slope", 0.03, 0.002);
    addParameter("Derv C/O Freq", 7.0, 0.002);

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
    sf1econf.minCutoffFrequency =
        params[0];  // addParameter("Min Cut Off Freq", 0.06);
    sf1econf.cutoffSlope = params[1];  // addParameter("Cut Off Slope", 0.1);
    sf1econf.derivativeCutoffFrequency =
        params[2];  // addParameter("Derv C/O Freq", 1.0);
    oneeufilters[channel].config = sf1econf;  // Update configuration on the fly
    return SF1eFilterDo(&oneeufilters[channel], value);
  }

  const char *getFilterName() override { return "One Euro"; }

 private:
  SF1eFilter oneeufilters[NUM_ANALOGS];
  SF1eFilterConfiguration sf1econf;

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
    return 1.0f / (1.0f + tau / period);
  }

  float SF1eFilterDo(SF1eFilter *filter, float x)
  {
    float dx = 0.f;

    if (filter->xfilt.usedBefore) {
      dx = (x - filter->xfilt.xprev) * (1.0f / period);
    }

    float edx = SFLowPassFilterDo(
        &(filter->dxfilt), dx,
        SF1eFilterAlpha(filter, filter->config.derivativeCutoffFrequency));
    float cutoff = filter->config.minCutoffFrequency +
                   filter->config.cutoffSlope * fabs(edx);
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
    addParameter("Jitter Alpha", 4, 1);
  }

  float doFilter(int channel, float value) override
  {
    uint32_t previous = (uint32_t)anaFilt[channel] / (uint32_t)params[0];
    uint32_t v = value;
    uint32_t diff = (v > previous) ? (v - previous) : (previous - v);
    if (diff < 20) {
      // apply jitter filter
      anaFilt[channel] = (anaFilt[channel] - previous) + value;
    } else {
      // use unfiltered value
      anaFilt[channel] = (uint32_t)value * (uint32_t)params[0];
    }

    return anaFilt[channel] / (uint32_t)params[0];
  }

  void setParameter(int p, float val) override
  {
    Filter::setParameter(p, val);
    if (params[0] == 0)  // prevent div/zero
      params[0] = 4;     // JITTER_ALPHA
  }

  const char *getFilterName() override { return "Jitter"; }

 private:
  float anaFilt[NUM_ANALOGS];
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// 1 AUD Filter

#define ONEAUD_DEFAULT_DCUTOFF (50.0f)

// Some compilers mis-optimise the use of float constants and get better
// performance with an explicit declaration
static const float FASTZERO = 0.0f;
static const float FAST2PI = 6.28318531f;

/** Two PT1 filters with a common k
 *  Updates are processed by both PT1 sequentially, providing 2nd order
 * filtering at the output
 */
class DoublePT1filter
{
 private:
  float current1, current2, k;

 public:
  DoublePT1filter()
  {
    current1 = FASTZERO;
    current2 = FASTZERO;
  }
  float update(const float x)
  {
    current1 = current1 + k * (x - current1);
    current2 = current2 + k * (current1 - current2);
    return current2;
  }
  void setK(const float newK) { k = newK; }
  float getCurrent() { return current2; }
};

class OneAUDfilter
{
 private:
  float prevInput, prevSmoothed;
  float minFreq, maxFreq;
  float beta;
  float sampleRate;
  float kScale;  // 2 * PI / sampleRate
  float dCutoff;
  float maxSlewPerSecond, maxSlewPerSample;
  bool slewDisabled;

  // The filters for the derivative and output
  DoublePT1filter dFilt, oFilt;

  /** Limit the maximum per step delta in the input
   * This helps to remove the impact of measurement spikes in the data that
   * exceed what is physically possible in the domain space. The limit should be
   * higher than the noise threshold.
   */
  float slewLimit(const float x)
  {
    if (slewDisabled) {
      return x;
    }

    const float s = x - prevInput;

    if (s > maxSlewPerSample) {
      return prevInput + maxSlewPerSample;
    } else if ((-s) > maxSlewPerSample) {
      return prevInput - maxSlewPerSample;
    }

    return x;
  }

 public:
  /** Constructor
   * @param minFreq       cutoff frequency when the input is unchanging
   * @param maxFreq       cutoff frequency when the input is rapidly changing
   * @param beta          the rate at which the cutoff frequency is raised when
   * the input is changing
   * @param sampleRate    the frequency at which the input data was sampled (Hz)
   * @param dCutoffFreq   the cutoff frequency for the derivative filter.
   * Optional, default ONEAUD_DEFAULT_DCUTOFF Hz
   * @param maxSlew       the maximum expected rate of change in steps per
   * second. 0 disables slew limiting. Optional, default 0
   * @param initialValue  a hint for the expected value to allow quicker initial
   * settling at startup. Optional, default 0
   */
  OneAUDfilter() {}
  OneAUDfilter(const float _minFreq, const float _maxFreq, const float _beta,
               const float _sampleRate,
               const float _dCutoff = ONEAUD_DEFAULT_DCUTOFF,
               const float _maxSlew = 0.0f, const float initialValue = 0.0f)
  {
    // save the params
    minFreq = _minFreq;
    maxFreq = _maxFreq;
    beta = _beta;
    sampleRate = _sampleRate;
    maxSlewPerSecond = _maxSlew;
    dCutoff = _dCutoff;

    // calculate derived values
    kScale = FAST2PI / _sampleRate;
    maxSlewPerSample = _maxSlew / _sampleRate;
    slewDisabled = (_maxSlew == FASTZERO);

    // other setup
    prevInput = initialValue;
    prevSmoothed = FASTZERO;

    // set k for the derivative
    const float k = dCutoff * kScale;
    dFilt.setK(k);

    // Set k for the output filter
    const float kOut = minFreq * kScale;
    oFilt.setK(kOut);
  }

  void setParameters(const float _minFreq, const float _maxFreq,
                     const float _beta, const float _sampleRate,
                     const float _dCutoff = ONEAUD_DEFAULT_DCUTOFF,
                     const float _maxSlew = 0.0f,
                     const float initialValue = 0.0f)
  {
    // save the params
    minFreq = _minFreq;
    maxFreq = _maxFreq;
    beta = _beta;
    sampleRate = _sampleRate;
    maxSlewPerSecond = _maxSlew;
    dCutoff = _dCutoff;

    // calculate derived values
    kScale = FAST2PI / _sampleRate;
    maxSlewPerSample = _maxSlew / _sampleRate;
    slewDisabled = (_maxSlew == FASTZERO);

    // other setup
    prevInput = initialValue;
    prevSmoothed = FASTZERO;

    // set k for the derivative
    const float k = dCutoff * kScale;
    dFilt.setK(k);

    // Set k for the output filter
    const float kOut = minFreq * kScale;
    oFilt.setK(kOut);
  }

  // change the sample rate that the filter is running at
  void setSampleRate(const float newSampleRate)
  {
    sampleRate = newSampleRate;
    maxSlewPerSample = maxSlewPerSecond / sampleRate;

    // precompute 2 * PI / sampleRate and save for future use
    kScale = FAST2PI / sampleRate;

    float kD = dCutoff * kScale;
    // make sure kD is reasonable
    if (kD >= 1.0f) {
      kD = 0.99f;
    }
    dFilt.setK(kD);

    // The output PT1s get a new K at each update, so no need to set here
  }

  // update the filter with a new value
  float update(const float newValue)
  {
    // slew limit the input
    const float limitedNew = slewLimit(newValue);

    // apply the derivative filter to the input
    const float df = dFilt.update(limitedNew);

    // ## get differential of filtered input
    const float dx = (df - prevSmoothed) * sampleRate;
    const float absDx = dx >= FASTZERO ? dx : -dx;

    // update prevSmoothed
    prevSmoothed = df;

    // ## adjust cutoff upwards using dx and Beta
    float fMain = minFreq + (beta * absDx);
    if (fMain > maxFreq) {
      fMain = maxFreq;
    }

    // ## get the k value for the cutoff
    // kCutoff = 2*PI*cutoff/sampleRate.
    // 2*PI/sampleRate has been pre-computed and held in kScale
    const float kMain = fMain * kScale;
    oFilt.setK(kMain);

    // apply the main filter to the input
    const float mf = oFilt.update(limitedNew);

    // update the previous value
    prevInput = newValue;

    return mf;
  }

  // get the current filtered value
  float getCurrent() { return oFilt.getCurrent(); }
};

class AUDFilter : public Filter
{
 public:
  AUDFilter() : Filter()
  {
    addParameter("Min Freq", 100, 0.5);
    addParameter("Max Freq", 200, 0.5);
    addParameter("Beta", 0.01, 0.01);
    addParameter("dCutoffFreq", ONEAUD_DEFAULT_DCUTOFF, 0.1);
    addParameter("MaxSlew", 0, 0.01);
  }

  float doFilter(int channel, float value) override
  {
    audfilt[channel].setSampleRate(1.0f/getPeriod());
    audfilt[channel].setParameters(params[0], params[1], params[2], params[3],
                                   params[4]);
    return audfilt[channel].update(value);
  }

  const char *getFilterName() override { return "1AUD Filter"; }

 private:
  OneAUDfilter audfilt[NUM_ANALOGS];
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Do Nothing Filter (Off)

class OffFilter : public Filter
{
 public:
  OffFilter() : Filter() {}

  float doFilter(int channel, float value) override { return value; }
  const char *getFilterName() override { return "Off"; }
};

// All filters and data.
typedef struct {
  float outputs[NUM_ANALOGS];
  Filter *filter;
} FilterList;

extern std::list<FilterList> filters;
extern int filterchoice;
extern int debugfilter;



