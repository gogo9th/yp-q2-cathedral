#pragma once

#include <memory>
#include "filter.h"


class DNSE_CH : public Filter
{
public:
    DNSE_CH(int roomSize, int gain);
    ~DNSE_CH();

    void setSamplerate(int sampleRate) override;

    virtual void filter(sample_t l, sample_t r,
                        sample_t * l_out, sample_t * r_out) override;

private:
    struct PresetGain;
    class FilterChain;
    class APFilter;
    class DelayFilter;
    class DelaySplitFilter;
    class DelayShiftFilter;
    class VbrFilter;
    class ToneFilter;

    int roomSize_;
    int totalGain_;
    const PresetGain * presetGain_;

    std::unique_ptr<ToneFilter>         toneFilter_;
    std::unique_ptr<DelaySplitFilter>   dsFilter_;
    std::unique_ptr<APFilter>           er_ap1_;
    std::unique_ptr<APFilter>           er_ap2_;
    std::unique_ptr<FilterChain>        ch1_;
    std::unique_ptr<FilterChain>        ch2_;
    std::unique_ptr<DelayShiftFilter>   delay1_;
    std::unique_ptr<DelayShiftFilter>   delay2_;
};
