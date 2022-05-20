#include "stdafx.h"

#include <audioengineextensionapo.h>

#include "Q2APO.h"
#include "log.hpp"

#pragma comment(lib, "legacy_stdio_definitions.lib")


const CRegAPOProperties<1> Q2APOMFX::regProperties(
    __uuidof(Q2APOMFX),
    L"Q2APO", L"" /* no copyright specified */, 1, 0,
    __uuidof(IAudioProcessingObject),
    (APO_FLAG)(APO_FLAG_FRAMESPERSECOND_MUST_MATCH | APO_FLAG_BITSPERSAMPLE_MUST_MATCH | APO_FLAG_INPLACE)
);

// {6BB23FB8-BE73-4E55-B04C-FB7BF4866AC5}
DEFINE_GUID(Q2EffectCH, 0x6bb23fb8, 0xbe73, 0x4e55, 0xb0, 0x4c, 0xfb, 0x7b, 0xf4, 0x86, 0x6a, 0xc5);

struct CUSTOM_FORMAT_ITEM
{
    UNCOMPRESSEDAUDIOFORMAT format;
    LPCWSTR                 name;
};

static const struct
{
    UNCOMPRESSEDAUDIOFORMAT format;
    LPCWSTR                 name;
}
_availableFormatsPcm[] =
{
    {{KSDATAFORMAT_SUBTYPE_PCM, 2, 2, 16, 32000, KSAUDIO_SPEAKER_STEREO}, L"Wave 32 KHz, 16-bit, stereo"},
    {{KSDATAFORMAT_SUBTYPE_PCM, 2, 2, 16, 44100, KSAUDIO_SPEAKER_STEREO}, L"Wave 44.1 KHz, 16-bit, stereo"},
    {{KSDATAFORMAT_SUBTYPE_PCM, 2, 2, 16, 48000, KSAUDIO_SPEAKER_STEREO}, L"Wave 48 KHz, 16-bit, stereo"},
},
 _availableFormatsFloat[] =
{
    {{KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 2, 4, 32, 32000, KSAUDIO_SPEAKER_STEREO}, L"Wave 32 KHz, float, stereo"},
    {{KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 2, 4, 32, 44100, KSAUDIO_SPEAKER_STEREO}, L"Wave 44.1 KHz, float, stereo"},
    {{KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 2, 4, 32, 48000, KSAUDIO_SPEAKER_STEREO}, L"Wave 48 KHz, float, stereo"},
}
;

static bool isEqual(const UNCOMPRESSEDAUDIOFORMAT & l, const UNCOMPRESSEDAUDIOFORMAT & r)
{
    return l.guidFormatType == r.guidFormatType
        && l.dwSamplesPerFrame == r.dwSamplesPerFrame
        && l.dwBytesPerSampleContainer == r.dwBytesPerSampleContainer
        && l.dwValidBitsPerSample == r.dwValidBitsPerSample
        && l.fFramesPerSecond == r.fFramesPerSecond
        && l.dwChannelMask == r.dwChannelMask;
};

//

template<typename T>
std::basic_ostream<T> & operator<<(std::basic_ostream<T> & os, IAudioMediaType * mediaType)
{
    if (!mediaType)
    {
        os << LS<T>("MediaType { }");
        return os;
    }

    os << LS<T>("MediaType { Compressed: ");

    BOOL isCompressed;
    HRESULT icfr = mediaType->IsCompressedFormat(&isCompressed);
    if (SUCCEEDED(icfr))
    {
        os << isCompressed;
    }
    else
    {
        os << LS<T>(" <Err ") << icfr << LS<T>(">");
    }

    auto format = mediaType->GetAudioFormat();
    os << LS<T>(" tag: ") << format->wFormatTag
        << LS<T>(" ch: ") << format->nChannels
        << LS<T>(" sps: ") << format->nSamplesPerSec
        << LS<T>(" bps: ") << format->nAvgBytesPerSec
        << LS<T>(" block: ") << format->nBlockAlign
        << LS<T>(" bits/s: ") << format->wBitsPerSample;
    if (format->cbSize >= 22)
    {
        auto formatEx = (WAVEFORMATEXTENSIBLE *)format;
        os << LS<T>(" | samples: ") << formatEx->Samples.wSamplesPerBlock
            << LS<T>(" cm: ") << formatEx->dwChannelMask
            << LS<T>(" subformat: ") << formatEx->SubFormat;
    }
    else
    {
        os << LS<T>(" cb: ") << format->cbSize;
    }
    os << LS<T>(" }");

    return os;
}

