#include "audio.h"

#include <cmath>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <exception>
#include <iostream>
#include <mutex>
#include <vector>
#include <thread>

#include <portaudio.h>

#include <sndfile.h>

class AudioFile {

    friend class AudioOutputStream;

public:

    AudioFile(const AudioFile& af) = delete;
    AudioFile& operator=(const AudioFile& af) = delete;

    static AudioFile* create(const std::string& filePath, int outputSampleRate, int numOutputChannels, size_t outputFramesPerBuffer, bool loop, bool streamPlayback);

    static void destroy(AudioFile* pFile);

private:

    AudioFile() {}

    std::vector<float> m_frameData;
    size_t m_frameIndex = 0;

    float m_frameInterp = 0.0f;

    float m_maxAmplitude = 0.0f;

    SNDFILE* m_pSndFile = nullptr;
    SF_INFO m_sndFileInfo = {};

    bool m_loop = false;

    bool m_streamPlayback = false;

    bool m_resample = false;
    bool m_upsample = false;

    std::atomic_bool m_finished = false;

};

struct AudioOutputStreamData {

    std::vector<AudioFile*> pPlayingFiles;
    std::vector<glm::vec3> positions;
    std::vector<float> innerRadii;
    std::vector<float> outerRadii;
    std::vector<bool> usePositions;
    std::vector<float> volumes;
    std::vector<float> gains;
    glm::mat4 listenerInverseTransform;

};

struct AudioOutputStreamConcurrentState {
    int readActiveIndex = 0;
    bool hasNewData = false;
};

class AudioOutputStream {

public:

    AudioOutputStream(PaDeviceIndex deviceIndex, int sampleRate, int numChannels, size_t framesPerBuffer);

    ~AudioOutputStream();

    void start();

    void stop();

    void addFile(AudioFile* pFile, float volume);

    void addFile(AudioFile* pFile, float volume, const glm::vec3& position, float innerRadius, float outerRadius);

    bool update();

    void setListenerInverseTransform(const glm::mat4& mat);

    int getSampleRate() const;

    int getNumChannels() const;

    int getFramesPerBuffer() const;

    void setMasterVolume(float volume);

private:

    PaStream *m_pStream;

    int m_sampleRate;
    int m_numChannels;
    int m_framesPerBuffer;

    float m_masterVolume = 1.0f;

    AudioOutputStreamData m_dataBuffer[2];

    std::atomic<AudioOutputStreamConcurrentState> m_currentStateAtomic;

    //std::vector<AudioFile*> m_pNextPlayingFiles;
    AudioOutputStreamData m_mainData;
    std::vector<AudioFile*> m_pFilesToRemove;

    static int outputStreamCallback(
        const void* pInput,
        void* pOutput,
        unsigned long frameCount,
        const PaStreamCallbackTimeInfo* pTimeInfo,  // Don't use this for timing, it seems to be broken when the host API is ALSA + PulseAudio
        PaStreamCallbackFlags statusFlags,
        void* pUserData);

};

class AudioEngine {

public:

    AudioEngine();

    ~AudioEngine();

    void init();

    void cleanup();

    AudioOutputStream* const getOutputStream() const;

private:

    AudioOutputStream* m_pOutputStream;

    bool m_cleanedUp;

};

