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

#include <lsp-plug.in/plug-fw/meta/ports.h>
#include <lsp-plug.in/shared/meta/developers.h>
#include <private/meta/limiter.h>

#define LSP_PLUGINS_LIMITER_VERSION_MAJOR       1
#define LSP_PLUGINS_LIMITER_VERSION_MINOR       0
#define LSP_PLUGINS_LIMITER_VERSION_MICRO       11

#define LSP_PLUGINS_LIMITER_VERSION  \
    LSP_MODULE_VERSION( \
        LSP_PLUGINS_LIMITER_VERSION_MAJOR, \
        LSP_PLUGINS_LIMITER_VERSION_MINOR, \
        LSP_PLUGINS_LIMITER_VERSION_MICRO  \
    )

namespace lsp
{
    namespace meta
    {
        //-------------------------------------------------------------------------
        // Limiter
        static const int plugin_classes[]           = { C_LIMITER, -1 };
        static const int clap_features_mono[]       = { CF_AUDIO_EFFECT, CF_LIMITER, CF_MONO, -1 };
        static const int clap_features_stereo[]     = { CF_AUDIO_EFFECT, CF_LIMITER, CF_STEREO, -1 };

        static port_item_t limiter_oper_modes[] =
        {
            { "Herm Thin",      "limiter.herm_thin"     },
            { "Herm Wide",      "limiter.herm_wide"     },
            { "Herm Tail",      "limiter.herm_tail"     },
            { "Herm Duck",      "limiter.herm_duck"     },

            { "Exp Thin",       "limiter.exp_thin"      },
            { "Exp Wide",       "limiter.exp_wide"      },
            { "Exp Tail",       "limiter.exp_tail"      },
            { "Exp Duck",       "limiter.exp_duck"      },

            { "Line Thin",      "limiter.line_thin"     },
            { "Line Wide",      "limiter.line_wide"     },
            { "Line Tail",      "limiter.line_tail"     },
            { "Line Duck",      "limiter.line_duck"     },

            { NULL, NULL }
        };

        static port_item_t limiter_ovs_modes[] =
        {
            { "None",           "oversampler.none"      },

            { "Half x2(2L)",    "oversampler.half.2x2"  },
            { "Half x2(3L)",    "oversampler.half.2x3"  },
            { "Half x3(2L)",    "oversampler.half.3x2"  },
            { "Half x3(3L)",    "oversampler.half.3x3"  },
            { "Half x4(2L)",    "oversampler.half.4x2"  },
            { "Half x4(3L)",    "oversampler.half.4x3"  },
            { "Half x6(2L)",    "oversampler.half.6x2"  },
            { "Half x6(3L)",    "oversampler.half.6x3"  },
            { "Half x8(2L)",    "oversampler.half.8x2"  },
            { "Half x8(3L)",    "oversampler.half.8x3"  },

            { "Full x2(2L)",    "oversampler.full.2x2"  },
            { "Full x2(3L)",    "oversampler.full.2x3"  },
            { "Full x3(2L)",    "oversampler.full.3x2"  },
            { "Full x3(3L)",    "oversampler.full.3x3"  },
            { "Full x4(2L)",    "oversampler.full.4x2"  },
            { "Full x4(3L)",    "oversampler.full.4x3"  },
            { "Full x6(2L)",    "oversampler.full.6x2"  },
            { "Full x6(3L)",    "oversampler.full.6x3"  },
            { "Full x8(2L)",    "oversampler.full.8x2"  },
            { "Full x8(3L)",    "oversampler.full.8x3"  },

            { NULL, NULL }
        };

        static port_item_t limiter_dither_modes[] =
        {
            { "None",           "dither.none"           },
            { "7bit",           "dither.bits.7"         },
            { "8bit",           "dither.bits.8"         },
            { "11bit",          "dither.bits.11"        },
            { "12bit",          "dither.bits.12"        },
            { "15bit",          "dither.bits.15"        },
            { "16bit",          "dither.bits.16"        },
            { "23bit",          "dither.bits.23"        },
            { "24bit",          "dither.bits.24"        },
            { NULL, NULL }
        };

