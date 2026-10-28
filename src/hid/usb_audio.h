#pragma once

#include <cstddef>
#include <cstdint>

namespace daisy
{
/** USB Audio Class 2.0 stream, device to host only.
 *  Stereo, 16 bit, fixed sample rate (see tusb_config.h).
 *
 *  Write() runs in the audio callback and queues samples into the TinyUSB
 *  EP IN FIFO. Isochronous packets are built by TinyUSB inside the USB
 *  interrupt, so tud_task() latency does not affect the stream.
 */
class USBAudioHandle
{
  public:
    USBAudioHandle()  = default;
    ~USBAudioHandle() = default;

    void Init();

    // Interleave, convert and queue one block.
    //Does nothing while the host is not streaming.

    void Write(const float* left, const float* right, size_t size);

    bool IsStreaming() const;

    // Blocks dropped because the FIFO had no room, since last call
    uint32_t GetAndClearOverflows();

    // Zero length packets sent while streaming, since last call.
    //  One is expected every time the host opens the stream.

    uint32_t GetAndClearUnderruns();
};
} // namespace daisy