AudioFile* AudioFile::create(const std::string& filePath, int outputSampleRate, int numOutputChannels, size_t outputFramesPerBuffer, bool loop, bool streamPlayback) {
    AudioFile* pFile = new AudioFile();

    /*pFile->m_numOutputChannels = numOutputChannels;
    pFile->m_outputFramesPerBuffer = outputFramesPerBuffer;
    pFile->m_outputSampleRate = outputSampleRate;*/

    pFile->m_loop = loop;
    pFile->m_streamPlayback = streamPlayback;

    pFile->m_pSndFile = sf_open(filePath.c_str(), SFM_READ, &pFile->m_sndFileInfo);
    if (!pFile->m_pSndFile) {
        throw std::runtime_error("Failed to load audio file: " + filePath + ". Error: " + sf_strerror(nullptr));
    }
    const SF_INFO& info = pFile->m_sndFileInfo;

    if (streamPlayback) {
        pFile->m_resample = (info.samplerate != outputSampleRate);
        if (pFile->m_resample) {
            pFile->m_frameData.resize(2*info.channels);
        }
        pFile->m_maxAmplitude = 1.0f; // worst case, we don't know
    } else {
        if (info.channels == numOutputChannels && info.samplerate == outputSampleRate) {
            pFile->m_frameData.resize(info.channels * info.frames);
            if (sf_readf_float(pFile->m_pSndFile, pFile->m_frameData.data(), info.frames) != info.frames) {
                std::cerr << "Weird read on audio file: " << filePath << std::endl;
            }
            if (int err = sf_close(pFile->m_pSndFile); err != SF_ERR_NO_ERROR) {
                std::cerr << "Error: " << sf_error_number(err) << " on closing audio file: " << filePath << std::endl;
            }
        } else {
            std::vector<float> tempData(info.channels * info.frames);
            if (sf_readf_float(pFile->m_pSndFile, tempData.data(), info.frames) != info.frames) {
                std::cerr << "Weird read on audio file: " << filePath << std::endl;
            }
            if (int err = sf_close(pFile->m_pSndFile); err != SF_ERR_NO_ERROR) {
                std::cerr << "Error: " << sf_error_number(err) << " on closing audio file: " << filePath << std::endl;
            }

            if (info.channels != numOutputChannels) {  // Remap channels
                if (info.channels != 1 && info.channels != 2) {
                    throw std::runtime_error("Audio file: " + filePath + " has " + std::to_string(info.channels) + " channels. Only mono and stereo files are supported.");
                }

                if (numOutputChannels == 1) {  // convert stereo to mono
                    pFile->m_frameData.resize(info.frames);
                    for (size_t i = 0; i < pFile->m_frameData.size(); ++i) {
                        pFile->m_frameData[i] = 0.5f * (tempData[2*i] + tempData[2*i+1]);
                    }
                } else {  // convert mono to stereo
                    pFile->m_frameData.resize(2*info.frames);
                    for (size_t i = 0; i < pFile->m_frameData.size(); ++i) {
                        pFile->m_frameData[i] = tempData[i/2];
                    }
                }

                // Do we still need to resample? If so, copy back into temp storage
                if (info.samplerate != outputSampleRate) tempData.assign(pFile->m_frameData.begin(), pFile->m_frameData.end());
            }

            if (info.samplerate != outputSampleRate) {  // Resample
                size_t numOutputFrames = static_cast<size_t>(info.frames * outputSampleRate / info.samplerate);
                pFile->m_frameData.resize(numOutputFrames * numOutputChannels);

                for (size_t i = 0; i < numOutputFrames; ++i) {
                    size_t iindex = static_cast<size_t>(i * info.samplerate / outputSampleRate);
                    float interp = static_cast<float>(i * info.samplerate) / outputSampleRate - iindex;

                    for (int j = 0; j < numOutputChannels; ++j) {
                        if (i+1 == numOutputFrames) {
                            pFile->m_frameData[i * numOutputChannels + j] = tempData[iindex * numOutputChannels + j];
                        } else {
                            pFile->m_frameData[i * numOutputChannels + j] =
                                (1.0f - interp) * tempData[iindex * numOutputChannels + j] + interp * tempData[(iindex + 1) * numOutputChannels + j];
                        }
                        pFile->m_maxAmplitude = std::max(pFile->m_maxAmplitude, std::abs(pFile->m_frameData[i * numOutputChannels + j]));

                        /*if ( pFile->m_frameData[i * numOutputChannels + j] > 1.1f) {
                            std::cout << pFile->m_frameData[i * numOutputChannels + j] << " "
                                    << (i * numOutputChannels + j) << " "
                                    << (iindex * numOutputChannels + j) << " "
                                    << pFile->m_frameData.size() << " "
                                    << tempData.size() << " "
                                    << interp << std::endl;

                        }*/
                    }
                }
            }
        }
        pFile->m_pSndFile = nullptr;
    }

    return pFile;
}