        #define LIMIT_COMMON    \
            BYPASS,             \
            IN_GAIN,            \
            OUT_GAIN,           \
            AMP_GAIN100("scp", "Sidechain preamp", GAIN_AMP_0_DB), \
            SWITCH("alr", "Automatic level regulation", 1.0f), \
            LOG_CONTROL("alr_at", "Automatic level regulation attack time", U_MSEC, limiter_metadata::ALR_ATTACK_TIME), \
            LOG_CONTROL("alr_rt", "Automatic level regulation release time", U_MSEC, limiter_metadata::ALR_RELEASE_TIME), \
            COMBO("mode", "Operating mode", limiter_metadata::LOM_DEFAULT, limiter_oper_modes), \
            LOG_CONTROL("th", "Threshold", U_GAIN_AMP, limiter_metadata::THRESHOLD), \
            LOG_CONTROL("knee", "Knee", U_GAIN_AMP, limiter_metadata::KNEE), \
            SWITCH("boost", "Gain boost", 1.0f), \
            LOG_CONTROL("lk", "Lookahead", U_MSEC, limiter_metadata::LOOKAHEAD), \
            LOG_CONTROL("at", "Attack time", U_MSEC, limiter_metadata::ATTACK_TIME), \
            LOG_CONTROL("rt", "Release time", U_MSEC, limiter_metadata::RELEASE_TIME), \
            COMBO("ovs", "Oversampling", limiter_metadata::OVS_DEFAULT, limiter_ovs_modes),           \
            COMBO("dith", "Dithering", limiter_metadata::DITHER_DEFAULT, limiter_dither_modes),           \
            SWITCH("pause", "Pause graph analysis", 0.0f), \
            TRIGGER("clear", "Clear graph analysis")

        #define LIMIT_COMMON_SC \
            SWITCH("extsc", "External sidechain", 0.0f)

        #define LIMIT_COMMON_MONO       \
            LIMIT_COMMON
        #define LIMIT_COMMON_STEREO     \
            LIMIT_COMMON_MONO, \
            LOG_CONTROL("slink", "Stereo linking", U_PERCENT, limiter_metadata::LINKING)

        #define LIMIT_COMMON_SC_MONO    \
            LIMIT_COMMON, \
            LIMIT_COMMON_SC

        #define LIMIT_COMMON_SC_STEREO  \
            LIMIT_COMMON_STEREO, \
            LIMIT_COMMON_SC

        #define LIMIT_METERS(id, label) \
            SWITCH("igv" id, "Input graph visibility" label, 1.0f), \
            SWITCH("ogv" id, "Output graph visibility" label, 1.0f), \
            SWITCH("scgv" id, "Sidechain graph visibility" label, 1.0f), \
            SWITCH("grgv" id, "Gain graph visibility" label, 1.0f), \
            METER_OUT_GAIN("ilm" id, "Input level meter" label, GAIN_AMP_0_DB), \
            METER_OUT_GAIN("olm" id, "Output level meter" label, GAIN_AMP_0_DB), \
            METER_OUT_GAIN("sclm" id, "Sidechain level meter" label, GAIN_AMP_0_DB), \
            METER_GAIN_DFL("grlm" id, "Gain reduction level meter" label, GAIN_AMP_0_DB, GAIN_AMP_0_DB), \
            MESH("ig" id, "Input graph" label, 2, limiter_metadata::HISTORY_MESH_SIZE), \
            MESH("og" id, "Output graph" label, 2, limiter_metadata::HISTORY_MESH_SIZE), \
            MESH("scg" id, "Sidechain graph" label, 2, limiter_metadata::HISTORY_MESH_SIZE), \
            MESH("grg" id, "Gain graph" label, 2, limiter_metadata::HISTORY_MESH_SIZE)

        #define LIMIT_METERS_MONO       LIMIT_METERS("", "")
        #define LIMIT_METERS_STEREO     LIMIT_METERS("_l", " Left"), LIMIT_METERS("_r", " Right")

        static const port_t limiter_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            LIMIT_COMMON_MONO,
            LIMIT_METERS_MONO,

            PORTS_END
        };

        static const port_t limiter_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            LIMIT_COMMON_STEREO,
            LIMIT_METERS_STEREO,

