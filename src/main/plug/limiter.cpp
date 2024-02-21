/*
 * Copyright (C) 2023 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2023 Vladimir Sadovnikov <sadko4u@gmail.com>
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

#include <private/plugins/limiter.h>
#include <lsp-plug.in/common/alloc.h>
#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/dsp/dsp.h>
#include <lsp-plug.in/dsp-units/units.h>
#include <lsp-plug.in/stdlib/math.h>
#include <lsp-plug.in/shared/id_colors.h>

#define LIMIT_BUFSIZE           8192
#define LIMIT_BUFMULTIPLE       16

namespace lsp
{
    namespace plugins
    {
        static plug::IPort *TRACE_PORT(plug::IPort *p)
        {
            lsp_trace("  port id=%s", (p)->metadata()->id);
            return p;
        }

        //-------------------------------------------------------------------------
        // Plugin factory
        typedef struct plugin_settings_t
        {
            const meta::plugin_t   *metadata;
            bool                    sc;
            bool                    stereo;
        } plugin_settings_t;

        static const meta::plugin_t *plugins[] =
        {
            &meta::limiter_mono,
            &meta::limiter_stereo,
            &meta::sc_limiter_mono,
            &meta::sc_limiter_stereo
        };

        static const plugin_settings_t plugin_settings[] =
        {
            { &meta::limiter_mono,       false, false       },
            { &meta::limiter_stereo,     false, true        },
            { &meta::sc_limiter_mono,    true,  false       },
            { &meta::sc_limiter_stereo,  true,  true        },

            { NULL, 0, false }
        };

        static plug::Module *plugin_factory(const meta::plugin_t *meta)
        {
            for (const plugin_settings_t *s = plugin_settings; s->metadata != NULL; ++s)
                if (s->metadata == meta)
                    return new limiter(s->metadata, s->sc, s->stereo);
            return NULL;
        }

        static plug::Factory factory(plugin_factory, plugins, 4);

        //-------------------------------------------------------------------------
        limiter::limiter(const meta::plugin_t *metadata, bool sc, bool stereo): plug::Module(metadata)
        {
            nChannels       = (stereo) ? 2 : 1;
            bSidechain      = sc;
            vChannels       = NULL;
            vTime           = NULL;
            bPause          = false;
            bClear          = false;
            bExtSc          = false;
            bScListen       = false;
            fInGain         = GAIN_AMP_0_DB;
            fOutGain        = GAIN_AMP_0_DB;
            fPreamp         = GAIN_AMP_0_DB;
            fStereoLink     = 1.0f;
            pIDisplay       = NULL;
            bUISync         = true;

            pBypass         = NULL;
            pInGain         = NULL;
            pOutGain        = NULL;
            pPreamp         = NULL;

            pAlrOn          = NULL;
            pAlrAttack      = NULL;
            pAlrRelease     = NULL;
            pMode           = NULL;
            pThresh         = NULL;
            pLookahead      = NULL;
            pAttack         = NULL;
            pRelease        = NULL;
            pPause          = NULL;
            pClear          = NULL;
            pExtSc          = NULL;
            pScListen       = NULL;
            pKnee           = NULL;
            pBoost          = NULL;
            pOversampling   = NULL;
            pDithering      = NULL;
            pStereoLink     = NULL;

            pData           = NULL;
        }

        limiter::~limiter()
        {
            do_destroy();
        }

        void limiter::init(plug::IWrapper *wrapper, plug::IPort **ports)
        {
            plug::Module::init(wrapper, ports);

            // Allocate channels
            vChannels       = new channel_t[nChannels];
            if (vChannels == NULL)
                return;

            // Allocate temporary buffers
            size_t c_data   = LIMIT_BUFSIZE * sizeof(float);
            size_t h_data   = meta::limiter_metadata::HISTORY_MESH_SIZE * sizeof(float);
            size_t allocate = c_data * 4 * nChannels + h_data;

            uint8_t *ptr    = alloc_aligned<uint8_t>(pData, allocate, DEFAULT_ALIGN);
            if (ptr == NULL)
                return;

            vTime           = reinterpret_cast<float *>(ptr);
            ptr            += h_data;

            float lk_latency= int(dspu::samples_to_millis(MAX_SAMPLE_RATE, meta::limiter_metadata::OVERSAMPLING_MAX)) +
                              meta::limiter_metadata::LOOKAHEAD_MAX + 1.0f;

            // Initialize channels
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];

                // Initialize channel
                c->vIn          = NULL;
                c->vSc          = NULL;
                c->vOut         = NULL;

                c->vDataBuf     = reinterpret_cast<float *>(ptr);
                ptr            += c_data;
                c->vScBuf       = reinterpret_cast<float *>(ptr);
                ptr            += c_data;
                c->vGainBuf     = reinterpret_cast<float *>(ptr);
                ptr            += c_data;
                c->vOutBuf      = reinterpret_cast<float *>(ptr);
                ptr            += c_data;

                c->bOutVisible  = true;
                c->bGainVisible = true;
                c->bScVisible   = true;

                for (size_t j=0; j<G_TOTAL; ++j)
                    c->bVisible[j]  = true;
                for (size_t j=0; j<G_TOTAL; ++j)
                    c->pVisible[j]  = NULL;
                for (size_t j=0; j<G_TOTAL; ++j)
                    c->pGraph[j]    = NULL;
                for (size_t j=0; j<G_TOTAL; ++j)
                    c->pMeter[j]    = NULL;

                c->pIn          = NULL;
                c->pOut         = NULL;
                c->pSc          = NULL;

                // Initialize oversampler
                if (!c->sOver.init())
                    return;
                if (!c->sScOver.init())
                    return;

                // Initialize limiter with latency compensation gap
                if (!c->sLimit.init(MAX_SAMPLE_RATE * meta::limiter_metadata::OVERSAMPLING_MAX, lk_latency))
                    return;

                if (!c->sDataDelay.init(dspu::millis_to_samples(MAX_SAMPLE_RATE * meta::limiter_metadata::OVERSAMPLING_MAX, lk_latency) + LIMIT_BUFSIZE))
                    return;
                if (!c->sDryDelay.init(dspu::millis_to_samples(MAX_SAMPLE_RATE, lk_latency + c->sOver.max_latency())))
                    return;
            }

            lsp_assert(ptr <= &pData[allocate + DEFAULT_ALIGN]);

            size_t port_id = 0;

            // Bind audio ports
            lsp_trace("Binding audio ports");
            for (size_t i=0; i<nChannels; ++i)
                vChannels[i].pIn        = TRACE_PORT(ports[port_id++]);
            for (size_t i=0; i<nChannels; ++i)
                vChannels[i].pOut       = TRACE_PORT(ports[port_id++]);

            if (bSidechain)
            {
                for (size_t i=0; i<nChannels; ++i)
                    vChannels[i].pSc        = TRACE_PORT(ports[port_id++]);
            }

            // Bind common ports
            lsp_trace("Binding common ports");
            pBypass         = TRACE_PORT(ports[port_id++]);
            pInGain         = TRACE_PORT(ports[port_id++]);
            pOutGain        = TRACE_PORT(ports[port_id++]);
            pPreamp         = TRACE_PORT(ports[port_id++]);
            pAlrOn          = TRACE_PORT(ports[port_id++]);
            pAlrAttack      = TRACE_PORT(ports[port_id++]);
            pAlrRelease     = TRACE_PORT(ports[port_id++]);
            pMode           = TRACE_PORT(ports[port_id++]);
            pThresh         = TRACE_PORT(ports[port_id++]);
            pKnee           = TRACE_PORT(ports[port_id++]);
            pBoost          = TRACE_PORT(ports[port_id++]);
            pLookahead      = TRACE_PORT(ports[port_id++]);
            pAttack         = TRACE_PORT(ports[port_id++]);
            pRelease        = TRACE_PORT(ports[port_id++]);
            pOversampling   = TRACE_PORT(ports[port_id++]);
            pDithering      = TRACE_PORT(ports[port_id++]);
            pPause          = TRACE_PORT(ports[port_id++]);
            pClear          = TRACE_PORT(ports[port_id++]);

            if (nChannels > 1)
                pStereoLink     = TRACE_PORT(ports[port_id++]);

            if (bSidechain)
                pExtSc          = TRACE_PORT(ports[port_id++]);

            // Bind history ports for each channel
            lsp_trace("Binding history ports");
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];

                // Visibility ports
                for (size_t j=0; j<G_TOTAL; ++j)
                    c->pVisible[j]  = TRACE_PORT(ports[port_id++]);

                // Metering ports
                for (size_t j=0; j<G_TOTAL; ++j)
                    c->pMeter[j]    = TRACE_PORT(ports[port_id++]);

                // Graph ports
                for (size_t j=0; j<G_TOTAL; ++j)
                    c->pGraph[j]    = TRACE_PORT(ports[port_id++]);
            }

            float delta     = meta::limiter_metadata::HISTORY_TIME / (meta::limiter_metadata::HISTORY_MESH_SIZE - 1);
            for (size_t i=0; i<meta::limiter_metadata::HISTORY_MESH_SIZE; ++i)
                vTime[i]    = meta::limiter_metadata::HISTORY_TIME - i*delta;

            // Initialize dither
            sDither.init();
        }

        void limiter::destroy()
        {
            plug::Module::destroy();
            do_destroy();
        }

        void limiter::do_destroy()
        {
            if (pData != NULL)
            {
                free_aligned(pData);
                pData = NULL;
            }
            if (vChannels != NULL)
            {
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c    = &vChannels[i];
                    c->sLimit.destroy();
                    c->sOver.destroy();
                    c->sScOver.destroy();
                }

                delete [] vChannels;
                vChannels       = NULL;
            }
            if (pIDisplay != NULL)
            {
                pIDisplay->destroy();
                pIDisplay = NULL;
            }
        }

        void limiter::update_sample_rate(long sr)
        {
            size_t max_sample_rate      = sr * meta::limiter_metadata::OVERSAMPLING_MAX;
            size_t real_sample_rate     = vChannels[0].sOver.get_oversampling() * sr;
            float scaling_factor        = meta::limiter_metadata::HISTORY_TIME / meta::limiter_metadata::HISTORY_MESH_SIZE;

            size_t max_samples_per_dot  = dspu::seconds_to_samples(max_sample_rate, scaling_factor);
            size_t real_samples_per_dot = dspu::seconds_to_samples(real_sample_rate, scaling_factor);

            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c = &vChannels[i];

                c->sBypass.init(sr);
                c->sOver.set_sample_rate(sr);
                c->sScOver.set_sample_rate(sr);
                c->sLimit.set_mode(dspu::LM_HERM_THIN);
                c->sLimit.set_sample_rate(real_sample_rate);
                c->sBlink.init(sr);
                c->sBlink.set_default_off(1.0f);

                for (size_t j=0; j<G_TOTAL; ++j)
                {
                    c->sGraph[j].init(meta::limiter_metadata::HISTORY_MESH_SIZE, max_samples_per_dot);
                    c->sGraph[j].set_period(real_samples_per_dot);
                }

                c->sGraph[G_GAIN].fill(GAIN_AMP_0_DB);
                c->sGraph[G_GAIN].set_method(dspu::MM_MINIMUM);
            }
        }

        dspu::over_mode_t limiter::get_oversampling_mode(size_t mode)
        {
            #define L_KEY(x) \
                case meta::limiter_metadata::OVS_HALF_ ## x: \
                case meta::limiter_metadata::OVS_FULL_ ## x: \
                    return dspu::OM_LANCZOS_ ## x;

            switch (mode)
            {
                L_KEY(2X16BIT)
                L_KEY(2X24BIT)
                L_KEY(3X16BIT)
                L_KEY(3X24BIT)
                L_KEY(4X16BIT)
                L_KEY(4X24BIT)
                L_KEY(6X16BIT)
                L_KEY(6X24BIT)
                L_KEY(8X16BIT)
                L_KEY(8X24BIT)

                case meta::limiter_metadata::OVS_NONE:
                default:
                    return dspu::OM_NONE;
            }
            #undef L_KEY
            return dspu::OM_NONE;
        }

        bool limiter::get_filtering(size_t mode)
        {
            return (mode >= meta::limiter_metadata::OVS_FULL_2X16BIT) && (mode <= meta::limiter_metadata::OVS_FULL_8X24BIT);
        }

        dspu::limiter_mode_t limiter::get_limiter_mode(size_t mode)
        {
            switch (mode)
            {
                case meta::limiter_metadata::LOM_HERM_THIN:
                    return dspu::LM_HERM_THIN;
                case meta::limiter_metadata::LOM_HERM_WIDE:
                    return dspu::LM_HERM_WIDE;
                case meta::limiter_metadata::LOM_HERM_TAIL:
                    return dspu::LM_HERM_TAIL;
                case meta::limiter_metadata::LOM_HERM_DUCK:
                    return dspu::LM_HERM_DUCK;

                case meta::limiter_metadata::LOM_EXP_THIN:
                    return dspu::LM_EXP_THIN;
                case meta::limiter_metadata::LOM_EXP_WIDE:
                    return dspu::LM_EXP_WIDE;
                case meta::limiter_metadata::LOM_EXP_TAIL:
                    return dspu::LM_EXP_TAIL;
                case meta::limiter_metadata::LOM_EXP_DUCK:
                    return dspu::LM_EXP_DUCK;

                case meta::limiter_metadata::LOM_LINE_THIN:
                    return dspu::LM_LINE_THIN;
                case meta::limiter_metadata::LOM_LINE_WIDE:
                    return dspu::LM_LINE_WIDE;
                case meta::limiter_metadata::LOM_LINE_TAIL:
                    return dspu::LM_LINE_TAIL;
                case meta::limiter_metadata::LOM_LINE_DUCK:
                    return dspu::LM_LINE_DUCK;

                default:
                    break;
            }
            return dspu::LM_HERM_THIN;
        }

        size_t limiter::get_dithering(size_t mode)
        {
            switch (mode)
            {
                case meta::limiter_metadata::DITHER_7BIT:
                    return 7;
                case meta::limiter_metadata::DITHER_8BIT:
                    return 8;
                case meta::limiter_metadata::DITHER_11BIT:
                    return 11;
                case meta::limiter_metadata::DITHER_12BIT:
                    return 12;
                case meta::limiter_metadata::DITHER_15BIT:
                    return 15;
                case meta::limiter_metadata::DITHER_16BIT:
                    return 16;
                case meta::limiter_metadata::DITHER_23BIT:
                    return 23;
                case meta::limiter_metadata::DITHER_24BIT:
                    return 24;

                case meta::limiter_metadata::DITHER_NONE:
                default:
                    return 0;
            }
            return 0;
        }

        void limiter::sync_latency()
        {
            channel_t *c = &vChannels[0];
            size_t latency =
                    c->sLimit.get_latency() / c->sScOver.get_oversampling()
                    + c->sScOver.latency();

            for (size_t i=0; i<nChannels; ++i)
                vChannels[i].sDryDelay.set_delay(latency);
            set_latency(latency);
        }

        void limiter::update_settings()
        {
            bPause                      = pPause->value() >= 0.5f;
            bClear                      = pClear->value() >= 0.5f;

            size_t ovs_mode             = pOversampling->value();
            dspu::over_mode_t mode      = get_oversampling_mode(ovs_mode);
            bool filtering              = get_filtering(ovs_mode);
            size_t dither               = get_dithering(pDithering->value());
            float scaling_factor        = meta::limiter_metadata::HISTORY_TIME / meta::limiter_metadata::HISTORY_MESH_SIZE;

            bool bypass                 = pBypass->value() >= 0.5f;
            float thresh                = pThresh->value();
            float lk_ahead              = pLookahead->value();
            float attack                = pAttack->value();
            float release               = pRelease->value();
            float knee                  = pKnee->value();
            bool alr_on                 = pAlrOn->value() >= 0.5f;
            float alr_attack            = pAlrAttack->value();
            float alr_release           = pAlrRelease->value();
            fStereoLink                 = (pStereoLink != NULL) ? pStereoLink->value()*0.01f : 1.0f;
            bExtSc                      = (pExtSc != NULL) ? pExtSc->value() >= 0.5f : false;

            bool boost                  = pBoost->value();
            fOutGain                    = pOutGain->value();
            if (boost)
                fOutGain                   /= thresh;

            fInGain                     = pInGain->value();

            fPreamp                     = pPreamp->value();
            dspu::limiter_mode_t op_mode= get_limiter_mode(pMode->value());

            sDither.set_bits(dither);

            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c = &vChannels[i];

                // Update settings for each channel
                c->sBypass.set_bypass(bypass);
                c->sOver.set_mode(mode);
                c->sOver.set_filtering(filtering);
                if (c->sOver.modified())
                    c->sOver.update_settings();

                c->sScOver.set_mode(mode);
                c->sScOver.set_filtering(false);
                if (c->sScOver.modified())
                    c->sScOver.update_settings();

                size_t real_sample_rate     = c->sOver.get_oversampling() * fSampleRate;
                size_t real_samples_per_dot = dspu::seconds_to_samples(real_sample_rate, scaling_factor);

                // Update lookahead because oversampling adds extra latency
                float lk_ahead_ch = lk_ahead + dspu::samples_to_millis(fSampleRate, c->sScOver.latency());

                // Cleanup the data delay if limiter's sample rate is going to chane
                if (c->sLimit.sample_rate() != real_sample_rate)
                    c->sDataDelay.clear();

                // Update settings for limiter
                c->sLimit.set_mode(op_mode);
                c->sLimit.set_sample_rate(real_sample_rate);
                c->sLimit.set_lookahead(lk_ahead_ch);
                c->sLimit.set_threshold(thresh, !boost);
                c->sLimit.set_attack(attack);
                c->sLimit.set_release(release);
                c->sLimit.set_knee(knee);
                c->sLimit.set_alr(alr_on);
                c->sLimit.set_alr_attack(alr_attack);
                c->sLimit.set_alr_release(alr_release);
                c->sLimit.update_settings();

                // Update the data delay
                c->sDataDelay.set_delay(c->sLimit.get_latency());

                // Update meters
                for (size_t j=0; j<G_TOTAL; ++j)
                {
                    c->sGraph[j].set_period(real_samples_per_dot);
                    c->bVisible[j]  = c->pVisible[j]->value() >= 0.5f;
                }
            }

            // Report latency
            sync_latency();
        }

        void limiter::process(size_t samples)
        {
            // Bind audio ports
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];
                c->vIn          = c->pIn->buffer<float>();
                c->vOut         = c->pOut->buffer<float>();
                c->vSc          = ((c->pSc != NULL) && (bExtSc)) ? c->pSc->buffer<float>() : NULL;
            }

            // Get oversampling times
            size_t times        = vChannels[0].sOver.get_oversampling();
            size_t buf_size     = (LIMIT_BUFSIZE / times) & (~(LIMIT_BUFMULTIPLE-1));

            // Process samples
            for (size_t nsamples = samples; nsamples > 0; )
            {
                // Perform oversampling of signal and sidechain
                size_t to_do    = lsp_min(buf_size, nsamples);
                size_t to_doxn  = to_do * times;

                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c    = &vChannels[i];

                    // Apply input gain if needed
                    if (fInGain != GAIN_AMP_0_DB)
                    {
                        dsp::mul_k3(c->vOutBuf, c->vIn, fInGain, to_do);
                        c->sOver.upsample(c->vDataBuf, c->vOutBuf, to_do);
                    }
                    else
                        c->sOver.upsample(c->vDataBuf, c->vIn, to_do);

                    // Process sidechain signal
                    if (c->vSc != NULL)
                    {
                        if (fPreamp != GAIN_AMP_0_DB)
                        {
                            dsp::mul_k3(c->vOutBuf, c->vSc, fPreamp, to_do);
                            c->sScOver.upsample(c->vScBuf, c->vOutBuf, to_do);
                        }
                        else
                            c->sScOver.upsample(c->vScBuf, c->vSc, to_do);
                    }
                    else
                    {
                        if (fPreamp != GAIN_AMP_0_DB)
                            dsp::mul_k3(c->vScBuf, c->vDataBuf, fPreamp, to_doxn);
                        else
                            dsp::copy(c->vScBuf, c->vDataBuf, to_doxn);
                    }

                    // Update graphs
                    c->sGraph[G_IN].process(c->vDataBuf, to_doxn);
                    c->sGraph[G_SC].process(c->vScBuf, to_doxn);
                    c->pMeter[G_IN]->set_value(dsp::max(c->vDataBuf, to_doxn));
                    c->pMeter[G_SC]->set_value(dsp::max(c->vScBuf, to_doxn));

                    // Perform processing by limiter
                    c->sLimit.process(c->vGainBuf, c->vScBuf, to_doxn);
                    c->sDataDelay.process(c->vDataBuf, c->vDataBuf, to_doxn);
                }

                // Perform stereo linking
                if (nChannels == 2)
                {
                    float *cl = vChannels[0].vGainBuf;
                    float *cr = vChannels[1].vGainBuf;

                    for (size_t i=0; i<to_doxn; ++i)
                    {
                        float gl = cl[i];
                        float gr = cr[i];

                        if (gl < gr)
                            cr[i] = gr + (gl - gr) * fStereoLink;
                        else
                            cl[i] = gl + (gr - gl) * fStereoLink;
                    }
                }

                // Perform downsampling and post-processing of signal and sidechain
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c    = &vChannels[i];

                    // Update output signal: adjust gain
                    dsp::fmmul_k3(c->vDataBuf, c->vGainBuf, fOutGain, to_doxn);

                    // Do metering
                    c->sGraph[G_OUT].process(c->vDataBuf, to_doxn);
                    c->pMeter[G_OUT]->set_value(dsp::max(c->vDataBuf, to_doxn));

                    c->sGraph[G_GAIN].process(c->vGainBuf, to_doxn);
                    float gain = dsp::min(c->vGainBuf, to_doxn);
                    if (gain < 1.0f)
                        c->sBlink.blink_min(gain);

                    // Do Downsampling and bypassing
                    c->sOver.downsample(c->vOutBuf, c->vDataBuf, to_do);            // Downsample
                    sDither.process(c->vOutBuf, c->vOutBuf, to_do);                 // Apply dithering
                    c->sDryDelay.process(c->vDataBuf, c->vIn, to_do);               // Apply dry delay
                    c->sBypass.process(c->vOut, c->vDataBuf, c->vOutBuf, to_do);    // Pass thru bypass

                    // Update pointers
                    c->vIn         += to_do;
                    c->vOut        += to_do;
                    if (c->vSc != NULL)
                        c->vSc         += to_do;
                }

                // Decrement number of samples for processing
                nsamples   -= to_do;
            }

            // Report gain reduction
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];
                c->pMeter[G_GAIN]->set_value(c->sBlink.process(samples));
            }

            // Output history
            if ((!bPause) || (bClear) || (bUISync))
            {
                // Process mesh requests
                for (size_t i=0; i<nChannels; ++i)
                {
                    // Get channel
                    channel_t *c        = &vChannels[i];

                    for (size_t j=0; j<G_TOTAL; ++j)
                    {
                        // Check that port is bound
                        if (c->pGraph[j] == NULL)
                            continue;

                        // Clear data if requested
                        if (bClear)
                            dsp::fill_zero(c->sGraph[j].data(), meta::limiter_metadata::HISTORY_MESH_SIZE);

                        // Get mesh
                        plug::mesh_t *mesh    = c->pGraph[j]->buffer<plug::mesh_t>();
                        if ((mesh != NULL) && (mesh->isEmpty()))
                        {
                            // Fill mesh with new values
                            if ((j == G_IN) || (j == G_GAIN))
                            {
                                float *x = mesh->pvData[0];
                                float *y = mesh->pvData[1];
                                float y_zero = (j == G_IN) ? 0.0f : 1.0f;

                                dsp::copy(&x[1], vTime, meta::limiter_metadata::HISTORY_MESH_SIZE);
                                dsp::copy(&y[1], c->sGraph[j].data(), meta::limiter_metadata::HISTORY_MESH_SIZE);

                                x[0] = x[1];
                                y[0] = y_zero;

                                x[meta::limiter_metadata::HISTORY_MESH_SIZE+1] = x[meta::limiter_metadata::HISTORY_MESH_SIZE];
                                y[meta::limiter_metadata::HISTORY_MESH_SIZE+1] = y_zero;

                                mesh->data(2, meta::limiter_metadata::HISTORY_MESH_SIZE+2);
                            }
                            else
                            {
                                dsp::copy(mesh->pvData[0], vTime, meta::limiter_metadata::HISTORY_MESH_SIZE);
                                dsp::copy(mesh->pvData[1], c->sGraph[j].data(), meta::limiter_metadata::HISTORY_MESH_SIZE);
                                mesh->data(2, meta::limiter_metadata::HISTORY_MESH_SIZE);
                            }
                        }
                    } // for j
                }

                // Clear sync flag
                bUISync = false;
            }

            // Request for redraw
            if (pWrapper != NULL)
                pWrapper->query_display_draw();
        }

        void limiter::ui_activated()
        {
            bUISync = true;
        }

        bool limiter::inline_display(plug::ICanvas *cv, size_t width, size_t height)
        {
            // Check proportions
            if (height > (M_RGOLD_RATIO * width))
                height  = M_RGOLD_RATIO * width;

            // Init canvas
            if (!cv->init(width, height))
                return false;
            width   = cv->width();
            height  = cv->height();

            // Clear background
            bool bypassing = vChannels[0].sBypass.bypassing();
            cv->set_color_rgb((bypassing) ? CV_DISABLED : CV_BACKGROUND);
            cv->paint();

            // Calc axis params
            float zy    = 1.0f/GAIN_AMP_M_48_DB;
            float dx    = -float(width/meta::limiter_metadata::HISTORY_TIME);
            float dy    = height/(logf(GAIN_AMP_M_48_DB)-logf(GAIN_AMP_0_DB));

            // Draw axis
            cv->set_line_width(1.0);

            // Draw vertical lines
            cv->set_color_rgb(CV_YELLOW, 0.5f);
            for (float i=1.0; i < (meta::limiter_metadata::HISTORY_TIME-0.1); i += 1.0f)
            {
                float ax = width + dx*i;
                cv->line(ax, 0, ax, height);
            }

            // Draw horizontal lines
            cv->set_color_rgb(CV_WHITE, 0.5f);
            for (float i=GAIN_AMP_M_48_DB; i<GAIN_AMP_0_DB; i *= GAIN_AMP_P_24_DB)
            {
                float ay = height + dy*(logf(i*zy));
                cv->line(0, ay, width, ay);
            }

            // Allocate buffer: t, f(t), x, y
            pIDisplay           = core::IDBuffer::reuse(pIDisplay, 4, width);
            core::IDBuffer *b   = pIDisplay;
            if (b == NULL)
                return false;

            static uint32_t c_colors[] = {
                    CV_MIDDLE_CHANNEL_IN, CV_MIDDLE_CHANNEL, CV_BRIGHT_GREEN, CV_BRIGHT_BLUE,
                    CV_LEFT_CHANNEL_IN, CV_LEFT_CHANNEL, CV_BRIGHT_GREEN, CV_BRIGHT_BLUE,
                    CV_RIGHT_CHANNEL_IN, CV_RIGHT_CHANNEL, CV_BRIGHT_GREEN, CV_BRIGHT_BLUE
                   };
            uint32_t *cols      = (nChannels > 1) ? &c_colors[G_TOTAL] : c_colors;
            float r             = meta::limiter_metadata::HISTORY_MESH_SIZE/float(width);

            for (size_t j=0; j<width; ++j)
            {
                size_t k        = r*j;
                b->v[0][j]      = vTime[k];
            }

            cv->set_line_width(2.0f);
            for (size_t j=0; j<G_TOTAL; ++j)
            {
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c    = &vChannels[i];
                    if (!c->bVisible[j])
                        continue;

                    // Initialize values
                    float *ft       = c->sGraph[j].data();
                    for (size_t k=0; k<width; ++k)
                        b->v[1][k]      = ft[size_t(r*k)];

                    // Initialize coords
                    dsp::fill(b->v[2], width, width);
                    dsp::fill(b->v[3], height, width);
                    dsp::fmadd_k3(b->v[2], b->v[0], dx, width);
                    dsp::axis_apply_log1(b->v[3], b->v[1], zy, dy, width);

                    // Draw channel
                    cv->set_color_rgb((bypassing) ? CV_SILVER : cols[j + i*G_TOTAL]);
                    cv->draw_lines(b->v[2], b->v[3], width);
                }
            }

            // Draw threshold
            cv->set_color_rgb(CV_MAGENTA, 0.5f);
            cv->set_line_width(1.0);
            {
                float ay = height + dy*(logf(vChannels[0].sLimit.get_threshold()*zy));
                cv->line(0, ay, width, ay);
            }

            return true;
        }

        void limiter::dump(dspu::IStateDumper *v) const
        {
            plug::Module::dump(v);

            v->write("nChannels", nChannels);
            v->write("bSidechain", bSidechain);
            v->begin_array("vChannels", vChannels, nChannels);
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];
                v->begin_object(c, sizeof(channel_t));
                {
                    v->write_object("sBypass", &c->sBypass);
                    v->write_object("sOver", &c->sOver);
                    v->write_object("sScOver", &c->sScOver);
                    v->write_object("sLimit", &c->sLimit);
                    v->write_object("sDataDelay", &c->sDataDelay);
                    v->write_object("sDryDelay", &c->sDryDelay);

                    v->begin_array("sGraph", c->sGraph, G_TOTAL);
                    for (size_t j=0; j<G_TOTAL; ++j)
                        v->write_object(&c->sGraph[j]);
                    v->end_array();

                    v->write_object("sBlink", &c->sBlink);

                    v->write("vIn", c->vIn);
                    v->write("vSc", c->vSc);
                    v->write("vOut", c->vOut);

                    v->write("vDataBuf", c->vDataBuf);
                    v->write("vScBuf", c->vScBuf);
                    v->write("vGainBuf", c->vGainBuf);
                    v->write("vOutBuf", c->vOutBuf);

                    v->writev("bVisible", c->bVisible, G_TOTAL);
                    v->write("bOutVisible", c->bOutVisible);
                    v->write("bGainVisible", c->bGainVisible);
                    v->write("bScVisible", c->bScVisible);

                    v->write("pIn", c->pIn);
                    v->write("pOut", c->pOut);
                    v->write("pSc", c->pSc);
                    v->writev("pVisible", c->pVisible, G_TOTAL);

                    v->writev("pGraph", c->pGraph, G_TOTAL);
                    v->writev("pMeter", c->pMeter, G_TOTAL);
                }
                v->end_object();
            }
            v->end_array();

            v->write("vTime", vTime);
            v->write("bPause", bPause);
            v->write("bClear", bClear);
            v->write("bExtSc", bExtSc);
            v->write("bScListen", bScListen);
            v->write("fInGain", fInGain);
            v->write("fOutGain", fOutGain);
            v->write("fPreamp", fPreamp);
            v->write("fStereoLink", fStereoLink);
            v->write("pIDisplay", pIDisplay);
            v->write("bUISync", bUISync);

            v->write_object("sDither", &sDither);

            v->write("pBypass", pBypass);
            v->write("pInGain", pInGain);
            v->write("pOutGain", pOutGain);
            v->write("pPreamp", pPreamp);
            v->write("pAlrOn", pAlrOn);
            v->write("pAlrAttack", pAlrAttack);
            v->write("pAlrRelease", pAlrRelease);
            v->write("pMode", pMode);
            v->write("pThresh", pThresh);
            v->write("pLookahead", pLookahead);
            v->write("pAttack", pAttack);
            v->write("pRelease", pRelease);
            v->write("pPause", pPause);
            v->write("pClear", pClear);
            v->write("pExtSc", pExtSc);
            v->write("pScListen", pScListen);
            v->write("pKnee", pKnee);
            v->write("pBoost", pBoost);
            v->write("pOversampling", pOversampling);
            v->write("pDithering", pDithering);
            v->write("pStereoLink", pStereoLink);
            v->write("pData", pData);
        }
    } /* namespace plugins */
} /* namespace lsp */


