#include "usb_audio.h"
#include "tusb.h"
#include "daisy_core.h"
#include <algorithm>

// C Callbacks and overall structure adapted from tinyusb example code, subject to MIT License

#ifndef DSY_UAC2_MAX_BLOCKSIZE
#define DSY_UAC2_MAX_BLOCKSIZE 64
#endif

namespace daisy
{

// num Tx channels, interleaved - pulled directly from tusb descriptors config
static constexpr size_t kNumTxChannels = CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX;

struct UAC2State
{
    int16_t  usb_txbuf[DSY_UAC2_MAX_BLOCKSIZE * kNumTxChannels];
    uint32_t samp_freq;
    uint32_t blocksize;
    uint8_t  clk_valid;
    audio20_control_range_4_n_t(1) sampleFreqRng; // Sample frequency range state
};

static UAC2State uac2_state;

void USBAudioHandle::Init(uint32_t blocksize)
{
    uac2_state.samp_freq = CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE;
    uac2_state.blocksize
        = std::min(blocksize, (uint32_t)DSY_UAC2_MAX_BLOCKSIZE);
    uac2_state.clk_valid = 1;

    uac2_state.sampleFreqRng.wNumSubRanges = 1;
    uac2_state.sampleFreqRng.subrange[0].bMin
        = CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE;
    uac2_state.sampleFreqRng.subrange[0].bMax
        = CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE;
    uac2_state.sampleFreqRng.subrange[0].bRes = 0;

    Prepare();
}

void USBAudioHandle::Prepare()
{
    std::fill(uac2_state.usb_txbuf,
              uac2_state.usb_txbuf + DSY_UAC2_MAX_BLOCKSIZE,
              (int16_t)0);
}

void USBAudioHandle::ProcessChannel(uint32_t     idx,
                                    const float *samples,
                                    uint32_t     size,
                                    uint32_t     offset)
{
    if(idx >= kNumTxChannels)
        return;

    uint32_t       count = 0;
    const uint32_t isize = std::min(size, uac2_state.blocksize - offset);

    // Interleaved channel write
    int16_t *pout = &uac2_state.usb_txbuf[offset * kNumTxChannels + idx];

    while(count < isize)
    {
        *pout = f2s16(samples[count]);
        pout += kNumTxChannels;
        count++;
    }
}

void USBAudioHandle::WriteToEndpoint()
{
    const uint16_t size
        = uac2_state.blocksize * kNumTxChannels * sizeof(int16_t);
    uint16_t res = tud_audio_write(uac2_state.usb_txbuf, size);
    if(res != size)
    {
        // LOG("[USB AUDIO] write size mismatch");
    }
}

/// --- Private Globals for C Callback Implementations ---

extern "C"
{
    //--------------------------------------------------------------------+
    // Application Callback API Implementations
    //--------------------------------------------------------------------+

    // // Invoked when audio class specific set request received for an EP
    bool tud_audio_set_req_ep_cb(uint8_t                       rhport,
                                 tusb_control_request_t const *p_request,
                                 uint8_t                      *pBuff)
    {
        (void)rhport;
        (void)pBuff;

        // We do not support any set range requests here, only current value requests
        TU_VERIFY(p_request->bRequest == AUDIO20_CS_REQ_CUR);

        // Page 91 in UAC2 specification
        uint8_t channelNum = TU_U16_LOW(p_request->wValue);
        uint8_t ctrlSel    = TU_U16_HIGH(p_request->wValue);
        uint8_t ep         = TU_U16_LOW(p_request->wIndex);

        (void)channelNum;
        (void)ctrlSel;
        (void)ep;

        return false; // Yet not implemented
    }

    // Invoked when audio class specific set request received for an interface
    bool tud_audio_set_req_itf_cb(uint8_t                       rhport,
                                  tusb_control_request_t const *p_request,
                                  uint8_t                      *pBuff)
    {
        (void)rhport;
        (void)pBuff;

        // We do not support any set range requests here, only current value requests
        TU_VERIFY(p_request->bRequest == AUDIO20_CS_REQ_CUR);

        // Page 91 in UAC2 specification
        uint8_t channelNum = TU_U16_LOW(p_request->wValue);
        uint8_t ctrlSel    = TU_U16_HIGH(p_request->wValue);
        uint8_t itf        = TU_U16_LOW(p_request->wIndex);

        (void)channelNum;
        (void)ctrlSel;
        (void)itf;

        return false; // Yet not implemented
    }

    // Invoked when audio class specific set request received for an entity
    bool tud_audio_set_req_entity_cb(uint8_t                       rhport,
                                     tusb_control_request_t const *p_request,
                                     uint8_t                      *pBuff)
    {
        (void)rhport;

        // Page 91 in UAC2 specification
        // uint8_t channelNum = TU_U16_LOW(p_request->wValue);
        // uint8_t ctrlSel    = TU_U16_HIGH(p_request->wValue);
        // uint8_t itf        = TU_U16_LOW(p_request->wIndex);
        uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

        // We do not support any set range requests here, only current value requests
        TU_VERIFY(p_request->bRequest == AUDIO20_CS_REQ_CUR);

        // If request is for our feature unit
        if(entityID == 2)
        {
            // we dont' care about feature unit (vol/mute)
            return false;
        }
        return false; // Yet not implemented
    }

    // Invoked when audio class specific get request received for an EP
    bool tud_audio_get_req_ep_cb(uint8_t                       rhport,
                                 tusb_control_request_t const *p_request)
    {
        (void)rhport;

        // Page 91 in UAC2 specification
        uint8_t channelNum = TU_U16_LOW(p_request->wValue);
        uint8_t ctrlSel    = TU_U16_HIGH(p_request->wValue);
        uint8_t ep         = TU_U16_LOW(p_request->wIndex);

        (void)channelNum;
        (void)ctrlSel;
        (void)ep;

        //	return tud_control_xfer(rhport, p_request, &tmp, 1);

        return false; // Yet not implemented
    }

    // Invoked when audio class specific get request received for an interface
    bool tud_audio_get_req_itf_cb(uint8_t                       rhport,
                                  tusb_control_request_t const *p_request)
    {
        (void)rhport;

        // Page 91 in UAC2 specification
        uint8_t channelNum = TU_U16_LOW(p_request->wValue);
        uint8_t ctrlSel    = TU_U16_HIGH(p_request->wValue);
        uint8_t itf        = TU_U16_LOW(p_request->wIndex);

        (void)channelNum;
        (void)ctrlSel;
        (void)itf;

        return false; // Yet not implemented
    }

    // Invoked when audio class specific get request received for an entity
    bool tud_audio_get_req_entity_cb(uint8_t                       rhport,
                                     tusb_control_request_t const *p_request)
    {
        (void)rhport;

        // Page 91 in UAC2 specification
        // uint8_t channelNum = TU_U16_LOW(p_request->wValue);
        // uint8_t itf = TU_U16_LOW(p_request->wIndex); 			// Since we have only one audio function implemented, we do not need the itf value
        uint8_t ctrlSel  = TU_U16_HIGH(p_request->wValue);
        uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

        // Input terminal (Microphone input)
        if(entityID == 1)
        {
            switch(ctrlSel)
            {
                case AUDIO20_TE_CTRL_CONNECTOR:
                {
                    // The terminal connector control only has a get request with only the CUR attribute.
                    audio20_desc_channel_cluster_t ret;

                    // Those are dummy values for now
                    ret.bNrChannels     = 1;
                    ret.bmChannelConfig = (audio20_channel_config_t)0;
                    ret.iChannelNames   = 0;

                    // TU_LOG2("    Get terminal connector\r\n");

                    return tud_audio_buffer_and_schedule_control_xfer(
                        rhport, p_request, (void *)&ret, sizeof(ret));
                }
                break;

                    // Unknown/Unsupported control selector
                default: TU_BREAKPOINT(); return false;
            }
        }

        // Feature unit
        if(entityID == 2)
        {
            // we dont' care about feature unit (vol/mute)
            return false;
        }

        // Clock Source unit
        if(entityID == 4)
        {
            switch(ctrlSel)
            {
                case AUDIO20_CS_CTRL_SAM_FREQ:
                    // channelNum is always zero in this case
                    switch(p_request->bRequest)
                    {
                        case AUDIO20_CS_REQ_CUR:
                            // TU_LOG2("    Get Sample Freq.\r\n");
                            // Buffered control transfer is needed for IN flow control to work
                            return tud_audio_buffer_and_schedule_control_xfer(
                                rhport,
                                p_request,
                                &uac2_state.samp_freq,
                                sizeof(uac2_state.samp_freq));

                        case AUDIO20_CS_REQ_RANGE:
                            // TU_LOG2("    Get Sample Freq. range\r\n");
                            return tud_control_xfer(
                                rhport,
                                p_request,
                                &uac2_state.sampleFreqRng,
                                sizeof(uac2_state.sampleFreqRng));

                            // Unknown/Unsupported control
                        default: TU_BREAKPOINT(); return false;
                    }
                    break;

                case AUDIO20_CS_CTRL_CLK_VALID:
                    // Only cur attribute exists for this request
                    // TU_LOG2("    Get Sample Freq. valid\r\n");
                    return tud_control_xfer(rhport,
                                            p_request,
                                            &uac2_state.clk_valid,
                                            sizeof(uac2_state.clk_valid));

                // Unknown/Unsupported control
                default: TU_BREAKPOINT(); return false;
            }
        }

        // TU_LOG2("  Unsupported entity: %d\r\n", entityID);
        return false; // Yet not implemented
    }
}
} // namespace daisy