            PORTS_END
        };

        static const port_t sc_limiter_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            PORTS_MONO_SIDECHAIN,
            LIMIT_COMMON_SC_MONO,
            LIMIT_METERS_MONO,

            PORTS_END
        };

        static const port_t sc_limiter_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            PORTS_STEREO_SIDECHAIN,
            LIMIT_COMMON_SC_STEREO,
            LIMIT_METERS_STEREO,

            PORTS_END
        };

        const meta::bundle_t limiter_bundle =
        {
            "limiter",
            "Limiter",
            B_DYNAMICS,
            "laExcuCMDY4",
            "This plugin implements a limiter with flexible configuration. In most cases\nit acts as a brick-wall limiter but there are several settings for which is\nacts as an compressor with extreme settings, so the output signal may exceed\nthe limiter's threshold. It prevents input signal from raising over the\nspecified Threshold."
        };

        // Limiter
        const meta::plugin_t  limiter_mono =
        {
            "Begrenzer Mono",
            "Limiter Mono",
            "B1M",
            &developers::v_sadovnikov,
            "limiter_mono",
            LSP_LV2_URI("limiter_mono"),
            LSP_LV2UI_URI("limiter_mono"),
            "jz5z",
            LSP_LADSPA_LIMITER_BASE + 0,
            LSP_LADSPA_URI("limiter_mono"),
            LSP_CLAP_URI("limiter_mono"),
            LSP_PLUGINS_LIMITER_VERSION,
            plugin_classes,
            clap_features_mono,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            limiter_mono_ports,
            "dynamics/limiter/single/mono.xml",
            NULL,
            mono_plugin_port_groups,
            &limiter_bundle
        };

        const meta::plugin_t  limiter_stereo =
        {
            "Begrenzer Stereo",
            "Limiter Stereo",
            "B1S",
            &developers::v_sadovnikov,
            "limiter_stereo",
            LSP_LV2_URI("limiter_stereo"),
            LSP_LV2UI_URI("limiter_stereo"),
            "rfuc",
            LSP_LADSPA_LIMITER_BASE + 1,
            LSP_LADSPA_URI("limiter_stereo"),
            LSP_CLAP_URI("limiter_stereo"),
            LSP_PLUGINS_LIMITER_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            limiter_stereo_ports,
            "dynamics/limiter/single/stereo.xml",
            NULL,
            stereo_plugin_port_groups,
            &limiter_bundle
        };

        const meta::plugin_t  sc_limiter_mono =
        {
            "Sidechain-Begrenzer Mono",
            "Sidechain Limiter Mono",
            "SCB1M",
            &developers::v_sadovnikov,
            "sc_limiter_mono",
            LSP_LV2_URI("sc_limiter_mono"),
            LSP_LV2UI_URI("sc_limiter_mono"),
            "kyzu",
            LSP_LADSPA_LIMITER_BASE + 2,
            LSP_LADSPA_URI("sc_limiter_mono"),
            LSP_CLAP_URI("sc_limiter_mono"),
            LSP_PLUGINS_LIMITER_VERSION,
            plugin_classes,
            clap_features_mono,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            sc_limiter_mono_ports,
            "dynamics/limiter/single/mono.xml",
            NULL,
            mono_plugin_sidechain_port_groups,
            &limiter_bundle
        };

        const meta::plugin_t  sc_limiter_stereo =
        {
            "Sidechain-Begrenzer Stereo",
            "Sidechain Limiter Stereo",
            "SCB1S",
            &developers::v_sadovnikov,
            "sc_limiter_stereo",
            LSP_LV2_URI("sc_limiter_stereo"),
            LSP_LV2UI_URI("sc_limiter_stereo"),
            "zwf7",
            LSP_LADSPA_LIMITER_BASE + 3,
            LSP_LADSPA_URI("sc_limiter_stereo"),
            LSP_CLAP_URI("sc_limiter_stereo"),
            LSP_PLUGINS_LIMITER_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            sc_limiter_stereo_ports,
            "dynamics/limiter/single/stereo.xml",
            NULL,
            stereo_plugin_sidechain_port_groups,
            &limiter_bundle
        };
    } /* namespace meta */
} /* namespace lsp */
