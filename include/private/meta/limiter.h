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

#ifndef PRIVATE_META_LIMITER_H_
#define PRIVATE_META_LIMITER_H_

#include <lsp-plug.in/plug-fw/meta/types.h>
#include <lsp-plug.in/plug-fw/const.h>


namespace lsp
{
    namespace meta
    {
        struct limiter_metadata
        {
            static constexpr float  HISTORY_TIME            = 4.0f;     // Amount of time to display history [s]
            static constexpr size_t HISTORY_MESH_SIZE       = 560;      // 420 dots for history
            static constexpr size_t OVERSAMPLING_MAX        = 8;        // Maximum 8x oversampling

            static constexpr float  LOOKAHEAD_MIN           = 0.1f;     // No lookahead [ms]
            static constexpr float  LOOKAHEAD_MAX           = 20.0f;    // Maximum Lookahead [ms]
            static constexpr float  LOOKAHEAD_DFL           = 5.0f;     // Default Lookahead [ms]
            static constexpr float  LOOKAHEAD_STEP          = 0.005f;   // Lookahead step

            static constexpr float  ATTACK_TIME_MIN         = 0.25f;
            static constexpr float  ATTACK_TIME_MAX         = 20.0f;
            static constexpr float  ATTACK_TIME_DFL         = 5.0f;
            static constexpr float  ATTACK_TIME_STEP        = 0.0025f;

            static constexpr float  RELEASE_TIME_MIN        = 0.25f;
            static constexpr float  RELEASE_TIME_MAX        = 20.0f;
            static constexpr float  RELEASE_TIME_DFL        = 5.0f;
            static constexpr float  RELEASE_TIME_STEP       = 0.0025f;

            static constexpr float  ALR_ATTACK_TIME_MIN     = 0.1f;
            static constexpr float  ALR_ATTACK_TIME_MAX     = 200.0f;
            static constexpr float  ALR_ATTACK_TIME_DFL     = 5.0f;
            static constexpr float  ALR_ATTACK_TIME_STEP    = 0.0025f;

            static constexpr float  ALR_RELEASE_TIME_MIN    = 10.0f;
            static constexpr float  ALR_RELEASE_TIME_MAX    = 1000.0f;
            static constexpr float  ALR_RELEASE_TIME_DFL    = 50.0f;
            static constexpr float  ALR_RELEASE_TIME_STEP   = 0.0025f;

            static constexpr float  THRESHOLD_MIN           = GAIN_AMP_M_48_DB;
            static constexpr float  THRESHOLD_MAX           = GAIN_AMP_0_DB;
            static constexpr float  THRESHOLD_DFL           = GAIN_AMP_0_DB;
            static constexpr float  THRESHOLD_STEP          = 0.01f;

            static constexpr float  KNEE_MIN                = GAIN_AMP_M_12_DB;
            static constexpr float  KNEE_MAX                = GAIN_AMP_P_12_DB;
            static constexpr float  KNEE_DFL                = GAIN_AMP_0_DB;
            static constexpr float  KNEE_STEP               = 0.01f;

            static constexpr float  LINKING_MIN             = 0;
            static constexpr float  LINKING_MAX             = 100.0f;
            static constexpr float  LINKING_DFL             = 100.0f;
            static constexpr float  LINKING_STEP            = 0.01f;

            enum oversampling_mode_t
            {
                OVS_NONE,

                OVS_HALF_2X2,
                OVS_HALF_2X3,
                OVS_HALF_3X2,
                OVS_HALF_3X3,
                OVS_HALF_4X2,
                OVS_HALF_4X3,
                OVS_HALF_6X2,
                OVS_HALF_6X3,
                OVS_HALF_8X2,
                OVS_HALF_8X3,

                OVS_FULL_2X2,
                OVS_FULL_2X3,
                OVS_FULL_3X2,
                OVS_FULL_3X3,
                OVS_FULL_4X2,
                OVS_FULL_4X3,
                OVS_FULL_6X2,
                OVS_FULL_6X3,
                OVS_FULL_8X2,
                OVS_FULL_8X3,

                OVS_DEFAULT     = OVS_NONE
            };

            enum limiter_mode_t
            {
                LOM_HERM_THIN,
                LOM_HERM_WIDE,
                LOM_HERM_TAIL,
                LOM_HERM_DUCK,

                LOM_EXP_THIN,
                LOM_EXP_WIDE,
                LOM_EXP_TAIL,
                LOM_EXP_DUCK,

                LOM_LINE_THIN,
                LOM_LINE_WIDE,
                LOM_LINE_TAIL,
                LOM_LINE_DUCK,

                LOM_DEFAULT     = LOM_HERM_THIN
            };

            enum dithering_t
            {
                DITHER_NONE,
                DITHER_7BIT,
                DITHER_8BIT,
                DITHER_11BIT,
                DITHER_12BIT,
                DITHER_15BIT,
                DITHER_16BIT,
                DITHER_23BIT,
                DITHER_24BIT,

                DITHER_DEFAULT  = DITHER_NONE
            };
        };

        extern const meta::plugin_t limiter_mono;
        extern const meta::plugin_t limiter_stereo;
        extern const meta::plugin_t sc_limiter_mono;
        extern const meta::plugin_t sc_limiter_stereo;
    } // namespace meta
} // namespace lsp


#endif /* PRIVATE_META_LIMITER_H_ */
