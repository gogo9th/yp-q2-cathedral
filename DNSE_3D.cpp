
#include <cassert>

#include "DNSE_3D.h"
#include "utils.h"
#include "armspecific.h"


DNSE_3D::IIRbiquad3D::IIRbiquad3D(const int16_t(&hIIR)[4])
{
    copy(hIIR_, hIIR);
    set(iirb_, 0);
}

int DNSE_3D::IIRbiquad3D::filter(int16_t in)
{
    int nxt = smulw(32 * in, hIIR_[0]) + smulw(iirb_[1], hIIR_[3]) + smulw(iirb_[0], hIIR_[2]);
    int out = nxt + smulw(iirb_[0], hIIR_[1]);
    iirb_[1] = iirb_[0];
    iirb_[0] = 4 * nxt;
    return out;
}

//

DNSE_3D::FIR4hrtf::FIR4hrtf(int hrdel, const int(&head)[4])
{
    assert(hrdel <= 17);
    delayL_.resize(hrdel + 1);
    delayR_.resize(hrdel + 1);
    copy(head_, head);
}

std::pair<int, int> DNSE_3D::FIR4hrtf::filter(int l, int r)
{
    auto r1 = smulw(delayL_.back(), head_[1]) + smulw(delayR_[0], head_[3]);
    auto r2 = smulw(delayR_.back(), head_[1]) + smulw(delayL_[0], head_[3]);
    r1 += smulw(l, head_[0]) + smulw(delayR_[1], head_[2]);
    r2 += smulw(r, head_[0]) + smulw(delayL_[1], head_[2]);
    delayL_.push_back(l);
    delayR_.push_back(r);

    return { r1, r2 };
}

//

DNSE_3D::Reverb::Reverb(const int(&fincoef)[4])
{
    copy(fincoef_, fincoef);
}

std::pair<int, int> DNSE_3D::Reverb::filter(int l, int r, int iir_l, int iir_r)
{
    // reverb_and_effect

    auto r1 = smulw(4 * l, fincoef_[0]) + smulw(4 * r, fincoef_[3]);
    auto r2 = smulw(4 * r, fincoef_[0]) + smulw(4 * l, fincoef_[3]);

    r1 += smulw(4 * iir_l, fincoef_[1]) + smulw(4 * iir_r, fincoef_[2]);
    r2 += smulw(4 * iir_r, fincoef_[1]) + smulw(4 * iir_l, fincoef_[2]);

    return { r1, r2 };
}

//

DNSE_3D::DNSE_3D(int st_eff, int st_rev, int st_hrdel)
    : Filter({ 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000 })
    , st_eff_(st_eff)
    , st_rev_(st_rev)
    , st_hrdel_(st_hrdel)
{}

void DNSE_3D::setSamplerate(int sampleRate)
{
    int mhrdelOff;
    switch (sampleRate)
    {
        case  8000:
            mhrdelOff = 0;
            break;
        case 11025:
            mhrdelOff = 1;
            break;
        case 12000:
            mhrdelOff = 2;
            break;
        case 16000:
            mhrdelOff = 3;
            break;
        case 22050:
            mhrdelOff = 4;
            break;
        case 24000:
            mhrdelOff = 5;
            break;
        case 32000:
            mhrdelOff = 6;
            break;
        case 48000:
            mhrdelOff = 8;
            break;
        case 44100:
        default:
            mhrdelOff = 7;
            break;
    }

    const int16_t hiir[9][4] = {
        { 0x2281, -0x7CD,  0x194F, -0x12A },
        { 0x23FF, -0xC41,  0x2438, -0x2C1 },
        { 0x2470, -0xDA0,  0x271F, -0x35F },
        { 0x2624, -0x12AF, 0x30CD, -0x60F },
        { 0x2824, -0x1872, 0x3B10, -0xA0B },
        { 0x28A8, -0x19DE, 0x3D95, -0xB2F },
        { 0x2A02, -0x1DEF, 0x454F, -0xF12 },
        { 0x2AAB, -0x2000, 0x4AE2, -0x11DA },
        { 0x2AAB, -0x2000, 0x4BAB, -0x121D },
    };

    iir_l_ = IIRbiquad3D(hiir[mhrdelOff]);
    iir_r_ = IIRbiquad3D(hiir[mhrdelOff]);

    static const int head[9][4] = {
        0x568D, -0x2C75, 0x1958, 0x10C0, 0x58C9, -0x3737, 0x1582, 0xC11,
        0x5954, -0x39CF, 0x1495, 0xAEF, 0x5AFD, -0x41CD, 0x11BC, 0x774,
        0x5C87, -0x4935, 0xF18, 0x43A, 0x5CE2, -0x4AE9, 0xE7D, 0x37C,
        0x5DEF, -0x4FF7, 0xCB0, 0x148, 0x5EDB, -0x5468, 0xB1A, -0xA7,
        0x5F10, -0x5566, 0xAC0, -0x116,
    };
    copy(head_, head[mhrdelOff]);

    static const int mhrdel[] = { 0x2E7, 0x400, 0x45B, 0x5CE, 0x800, 0x8B5, 0xB9C, 0x1000, 0x116A };
    // max value is 17
    khrdel_ = (mhrdel[mhrdelOff] * st_hrdel_ + 0x800) >> 12;

    fir_ = FIR4hrtf(khrdel_, head_);

    int fincoef[4];
    auto effectstrength = 409 * st_eff_;
    if (st_eff_ < 9 && st_rev_ < 9)
    {
        auto eff = (0x332000 * st_eff_) >> 12;
        fincoef[2] = (-120 * eff) >> 10;
        fincoef[1] = (113 * eff) >> 7;

        auto fc3 = -((((effectstrength << 13) >> 12) * 409 * st_rev_) >> 12);
        fincoef[0] = 4 * (-effectstrength - fc3 + 4096);
        fincoef[3] = 4 * fc3;
    }
    else
    {
        //effectstrength = 0;
        copy(fincoef, { 0, 0, 0, 0 });
    }

    reverb_ = Reverb(fincoef);
}

DNSE_3D::~DNSE_3D()
{}

void DNSE_3D::filter(int16_t l, const int16_t r, int16_t * l_out, int16_t * r_out)
{
    auto lbIIR = iir_l_.filter(l);
    auto rbIIR = iir_r_.filter(r);

    auto [lbFIR, rbFIR] = fir_.filter(lbIIR, rbIIR);
    auto [lbRev, rbRev] = reverb_.filter(l, r, lbFIR, rbFIR);

    *l_out = std::min(0x7FFF, std::max(-0x8000, lbRev));
    *r_out = std::min(0x7FFF, std::max(-0x8000, rbRev));
}

