/*
This file is part of Darling.

Copyright (C) 2020 Lubos Dolezel
Copyright (C) 2026 VibeDarling Project

Darling is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Darling is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Darling.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "AudioHardwareStreamPA.h"
#include "AudioHardwareImplPA.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <vector>
#include <type_traits>
#include <limits>

AudioHardwareStreamPA::AudioHardwareStreamPA(AudioHardwareImplPA* hw, AudioDeviceIOProc callback, void* clientData, bool isInput)
: AudioHardwareStream(hw, isInput), m_paHw(hw), m_callback(callback), m_clientData(clientData), m_isInput(isInput)
{
	hw->getPAContext(^(pa_context* context) {
		if (!context)
		{
			std::cerr << "[CoreAudio PA] Failed to get PulseAudio context\n";
			return;
		}
		
		pa_sample_spec spec = AudioHardwareImplPA::paSampleSpecForASBD(hw->asbd(), &m_convertSignedUnsigned);
		
		if (!pa_sample_spec_valid(&spec))
		{
			std::cerr << "[CoreAudio PA] Failed to create a valid pa_sample_spec\n";
			return;
		}

		PAThreadedMainLoop* loop = hw->loop();
		if (loop) loop->lock();

		m_stream = pa_stream_new(context, isInput ? "CoreAudio Input" : "CoreAudio Output", &spec, nullptr);
		if (!m_stream)
		{
			std::cerr << "[CoreAudio PA] pa_stream_new failed\n";
			if (loop) loop->unlock();
			return;
		}

		pa_stream_set_state_callback(m_stream, [](pa_stream *s, void *userdata) {
			AudioHardwareStreamPA* This = static_cast<AudioHardwareStreamPA*>(userdata);
			pa_stream_state_t state = pa_stream_get_state(s);
			if (state == PA_STREAM_READY)
			{
				if (This->m_running)
				{
					pa_operation* op = pa_stream_cork(s, 0, nullptr, nullptr);
					if (op) pa_operation_unref(op);
					pa_operation* op2 = pa_stream_trigger(s, nullptr, nullptr);
					if (op2) pa_operation_unref(op2);
				}
			}
			else if (state == PA_STREAM_FAILED)
			{
				std::cerr << "[CoreAudio PA] PulseAudio stream failed: " << pa_strerror(pa_context_errno(pa_stream_get_context(s))) << std::endl;
			}
			if (This->m_paHw && This->m_paHw->loop())
				This->m_paHw->loop()->signal(0);
		}, this);

		if (!isInput)
		{
			struct pa_buffer_attr battr;
			battr.fragsize = uint32_t(-1);
			battr.maxlength = uint32_t(-1);
			battr.minreq = uint32_t(-1);
			battr.prebuf = 0;
			battr.tlength = uint32_t(-1);

			pa_cvolume cvol;
			pa_cvolume_set(&cvol, spec.channels, PA_VOLUME_NORM);

			pa_stream_set_write_callback(m_stream, paStreamWriteCB, this);

			pa_stream_connect_playback(m_stream, nullptr, &battr,
				pa_stream_flags_t(PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_START_UNMUTED),
				&cvol, nullptr);
		}
		else
		{
			struct pa_buffer_attr battr;
			battr.fragsize = uint32_t(-1);
			battr.maxlength = uint32_t(m_bufferSize);
			battr.minreq = uint32_t(-1);
			battr.prebuf = battr.maxlength;
			battr.tlength = uint32_t(-1);

			pa_stream_set_read_callback(m_stream, paStreamReadCB, this);

			pa_stream_connect_record(m_stream, nullptr, &battr,
				pa_stream_flags_t(PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE)
			);
		}

		if (loop) loop->unlock();
	});
}

AudioHardwareStreamPA::~AudioHardwareStreamPA()
{
	PAThreadedMainLoop* loop = m_paHw ? m_paHw->loop() : nullptr;
	if (loop) loop->lock();

	if (m_stream)
	{
		pa_stream_disconnect(m_stream);
		pa_stream_unref(m_stream);
		m_stream = nullptr;
	}

	if (loop) loop->unlock();
}

void AudioHardwareStreamPA::start()
{
	PAThreadedMainLoop* loop = m_paHw ? m_paHw->loop() : nullptr;
	if (loop) loop->lock();

	m_running = true;
	if (m_stream)
	{
		int waitCycles = 20; // 20 * 50ms = 1000ms max wait
		while (pa_stream_get_state(m_stream) == PA_STREAM_CREATING && waitCycles-- > 0)
		{
			if (loop) loop->wait();
			else break;
		}
		if (pa_stream_get_state(m_stream) == PA_STREAM_READY)
		{
			pa_operation* op = pa_stream_cork(m_stream, 0, nullptr, nullptr);
			if (op) pa_operation_unref(op);
			pa_operation* op2 = pa_stream_trigger(m_stream, nullptr, nullptr);
			if (op2) pa_operation_unref(op2);
		}
	}

	if (loop) loop->unlock();
}

void AudioHardwareStreamPA::stop()
{
	PAThreadedMainLoop* loop = m_paHw ? m_paHw->loop() : nullptr;
	if (loop) loop->lock();

	m_running = false;
	if (m_stream && pa_stream_get_state(m_stream) == PA_STREAM_READY)
	{
		pa_stream_cork(m_stream, true, nullptr, nullptr);
	}

	if (loop) loop->unlock();
}

void AudioHardwareStreamPA::paStreamWriteCB(pa_stream* s, size_t length, void* self)
{
	AudioHardwareStreamPA* This = static_cast<AudioHardwareStreamPA*>(self);

	if (!This->m_running)
	{
		void* pdata = nullptr;
		size_t rqsize = length;
		if (pa_stream_begin_write(This->m_stream, &pdata, &rqsize) == 0 && pdata && rqsize > 0)
		{
			memset(pdata, 0, rqsize);
			pa_stream_write(This->m_stream, pdata, rqsize, nullptr, 0, PA_SEEK_RELATIVE);
		}
		return;
	}

	if (pa_stream_is_corked(This->m_stream))
	{
		pa_operation* op = pa_stream_cork(This->m_stream, 0, nullptr, nullptr);
		if (op) pa_operation_unref(op);
		pa_operation* op2 = pa_stream_trigger(This->m_stream, nullptr, nullptr);
		if (op2) pa_operation_unref(op2);
	}

	AudioTimeStamp nowTs = {0};
	nowTs.mFlags = kAudioTimeStampHostTimeValid | kAudioTimeStampSampleTimeValid;
	nowTs.mHostTime = AudioGetCurrentHostTime();
	nowTs.mSampleTime = This->m_sampleTime;

	AudioTimeStamp outTs = nowTs;
	AudioBufferList* abl = static_cast<AudioBufferList*>(alloca(sizeof(AudioBufferList) + sizeof(AudioBuffer)));

	size_t done = 0;

	while (done < length && This->m_running)
	{
		const auto asbd = This->m_paHw->asbd();
		abl->mNumberBuffers = 1;
		abl->mBuffers[0].mNumberChannels = asbd.mChannelsPerFrame ? asbd.mChannelsPerFrame : 2;

		size_t rqsize = std::min<UInt32>(This->m_bufferSize, length - done);
		void* pdata = nullptr;
		int err = pa_stream_begin_write(This->m_stream, &pdata, &rqsize);
		if (err < 0 || !pdata || rqsize == 0)
		{
			if (err < 0)
			{
				std::cerr << "[CoreAudio PA] pa_stream_begin_write failed: " << err << std::endl;
				pa_stream_cork(This->m_stream, true, nullptr, nullptr);
			}
			break;
		}

		abl->mBuffers[0].mData = pdata;
		abl->mBuffers[0].mDataByteSize = rqsize;

		OSStatus status = This->m_callback(This->m_paHw->id(), &nowTs, nullptr, nullptr, abl, &outTs, This->m_clientData);

		UInt32 bpf = asbd.mBytesPerFrame ? asbd.mBytesPerFrame : 8;
		This->m_sampleTime += (Float64)(rqsize / bpf);

		if (status != noErr || !abl->mBuffers[0].mDataByteSize)
		{
			memset(pdata, 0, rqsize);
		}
		else if (abl->mBuffers[0].mDataByteSize < rqsize)
		{
			memset((uint8_t*)pdata + abl->mBuffers[0].mDataByteSize, 0, rqsize - abl->mBuffers[0].mDataByteSize);
		}

		if (This->m_convertSignedUnsigned)
			This->transformSignedUnsigned(abl);

		int rv = pa_stream_write(This->m_stream, pdata, rqsize, nullptr, 0, PA_SEEK_RELATIVE);
		if (rv != 0)
		{
			std::cerr << "[CoreAudio PA] pa_stream_write failed: " << rv << std::endl;
			break;
		}

		if (!This->m_running)
		{
			pa_stream_cork(This->m_stream, true, nullptr, nullptr);
			break;
		}

		done += rqsize;
	}
}

void AudioHardwareStreamPA::paStreamReadCB(pa_stream* s, size_t length, void* self)
{
	AudioHardwareStreamPA* This = static_cast<AudioHardwareStreamPA*>(self);

	if (!This->m_running)
		return;

	AudioTimeStamp fake = {0};
	AudioBufferList* abl = static_cast<AudioBufferList*>(alloca(sizeof(AudioBufferList) + sizeof(AudioBuffer)));
	std::vector<uint8_t> hole;

	while (true)
	{
		abl->mNumberBuffers = 1;
		abl->mBuffers[0].mNumberChannels = 2;

		size_t nbytes;

		int rv = pa_stream_peek(This->m_stream, (const void**) &abl->mBuffers[0].mData, &nbytes);

		if (rv < 0 || !nbytes)
		{
			break;
		}

		if (nbytes && !abl->mBuffers[0].mData)
		{
			hole.resize(nbytes);
			abl->mBuffers[0].mData = hole.data();
		}
		abl->mBuffers[0].mDataByteSize = nbytes;

		if (This->m_convertSignedUnsigned)
			This->transformSignedUnsigned(abl);

		OSStatus status = This->m_callback(This->m_paHw->id(), &fake, abl, &fake, nullptr, nullptr, This->m_clientData);

		pa_stream_drop(This->m_stream);

		if (status != noErr)
		{
			pa_stream_cork(This->m_stream, true, nullptr, nullptr);
			break;
		}
	}
}

template <typename T>
void transform(typename std::make_unsigned<T>::type* data)
{
	typedef typename std::make_signed<T>::type signed_type;
	signed_type* s = reinterpret_cast<signed_type*>(data);

	*s = *data + std::numeric_limits<signed_type>::min();
}

void AudioHardwareStreamPA::transformSignedUnsigned(AudioBufferList* abl) const
{
	const auto asbd = m_paHw->asbd();
	const bool revEndian = (asbd.mFormatFlags & kAudioFormatFlagIsBigEndian) != (asbd.mFormatFlags & kAudioFormatFlagsNativeEndian);

	for (int i = 0; i < abl->mNumberBuffers; i++)
	{
		AudioBuffer* buf = &abl->mBuffers[i];

		switch (asbd.mBitsPerChannel)
		{
			case 8:
				for (int j = 0; j < buf->mDataByteSize; j++)
					transform<uint8_t>(reinterpret_cast<uint8_t*>(buf->mData) + j);
				break;
			case 16:
				if (!revEndian)
				{
					for (int j = 0; j < buf->mDataByteSize / sizeof(uint16_t); j++)
						transform<uint16_t>(reinterpret_cast<uint16_t*>(buf->mData) + j);
				}
				else
				{
					for (int j = 0; j < buf->mDataByteSize / sizeof(uint16_t); j++)
					{
						uint16_t v = __builtin_bswap16(*(reinterpret_cast<uint16_t*>(buf->mData) + j));
						transform<uint16_t>(&v);
						*(reinterpret_cast<uint16_t*>(buf->mData) + j) = __builtin_bswap16(v);
					}
				}
				break;
			case 24:
				for (int j = 0; j < buf->mDataByteSize / sizeof(uint32_t); j++)
				{
					uint32_t v = *(reinterpret_cast<uint32_t*>(buf->mData) + j);
					if (revEndian)
						v = __builtin_bswap32(v);
					
					if (v & 0x800000)
						v |= 0xff000000;

					transform<uint32_t>(&v);
					
					v &= 0xffffff;
					if (revEndian)
						v = __builtin_bswap32(v);
					
					*(reinterpret_cast<uint32_t*>(buf->mData) + j) = v;
				}
				break;
			case 32:
				if (!revEndian)
				{
					for (int j = 0; j < buf->mDataByteSize / sizeof(uint32_t); j++)
						transform<uint32_t>(reinterpret_cast<uint32_t*>(buf->mData) + j);
				}
				else
				{
					for (int j = 0; j < buf->mDataByteSize / sizeof(uint32_t); j++)
					{
						uint32_t v = __builtin_bswap32(*(reinterpret_cast<uint32_t*>(buf->mData) + j));
						transform<uint32_t>(&v);
						*(reinterpret_cast<uint32_t*>(buf->mData) + j) = __builtin_bswap32(v);
					}
				}
				break;
		}
	}
}
