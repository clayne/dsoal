#include "dsoundoal.h"

#include <algorithm>

#include <ks.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <optional>

#include "comhelpers.h"
#include "comptr.h"
#include "dsoal.h"
#include "expected.h"
#include "guidprinter.h"
#include "logging.h"


namespace {

using voidp = void*;
using cvoidp = const void*;

struct ALCdeviceDeleter {
    void operator()(ALCdevice *device) { alcCloseDevice(device); }
};
using ALCdevicePtr = std::unique_ptr<ALCdevice,ALCdeviceDeleter>;

struct ALCcontextDeleter {
    void operator()(ALCcontext *context)
    {
        if(context == alcGetThreadContext())
            alcSetThreadContext(nullptr);
        alcDestroyContext(context);
    }
};
using ALCcontextPtr = std::unique_ptr<ALCcontext,ALCcontextDeleter>;


std::optional<DWORD> GetSpeakerConfig(IMMDevice *device, const GUID &devid)
{
    ComPtr<IPropertyStore> ps;
    HRESULT hr{device->OpenPropertyStore(STGM_READ, ds::out_ptr(ps))};
    if(FAILED(hr))
    {
        WARN("GetSpeakerConfig IMMDevice::OpenPropertyStore failed: %08lx\n", hr);
        return std::nullopt;
    }

    PropVariant pv;
    hr = ps->GetValue(PKEY_AudioEndpoint_GUID, pv.get());
    if(FAILED(hr) || pv->vt != VT_LPWSTR)
    {
        WARN("GetSpeakerConfig IPropertyStore::GetValue(GUID) failed: %08lx\n", hr);
        return std::nullopt;
    }

    GUID thisId{};
    CLSIDFromString(pv->pwszVal, &thisId);

    pv.clear();

    if(devid != thisId)
        return std::nullopt;

    DWORD speakerconf{DSSPEAKER_7POINT1_SURROUND};

    hr = ps->GetValue(PKEY_AudioEndpoint_PhysicalSpeakers, pv.get());
    if(FAILED(hr))
    {
        WARN("GetSpeakerConfig IPropertyStore::GetValue(PhysicalSpeakers) failed: %08lx\n", hr);
        return speakerconf;
    }
    if(pv->vt != VT_UI4)
    {
        WARN("GetSpeakerConfig PhysicalSpeakers is not a ULONG: 0x%04x\n", pv->vt);
        return speakerconf;
    }

    const ULONG phys_speakers{pv->ulVal};

#define BIT_MATCH(v, b) (((v)&(b)) == (b))
    if(BIT_MATCH(phys_speakers, KSAUDIO_SPEAKER_7POINT1))
        speakerconf = DSSPEAKER_7POINT1;
    else if(BIT_MATCH(phys_speakers, KSAUDIO_SPEAKER_7POINT1_SURROUND))
        speakerconf = DSSPEAKER_7POINT1_SURROUND;
    else if(BIT_MATCH(phys_speakers, KSAUDIO_SPEAKER_5POINT1))
        speakerconf = DSSPEAKER_5POINT1_BACK;
    else if(BIT_MATCH(phys_speakers, KSAUDIO_SPEAKER_5POINT1_SURROUND))
        speakerconf = DSSPEAKER_5POINT1_SURROUND;
    else if(BIT_MATCH(phys_speakers, KSAUDIO_SPEAKER_QUAD))
        speakerconf = DSSPEAKER_QUAD;
    else if(BIT_MATCH(phys_speakers, KSAUDIO_SPEAKER_STEREO))
        speakerconf = DSSPEAKER_COMBINED(DSSPEAKER_STEREO, DSSPEAKER_GEOMETRY_WIDE);
    else if(BIT_MATCH(phys_speakers, KSAUDIO_SPEAKER_MONO))
        speakerconf = DSSPEAKER_MONO;
    else
    {
        FIXME("GetSpeakerConfig Unhandled physical speaker layout: 0x%08lx\n",
            phys_speakers);
        return speakerconf;
    }
#undef BIT_MATCH

    if(DSSPEAKER_CONFIG(speakerconf) == DSSPEAKER_STEREO)
    {
        hr = ps->GetValue(PKEY_AudioEndpoint_FormFactor, pv.get());
        if(FAILED(hr))
            WARN("GetSpeakerConfig IPropertyStore::GetValue(FormFactor) failed: %08lx\n", hr);
        else if(pv->vt != VT_UI4)
            WARN("GetSpeakerConfig FormFactor is not a ULONG: 0x%04x\n", pv->vt);
        else if(pv->ulVal == Headphones || pv->ulVal == Headset)
            speakerconf = DSSPEAKER_HEADPHONE;
    }

    TRACE("GetSpeakerConfig Got config %d:%d from physical speakers 0x%08lx\n",
        DSSPEAKER_GEOMETRY(speakerconf), DSSPEAKER_CONFIG(speakerconf), phys_speakers);

    return speakerconf;
}

ds::expected<std::unique_ptr<SharedDevice>,HRESULT> CreateDeviceShare(GUID &guid)
{
    TRACE("CreateDeviceShare Creating shared device %s\n", GuidPrinter{guid}.c_str());

    DWORD speakerconf{DSSPEAKER_7POINT1_SURROUND};

    ComWrapper com;
    ComPtr<IMMDeviceEnumerator> devenum;
    HRESULT hr{CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_INPROC_SERVER,
        IID_IMMDeviceEnumerator, ds::out_ptr(devenum))};
    if(FAILED(hr))
    {
        ERR("CreateDeviceShare CoCreateInstance failed: %08lx\n", hr);
        return hr;
    }