template<typename T>
std::basic_ostream<T> & operator<<(std::basic_ostream<T> & os, const UNCOMPRESSEDAUDIOFORMAT & format)
{
    os << LS<T>("Format { ")
        << format.guidFormatType
        << LS<T>(" s/f: ") << format.dwSamplesPerFrame
        << LS<T>(" b/s: ") << format.dwBytesPerSampleContainer
        << LS<T>(" bits/s: ") << format.dwValidBitsPerSample
        << LS<T>(" fps: ") << format.fFramesPerSecond
        << LS<T>(" cm: ") << format.dwChannelMask
        << LS<T>(" }");

    return os;
}

//

STDMETHODIMP Q2APOMFX::GetLatency(HNSTIME * pTime)
{
    if (!pTime) return E_POINTER;

    if (!m_bIsLocked)
        return APOERR_ALREADY_UNLOCKED;

    *pTime = 0;

    return S_OK;
}


STDMETHODIMP Q2APOMFX::Initialize(UINT32 cbDataSize, BYTE * pbyData)
{
    // init is called multiple times
    // init may be called while locked o_O

    if ((NULL == pbyData) && (0 != cbDataSize)) return E_INVALIDARG;
    if ((NULL != pbyData) && (0 == cbDataSize)) return E_POINTER;

    if (cbDataSize == sizeof(APOInitSystemEffects))
    {
        msg() << "Init APOInitSystemEffects L " << m_bIsLocked;

        APOInitSystemEffects * eff = (APOInitSystemEffects *)pbyData;
    }
    else if (cbDataSize == sizeof(APOInitSystemEffects2))
    {
        auto eff = (APOInitSystemEffects2 *)pbyData;
        // AUDIO_SIGNALPROCESSINGMODE_DEFAULT
        msg() << "Init APOInitSystemEffects 2" << " mode " << eff->AudioProcessingMode << " L " << m_bIsLocked;
    }
    else if (cbDataSize == sizeof(APOInitSystemEffects3))
    {
        msg() << "APOInitSystemEffects 3 L " << m_bIsLocked;
    }
    else
    {
        return E_INVALIDARG;
    }

    return CBaseAudioProcessingObject::Initialize(cbDataSize, pbyData);
}


