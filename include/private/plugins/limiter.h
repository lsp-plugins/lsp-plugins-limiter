/*
 * Copyright (C) 2021 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2021 Vladimir Sadovnikov <sadko4u@gmail.com>
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

                typedef struct channel_t
                {
                    dspu::Bypass        sBypass;            // Bypass
                    dspu::Oversampler   sOver;              // Oversampler object for signal
                    dspu::Oversampler   sScOver;            // Sidechain oversampler object for signal
                    dspu::Limiter       sLimit;             // Limiter
                    dspu::Delay         sDryDelay;          // Dry delay
                    dspu::MeterGraph    sGraph[G_TOTAL];    // Input meter graph
                    dspu::Blink         sBlink;             // Gain blink

                    const float        *vIn;                // Input data
                    const float        *vSc;                // Sidechain data
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
                    plug::IPort        *pVisible[G_TOTAL];  // Input visibility

                    plug::IPort        *pGraph[G_TOTAL];    // History graphs
                    plug::IPort        *pMeter[G_TOTAL];    // Meters
                } channel_t;

            protected:
                size_t              nChannels;      // Number of channels
                bool                bSidechain;     // Sidechain presence flag
                channel_t          *vChannels;      // Audio channels
                float              *vTime;          // Time points buffer
                bool                bPause;         // Pause button
                bool                bClear;         // Clear button
                bool                bExtSc;         // External sidechain
                bool                bScListen;      // Sidechain listen
                float               fInGain;        // Input gain
                float               fOutGain;       // Output gain
                float               fPreamp;        // Sidechain pre-amplification
                size_t              nOversampling;  // Oversampling
                float               fStereoLink;    // Stereo linking
                core::IDBuffer     *pIDisplay;      // Inline display buffer
                bool                bUISync;        // Synchronize with UI

                dspu::Dither        sDither;        // Dither

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
                plug::IPort        *pExtSc;         // External sidechain
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
                void                        sync_latency();

            public:
                explicit limiter(const meta::plugin_t *metadata, bool sc, bool stereo);
                virtual ~limiter();

            public:
                virtual void        init(plug::IWrapper *wrapper, plug::IPort **ports);
                virtual void        destroy();

                virtual void        update_settings();
                virtual void        update_sample_rate(long sr);
                virtual void        ui_activated();

                virtual void        process(size_t samples);
                virtual bool        inline_display(plug::ICanvas *cv, size_t width, size_t height);

                virtual void        dump(dspu::IStateDumper *v) const;
        };
    } // namespace plugins
} // namespace lsp

#endif /* PRIVATE_PLUGINS_LIMITER_H_ */
