
#if !defined(SOUNDOUT_NO_LINK_LIB) && defined(_MSC_VER)
    #if defined (_WIN64)
        #if defined(DEBUG) || defined (_DEBUG)
            #pragma comment (lib,"soundout_64d.lib")
        #else
            #pragma comment (lib,"soundout_64.lib")
        #endif
    #else
        #if defined(DEBUG) || defined (_DEBUG)
            #pragma comment (lib,"soundout_d.lib")
        #else
            #pragma comment (lib,"soundout.lib")
        #endif
    #endif
#endif

#include <stdexcept>
#include <cstdint>

/// Note:  All "sizes" are in BYTES

class SoundOut
{
private:
    friend class    Locker;
    class           Impl;

public:
                    SoundOut()                                      : impl(nullptr) {                                                                   }
                    ~SoundOut()                                                     { destroy();                                                        }
                    SoundOut(SoundOut&& rhs)                        : impl(rhs.impl){ rhs.impl = nullptr;                                               }
    SoundOut&       operator = (SoundOut&& rhs)                                     { destroy(); impl = rhs.impl;  rhs.impl = nullptr;  return *this;   }

    ////////////////////////////////////////

    class Exception : public std::runtime_error { public: Exception(const char* str) : runtime_error(str) {} };
    
    ////////////////////////////////////////
    class Locker
    {
    public:
        int                         getSize(int bufindex) const     { return size[!!bufindex];                          }
        void*                       getBuffer(int bufindex)         { return buffer[!!bufindex];                        }
        template <typename T>   T*  getBuffer(int bufindex)         { return reinterpret_cast<T*>(buffer[!!bufindex]);  }
        void                        setWritten(int samplesWritten)  { written = samplesWritten;                         }
                                    ~Locker()                       { if(host) SoundOut::unlock(host,*this);            }
                                    
        Locker(Locker&& rhs)
        {
            written = rhs.written;
            size[0] = rhs.size[0];
            size[1] = rhs.size[1];
            buffer[0] = rhs.buffer[0];
            buffer[1] = rhs.buffer[1];
            host = rhs.host;
            rhs.host = nullptr;
        }

    private:
        friend class    SoundOut;
        friend class    SoundOut::Impl;

        int             written;
        int             size[2];
        void*           buffer[2];
        SoundOut::Impl* host;

                        Locker() {}
                        Locker(const Locker&) = delete;
                        Locker& operator = (const Locker&) = delete;
    };

    
    ////////////////////////////////////////

                    SoundOut(int samplerate, bool stereo, int latency);
    bool            isPlaying() const;
    int             canWrite();
    Locker          lock(int bytes = -1);
    void            play();
    void            stop(bool flush);

    
    ////////////////////////////////////////
private:
    friend class    Locker;
    class           Impl;

    static void     unlock(Impl* impl, const Locker& lock);
    void            destroy();
    void            verify() const      { verify(impl);     }
    static void     verify(Impl* i);

                    SoundOut(const SoundOut& rhs) = delete;
    SoundOut&       operator = (const SoundOut& rhs) = delete;

    Impl*           impl;
};