void AudioFile::destroy(AudioFile* pFile) {
    if (pFile) {
        if (pFile->m_pSndFile) {
            if (int err = sf_close(pFile->m_pSndFile); err != SF_ERR_NO_ERROR) {
                std::cerr << "Error: " << sf_error_number(err) << " on closing audio file." << std::endl;
            }
        }
        delete pFile;
    }
}

AudioOutputStream::AudioOutputStream(PaDeviceIndex deviceIndex, int sampleRate, int numChannels, size_t framesPerBuffer) :
    m_sampleRate(sampleRate),
    m_numChannels(numChannels),
    m_framesPerBuffer(framesPerBuffer)
{
    const PaDeviceInfo* pDeviceInfo = Pa_GetDeviceInfo(deviceIndex);

    PaStreamParameters openParameters = {};
    openParameters.channelCount = numChannels;
    openParameters.device = deviceIndex;
    openParameters.sampleFormat = paFloat32;
    openParameters.suggestedLatency = pDeviceInfo->defaultLowOutputLatency;

    PaError err;
    err = Pa_OpenStream(&m_pStream, nullptr, &openParameters, static_cast<double>(sampleRate), framesPerBuffer, 0, AudioOutputStream::outputStreamCallback, this);
    if (err != paNoError) {
        throw std::runtime_error("Error creating audio output stream: " + std::string(Pa_GetErrorText(err)));
    }
}

AudioOutputStream::~AudioOutputStream() {
    if (m_pStream) {
        PaError err = Pa_CloseStream(m_pStream);
        if (err != paNoError) {
            std::cerr << "Error closing audio output stream: " << Pa_GetErrorText(err) << std::endl;
        }
    }
    for (AudioFile* pFile : m_mainData.pPlayingFiles) {
        AudioFile::destroy(pFile);
    }
}

void AudioOutputStream::start() {
    PaError err = Pa_StartStream(m_pStream);
    if (err != paNoError) {
        throw std::runtime_error("Error starting audio output stream: " + std::string(Pa_GetErrorText(err)));
    }
}

void AudioOutputStream::stop() {
    PaError err = Pa_StopStream(m_pStream);
    if (err != paNoError) {
        throw std::runtime_error("Error stopping audio output stream: " + std::string(Pa_GetErrorText(err)));
    }
}

void AudioOutputStream::addFile(AudioFile* pFile, float volume) {
    m_mainData.pPlayingFiles.push_back(pFile);
    m_mainData.positions.emplace_back();
    m_mainData.innerRadii.emplace_back();
    m_mainData.outerRadii.emplace_back();
    m_mainData.usePositions.push_back(false);
    m_mainData.volumes.push_back(volume);
}

void AudioOutputStream::addFile(AudioFile* pFile, float volume, const glm::vec3& position, float innerRadius, float outerRadius) {
    m_mainData.pPlayingFiles.push_back(pFile);
    m_mainData.positions.push_back(position);
    m_mainData.innerRadii.push_back(innerRadius);
    m_mainData.outerRadii.push_back(outerRadius);
    m_mainData.usePositions.push_back(true);
    m_mainData.volumes.push_back(volume);
}

