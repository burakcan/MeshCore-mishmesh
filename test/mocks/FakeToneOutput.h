#pragma once
#include <mishmesh/sound/ToneOutput.h>
#include <vector>

struct FakeToneOutput : mishmesh::sound::IToneOutput {
  struct Call { uint16_t freq; mishmesh::sound::VolumeLevel vol; bool silence; };
  std::vector<Call> calls;
  bool begun = false;
  void begin() override { begun = true; }
  void tone(uint16_t f, mishmesh::sound::VolumeLevel v) override { calls.push_back({f, v, false}); }
  void silence() override { calls.push_back({0, mishmesh::sound::VolumeLevel::Mute, true}); }
};
