#ifndef AUDIO_H_INCLUDED
#define AUDIO_H_INCLUDED

#include <string>

#include <glm/glm.hpp>

class Audio {

public:

    typedef uint32_t SoundID;
    static constexpr SoundID SOUND_NULL = std::numeric_limits<SoundID>::max();

    static void init();

    static void cleanup();

    static SoundID addSound(const std::string& filePath, bool loop, bool stream = false);

    static void setListenerInverseTransform(const glm::mat4& mat);

    static void playSound(SoundID sound, float volume);
    static void playSound(SoundID sound, float volume, const glm::vec3& position, float innerRadius, float outerRadius);

    static void update(bool block = false);

    static void setMasterVolume(float volume);

};

#endif // AUDIO_H_INCLUDED