bool AudioOutputStream::update() {
    AudioOutputStreamConcurrentState ccstate = m_currentStateAtomic.load();
    if (ccstate.hasNewData) return false;  // the callback hasn't consumed our new data by flipping the readActiveIndex yet, but it will soon so just wait until then
    // if the concurrentState hasNewData flag isn't set, the callback will not change the data so we can be sure this is safe without locking

    // remove sounds we flagged last update (this way we avoid contamination since the callback for sure doesn't have access to these anymore)
    for (AudioFile* pFile : m_pFilesToRemove) AudioFile::destroy(pFile);
    m_pFilesToRemove.clear();

    // remove finished sounds
    std::vector<bool> toRemove(m_mainData.pPlayingFiles.size(), false);
    bool removeAny = false;
    for (size_t i = 0; i < m_mainData.pPlayingFiles.size(); ++i) {
        if (m_mainData.pPlayingFiles[i]->m_finished) {
            if (m_mainData.pPlayingFiles[i]->m_loop) {
                m_mainData.pPlayingFiles[i]->m_finished = false;
                if (m_mainData.pPlayingFiles[i]->m_streamPlayback) {
                    sf_seek(m_mainData.pPlayingFiles[i]->m_pSndFile, 0, SF_SEEK_SET);
                } else {
                    m_mainData.pPlayingFiles[i]->m_frameIndex = 0;
                }
            } else {
                toRemove[i] = true;
                removeAny = true;
            }
        }
    }
    if (removeAny) {
        for (int i = toRemove.size()-1; i >= 0; --i) {
            if (toRemove[i]) {
                m_pFilesToRemove.push_back(m_mainData.pPlayingFiles[i]);
                m_mainData.pPlayingFiles.erase(m_mainData.pPlayingFiles.begin() + i);
                m_mainData.positions.erase(m_mainData.positions.begin() + i);
                m_mainData.innerRadii.erase(m_mainData.innerRadii.begin() + i);
                m_mainData.outerRadii.erase(m_mainData.outerRadii.begin() + i);
                m_mainData.usePositions.erase(m_mainData.usePositions.begin() + i);
            }
        }
    }

    m_mainData.gains.resize(m_numChannels * m_mainData.pPlayingFiles.size(), 0.0f);
    float gainSum[2] = {0.0f, 0.0f};
    for (size_t i = 0; i < m_mainData.pPlayingFiles.size(); ++i) {
        if (m_mainData.usePositions[i]) {
            glm::vec3 relativePos = glm::vec3(m_mainData.listenerInverseTransform * glm::vec4(m_mainData.positions[i], 1.0f));
            float distance = glm::length(relativePos);

            float attenuation = 1.0f;
            float pan = 0.5f;
            if (distance > m_mainData.innerRadii[i]) {
                if (distance > m_mainData.outerRadii[i]) continue;
                /*float maxFalloff = 1.0f / (1.0f + m_mainData.outerRadii[i] - m_mainData.innerRadii[i]);
                float falloff = 1.0f / (1.0f + distance - m_mainData.innerRadii[i]);
                attenuation = (falloff - maxFalloff) / (1.0f - maxFalloff);*/
                attenuation = 1.0f - ((distance - m_mainData.innerRadii[i]) / (m_mainData.outerRadii[i] - m_mainData.innerRadii[i]));
                attenuation *= attenuation;
                //if (std::abs(attenuation) > 1.0f) std::cout << attenuation << std::endl;

                relativePos /= distance;
                pan = (1.0f + relativePos.x) / 2.0f;
            }

            if (m_numChannels == 1) {
                m_mainData.gains[i] = attenuation * m_mainData.volumes[i];
                gainSum[0] += m_mainData.gains[i] * m_mainData.pPlayingFiles[i]->m_maxAmplitude;
            } else {
                m_mainData.gains[2*i] = glm::cos((float) M_PI * pan / 2.0f) * attenuation * m_mainData.volumes[i];
                m_mainData.gains[2*i+1] = glm::sin((float) M_PI * pan / 2.0f) * attenuation * m_mainData.volumes[i];
                gainSum[0] += m_mainData.gains[2*i] * m_mainData.pPlayingFiles[i]->m_maxAmplitude;
                gainSum[1] += m_mainData.gains[2*i+1] * m_mainData.pPlayingFiles[i]->m_maxAmplitude;
            }
        } else {
            if (m_numChannels == 1) {
                m_mainData.gains[i] = m_mainData.volumes[i];
                gainSum[0] += m_mainData.gains[i] * m_mainData.pPlayingFiles[i]->m_maxAmplitude;
            } else {
                m_mainData.gains[2*i] = m_mainData.volumes[i];
                m_mainData.gains[2*i+1] = m_mainData.volumes[i];
                gainSum[0] += m_mainData.gains[2*i] * m_mainData.pPlayingFiles[i]->m_maxAmplitude;
                gainSum[1] += m_mainData.gains[2*i+1] * m_mainData.pPlayingFiles[i]->m_maxAmplitude;
            }
        }
    }
    for (int i = 0; i < m_numChannels; ++i) {
        if (gainSum[i] > m_masterVolume) {
            for (size_t j = 0; j < m_mainData.pPlayingFiles.size(); ++j) {
                m_mainData.gains[j*m_numChannels+i] *= m_masterVolume / gainSum[i];
            }
        }
    }


    m_dataBuffer[1 - ccstate.readActiveIndex] = m_mainData;
    ccstate.hasNewData = true;
    m_currentStateAtomic.store(ccstate);
    return true;
}