    ComPtr<IMMDeviceCollection> coll;
    hr = devenum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, ds::out_ptr(coll));
    if(FAILED(hr))
    {
        WARN("CreateDeviceShare IMMDeviceEnumerator::EnumAudioEndpoints failed: %08lx\n", hr);
        return hr;
    }

    UINT count{};
    hr = coll->GetCount(&count);
    if(FAILED(hr))
    {
        WARN("CreateDeviceShare IMMDeviceCollection::GetCount failed: %08lx\n", hr);
        return hr;
    }

    for(UINT i{0};i < count;++i)
    {
        ComPtr<IMMDevice> device;
        hr = coll->Item(i, ds::out_ptr(device));
        if(FAILED(hr)) continue;

        if(auto config = GetSpeakerConfig(device.get(), guid))
        {
            speakerconf = *config;
            break;
        }
    }

    char drv_name[64]{};

    LPOLESTR guid_str{};
    hr = StringFromCLSID(guid, &guid_str);
    if(FAILED(hr))
    {
        ERR("CreateDeviceShare Failed to convert GUID to string\n");
        return hr;
    }
    WideCharToMultiByte(CP_UTF8, 0, guid_str, -1, drv_name, sizeof(drv_name), NULL, NULL);
    drv_name[sizeof(drv_name)-1] = 0;
    CoTaskMemFree(guid_str);
    guid_str = nullptr;

    hr = DSERR_NODRIVER;
    ALCdevicePtr aldev{alcOpenDevice(drv_name)};
    if(!aldev)
    {
        WARN("CreateDeviceShare Couldn't open device \"%s\", 0x%04x\n", drv_name,
            alcGetError(nullptr));
        return hr;
    }
    TRACE("CreateDeviceShare Opened AL device: %s\n",
        alcIsExtensionPresent(aldev.get(), "ALC_ENUMERATE_ALL_EXT") ?
        alcGetString(aldev.get(), ALC_ALL_DEVICES_SPECIFIER) :
        alcGetString(aldev.get(), ALC_DEVICE_SPECIFIER));

    const ALint attrs[]{
        ALC_MONO_SOURCES, MaxSources,
        ALC_STEREO_SOURCES, 0,
        0
    };
    ALCcontextPtr alctx{alcCreateContext(aldev.get(), attrs)};
    if(!alctx)
    {
        WARN("CreateDeviceShare Couldn't create context, 0x%04x\n", alcGetError(aldev.get()));
        return hr;
    }

    auto shared = std::make_unique<SharedDevice>();
    shared->mId = guid;
    shared->mSpeakerConfig = speakerconf;
    shared->mDevice = aldev.release();
    shared->mContext = alctx.release();

    return shared;
}

} // namespace


SharedDevice::~SharedDevice()
{
    if(mContext)
    {
        if(mContext == alcGetThreadContext())
            alcSetThreadContext(nullptr);
        alcDestroyContext(mContext);
    }
    if(mDevice)
        alcCloseDevice(mDevice);
}

std::mutex DSound8OAL::sDeviceListMutex;
std::vector<std::unique_ptr<SharedDevice>> DSound8OAL::sDeviceList;


ComPtr<DSound8OAL> DSound8OAL::Create(bool is8)
{
    return ComPtr<DSound8OAL>{new DSound8OAL{is8}};
}

DSound8OAL::DSound8OAL(bool is8) : mIs8{is8} { };

DSound8OAL::~DSound8OAL()
{
    if(!mShared)
        return;

    std::lock_guard listlock{sDeviceListMutex};

    auto find_shared = [this](std::unique_ptr<SharedDevice> &shared)
    { return mShared == shared.get(); };

    auto shared_iter = std::find_if(sDeviceList.begin(), sDeviceList.end(), find_shared);
    if(shared_iter == sDeviceList.end()) return;

    (*shared_iter)->mUseCount -= 1;
    if((*shared_iter)->mUseCount == 0)
    {
        TRACE("DSound8OAL::~DSound8OAL Freeing shared device %s\n",
            GuidPrinter{(*shared_iter)->mId}.c_str());
        sDeviceList.erase(shared_iter);
    }
}


