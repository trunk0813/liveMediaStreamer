/*
 *  AVFramedQueue - A lock-free AV frame circular queue
 *  Copyright (C) 2014  Fundació i2CAT, Internet i Innovació digital a Catalunya
 *
 *  This file is part of media-streamer.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Authors:  Marc Palau <marc.palau@i2cat.net>
 *            David Cassany <david.cassany@i2cat.net>
 */

#include "AVFramedQueue.hh"
#include "VideoFrame.hh"
#include "AudioFrame.hh"
#include "Utils.hh"

AVFramedQueue::AVFramedQueue(struct ConnectionData cData, unsigned maxFrames) : FrameQueue(cData), max(maxFrames)
{

}

AVFramedQueue::~AVFramedQueue()
{
    for (unsigned i = 0; i<max; i++) {
        delete frames[i];
    }
}

Frame* AVFramedQueue::getRear() 
{
    if ((rear + 1) % max == front){
        return NULL;
    }
    
    return frames[rear];
}

Frame* AVFramedQueue::getFront() 
{
    if(rear == front) {
        return NULL;
    }

    return frames[front];
}

int AVFramedQueue::addFrame() 
{
    rear =  (rear + 1) % max;
    return connectionData.rFilterId;
}

int AVFramedQueue::removeFrame() 
{
    front = (front + 1) % max;
    return connectionData.wFilterId;
}

void AVFramedQueue::flush() 
{
    rear = (rear + (max - 1)) % max;
}

Frame* AVFramedQueue::forceGetRear()
{
    Frame *frame;
    while ((frame = getRear()) == NULL) {
        utils::debugMsg("Frame discarted by AVFramedQueue");
        flush();
    }
    return frame;
}

Frame* AVFramedQueue::forceGetFront()
{
    return frames[(front + (max - 1)) % max]; 
}

const unsigned AVFramedQueue::getElements()
{
    return front > rear ? (max - front + rear) : (rear - front);
}


////////////////////////////////////////////
//VIDEO FRAME QUEUE METHODS IMPLEMENTATION//
////////////////////////////////////////////

VideoFrameQueue* VideoFrameQueue::createNew(struct ConnectionData cData, VCodecType codec, unsigned maxFrames, PixType pixelFormat, const uint8_t *extradata, int extradata_size)
{
    VideoFrameQueue* q = new VideoFrameQueue(cData, codec, maxFrames, pixelFormat);

    if (!q->setup()) {
        utils::errorMsg("VideoFrameQueue setup error!");
        delete q;
        return NULL;
    }

    q->extradata = extradata;
    q->extradata_size = extradata_size;

    return q;
}


VideoFrameQueue::VideoFrameQueue(struct ConnectionData cData, VCodecType codec_, unsigned maxFrames, PixType pixelFormat_) :
AVFramedQueue(cData, maxFrames), codec(codec_), pixelFormat(pixelFormat_)
{

}

bool VideoFrameQueue::setup()
{
    switch(codec) {
        case H264:
        case H265:
            for (unsigned i=0; i<max; i++) {
                frames[i] = InterleavedVideoFrame::createNew(codec, MAX_H264_OR_5_NAL_SIZE);
            }
            break;
        case VP8:
            for (unsigned i=0; i<max; i++) {
                frames[i] = InterleavedVideoFrame::createNew(codec, LENGTH_VP8);
            }
            break;
        case RAW:
            if (pixelFormat == P_NONE) {
                utils::errorMsg("No pixel fromat defined");
                return false;
                break;
            }
            for (unsigned i=0; i<max; i++) {
                frames[i] = InterleavedVideoFrame::createNew(codec, DEFAULT_WIDTH, DEFAULT_HEIGHT, pixelFormat);
            }
            break;
        default:
            utils::errorMsg("[Video Frame Queue] Codec not supported!");
            return false;
    }

    return true;
}

////////////////////////////////////////////
//AUDIO FRAME QUEUE METHODS IMPLEMENTATION//
////////////////////////////////////////////

unsigned getMaxSamples(unsigned sampleRate);

AudioFrameQueue* AudioFrameQueue::createNew(struct ConnectionData cData, ACodecType codec, unsigned maxFrames, unsigned sampleRate, 
                                             unsigned channels, SampleFmt sFmt, const uint8_t *extradata, int extradata_size)
{
    AudioFrameQueue* q = new AudioFrameQueue(cData, codec, maxFrames, sFmt, sampleRate, channels);

    if (!q->setup()) {
        utils::errorMsg("AudioFrameQueue setup error!");
        delete q;
        return NULL;
    }

    q->extradata = extradata;
    q->extradata_size = extradata_size;

    return q;
}

AudioFrameQueue::AudioFrameQueue(struct ConnectionData cData, ACodecType codec_, unsigned maxFrames, SampleFmt sFmt, unsigned sampleRate_, unsigned channels_)
: AVFramedQueue(cData, maxFrames), codec(codec_), sampleFormat(sFmt), sampleRate(sampleRate_), channels(channels_)
{

}

bool AudioFrameQueue::setup()
{
    switch(codec) {
        case OPUS:
        case AAC:
        case MP3:
            sampleFormat = S16;
            for (unsigned i=0; i<max; i++) {
                frames[i] = InterleavedAudioFrame::createNew(channels, sampleRate, AudioFrame::getMaxSamples(sampleRate), codec, sampleFormat);
            }
            break;
        case PCMU:
        case PCM:
            if (sampleFormat == U8 || sampleFormat == S16 || sampleFormat == FLT) {
                for (unsigned i=0; i<max; i++) {
                    frames[i] = InterleavedAudioFrame::createNew(channels, sampleRate, AudioFrame::getMaxSamples(sampleRate), codec, sampleFormat);
                }
            } else if (sampleFormat == U8P || sampleFormat == S16P || sampleFormat == FLTP) {
                for (unsigned i=0; i<max; i++) {
                    frames[i] = PlanarAudioFrame::createNew(channels, sampleRate, AudioFrame::getMaxSamples(sampleRate), codec, sampleFormat);
                }
            } else {
                 utils::errorMsg("[Audio Frame Queue] Sample format not supported!");
                 return false;
            }
            break;
        case G711:
            channels = 1;
            sampleRate = 8000;
            sampleFormat = U8;
            for (unsigned i=0; i<max; i++) {
                frames[i] = InterleavedAudioFrame::createNew(channels, sampleRate, AudioFrame::getMaxSamples(sampleRate), codec, sampleFormat);
            }
            break;
        default:
            utils::errorMsg("[Audio Frame Queue] Codec not supported!");
            return false;
    }

    return true;

}