void AudioOutputStream::setListenerInverseTransform(const glm::mat4& mat) {
    m_mainData.listenerInverseTransform = mat;
}

void AudioOutputStream::setMasterVolume(float volume) {
    m_masterVolume = volume;
}

int AudioOutputStream::getSampleRate() const { return m_sampleRate; }
int AudioOutputStream::getNumChannels() const { return m_numChannels; }
int AudioOutputStream::getFramesPerBuffer() const { return m_framesPerBuffer; }

int AudioOutputStream::outputStreamCallback(const void* pInput, void* pOutput, unsigned long frameCount, const PaStreamCallbackTimeInfo* pTimeInfo, PaStreamCallbackFlags statusFlags, void* pUserData) {
    AudioOutputStream* pStream = static_cast<AudioOutputStream*>(pUserData);

    float* pFloatOutput = static_cast<float*>(pOutput);

    size_t numItems = static_cast<size_t>(frameCount * pStream->m_numChannels);

    std::fill(pFloatOutput, pFloatOutput + numItems, 0.0f);

    AudioOutputStreamConcurrentState ccstate = pStream->m_currentStateAtomic.load();
    int readActiveIndex = ccstate.hasNewData ? 1-ccstate.readActiveIndex : ccstate.readActiveIndex;
    const AudioOutputStreamData& data = pStream->m_dataBuffer[readActiveIndex];

    if (ccstate.hasNewData) {
        ccstate.readActiveIndex = readActiveIndex;
        ccstate.hasNewData = false;
        pStream->m_currentStateAtomic.store(ccstate);
    }

    for (size_t i = 0; i < data.pPlayingFiles.size(); ++i) {
        AudioFile* pFile = data.pPlayingFiles[i];
        if (pFile->m_finished) continue;

        /*float gain[2] = {1.0f, 1.0f};
        if (data.usePositions[i]) {
            glm::vec3 relativePos = glm::vec3(data.listenerInverseTransform * glm::vec4(data.positions[i], 1.0f));
            float distance = glm::length(relativePos);

            if (distance > data.innerRadii[i]) {
                if (distance > data.outerRadii[i]) continue;
                float maxFalloff = 1.0f / (1.0f + data.outerRadii[i] - data.innerRadii[i]);
                float falloff = 1.0f / (1.0f + distance - data.innerRadii[i]);
                float attenuation = (falloff - maxFalloff) / (1.0f - maxFalloff);

                if (pStream->getNumChannels() == 1) gain[0] = attenuation;
                else {
                    relativePos /= distance;
                    float pan = (1.0f + relativePos.x) / 2.0f;
                    gain[0] = glm::cos((float) M_PI * pan / 2.0f) * attenuation;
                    gain[1] = glm::sin((float) M_PI * pan / 2.0f) * attenuation;
                }
            }
        }
        gain[0] *= data.volumes[i];
        gain[1] *= data.volumes[i];*/
        float gain[2];
        if (pStream->getNumChannels() == 1) {
            gain[0] = data.gains[i];
            gain[1] = 0.0f;
        } else {
            gain[0] = data.gains[2*i];
            gain[1] = data.gains[2*i+1];
        }

        if (pFile->m_streamPlayback) {
            const SF_INFO& fileInfo = pFile->m_sndFileInfo;
            if (pFile->m_resample) {
                float sampleRatio = static_cast<float>(fileInfo.samplerate) / pStream->m_sampleRate;
                if (sf_readf_float(pFile->m_pSndFile, &pFile->m_frameData[fileInfo.channels], 1)) {
                    float frac = 1.0f;  // fractional value to be used for interpolation factor
                    for (size_t i = 0; i < frameCount; ++i) {
                        if (frac > 2.0f) {  // only will happen when downsampling
                            // rewrite both interpolation frames
                            // instead of reading frames that won't contribute to any output frame (in the case of a much higher input sample rate),
                            //   seek ahead to the position where the next 2 frames to read will be the right ones for this frame's output
                            sf_count_t seekFrames = static_cast<sf_count_t>(frac) - 2;
                            if (seekFrames > 0 && sf_seek(pFile->m_pSndFile, seekFrames, SF_SEEK_CUR) == -1) {
                                // We're at EOF
                                if (pFile->m_loop) {
                                    // Back to beginning
                                    sf_seek(pFile->m_pSndFile, 0, SF_SEEK_SET);
                                } else {
                                    pFile->m_finished = true;
                                    break;
                                }
                            }
                            sf_count_t framesRead = sf_readf_float(pFile->m_pSndFile, &pFile->m_frameData[0], 2);
                            if (framesRead < 2) {
                                if (pFile->m_loop) {
                                    if (framesRead == 1) {
                                        // back to beginning and read first frame
                                        sf_seek(pFile->m_pSndFile, 0, SF_SEEK_SET);
                                        sf_readf_float(pFile->m_pSndFile, &pFile->m_frameData[fileInfo.channels], 1);
                                    } else {
                                        // back to beginning and read first two frames
                                        sf_seek(pFile->m_pSndFile, 0, SF_SEEK_SET);
                                        sf_readf_float(pFile->m_pSndFile, &pFile->m_frameData[0], 2);
                                    }
                                } else {
                                    // technically, we may have read 1 last frame but I'm not gonna sweat that detail
                                    pFile->m_finished = true;
                                    break;
                                }
                            }
                            frac -= static_cast<int>(frac);
                        } else if (frac >= 1.0f) {  // switch first and second frame, read next frame into second
                            for (int j = 0; j < fileInfo.channels; ++j) {
                                pFile->m_frameData[j] = pFile->m_frameData[j + fileInfo.channels];
                            }
                            if (sf_readf_float(pFile->m_pSndFile, &pFile->m_frameData[fileInfo.channels], 1) == 0) {
                                if (pFile->m_loop) {
                                    // back to beginning
                                    sf_seek(pFile->m_pSndFile, 0, SF_SEEK_SET);
                                    sf_readf_float(pFile->m_pSndFile, &pFile->m_frameData[fileInfo.channels], 1);
                                } else {
                                    // if there's no data to read, the file must be finished;
                                    pFile->m_finished = true;
                                    break;
                                }
                            }
                            frac -= 1.0f;
                        }
                        if (pStream->m_numChannels == fileInfo.channels) {
                            for (int j = 0; j < pStream->m_numChannels; ++j) {
                                pFloatOutput[i*pStream->m_numChannels+j] += gain[j] * ((1.0f - frac) * pFile->m_frameData[j] + frac * pFile->m_frameData[j + fileInfo.channels]);
                            }
                        } else if (pStream->m_numChannels == 1) {  // stereo -> mono
                            pFloatOutput[i] += gain[0] * 0.5f * ((1.0f - frac) * (pFile->m_frameData[0] + pFile->m_frameData[1]) + frac * (pFile->m_frameData[2] + pFile->m_frameData[3]));
                        } else {  // mono -> stereo
                            pFloatOutput[2*i] += gain[0] * ((1.0f - frac) * pFile->m_frameData[0] + frac * pFile->m_frameData[1]);
                            pFloatOutput[2*i+1] += gain[1] * ((1.0f - frac) * pFile->m_frameData[0] + frac * pFile->m_frameData[1]);
                        }

                        frac += sampleRatio;
                    }
                } else {  // if there's no data to read, the file must be finished;
                    pFile->m_finished = true;
                }
            } else {
                for (size_t i = 0; i < frameCount; ++i) {
                    float frame[2];
                    if (!sf_readf_float(pFile->m_pSndFile, frame, 1)) {
                        if (pFile->m_loop) {
                            sf_seek(pFile->m_pSndFile, 0, SF_SEEK_SET);
                            sf_readf_float(pFile->m_pSndFile, frame, 1);
                        } else {
                            pFile->m_finished = true;
                            break;
                        }
                    }

                    if (fileInfo.channels == pStream->m_numChannels) {
                        for (int j = 0; j < pStream->m_numChannels; ++j)
                            pFloatOutput[i*pStream->m_numChannels + j] += gain[j] * frame[j];
                    } else if (pStream->m_numChannels == 1) {  // stereo -> mono
                        pFloatOutput[i] += gain[0] * 0.5f * (frame[0] + frame[1]);
                    } else {  // mono -> stereo
                        pFloatOutput[2*i] += gain[0] * frame[0];
                        pFloatOutput[2*i+1] += gain[1] * frame[0];
                    }
                }
            }
        } else {
            for (size_t i = 0; i < numItems; ++i) {
                if (pFile->m_frameIndex >= pFile->m_frameData.size()) {
                    if (pFile->m_loop) {
                        pFile->m_frameIndex = 0;
                    } else {
                        pFile->m_finished = true;
                        break;
                    }
                }
                //float outputCache = pFloatOutput[i];
                pFloatOutput[i] += gain[i%pStream->m_numChannels] * pFile->m_frameData[pFile->m_frameIndex++];
            }
        }
    }

    /*bool clip = false;
    float maxOutput = 1.0f;
    for (size_t i = 0; i < numItems; ++i) {
        if (std::abs(pFloatOutput[i]) > maxOutput) {
            clip = true;
            maxOutput = std::abs(pFloatOutput[i]);
        }
    }

    if (clip) {
        std::cout << "clip" << maxOutput << std::endl;
    }*/

    return paContinue;
}