HRESULT STDMETHODCALLTYPE DSound8OAL::QueryInterface(REFIID riid, void** ppvObject) noexcept
{
    DEBUG("DSound8OAL::QueryInterface (%p)->(%s, %p)\n", voidp{this}, GuidPrinter{riid}.c_str(),
        voidp{ppvObject});

    *ppvObject = NULL;
    if(riid == IID_IUnknown)
    {
        AddRef();
        *ppvObject = static_cast<IUnknown*>(as<IDirectSound8*>());
        return S_OK;
    }
    if(riid == IID_IDirectSound8)
    {
        if(!mIs8) UNLIKELY
        {
            WARN("DSound8OAL::QueryInterface Requesting IDirectSound8 iface for non-DS8 object\n");
            return E_NOINTERFACE;
        }
        AddRef();
        *ppvObject = as<IDirectSound8*>();
        return S_OK;
    }
    if(riid == IID_IDirectSound)
    {
        AddRef();
        *ppvObject = as<IDirectSound*>();
        return S_OK;
    }

    FIXME("Unhandled GUID: %s\n", GuidPrinter{riid}.c_str());
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE DSound8OAL::AddRef() noexcept
{
    const auto ret = mRef.fetch_add(std::memory_order_relaxed) + 1;
    DEBUG("DSound8OAL::AddRef (%p) ref %lu\n", voidp{this}, ret);
    return ret;
}

ULONG STDMETHODCALLTYPE DSound8OAL::Release() noexcept
{
    const auto ret = mRef.fetch_sub(std::memory_order_relaxed) - 1;
    DEBUG("DSound8OAL::Release (%p) ref %lu\n", voidp{this}, ret);
    if(ret == 0) UNLIKELY
        delete this;
    return ret;
}


HRESULT STDMETHODCALLTYPE DSound8OAL::CreateSoundBuffer(const DSBUFFERDESC *bufferDesc,
    IDirectSoundBuffer **dsBuffer, IUnknown *outer) noexcept
{
    DEBUG("DSound8OAL::CreateSoundBuffer (%p)->(%p, %p, %p)\n", voidp{this}, cvoidp{bufferDesc},
        voidp{dsBuffer}, voidp{outer});
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DSound8OAL::GetCaps(DSCAPS *dsCaps) noexcept
{
    DEBUG("DSound8OAL::GetCaps (%p)->(%p)\n", voidp{this}, voidp{dsCaps});
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DSound8OAL::DuplicateSoundBuffer(IDirectSoundBuffer *origBuffer,
    IDirectSoundBuffer **dupBuffer) noexcept
{
    DEBUG("DSound8OAL::DuplicateSoundBuffer (%p)->(%p, %p)\n", voidp{this}, voidp{origBuffer},
        voidp{dupBuffer});
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DSound8OAL::SetCooperativeLevel(HWND hwnd, DWORD level) noexcept
{
    DEBUG("DSound8OAL::SetCooperativeLevel (%p)->(%p, %lu)\n", voidp{this}, voidp{hwnd}, level);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DSound8OAL::Compact() noexcept
{
    DEBUG("DSound8OAL::DuplicateSoundBuffer (%p)->()\n", voidp{this});
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DSound8OAL::GetSpeakerConfig(DWORD *speakerConfig) noexcept
{
    DEBUG("DSound8OAL::GetSpeakerConfig (%p)->(%p)\n", voidp{this}, voidp{speakerConfig});
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DSound8OAL::SetSpeakerConfig(DWORD speakerConfig) noexcept
{
    DEBUG("DSound8OAL::SetSpeakerConfig (%p)->(%lx)\n", voidp{this}, speakerConfig);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DSound8OAL::Initialize(const GUID *deviceId) noexcept
{
    DEBUG("DSound8OAL::Initialize (%p)->(%s)\n", voidp{this}, GuidPrinter{deviceId}.c_str());

    if(mShared)
    {
        WARN("DSound8OAL::Initialize Device already initialized\n");
        return DSERR_ALREADYINITIALIZED;
    }

    if(!deviceId || *deviceId == GUID_NULL)
        deviceId = &DSDEVID_DefaultPlayback;
    else if(*deviceId == DSDEVID_DefaultCapture || *deviceId == DSDEVID_DefaultVoiceCapture)
        return DSERR_NODRIVER;

    GUID devid{};
    HRESULT hr{GetDeviceID(*deviceId, devid)};
    if(FAILED(hr)) return hr;

    {
        auto find_id = [devid](std::unique_ptr<SharedDevice> &device)
        { return devid == device->mId; };

        std::unique_lock listlock{sDeviceListMutex};
        auto sharediter = std::find_if(sDeviceList.begin(), sDeviceList.end(), find_id);
        if(sharediter != sDeviceList.end())
        {
            (*sharediter)->mUseCount += 1;
            mShared = sharediter->get();
        }
        else
        {
            auto shared = CreateDeviceShare(devid);
            if(!shared) return shared.error();

            sDeviceList.emplace_back(std::move(shared).value());
            mShared = sDeviceList.back().get();
        }
    }

    return DS_OK;
}

HRESULT STDMETHODCALLTYPE DSound8OAL::VerifyCertification(DWORD *certified) noexcept
{
    DEBUG("DSound8OAL::VerifyCertification (%p)->(%p)\n", voidp{this}, voidp{certified});
    return E_NOTIMPL;
}
