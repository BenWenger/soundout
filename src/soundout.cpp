
#define SOUNDOUT_NO_LINK_LIB
#include "soundout.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <Windows.h>
#include <MMSystem.h>
#include <dsound.h>

#ifdef _MSC_VER
#pragma comment (lib,"dsound.lib")
#endif

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

class SoundOut::Impl
{
private:
	LPDIRECTSOUND8			ds =            nullptr;
	LPDIRECTSOUNDBUFFER		primary =       nullptr;
	LPDIRECTSOUNDBUFFER		secondary =     nullptr;
    DWORD                   bufferSize =    0;
    DWORD                   writePos =      0;
    bool                    playing =       false;

    friend class SoundOut;

    Impl(int samplerate, bool stereo, int latency)
    {
        try
        {
            ///////////////////////////////////////
            // Create the DirectSound object
	        if(FAILED(  DirectSoundCreate8(nullptr,&ds,nullptr)  ))
                throw Exception("Failed to create DirectSound8 device");

            // TODO - replace Desktop window with a temporary window?
	        if(FAILED(  ds->SetCooperativeLevel( ::GetDesktopWindow(), DSSCL_PRIORITY )     ))
                throw Exception("Failed to set cooperative level for DirectSound8 device");

            
            ///////////////////////////////////////
            // Primary buffer
            DSBUFFERDESC desc = {};
            desc.dwSize =           sizeof(desc);
            desc.dwFlags =          DSBCAPS_PRIMARYBUFFER;
	        if(FAILED(  ds->CreateSoundBuffer(&desc,&primary,nullptr)     ))
                throw Exception("Failed to create primary sound buffer");

            ///////////////////////////////////////
            // Define Format
            WAVEFORMATEX			wfx = {};
	        wfx.cbSize =				sizeof(wfx);
	        wfx.wFormatTag =			WAVE_FORMAT_PCM;
	        wfx.wBitsPerSample =		16;
	        wfx.nChannels =				(stereo ? 2 : 1);
	        wfx.nSamplesPerSec =		samplerate;
	        wfx.nBlockAlign =			wfx.nChannels * wfx.wBitsPerSample / 8;
	        wfx.nAvgBytesPerSec =		wfx.nBlockAlign * wfx.nSamplesPerSec;

	        // set the primary buffer to this format
	        if(FAILED(  primary->SetFormat(&wfx)     ))
                throw Exception("Failed to set format for primary buffer");
            
            ///////////////////////////////////////
            // Create secondary buffer
            bufferSize = latency * wfx.nSamplesPerSec / 1000;
	        bufferSize *= wfx.nBlockAlign;

	        desc.dwBufferBytes =	bufferSize;
	        desc.lpwfxFormat =		&wfx;
	        desc.dwFlags =			DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
            
	        if(FAILED(  ds->CreateSoundBuffer(&desc,&secondary,nullptr)     ))
                throw Exception("Failed to create secondary sound buffer");

            
            secondary->SetCurrentPosition(0);
            secondary->GetCurrentPosition(nullptr, &writePos);
            writePos += 4;
        }
        catch(...)
        {
            if(secondary)   secondary->Release();
            if(primary)     primary->Release();
            if(ds)          ds->Release();

            throw;
        }
    }


    //////////////////////////////////
    bool isPlaying() const { return playing; }

    int canWrite()
    {
        DWORD p;
	    secondary->GetCurrentPosition(&p, nullptr);

        if(p < writePos)
            return static_cast<int>( bufferSize - writePos + p );
        return static_cast<int>(p - writePos);
    }

    void play()
    {
        if(playing)     return;
        
        if(secondary->Play(0,0,DSBPLAY_LOOPING) == DSERR_BUFFERLOST)
        {
	        secondary->Restore();
	        secondary->Play(0,0,DSBPLAY_LOOPING);
        }

        playing = true;
    }

    void stop(bool flush)
    {
        if(playing)
        {
            secondary->Stop();
            playing = false;
        }

        if(flush)
        {
            DWORD sa = 0, sb = 0;
            void* ba = nullptr;
            void* bb = nullptr;
            if( secondary->Lock( 0, bufferSize, &ba, &sa, &bb, &sb, DSBLOCK_ENTIREBUFFER ) == DSERR_BUFFERLOST )
            {
                secondary->Restore();
                secondary->Lock( 0, bufferSize, &ba, &sa, &bb, &sb, DSBLOCK_ENTIREBUFFER );
            }
            
            std::memset( ba, 0, sa );
            std::memset( bb, 0, sb );
            
	        secondary->Unlock( ba, sa, bb, sb );
            secondary->SetCurrentPosition(0);
            secondary->GetCurrentPosition(nullptr, &writePos);
            writePos += 4;
        }
    }