AudioEngine::AudioEngine() :
    m_pOutputStream(nullptr),
    m_cleanedUp(true) {
}

AudioEngine::~AudioEngine() {
    if (!m_cleanedUp) cleanup();
}

void AudioEngine::init() {
    if (m_cleanedUp) {
        PaError err;
        if ((err = Pa_Initialize()) != paNoError) {
            throw std::runtime_error("Failed to initialize PortAudio: " + std::string(Pa_GetErrorText(err)));
        }

        PaHostApiIndex hostIndex = Pa_GetDefaultHostApi();
        const PaHostApiInfo* pHostInfo = Pa_GetHostApiInfo(hostIndex);

        if (hostIndex < 0 || !pHostInfo) throw std::runtime_error("PortAudio could not get default host API: " + std::string(Pa_GetErrorText(hostIndex)));

        PaDeviceIndex deviceIndex = pHostInfo->defaultOutputDevice;
        const PaDeviceInfo* pDeviceInfo = Pa_GetDeviceInfo(deviceIndex);

        if (deviceIndex < 0 || !pDeviceInfo) throw std::runtime_error("PortAudio could not get default output device: " + std::string(Pa_GetErrorText(deviceIndex)));

        m_pOutputStream = new AudioOutputStream(deviceIndex, 48000, 2, 0);

        m_cleanedUp = false;
    }
}

