// miniaudio's implementation, compiled once. Playback only, WASAPI only, WAV only.
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_MP3
#define MA_NO_FLAC
#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_ENABLE_WASAPI
#include <miniaudio.h>
