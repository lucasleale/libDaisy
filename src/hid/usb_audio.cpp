// C callbacks adapted from tinyusb example code, subject to MIT License

#include "usb_audio.h"
#include "tusb.h"
#include "daisy_core.h"

namespace
{
constexpr size_t kNumChannels  = CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX;
constexpr size_t kMaxBlockSize = 64;
static_assert(kNumChannels == 2, "Write() interleaves exactly two channels");

volatile bool     streaming      = false;
volatile uint32_t overflow_count = 0;
volatile uint32_t underrun_count = 0;

uint32_t samp_freq = CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE;
uint8_t  clk_valid = 1;
audio20_control_range_4_n_t(1) samp_freq_range;
} // namespace

namespace daisy
{
void USBAudioHandle::Init()
{
    samp_freq_range.wNumSubRanges    = 1;
    samp_freq_range.subrange[0].bMin = CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE;
    samp_freq_range.subrange[0].bMax = CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE;
    samp_freq_range.subrange[0].bRes = 0;
}

void USBAudioHandle::Write(const float* left, const float* right, size_t size)
{
    if(!streaming || !tud_mounted())
        return;

    if(size > kMaxBlockSize)
        size = kMaxBlockSize;

    const uint16_t bytes = size * kNumChannels * sizeof(int16_t);

    // The EP IN FIFO is overwritable: writing into a full FIFO would
    // overwrite data the USB interrupt may be reading. Drop the block instead.
    tu_fifo_t* ff = tud_audio_get_ep_in_ff();
    if(ff == nullptr || tu_fifo_remaining(ff) < bytes)
    {
        overflow_count++;
        return;
    }

    int16_t buf[kMaxBlockSize * kNumChannels];
    for(size_t i = 0; i < size; i++)
    {
        buf[i * 2]     = f2s16(left[i]);
        buf[i * 2 + 1] = f2s16(right[i]);
    }
    tud_audio_write(buf, bytes);
}

bool USBAudioHandle::IsStreaming() const
{
    return streaming;
}

uint32_t USBAudioHandle::GetAndClearOverflows()
{
    const uint32_t n = overflow_count;
    overflow_count   = 0;
    return n;
}

uint32_t USBAudioHandle::GetAndClearUnderruns()
{
    const uint32_t n = underrun_count;
    underrun_count   = 0;
    return n;
}
} // namespace daisy

extern "C"
{
    // Host selected an alternate setting: alt 0 means stream closed
    bool tud_audio_set_itf_cb(uint8_t                       rhport,
                              tusb_control_request_t const* p_request)
    {
        (void)rhport;
        streaming = tu_u16_low(p_request->wValue) != 0;
        return true;
    }

    bool tud_audio_set_itf_close_ep_cb(uint8_t                       rhport,
                                       tusb_control_request_t const* p_request)
    {
        (void)rhport;
        (void)p_request;
        streaming = false;
        return true;
    }

    // Runs in the USB interrupt after each ISO IN transfer
    bool tud_audio_tx_done_isr(uint8_t  rhport,
                               uint16_t n_bytes_sent,
                               uint8_t  func_id,
                               uint8_t  ep_in,
                               uint8_t  cur_alt_setting)
    {
        (void)rhport;
        (void)func_id;
        (void)ep_in;
        (void)cur_alt_setting;
        if(streaming && n_bytes_sent == 0)
            underrun_count++;
        return true;
    }

    bool tud_audio_get_req_entity_cb(uint8_t                       rhport,
                                     tusb_control_request_t const* p_request)
    {
        const uint8_t ctrl_sel  = TU_U16_HIGH(p_request->wValue);
        const uint8_t entity_id = TU_U16_HIGH(p_request->wIndex);

        // Input terminal
        if(entity_id == 1 && ctrl_sel == AUDIO20_TE_CTRL_CONNECTOR)
        {
            audio20_desc_channel_cluster_t ret;
            ret.bNrChannels     = kNumChannels;
            ret.bmChannelConfig = (audio20_channel_config_t)0;
            ret.iChannelNames   = 0;
            return tud_audio_buffer_and_schedule_control_xfer(
                rhport, p_request, &ret, sizeof(ret));
        }

        // Clock source
        if(entity_id == 4)
        {
            if(ctrl_sel == AUDIO20_CS_CTRL_SAM_FREQ)
            {
                // Buffered transfer: IN flow control reads the rate from it
                if(p_request->bRequest == AUDIO20_CS_REQ_CUR)
                    return tud_audio_buffer_and_schedule_control_xfer(
                        rhport, p_request, &samp_freq, sizeof(samp_freq));

                if(p_request->bRequest == AUDIO20_CS_REQ_RANGE)
                    return tud_control_xfer(rhport,
                                            p_request,
                                            &samp_freq_range,
                                            sizeof(samp_freq_range));
            }

            if(ctrl_sel == AUDIO20_CS_CTRL_CLK_VALID
               && p_request->bRequest == AUDIO20_CS_REQ_CUR)
                return tud_control_xfer(
                    rhport, p_request, &clk_valid, sizeof(clk_valid));
        }

        // Feature unit (no controls) and anything else: stall
        return false;
    }
}