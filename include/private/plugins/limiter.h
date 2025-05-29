/*
 * Copyright (C) 2025 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2025 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-limiter
 * Created on: 3 авг. 2021 г.
 *
 * lsp-plugins-limiter is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins-limiter is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins-limiter. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef PRIVATE_PLUGINS_LIMITER_H_
#define PRIVATE_PLUGINS_LIMITER_H_

#include <lsp-plug.in/plug-fw/plug.h>
#include <lsp-plug.in/plug-fw/core/IDBuffer.h>
#include <lsp-plug.in/dsp-units/ctl/Blink.h>
#include <lsp-plug.in/dsp-units/ctl/Bypass.h>
#include <lsp-plug.in/dsp-units/dynamics/Limiter.h>
#include <lsp-plug.in/dsp-units/util/Delay.h>
#include <lsp-plug.in/dsp-units/util/Dither.h>
#include <lsp-plug.in/dsp-units/util/MeterGraph.h>
#include <lsp-plug.in/dsp-units/util/Oversampler.h>
#include <lsp-plug.in/dsp-units/util/Sidechain.h>

#include <private/meta/limiter.h>

namespace lsp
{
    namespace plugins
    {
        /**
         * Limiter Plugin Series
         */
        class limiter: public plug::Module
        {
            protected:
                enum sc_graph_t
                {
                    G_IN,
                    G_OUT,
                    G_SC,
                    G_GAIN,

                    G_TOTAL
                };

                enum sc_mode_t
                {
                    SCM_INTERNAL,
                    SCM_EXTERNAL,
                    SCM_LINK,
                };

                typedef struct premix_t
                {
                    float                   fInToSc;            // Input -> Sidechain mix
                    float                   fInToLink;          // Input -> Link mix
                    float                   fLinkToIn;          // Link -> Input mix
                    float                   fLinkToSc;          // Link -> Sidechain mix
                    float                   fScToIn;            // Sidechain -> Input mix
                    float                   fScToLink;          // Sidechain -> Link mix

                    float                  *vIn[2];             // Input buffer
                    float                  *vOut[2];            // Output buffer
                    float                  *vSc[2];             // Sidechain buffer
                    float                  *vLink[2];           // Link buffer

                    float                  *vTmpIn[2];          // Replacement buffer for input
                    float                  *vTmpLink[2];        // Replacement buffer for link
                    float                  *vTmpSc[2];          // Replacement buffer for sidechain

                    plug::IPort            *pInToSc;            // Input -> Sidechain mix
                    plug::IPort            *pInToLink;          // Input -> Link mix
                    plug::IPort            *pLinkToIn;          // Link -> Input mix
                    plug::IPort            *pLinkToSc;          // Link -> Sidechain mix
                    plug::IPort            *pScToIn;            // Sidechain -> Input mix
                    plug::IPort            *pScToLink;          // Sidechain -> Link mix
                } premix_t;

                typedef struct channel_t
                {
                    dspu::Bypass        sBypass;            // Bypass
                    dspu::Oversampler   sOver;              // Oversampler object for signal
                    dspu::Oversampler   sScOver;            // Sidechain oversampler object for signal
                    dspu::Limiter       sLimit;             // Limiter
                    dspu::Delay         sDataDelay;         // Input signal delay
                    dspu::Delay         sDryDelay;          // Dry delay
                    dspu::MeterGraph    sGraph[G_TOTAL];    // Input meter graph
                    dspu::Blink         sBlink;             // Gain blink

                    float              *vIn;                // Input data
                    float              *vSc;                // Sidechain data
                    float              *vShmIn;             // Shared memory input
                    float              *vOut;               // Output data

                    float              *vDataBuf;           // Audio data buffer (oversampled)
                    float              *vScBuf;             // Sidechain buffer
                    float              *vGainBuf;           // Applying gain buffer
                    float              *vOutBuf;            // Output buffer

                    bool                bVisible[G_TOTAL];  // Input visibility
                    bool                bOutVisible;        // Output visibility
                    bool                bGainVisible;       // Gain visibility
                    bool                bScVisible;         // Sidechain visibility

                    plug::IPort        *pIn;                // Input port
                    plug::IPort        *pOut;               // Output port
                    plug::IPort        *pSc;                // Sidechain port
                    plug::IPort        *pShmIn;             // Sidechain port
                    plug::IPort        *pVisible[G_TOTAL];  // Input visibility

                    plug::IPort        *pGraph[G_TOTAL];    // History graphs
                    plug::IPort        *pMeter[G_TOTAL];    // Meters
                } channel_t;

            protected:
                uint32_t            nChannels;      // Number of channels
                bool                bSidechain;     // Sidechain presence flag
                bool                bPause;         // Pause button
                bool                bClear;         // Clear button
                bool                bScListen;      // Sidechain listen
                channel_t          *vChannels;      // Audio channels
                float              *vTime;          // Time points buffer
                uint32_t            nScMode;        // Sidechain mode
                float               fInGain;        // Input gain
                float               fOutGain;       // Output gain
                float               fPreamp;        // Sidechain pre-amplification
                float               fStereoLink;    // Stereo linking
                core::IDBuffer     *pIDisplay;      // Inline display buffer
                bool                bUISync;        // Synchronize with UI

                dspu::Dither        sDither;        // Dither
                premix_t            sPremix;        // Premix

                plug::IPort        *pBypass;        // Bypass port
                plug::IPort        *pInGain;        // Input gain
                plug::IPort        *pOutGain;       // Output gain
                plug::IPort        *pPreamp;        // Sidechain pre-amplification
                plug::IPort        *pAlrOn;         // Automatic level regulation
                plug::IPort        *pAlrAttack;     // Automatic level regulation attack
                plug::IPort        *pAlrRelease;    // Automatic level regulation release
                plug::IPort        *pMode;          // Operating mode
                plug::IPort        *pThresh;        // Limiter threshold
                plug::IPort        *pLookahead;     // Lookahead time
                plug::IPort        *pAttack;        // Attack time
                plug::IPort        *pRelease;       // Release time
                plug::IPort        *pPause;         // Pause gain
                plug::IPort        *pClear;         // Cleanup gain
                plug::IPort        *pScMode;        // Sidechain mode
                plug::IPort        *pScListen;      // Sidechain listen
                plug::IPort        *pKnee;          // Limiter knee
                plug::IPort        *pBoost;         // Gain boost
                plug::IPort        *pOversampling;  // Oversampling
                plug::IPort        *pDithering;     // Dithering
                plug::IPort        *pStereoLink;    // Stereo linking

                uint8_t            *pData;          // Allocated data

            protected:
                static dspu::over_mode_t    get_oversampling_mode(size_t mode);
                static bool                 get_filtering(size_t mode);
                static dspu::limiter_mode_t get_limiter_mode(size_t mode);
                static size_t               get_dithering(size_t mode);

            protected:
                uint32_t                    decode_sidechain_mode(uint32_t mode);
                void                        update_premix();
                void                        premix_channel(uint32_t channel, size_t count);
                void                        sync_latency();
                void                        do_destroy();

            public:
                explicit limiter(const meta::plugin_t *metadata, bool sc, bool stereo);
                virtual ~limiter() override;

            public:
                virtual void        init(plug::IWrapper *wrapper, plug::IPort **ports) override;
                virtual void        destroy() override;

                virtual void        update_settings() override;
                virtual void        update_sample_rate(long sr) override;
                virtual void        ui_activated() override;

                virtual void        process(size_t samples) override;
                virtual bool        inline_display(plug::ICanvas *cv, size_t width, size_t height) override;

                virtual void        dump(dspu::IStateDumper *v) const override;
        };
    } /* namespace plugins */
} /* namespace lsp */

#endif /* PRIVATE_PLUGINS_LIMITER_H_ */