void AudioEngine::cleanup() {
    if (!m_cleanedUp) {
        delete m_pOutputStream;

        PaError err;
        if ((err = Pa_Terminate()) != paNoError) {
            std::cerr << "Error on call to Pa_Terminate(): " << Pa_GetErrorText(err) << std::endl;
        }

        m_cleanedUp = true;
    }
}

AudioOutputStream* const AudioEngine::getOutputStream() const {
    return m_pOutputStream;
}

struct AudioFileData {
    std::string filePath;
    bool loop;
    bool stream;
};

AudioEngine g_audioEngine;

std::vector<AudioFileData> g_audioFileData;
std::vector<AudioFile*> g_pAudioFiles;

void Audio::init() {
    std::cout << "Initializing audio engine." << std::endl;
    g_audioEngine.init();
    g_audioEngine.getOutputStream()->start();
    std::cout << "Audio engine initialized successfully." << std::endl;
}

void Audio::cleanup() {
    g_audioEngine.getOutputStream()->stop();
    g_audioEngine.cleanup();
    for (AudioFile* pFile : g_pAudioFiles) if (pFile) AudioFile::destroy(pFile);
}

Audio::SoundID Audio::addSound(const std::string& filePath, bool loop, bool stream) {
    try {
        AudioFile* pAudioFile = AudioFile::create(filePath,
                                                  g_audioEngine.getOutputStream()->getSampleRate(),
                                                  g_audioEngine.getOutputStream()->getNumChannels(),
                                                  g_audioEngine.getOutputStream()->getFramesPerBuffer(),
                                                  loop, stream);
        g_pAudioFiles.push_back(pAudioFile);
        g_audioFileData.push_back({filePath, loop, stream});
        return static_cast<Audio::SoundID>(g_pAudioFiles.size() - 1);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return Audio::SOUND_NULL;
    }
}

