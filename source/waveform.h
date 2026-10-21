#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

// Source's two 960-sample blocks, drawing every eighth signed sample.
struct Waveform {
    std::array<int16_t, 1920> pcm{};
    size_t cursor = 0;
    void Clear() { pcm.fill(0); cursor = 0; }
    void Append(const int16_t* data, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            pcm[cursor] = data[i];
            cursor = (cursor + 1) % pcm.size();
        }
    }
    int16_t Point(size_t index) const { return pcm[(cursor + index * 8) % pcm.size()]; }
};