    void lock(SoundOut::Locker& lock, int bytes)
    {
        int canbytes = canWrite();
        if( (bytes < 0) || (bytes > canbytes) )
            bytes = canbytes;

        
        DWORD sa = 0, sb = 0;
        if( secondary->Lock( writePos, bytes, &lock.buffer[0], &sa, &lock.buffer[1], &sb, 0 ) == DSERR_BUFFERLOST )
        {
            secondary->Restore();
            secondary->Lock( writePos, bytes, &lock.buffer[0], &sa, &lock.buffer[1], &sb, 0 );
        }

        if(!sa || !lock.buffer[0]) { lock.buffer[0] = nullptr;  lock.size[0] = 0;   }
        else                       { lock.size[0] = static_cast<int>(sa);           }
        if(!sb || !lock.buffer[1]) { lock.buffer[1] = nullptr;  lock.size[1] = 0;   }
        else                       { lock.size[1] = static_cast<int>(sb);           }
        
        lock.host = this;
        lock.written = lock.size[0] + lock.size[1];
    }

    void unlock(const SoundOut::Locker& lock)
    {
        secondary->Unlock(  lock.buffer[0], static_cast<DWORD>( lock.size[0] ),
                            lock.buffer[1], static_cast<DWORD>( lock.size[1] ) );

        if(lock.written > 0)
            writePos = (writePos + lock.written) % bufferSize;
    }
};


////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

            SoundOut::SoundOut(int samplerate, bool stereo, int latency)    : impl( new Impl(samplerate,stereo,latency) )  {}
bool        SoundOut::isPlaying() const                                     { verify();     return impl->isPlaying();       }
int         SoundOut::canWrite()                                            { verify();     return impl->canWrite();        }
void        SoundOut::play()                                                { verify();            impl->play();            }
void        SoundOut::stop(bool flush)                                      { verify();            impl->stop(flush);       }
void        SoundOut::unlock(Impl* impl, const Locker& lock)                { verify(impl);        impl->unlock(lock);      }

inline void SoundOut::verify(Impl* i)
{
    if(!i)          throw Exception("SoundOut object not properly initialized");
}

void        SoundOut::destroy()
{
    delete impl;
    impl = nullptr;
}

SoundOut::Locker SoundOut::lock(int bytes)
{
    verify();

    Locker  lk;
    impl->lock(lk, bytes);
    return lk;
}


#if 0


//////////////////////////////////////////////////////////////////////////
//
//  SoundOut.cpp
//

#define SOUNDOUT_NO_LINK_LIB

#include <Windows.h>
#include "soundout.h"

#ifdef _MSC_VER
#pragma comment (lib,"dsound.lib")
#endif

#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p) = NULL; } }

/*
 *	Ctor / Dtor
 */

CSoundOut::CSoundOut()
{
	memset(this,0,sizeof(CSoundOut));
}

CSoundOut::~CSoundOut()
{
	Destroy();
}

/*
 *	Initialize
 */

int CSoundOut::Initialize(HWND wnd)
{
	if(lpDS)
		return 0;

	// create
	HRESULT res;
	res = DirectSoundCreate8(NULL,&lpDS,NULL);
	if(FAILED(res))
	{
		SAFE_RELEASE(lpDS);
		return 1;
	}

	// set co-op level
	res = lpDS->SetCooperativeLevel(wnd,DSSCL_PRIORITY);
	if(FAILED(res))
	{
		SAFE_RELEASE(lpDS);
		return 1;
	}

	// create primary surface
	DSBUFFERDESC		desc;
	memset(&desc,0,sizeof(DSBUFFERDESC));
	desc.dwSize =			sizeof(DSBUFFERDESC);
	desc.dwFlags =			DSBCAPS_PRIMARYBUFFER;

	res = lpDS->CreateSoundBuffer(&desc,&lpDSPrimary,NULL);
	if(FAILED(res))
	{
		SAFE_RELEASE(lpDSPrimary);
		SAFE_RELEASE(lpDS);
		return 1;
	}

	return 0;
}


/*
 *	Destroy
 */

void CSoundOut::Destroy()
{
	SAFE_RELEASE(lpDSSecondary);
	SAFE_RELEASE(lpDSPrimary);
	SAFE_RELEASE(lpDS);
	memset(this,0,sizeof(CSoundOut));
}

/*
 *	SetFormat
 */