void Audio::setListenerInverseTransform(const glm::mat4& mat) {
    g_audioEngine.getOutputStream()->setListenerInverseTransform(mat);
}

void Audio::playSound(Audio::SoundID sound, float volume) {
    if (sound < g_pAudioFiles.size()) {
        if (g_pAudioFiles[sound]) {
            g_audioEngine.getOutputStream()->addFile(g_pAudioFiles[sound], volume);
            g_pAudioFiles[sound] = nullptr;
        } else {
            AudioFile* pAudioFile = AudioFile::create(g_audioFileData[sound].filePath,
                                                      g_audioEngine.getOutputStream()->getSampleRate(),
                                                      g_audioEngine.getOutputStream()->getNumChannels(),
                                                      g_audioEngine.getOutputStream()->getFramesPerBuffer(),
                                                      g_audioFileData[sound].loop,
                                                      g_audioFileData[sound].stream);
            g_audioEngine.getOutputStream()->addFile(pAudioFile, volume);
        }
    }
}

void Audio::playSound(Audio::SoundID sound, float volume, const glm::vec3& position, float innerRadius, float outerRadius) {
    if (sound < g_pAudioFiles.size()) {
        if (g_pAudioFiles[sound]) {
            g_audioEngine.getOutputStream()->addFile(g_pAudioFiles[sound], volume, position, innerRadius, outerRadius);
            g_pAudioFiles[sound] = nullptr;
        } else {
            AudioFile* pAudioFile = AudioFile::create(g_audioFileData[sound].filePath,
                                                      g_audioEngine.getOutputStream()->getSampleRate(),
                                                      g_audioEngine.getOutputStream()->getNumChannels(),
                                                      g_audioEngine.getOutputStream()->getFramesPerBuffer(),
                                                      g_audioFileData[sound].loop,
                                                      g_audioFileData[sound].stream);
            g_audioEngine.getOutputStream()->addFile(pAudioFile, volume, position, innerRadius, outerRadius);
        }
    }
}

void Audio::update(bool block) {
    if (block) while(!g_audioEngine.getOutputStream()->update());
    else g_audioEngine.getOutputStream()->update();
}

void Audio::setMasterVolume(float volume) {
    g_audioEngine.getOutputStream()->setMasterVolume(volume);
}