STDMETHODIMP Q2APOMFX::IsInputFormatSupported(IAudioMediaType * pOutputFormat,
                                              IAudioMediaType * pRequestedInputFormat,
                                              IAudioMediaType ** ppSupportedInputFormat)
{
    if (!pRequestedInputFormat || !ppSupportedInputFormat) return E_POINTER;

    //auto hr = IsFormatTypeSupported(pOutputFormat, pRequestedInputFormat, &ppSupportedInputFormat, false);

    UNCOMPRESSEDAUDIOFORMAT ucReq;
    auto uc = pRequestedInputFormat->GetUncompressedAudioFormat(&ucReq);
    if (FAILED(uc)) return uc;

    msg() << "IIFS Req " << ucReq;
    if (checkFormat(ucReq))
    {
        msg() << " supported";
        *ppSupportedInputFormat = pRequestedInputFormat;
        (*ppSupportedInputFormat)->AddRef();
        return S_OK;
    }
    else
    {
        // propose most suitable
        const UNCOMPRESSEDAUDIOFORMAT * proposed;

        if (ucReq.guidFormatType == KSDATAFORMAT_SUBTYPE_PCM)
        {
            if (ucReq.fFramesPerSecond <= _availableFormatsPcm[0].format.fFramesPerSecond)
                proposed = &_availableFormatsPcm[0].format;
            else if (ucReq.fFramesPerSecond >= _availableFormatsPcm[std::size(_availableFormatsPcm) - 1].format.fFramesPerSecond)
                proposed = &_availableFormatsPcm[std::size(_availableFormatsPcm) - 1].format;
            else
                proposed = &_availableFormatsPcm[1].format;
        }
        else if (ucReq.guidFormatType == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
        {
            if (ucReq.fFramesPerSecond <= _availableFormatsFloat[0].format.fFramesPerSecond)
                proposed = &_availableFormatsFloat[0].format;
            else if (ucReq.fFramesPerSecond >= _availableFormatsFloat[std::size(_availableFormatsFloat) - 1].format.fFramesPerSecond)
                proposed = &_availableFormatsFloat[std::size(_availableFormatsFloat) - 1].format;
            else
                proposed = &_availableFormatsFloat[1].format;
        }
        else
        {
            err() << "unsupported format " << ucReq.guidFormatType;
            return APOERR_FORMAT_NOT_SUPPORTED;
        }

        //auto hr = CreateAudioMediaType(proposed, sizeof(*proposed), ppSupportedInputFormat);
        auto hr = CreateAudioMediaTypeFromUncompressedAudioFormat(proposed, ppSupportedInputFormat);
        msg() << " proposing " << hr;
        msg() << "IIFS Out " << *ppSupportedInputFormat;

        return SUCCEEDED(hr) ? S_FALSE : hr;
    }
}


STDMETHODIMP_(HRESULT __stdcall) Q2APOMFX::IsOutputFormatSupported(IAudioMediaType * pInputFormat,
                                                                   IAudioMediaType * pRequestedOutputFormat,
                                                                   IAudioMediaType ** ppSupportedOutputFormat)
{
    if (!pRequestedOutputFormat || !ppSupportedOutputFormat) return E_POINTER;

    msg() << "IOFS Req " << pRequestedOutputFormat;

    return IsFormatTypeSupported(pInputFormat, pRequestedOutputFormat, ppSupportedOutputFormat, false);
}


STDMETHODIMP_(HRESULT __stdcall) Q2APOMFX::GetInputChannelCount(UINT32 * pu32ChannelCount)
{
    if (!pu32ChannelCount) return E_POINTER;

    msg() << "GetInputChannelCount";

    *pu32ChannelCount = 2;

    return S_OK;
}

STDMETHODIMP Q2APOMFX::LockForProcess(UINT32 u32NumInputConnections, APO_CONNECTION_DESCRIPTOR ** ppInputConnections,
                                      UINT32 u32NumOutputConnections, APO_CONNECTION_DESCRIPTOR ** ppOutputConnections)
{
    msg() << "LockForProcess:  input " << u32NumInputConnections
        << " output " << u32NumOutputConnections;

    if (u32NumInputConnections == 0 || !ppInputConnections
        || u32NumOutputConnections == 0 || !ppOutputConnections)
    {
        return E_INVALIDARG;
    }

    // this apparently calls IOFS/IIFS internally
    auto hr = CBaseAudioProcessingObject::LockForProcess(u32NumInputConnections, ppInputConnections,
                                                         u32NumOutputConnections, ppOutputConnections);
    if (FAILED(hr)) return hr;

    if (u32NumInputConnections > 1 || u32NumOutputConnections > 1)
    {
        err() << "Extra connections are not supported: " << u32NumInputConnections << ' ' << u32NumOutputConnections;
    }

    UNCOMPRESSEDAUDIOFORMAT inFormat;
    hr = ppOutputConnections[0]->pFormat->GetUncompressedAudioFormat(&inFormat);
    if (FAILED(hr)) return hr;

    UNCOMPRESSEDAUDIOFORMAT outFormat;
    hr = ppOutputConnections[0]->pFormat->GetUncompressedAudioFormat(&outFormat);
    if (FAILED(hr)) return hr;

    msg() << "LockForProcess: <= " << inFormat;
    msg() << "LockForProcess: => " << outFormat;

    if (!isEqual(inFormat, outFormat))
    {
        err() << "Input and output format mismatch:";
        err() << inFormat;
        err() << outFormat;
        return CO_E_NOT_SUPPORTED;
    }

    isPCM_ = inFormat.guidFormatType == KSDATAFORMAT_SUBTYPE_PCM;
    auto sampleRate = (int)inFormat.fFramesPerSecond;

    dnse_ = std::make_unique<DNSE_CH>(10, 9, sampleRate);

    return hr;
}


STDMETHODIMP Q2APOMFX::UnlockForProcess()
{
    msg() << "UnlockForProcess";
    dnse_.reset();
    return CBaseAudioProcessingObject::UnlockForProcess();
}


STDMETHODIMP Q2APOMFX::GetEffectsList(_Outptr_result_buffer_maybenull_(*pcEffects) LPGUID * ppEffectsIds,
                                      _Out_ UINT * pcEffects,
                                      _In_ HANDLE Event)
{
    if (ppEffectsIds == NULL || pcEffects == NULL)
        return E_POINTER;

    HRESULT hr;
    BOOL effectsLocked = FALSE;
    UINT cEffects = 0;

    msg() << "GetEffectsList";

    {
        struct EffectControl
        {
            GUID effect;
            //BOOL control;
        };

        EffectControl list[] =
        {
            { Q2EffectCH,  /*m_fEnableSwapMFX*/  },
        };

        //if (!IsEqualGUID(m_AudioProcessingMode, AUDIO_SIGNALPROCESSINGMODE_RAW))
        //{
        //    // count the active effects
        //    for (UINT i = 0; i < ARRAYSIZE(list); i++)
        //    {
        //        if (list[i].control)
        //        {
        //            cEffects++;
        //        }
        //    }
        //}

        cEffects = 1;

        //if (0 == cEffects)
        //{
        //    *ppEffectsIds = NULL;
        //    *pcEffects = 0;
        //}
        //else
        {
            GUID * pEffectsIds = (LPGUID)CoTaskMemAlloc(sizeof(GUID) * cEffects);
            if (pEffectsIds == nullptr)
            {
                hr = E_OUTOFMEMORY;
                goto Exit;
            }

            // pick up the active effects
            UINT j = 0;
            for (UINT i = 0; i < std::size(list); i++)
            {
                //if (list[i].control)
                {
                    pEffectsIds[j++] = list[i].effect;
                }
            }

            *ppEffectsIds = pEffectsIds;
            *pcEffects = cEffects;
        }

        hr = S_OK;
    }

Exit:
    return hr;
}

#pragma AVRT_CODE_BEGIN
STDMETHODIMP_(void) Q2APOMFX::APOProcess(UINT32 u32NumInputConnections, APO_CONNECTION_PROPERTY ** ppInputConnections,
                                         UINT32 u32NumOutputConnections, APO_CONNECTION_PROPERTY ** ppOutputConnections)
{
    if (!dnse_)
    {
        return;
    }

    if (u32NumInputConnections > 1 || u32NumOutputConnections > 1)
    {
        err() << "Extra connections are not supported: " << u32NumInputConnections << ' ' << u32NumOutputConnections;
    }

    auto & iConn = ppInputConnections[0];
    auto & oConn = ppOutputConnections[0];

    switch (iConn->u32BufferFlags)
    {
        case BUFFER_VALID:
        {
            if (isPCM_)
            {
                // not tested
                auto inputFrames = reinterpret_cast<int16_t *>(iConn->pBuffer);
                auto outputFrames = reinterpret_cast<int16_t *>(oConn->pBuffer);

                for (unsigned i = 0; i < iConn->u32ValidFrameCount; i++)
                {
                    auto l = inputFrames[2 * i];
                    auto r = inputFrames[2 * i + 1];

                    dnse_->filter(l, r, outputFrames + 2 * i, outputFrames + 2 * i + 1);
                }
            }
            else
            {
                auto inputFrames = reinterpret_cast<float *>(iConn->pBuffer);
                auto outputFrames = reinterpret_cast<float *>(oConn->pBuffer);

                for (unsigned i = 0; i < iConn->u32ValidFrameCount; i++)
                {
                    auto l = int(inputFrames[2 * i] * 0x7FFF);
                    auto r = int(inputFrames[2 * i + 1] * 0x7FFF);

                    int16_t ol, or;
                    dnse_->filter(l, r, &ol, &or);

                    outputFrames[2 * i] = float(ol) / 0x7FFF;
                    outputFrames[2 * i + 1] = float(or) / 0x7FFF;
                }
            }

            //auto os = std::ofstream("f:\\Temp\\a.pcm", std::ios::binary | std::ios::app);
            //os.write((const char *)iConn->pBuffer, 2 * 4 * iConn->u32ValidFrameCount);

            oConn->u32ValidFrameCount = iConn->u32ValidFrameCount;
            oConn->u32BufferFlags = iConn->u32BufferFlags;

            break;
        }

        case BUFFER_SILENT:
        {
            oConn->u32ValidFrameCount = iConn->u32ValidFrameCount;
            oConn->u32BufferFlags = iConn->u32BufferFlags;

            break;
        }
    }
}
#pragma AVRT_CODE_END


STDMETHODIMP Q2APOMFX::GetFormatCount(UINT * pcFormats)
{
    msg() << "GetFormatCount";

    if (pcFormats == NULL) return E_POINTER;

    *pcFormats = UINT(std::size(_availableFormatsPcm) + std::size(_availableFormatsFloat));
    return S_OK;
}


STDMETHODIMP Q2APOMFX::GetFormat(UINT nFormat, IAudioMediaType ** ppFormat)
{
    msg() << "GetFormat " << nFormat;

    if (ppFormat == nullptr) return E_POINTER;
    if (nFormat >= std::size(_availableFormatsPcm) + std::size(_availableFormatsFloat)) return E_INVALIDARG;

    *ppFormat = NULL;

    auto hr = CreateAudioMediaTypeFromUncompressedAudioFormat(
        nFormat < std::size(_availableFormatsPcm) ? &_availableFormatsPcm[nFormat].format : &_availableFormatsFloat[nFormat].format,
        ppFormat);
    return hr;
}


STDMETHODIMP Q2APOMFX::GetFormatRepresentation(UINT nFormat, LPWSTR * ppwstrFormatRep)
{
    msg() << "GetFormatRepresentation " << nFormat;

    if (ppwstrFormatRep == nullptr) return E_POINTER;
    if (nFormat >= std::size(_availableFormatsPcm) + std::size(_availableFormatsFloat)) return E_INVALIDARG;

    auto & fmt = nFormat < std::size(_availableFormatsPcm) ? _availableFormatsPcm[nFormat] : _availableFormatsFloat[nFormat];

    auto nameLen = (wcslen(fmt.name) + 1) * sizeof(WCHAR);
    auto nameStr = (LPWSTR)CoTaskMemAlloc(nameLen);
    if (nameStr == NULL) return E_OUTOFMEMORY;

    std::copy(fmt.name, fmt.name + nameLen, nameStr);
    *ppwstrFormatRep = nameStr;
    return S_OK;
}

//

bool Q2APOMFX::checkFormat(const UNCOMPRESSEDAUDIOFORMAT & format)
{
    if (format.guidFormatType == KSDATAFORMAT_SUBTYPE_PCM)
    {
        for (auto & fmt : _availableFormatsPcm)
        {
            if (isEqual(fmt.format, format))
            {
                return true;
            }
        }
    }
    else if (format.guidFormatType == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
    {
        for (auto & fmt : _availableFormatsFloat)
        {
            if (isEqual(fmt.format, format))
            {
                return true;
            }
        }
    }


    return false;
}
