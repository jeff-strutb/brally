/* Streams.m: the host's decoded audio files on the iPhone (host.h
 * host_stream_*), as the Mac host has them (host/macos): Core Audio reads the
 * FLAC CD tracks in the app's music/cd (Boss Rally's soundtrack, the PC's
 * music of the Remaster Options) at the rate asked for, stereo floats. */
#import <AudioToolbox/AudioToolbox.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"

struct host_stream {
    ExtAudioFileRef f;
};

host_stream *host_stream_open(const char *path, int rate)
{
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path, (CFIndex)strlen(path), false);
    ExtAudioFileRef f = NULL;
    AudioStreamBasicDescription fmt;
    host_stream *s;
    OSStatus st;
    if (!url)
        return NULL;
    st = ExtAudioFileOpenURL(url, &f);
    CFRelease(url);
    if (st != noErr || !f)
        return NULL;
    memset(&fmt, 0, sizeof fmt);
    fmt.mSampleRate = rate;
    fmt.mFormatID = kAudioFormatLinearPCM;
    fmt.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    fmt.mFramesPerPacket = 1;
    fmt.mChannelsPerFrame = 2;
    fmt.mBitsPerChannel = 32;
    fmt.mBytesPerFrame = 8;
    fmt.mBytesPerPacket = 8;
    if (ExtAudioFileSetProperty(f, kExtAudioFileProperty_ClientDataFormat, sizeof fmt, &fmt) != noErr) {
        ExtAudioFileDispose(f);
        return NULL;
    }
    s = (host_stream *)calloc(1, sizeof *s);
    s->f = f;
    return s;
}

int host_stream_read(host_stream *s, float *lr, int frames)
{
    AudioBufferList bl;
    UInt32 n = (UInt32)frames;
    if (!s)
        return 0;
    bl.mNumberBuffers = 1;
    bl.mBuffers[0].mNumberChannels = 2;
    bl.mBuffers[0].mDataByteSize = (UInt32)frames * 8;
    bl.mBuffers[0].mData = lr;
    if (ExtAudioFileRead(s->f, &n, &bl) != noErr)
        return 0;
    return (int)n;
}

void host_stream_close(host_stream *s)
{
    if (s) {
        ExtAudioFileDispose(s->f);
        free(s);
    }
}
