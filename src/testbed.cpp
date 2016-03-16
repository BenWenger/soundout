
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <Windows.h>                // for GetASyncKeyState / Sleep only
#include "soundout.h"

#include <cstdint>
#include <iostream>
#include <cmath>
using namespace std;

namespace
{
    typedef std::int16_t    s16;

    // just use defaults.... too lazy to write an interface
    int                     samplerate =    44100;
    bool                    stereo =        true;
    int                     latency =       200;
    double                  toneHz =        220;
    double                  amp =           8000;

    const double            twopi = 2 * 3.1415926535897932384626433832795;
    double                  phase =         0.0;
    double                  rate =          0.0;
    
    //////////////////////////////////////////////
    //////////////////////////////////////////////

    int fill( s16* buf, int siz )
    {
        int written = 0;
        siz /= (stereo ? 4 : 2);

        while(siz > 0)
        {
            buf[0] = (s16)(amp * std::sin(phase));
            phase += rate;
            if(phase >= twopi)
                phase -= twopi;

            --siz;
            if(stereo)
            {
                buf[1] = buf[0];
                buf += 2;
                written += 4;
            }
            else
            {
                ++buf;
                written += 2;
            }
        }

        return written;
    }

    void fillAudio(SoundOut& snd)
    {
        auto lk( snd.lock() );
        fill( lk.getBuffer<s16>(0), lk.getSize(0) );
        fill( lk.getBuffer<s16>(1), lk.getSize(1) );
    }
}



int main()
{
    // if you were going to make an interface, put it here
    rate = toneHz * twopi / samplerate;

    ////////////////////////////
    SoundOut snd(samplerate, stereo, latency);
    fillAudio(snd);
    snd.play();

    std::cout << "Press Space to exit" << std::endl;


    while( !(::GetAsyncKeyState(VK_SPACE) & 0x8000) )
    {
        Sleep(10);
        fillAudio(snd);
    }

    return 0;
}