int CSoundOut::SetFormat(int samplerate,int stereo,int latency)
{
	if(!lpDS)
		return 1;

	// kill old buffer if existant
	if(lpDSSecondary)
	{
		if(!bPaused)
			lpDSSecondary->Stop();
		SAFE_RELEASE(lpDSSecondary);
	}

	// get the format ready
	nSampleRate = samplerate;
	bStereo = (stereo ? 1 : 0);

	WAVEFORMATEX			wfx;
	memset(&wfx,0,sizeof(WAVEFORMATEX));
	wfx.cbSize =				sizeof(WAVEFORMATEX);
	wfx.wFormatTag =			WAVE_FORMAT_PCM;
	wfx.wBitsPerSample =		16;
	wfx.nChannels =				(bStereo ? 2 : 1);
	wfx.nSamplesPerSec =		samplerate;
	wfx.nBlockAlign =			wfx.nChannels * wfx.wBitsPerSample / 8;
	wfx.nAvgBytesPerSec =		wfx.nBlockAlign * wfx.nSamplesPerSec;

	// set the primary buffer to this format
	HRESULT res;
	res = lpDSPrimary->SetFormat(&wfx);
	if(FAILED(res))
		return 1;

	// create secondary surface
	nBufSize = latency * wfx.nSamplesPerSec / 1000;
	nBufSize *= wfx.nBlockAlign;

	DSBUFFERDESC			desc;
	memset(&desc,0,sizeof(DSBUFFERDESC));
	desc.dwSize =			sizeof(DSBUFFERDESC);
	desc.dwBufferBytes =	nBufSize;
	desc.lpwfxFormat =		&wfx;
	desc.dwFlags =			DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;

	res = lpDS->CreateSoundBuffer(&desc,&lpDSSecondary,NULL);
	if(FAILED(res))
	{
		SAFE_RELEASE(lpDSSecondary);
		return 1;
	}

	bLocked = 0;
	bPaused = 1;
	return 0;
}

/*
 *	Start / Stop
 */

void CSoundOut::Start()
{
	if(!lpDSSecondary)
		return;
	if(!bPaused)
		return;

	if(lpDSSecondary->Play(0,0,DSBPLAY_LOOPING) == DSERR_BUFFERLOST)
	{
		lpDSSecondary->Restore();
		lpDSSecondary->Play(0,0,DSBPLAY_LOOPING);
	}
	bPaused = 0;
	
	lpDSSecondary->GetCurrentPosition(0,&nWriteCurs);
}

void CSoundOut::Stop()
{
	if(!lpDSSecondary)
		return;
	if(bPaused)
		return;

	bPaused = 1;
	lpDSSecondary->Stop();
	Flush();
}

/*
 *	Can Write
 */

DWORD CSoundOut::CanWrite()
{
	if(!lpDSSecondary)
		return 0;

	DWORD playpos = 0;
	lpDSSecondary->GetCurrentPosition(&playpos,0);

	if(playpos < nWriteCurs)
		return (nBufSize - nWriteCurs + playpos);

	return (playpos - nWriteCurs);
}

/*
 *	Lock
 */

void CSoundOut::Lock(int len, void** bufa,DWORD* siza,void** bufb,DWORD* sizb)
{
	if(bLocked)
		return;
	if(!lpDSSecondary)
		goto exit;

	if(len < 0)
		len = CanWrite();

	HRESULT res;
	res = lpDSSecondary->Lock(nWriteCurs,len,&pLockBufA,&nLockSizA,&pLockBufB,&nLockSizB,0);
	if(res == DSERR_BUFFERLOST)
	{
		lpDSSecondary->Restore();
		res = lpDSSecondary->Lock(nWriteCurs,len,&pLockBufA,&nLockSizA,&pLockBufB,&nLockSizB,0);
		if(FAILED(res))
			goto exit;
	}

	*bufa = pLockBufA;
	*siza = nLockSizA;
	*bufb = pLockBufB;
	*sizb = nLockSizB;
	bLocked = 1;
	return;

exit:
	*bufa = 0;
	*siza = 0;
	*bufb = 0;
	*sizb = 0;
	bLocked = 0;
}

/*
 *	Unlock
 */

void CSoundOut::Unlock(int written)
{
	if(!lpDSSecondary)
		return;
	if(!bLocked)
		return;

	lpDSSecondary->Unlock(pLockBufA,nLockSizA,pLockBufB,nLockSizB);
	nWriteCurs = (nWriteCurs + written) % nBufSize;
	bLocked = 0;
}

/*
 *	Flush
 */

void CSoundOut::Flush()
{
	void* ba;
	void* bb;
	DWORD sa;
	DWORD sb;

	HRESULT res;
	res = lpDSSecondary->Lock(0,0,&ba,&sa,&bb,&sb,DSBLOCK_ENTIREBUFFER);
	if(res == DSERR_BUFFERLOST)
	{
		lpDSSecondary->Restore();
		res = lpDSSecondary->Lock(0,0,&ba,&sa,&bb,&sb,DSBLOCK_ENTIREBUFFER);
		if(FAILED(res))
			return;
	}

	memset(ba,0,sa);
	memset(bb,0,sb);
	lpDSSecondary->Unlock(ba,sa,bb,sb);
	nWriteCurs = 0;
}

#endif