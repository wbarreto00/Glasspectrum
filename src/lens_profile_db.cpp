/*
 * Glasspectrum — Lens Profile Database
 * 50 embedded lens presets with compile-time validation.
 */

#include "lens_profile_db.h"
#include <cassert>
#include <cstring>

namespace glasspectrum {

// ── Helper macro for concise profile definition ────────────────────

#define PROFILE_NO_ANA(NAME, EV, DIST_PCT, K1, K2, K3, P1, P2, CA, COMA_S,     \
                       COMA_A, COMA_H, COMA_F, VIG_S, VIG_P, CC_EO, CC_X,      \
                       CC_Y, BL_T, BL_G, BL_F, BL_TR, BL_TG, BL_TB, FR_G,      \
                       FR_H, FR_W, FR_R, BK_BC, BK_CU, BK_CE, BK_OR, BK_SW,    \
                       SH_MC, SH_WG, SH_MC2, SH_ME, BREATH)                    \
  {NAME,                                                                       \
   EV,                                                                         \
   DIST_PCT,                                                                   \
   {K1, K2, K3, P1, P2},                                                       \
   CA,                                                                         \
   {COMA_S, COMA_A, COMA_H, COMA_F},                                           \
   {VIG_S, VIG_P},                                                             \
   {CC_EO, {CC_X, CC_Y}},                                                      \
   {BL_T, BL_G, BL_F, {BL_TR, BL_TG, BL_TB}},                                  \
   {FR_G, FR_H, FR_W, FR_R},                                                   \
   {BK_BC, BK_CU, BK_CE, BK_OR, BK_SW},                                        \
   {SH_MC, SH_WG, SH_MC2, SH_ME},                                              \
   BREATH,                                                                     \
   false,                                                                      \
   {0, 0, 0, 0}}

#define PROFILE_ANA(NAME, EV, DIST_PCT, K1, K2, K3, P1, P2, CA, COMA_S,        \
                    COMA_A, COMA_H, COMA_F, VIG_S, VIG_P, CC_EO, CC_X, CC_Y,   \
                    BL_T, BL_G, BL_F, BL_TR, BL_TG, BL_TB, FR_G, FR_H, FR_W,   \
                    FR_R, BK_BC, BK_CU, BK_CE, BK_OR, BK_SW, SH_MC, SH_WG,     \
                    SH_MC2, SH_ME, BREATH, ANA_SQ, ANA_OV, ANA_SF, ANA_BF)     \
  {                                                                            \
    NAME, EV, DIST_PCT, {K1, K2, K3, P1, P2}, CA,                              \
        {COMA_S, COMA_A, COMA_H, COMA_F}, {VIG_S, VIG_P},                      \
        {CC_EO, {CC_X, CC_Y}}, {BL_T, BL_G, BL_F, {BL_TR, BL_TG, BL_TB}},      \
        {FR_G, FR_H, FR_W, FR_R}, {BK_BC, BK_CU, BK_CE, BK_OR, BK_SW},         \
        {SH_MC, SH_WG, SH_MC2, SH_ME}, BREATH, true, {                         \
      ANA_SQ, ANA_OV, ANA_SF, ANA_BF                                           \
    }                                                                          \
  }

// ── The 50 lens presets ──────────────────────────────────────────

static const LensProfile s_profiles[] = {

    // 1. ARRI/Zeiss Master Prime
    PROFILE_NO_ANA("ARRI/Zeiss Master Prime", EVIDENCE_PUBLISHED, 0.3f, -0.002f,
                   0.0002f, 0.0f, 0.0f, 0.0f, 0.25f, 0.10f, 0.35f, 0.0f, 1.6f,
                   0.35f, 2.4f, false, 0.0f, 0.0f, 1.2f, 0.20f, 1.8f, 1.0f,
                   1.0f, 1.0f, 0.10f, 300.0f, 0.8f, 0.4f, 9, 0.85f, 0.20f,
                   0.05f, 0.05f, 0.85f, 0.05f, 0.90f, 0.85f, 1.0f),

    // 2. Zeiss Super Speed MkIII
    PROFILE_NO_ANA("Zeiss Super Speed MkIII", EVIDENCE_VENDOR_DESCRIBED, 0.9f,
                   -0.008f, 0.0014f, 0.0f, 0.0f, 0.0f, 0.60f, 0.24f, 0.60f,
                   10.0f, 1.2f, 0.75f, 2.1f, false, 0.001f, 0.0f, 0.95f, 0.42f,
                   2.1f, 1.0f, 1.0f, 1.0f, 0.22f, 305.0f, 1.3f, 0.6f, 7, 0.82f,
                   0.40f, 0.10f, 0.15f, 0.62f, 0.20f, 0.78f, 0.62f, 2.2f),

    // 3. Zeiss Standard Speed
    PROFILE_NO_ANA("Zeiss Standard Speed", EVIDENCE_VENDOR_DESCRIBED, 0.7f,
                   -0.006f, 0.0010f, 0.0f, 0.0f, 0.0f, 0.45f, 0.18f, 0.55f,
                   5.0f, 1.3f, 0.60f, 2.2f, false, 0.0005f, 0.0f, 1.0f, 0.34f,
                   2.0f, 1.0f, 1.0f, 1.0f, 0.18f, 300.0f, 1.1f, 0.55f, 7, 0.86f,
                   0.30f, 0.08f, 0.10f, 0.70f, 0.14f, 0.82f, 0.70f, 2.0f),

    // 4. ARRI/Zeiss Ultra Prime
    PROFILE_NO_ANA("ARRI/Zeiss Ultra Prime", EVIDENCE_VENDOR_DESCRIBED, 0.4f,
                   -0.003f, 0.0004f, 0.0f, 0.0f, 0.0f, 0.30f, 0.12f, 0.40f,
                   0.0f, 1.5f, 0.40f, 2.4f, false, 0.0f, 0.0f, 1.15f, 0.24f,
                   1.9f, 1.0f, 1.0f, 1.0f, 0.12f, 300.0f, 0.9f, 0.45f, 9, 0.88f,
                   0.22f, 0.05f, 0.06f, 0.82f, 0.08f, 0.88f, 0.84f, 1.2f),

    // 5. Cooke S4/i
    PROFILE_NO_ANA("Cooke S4/i", EVIDENCE_VENDOR_DESCRIBED, 0.5f, -0.004f,
                   0.0005f, 0.0f, 0.0f, 0.0f, 0.35f, 0.14f, 0.45f, 20.0f, 1.4f,
                   0.45f, 2.2f, false, 0.002f, 0.001f, 1.05f, 0.32f, 1.9f,
                   1.02f, 1.00f, 0.98f, 0.14f, 320.0f, 1.0f, 0.5f, 8, 0.90f,
                   0.25f, 0.08f, 0.06f, 0.78f, 0.10f, 0.86f, 0.80f, 1.5f),

    // 6. Cooke Speed Panchro
    PROFILE_NO_ANA("Cooke Speed Panchro", EVIDENCE_VENDOR_DESCRIBED, 1.0f,
                   -0.009f, 0.0018f, 0.0f, 0.0f, 0.0f, 0.65f, 0.26f, 0.65f,
                   18.0f, 1.1f, 0.90f, 1.9f, false, 0.002f, 0.0015f, 0.90f,
                   0.55f, 2.2f, 1.03f, 1.00f, 0.97f, 0.24f, 320.0f, 1.4f, 0.65f,
                   8, 0.92f, 0.45f, 0.12f, 0.18f, 0.55f, 0.28f, 0.72f, 0.55f,
                   3.2f),

    // 7. Cooke Panchro/i Classic
    PROFILE_NO_ANA("Cooke Panchro/i Classic", EVIDENCE_VENDOR_DESCRIBED, 0.9f,
                   -0.008f, 0.0016f, 0.0f, 0.0f, 0.0f, 0.55f, 0.22f, 0.62f,
                   15.0f, 1.2f, 0.80f, 2.0f, false, 0.0018f, 0.0012f, 0.92f,
                   0.50f, 2.1f, 1.03f, 1.00f, 0.98f, 0.22f, 320.0f, 1.3f, 0.62f,
                   8, 0.93f, 0.42f, 0.12f, 0.16f, 0.58f, 0.24f, 0.74f, 0.60f,
                   2.8f),

    // 8. Leitz Summilux-C
    PROFILE_NO_ANA("Leitz Summilux-C", EVIDENCE_VENDOR_DESCRIBED, 0.4f, -0.003f,
                   0.0004f, 0.0f, 0.0f, 0.0f, 0.28f, 0.12f, 0.42f, 8.0f, 1.4f,
                   0.42f, 2.3f, false, 0.0015f, 0.001f, 1.10f, 0.26f, 1.9f,
                   1.02f, 1.01f, 0.99f, 0.12f, 310.0f, 0.9f, 0.45f, 11, 0.92f,
                   0.22f, 0.05f, 0.07f, 0.84f, 0.08f, 0.90f, 0.86f, 1.3f),

    // 9. Leitz Summicron-C
    PROFILE_NO_ANA("Leitz Summicron-C", EVIDENCE_VENDOR_DESCRIBED, 0.45f,
                   -0.0035f, 0.0005f, 0.0f, 0.0f, 0.0f, 0.33f, 0.14f, 0.45f,
                   5.0f, 1.4f, 0.45f, 2.3f, false, 0.0005f, 0.0003f, 1.12f,
                   0.24f, 1.9f, 1.0f, 1.0f, 1.0f, 0.14f, 300.0f, 0.95f, 0.45f,
                   11, 0.90f, 0.22f, 0.05f, 0.06f, 0.80f, 0.10f, 0.88f, 0.84f,
                   1.4f),

    // 10. Panavision Primo
    PROFILE_NO_ANA("Panavision Primo", EVIDENCE_VENDOR_DESCRIBED, 0.35f,
                   -0.0025f, 0.0003f, 0.0f, 0.0f, 0.0f, 0.25f, 0.10f, 0.35f,
                   0.0f, 1.6f, 0.38f, 2.4f, false, 0.0f, 0.0f, 1.20f, 0.20f,
                   1.8f, 1.0f, 1.0f, 1.0f, 0.10f, 300.0f, 0.8f, 0.4f, 9, 0.90f,
                   0.18f, 0.04f, 0.05f, 0.88f, 0.04f, 0.92f, 0.88f, 1.0f),

    // 11. Panavision Primo 70
    PROFILE_NO_ANA("Panavision Primo 70", EVIDENCE_HEURISTIC, 0.30f, -0.0020f,
                   0.0002f, 0.0f, 0.0f, 0.0f, 0.22f, 0.09f, 0.32f, 0.0f, 1.7f,
                   0.35f, 2.4f, false, 0.0f, 0.0f, 1.25f, 0.18f, 1.8f, 1.0f,
                   1.0f, 1.0f, 0.09f, 300.0f, 0.75f, 0.38f, 9, 0.92f, 0.16f,
                   0.03f, 0.04f, 0.90f, 0.03f, 0.93f, 0.90f, 0.9f),

    // 12. Canon CN-E
    PROFILE_NO_ANA("Canon CN-E", EVIDENCE_HEURISTIC, 0.6f, -0.005f, 0.0008f,
                   0.0f, 0.0f, 0.0f, 0.40f, 0.16f, 0.45f, 0.0f, 1.4f, 0.55f,
                   2.2f, false, 0.0f, 0.0f, 1.10f, 0.28f, 1.9f, 1.0f, 1.0f,
                   1.0f, 0.16f, 300.0f, 1.0f, 0.5f, 9, 0.88f, 0.25f, 0.06f,
                   0.06f, 0.78f, 0.08f, 0.86f, 0.78f, 1.8f),

    // 13. Sigma Cine
    PROFILE_NO_ANA("Sigma Cine", EVIDENCE_HEURISTIC, 0.5f, -0.004f, 0.0006f,
                   0.0f, 0.0f, 0.0f, 0.35f, 0.12f, 0.40f, 0.0f, 1.5f, 0.45f,
                   2.4f, false, 0.0f, 0.0f, 1.20f, 0.20f, 1.8f, 1.0f, 1.0f,
                   1.0f, 0.12f, 300.0f, 0.9f, 0.45f, 9, 0.90f, 0.20f, 0.04f,
                   0.05f, 0.86f, 0.05f, 0.90f, 0.86f, 1.2f),

    // 14. Fujinon Premista
    PROFILE_NO_ANA("Fujinon Premista", EVIDENCE_HEURISTIC, 0.45f, -0.0035f,
                   0.0005f, 0.0f, 0.0f, 0.0f, 0.30f, 0.12f, 0.38f, 0.0f, 1.5f,
                   0.48f, 2.3f, false, 0.0f, 0.0f, 1.18f, 0.22f, 1.8f, 1.0f,
                   1.0f, 1.0f, 0.12f, 300.0f, 0.9f, 0.45f, 11, 0.90f, 0.20f,
                   0.05f, 0.05f, 0.84f, 0.05f, 0.88f, 0.84f, 1.5f),

    // 15. ARRI Signature Prime
    PROFILE_NO_ANA("ARRI Signature Prime", EVIDENCE_PUBLISHED, 0.35f, -0.0027f,
                   0.0003f, 0.0f, 0.0f, 0.0f, 0.24f, 0.10f, 0.35f, 5.0f, 1.6f,
                   0.40f, 2.4f, false, 0.001f, 0.0006f, 1.18f, 0.22f, 1.8f,
                   1.02f, 1.0f, 0.99f, 0.10f, 305.0f, 0.85f, 0.42f, 11, 0.93f,
                   0.20f, 0.04f, 0.06f, 0.88f, 0.05f, 0.92f, 0.88f, 1.2f),

    // 16. Zeiss Supreme Prime
    PROFILE_NO_ANA("Zeiss Supreme Prime", EVIDENCE_PUBLISHED, 0.35f, -0.0028f,
                   0.0003f, 0.0f, 0.0f, 0.0f, 0.26f, 0.10f, 0.34f, 0.0f, 1.6f,
                   0.38f, 2.4f, false, 0.0005f, 0.0002f, 1.20f, 0.20f, 1.8f,
                   1.0f, 1.0f, 1.0f, 0.10f, 300.0f, 0.8f, 0.4f, 11, 0.92f,
                   0.18f, 0.04f, 0.05f, 0.90f, 0.04f, 0.93f, 0.90f, 1.1f),

    // 17. Angenieux Optimo 24-290
    PROFILE_NO_ANA("Angenieux Optimo 24-290", EVIDENCE_VENDOR_DESCRIBED, 0.6f,
                   -0.005f, 0.0009f, 0.0f, 0.0f, 0.0f, 0.40f, 0.16f, 0.45f,
                   0.0f, 1.4f, 0.55f, 2.2f, false, 0.0f, 0.0f, 1.10f, 0.28f,
                   1.9f, 1.0f, 1.0f, 1.0f, 0.16f, 300.0f, 1.0f, 0.5f, 9, 0.88f,
                   0.25f, 0.06f, 0.06f, 0.80f, 0.08f, 0.86f, 0.78f, 1.2f),

    // 18. Angenieux Optimo 12x 26-320
    PROFILE_NO_ANA("Angenieux Optimo 12x 26-320", EVIDENCE_VENDOR_DESCRIBED,
                   0.65f, -0.0055f, 0.0010f, 0.0f, 0.0f, 0.0f, 0.42f, 0.17f,
                   0.46f, 0.0f, 1.4f, 0.58f, 2.2f, false, 0.0f, 0.0f, 1.10f,
                   0.28f, 1.9f, 1.0f, 1.0f, 1.0f, 0.17f, 300.0f, 1.0f, 0.5f, 9,
                   0.88f, 0.25f, 0.06f, 0.06f, 0.79f, 0.08f, 0.85f, 0.77f,
                   1.3f),

    // 19. Angenieux 17-80
    PROFILE_NO_ANA("Angenieux 17-80", EVIDENCE_HEURISTIC, 0.9f, -0.008f,
                   0.0016f, 0.0f, 0.0f, 0.0f, 0.55f, 0.22f, 0.55f, 10.0f, 1.2f,
                   0.75f, 2.0f, false, 0.001f, 0.0008f, 1.00f, 0.40f, 2.1f,
                   1.0f, 1.0f, 1.0f, 0.22f, 300.0f, 1.2f, 0.6f, 8, 0.85f, 0.35f,
                   0.10f, 0.12f, 0.65f, 0.18f, 0.78f, 0.62f, 2.5f),

    // 20. Angenieux 15-40
    PROFILE_NO_ANA("Angenieux 15-40", EVIDENCE_HEURISTIC, 1.0f, -0.009f,
                   0.0018f, 0.0f, 0.0f, 0.0f, 0.60f, 0.24f, 0.60f, 10.0f, 1.1f,
                   0.85f, 1.9f, false, 0.001f, 0.0008f, 0.98f, 0.45f, 2.2f,
                   1.0f, 1.0f, 1.0f, 0.24f, 300.0f, 1.3f, 0.62f, 8, 0.83f,
                   0.40f, 0.10f, 0.14f, 0.60f, 0.22f, 0.74f, 0.56f, 2.8f),

    // 21. Fujinon Cabrio
    PROFILE_NO_ANA("Fujinon Cabrio", EVIDENCE_HEURISTIC, 0.7f, -0.006f, 0.0011f,
                   0.0f, 0.0f, 0.0f, 0.45f, 0.18f, 0.50f, 0.0f, 1.4f, 0.60f,
                   2.1f, false, 0.0f, 0.0f, 1.08f, 0.30f, 2.0f, 1.0f, 1.0f,
                   1.0f, 0.18f, 300.0f, 1.1f, 0.55f, 9, 0.86f, 0.30f, 0.06f,
                   0.08f, 0.72f, 0.12f, 0.82f, 0.70f, 1.8f),

    // 22. Canon 30-300
    PROFILE_NO_ANA("Canon 30-300", EVIDENCE_HEURISTIC, 0.75f, -0.0065f, 0.0012f,
                   0.0f, 0.0f, 0.0f, 0.50f, 0.20f, 0.50f, 0.0f, 1.4f, 0.65f,
                   2.1f, false, 0.0f, 0.0f, 1.10f, 0.28f, 2.0f, 1.0f, 1.0f,
                   1.0f, 0.20f, 300.0f, 1.1f, 0.55f, 9, 0.86f, 0.30f, 0.06f,
                   0.08f, 0.74f, 0.10f, 0.84f, 0.72f, 1.6f),

    // 23. Panavision Primo Zoom
    PROFILE_NO_ANA("Panavision Primo Zoom", EVIDENCE_HEURISTIC, 0.45f, -0.0035f,
                   0.0005f, 0.0f, 0.0f, 0.0f, 0.30f, 0.12f, 0.40f, 0.0f, 1.5f,
                   0.48f, 2.3f, false, 0.0f, 0.0f, 1.18f, 0.22f, 1.8f, 1.0f,
                   1.0f, 1.0f, 0.12f, 300.0f, 0.9f, 0.45f, 9, 0.90f, 0.22f,
                   0.05f, 0.05f, 0.84f, 0.05f, 0.88f, 0.84f, 1.1f),

    // 24. Cooke Anamorphic/i
    PROFILE_ANA("Cooke Anamorphic/i", EVIDENCE_VENDOR_DESCRIBED, 0.9f, -0.008f,
                0.0015f, 0.0f, 0.0f, 0.0f, 0.55f, 0.20f, 0.55f, 15.0f, 1.2f,
                0.75f, 2.0f, false, 0.0025f, 0.0018f, 0.98f, 0.45f, 2.1f, 1.03f,
                1.00f, 0.98f, 0.22f, 315.0f, 1.3f, 0.6f, 11, 0.92f, 0.55f,
                0.10f, 0.18f, 0.62f, 0.20f, 0.80f, 0.68f, 2.6f, 2.0f, 0.7f,
                0.65f, 0.55f),

    // 25. Panavision T-Series Anamorphic
    PROFILE_ANA("Panavision T-Series Anamorphic", EVIDENCE_HEURISTIC, 1.1f,
                -0.010f, 0.0022f, 0.0f, 0.0f, 0.0f, 0.75f, 0.28f, 0.70f, 20.0f,
                1.0f, 0.90f, 1.8f, false, 0.002f, 0.0015f, 0.92f, 0.60f, 2.3f,
                1.0f, 0.98f, 1.05f, 0.28f, 310.0f, 1.5f, 0.7f, 11, 0.88f, 0.70f,
                0.14f, 0.25f, 0.50f, 0.30f, 0.70f, 0.52f, 3.5f, 2.0f, 0.85f,
                0.8f, 0.7f),

    // 26. Panavision C-Series Anamorphic
    PROFILE_ANA("Panavision C-Series Anamorphic", EVIDENCE_HEURISTIC, 1.3f,
                -0.012f, 0.0028f, 0.0f, 0.0f, 0.0f, 0.85f, 0.32f, 0.75f, 25.0f,
                0.95f, 1.05f, 1.7f, false, 0.002f, 0.0018f, 0.88f, 0.70f, 2.4f,
                1.0f, 0.98f, 1.06f, 0.32f, 305.0f, 1.6f, 0.75f, 11, 0.85f,
                0.80f, 0.16f, 0.32f, 0.45f, 0.35f, 0.65f, 0.45f, 4.0f, 2.0f,
                0.9f, 0.85f, 0.75f),

    // 27. Hawk Anamorphic
    PROFILE_ANA("Hawk Anamorphic", EVIDENCE_VENDOR_DESCRIBED, 1.0f, -0.009f,
                0.0020f, 0.0f, 0.0f, 0.0f, 0.70f, 0.28f, 0.70f, 20.0f, 1.0f,
                0.90f, 1.8f, false, 0.001f, 0.0005f, 0.98f, 0.55f, 2.2f, 1.0f,
                1.0f, 1.0f, 0.28f, 300.0f, 1.4f, 0.7f, 11, 0.88f, 0.75f, 0.12f,
                0.22f, 0.55f, 0.25f, 0.75f, 0.55f, 3.0f, 2.0f, 0.85f, 0.75f,
                0.6f),

    // 28. Kowa Cine Prominar Anamorphic
    PROFILE_ANA("Kowa Cine Prominar Anamorphic", EVIDENCE_VENDOR_DESCRIBED,
                1.4f, -0.013f, 0.0030f, 0.0f, 0.0f, 0.0f, 0.90f, 0.35f, 0.78f,
                20.0f, 0.9f, 1.10f, 1.7f, false, 0.0015f, 0.001f, 0.90f, 0.70f,
                2.4f, 1.0f, 1.0f, 1.0f, 0.35f, 305.0f, 1.7f, 0.8f, 11, 0.84f,
                0.85f, 0.18f, 0.28f, 0.48f, 0.35f, 0.65f, 0.45f, 4.2f, 2.0f,
                0.9f, 0.8f, 0.65f),

    // 29. LOMO Anamorphic Roundfront
    PROFILE_ANA("LOMO Anamorphic Roundfront", EVIDENCE_VENDOR_DESCRIBED, 1.6f,
                -0.015f, 0.0034f, 0.0f, 0.0f, 0.0f, 1.05f, 0.40f, 0.82f, 25.0f,
                0.85f, 1.20f, 1.6f, false, 0.001f, 0.0005f, 0.88f, 0.80f, 2.5f,
                1.0f, 0.98f, 1.02f, 0.40f, 300.0f, 1.8f, 0.85f, 11, 0.82f,
                0.90f, 0.20f, 0.35f, 0.42f, 0.40f, 0.58f, 0.38f, 5.0f, 2.0f,
                0.92f, 0.9f, 0.7f),

    // 30. Panavision E-Series Anamorphic
    PROFILE_ANA("Panavision E-Series Anamorphic", EVIDENCE_HEURISTIC, 1.2f,
                -0.011f, 0.0025f, 0.0f, 0.0f, 0.0f, 0.80f, 0.30f, 0.72f, 22.0f,
                0.95f, 1.00f, 1.7f, false, 0.0018f, 0.0015f, 0.90f, 0.65f, 2.4f,
                1.0f, 0.98f, 1.05f, 0.30f, 310.0f, 1.6f, 0.75f, 11, 0.86f,
                0.80f, 0.16f, 0.30f, 0.46f, 0.34f, 0.66f, 0.46f, 3.8f, 2.0f,
                0.88f, 0.82f, 0.72f),

    // 31. Panavision G-Series Anamorphic
    PROFILE_ANA("Panavision G-Series Anamorphic", EVIDENCE_HEURISTIC, 1.35f,
                -0.0125f, 0.0029f, 0.0f, 0.0f, 0.0f, 0.88f, 0.33f, 0.75f, 25.0f,
                0.92f, 1.05f, 1.65f, false, 0.0018f, 0.0016f, 0.88f, 0.72f,
                2.45f, 1.0f, 0.98f, 1.06f, 0.33f, 305.0f, 1.7f, 0.78f, 11,
                0.84f, 0.85f, 0.18f, 0.33f, 0.44f, 0.38f, 0.62f, 0.42f, 4.3f,
                2.0f, 0.9f, 0.85f, 0.74f),

    // 32. ARRI Master Anamorphic
    PROFILE_ANA("ARRI Master Anamorphic", EVIDENCE_VENDOR_DESCRIBED, 0.6f,
                -0.005f, 0.0009f, 0.0f, 0.0f, 0.0f, 0.35f, 0.14f, 0.45f, 0.0f,
                1.5f, 0.55f, 2.2f, false, 0.0005f, 0.0003f, 1.15f, 0.26f, 1.9f,
                1.0f, 1.0f, 1.0f, 0.14f, 300.0f, 1.0f, 0.5f, 11, 0.90f, 0.70f,
                0.08f, 0.12f, 0.82f, 0.08f, 0.88f, 0.84f, 1.4f, 2.0f, 0.8f,
                0.55f, 0.55f),

    // 33. Helios 44-2
    PROFILE_NO_ANA("Helios 44-2", EVIDENCE_VENDOR_DESCRIBED, 1.2f, -0.010f,
                   0.0020f, 0.0f, 0.0f, 0.0f, 0.70f, 0.30f, 0.70f, 10.0f, 1.0f,
                   0.90f, 1.8f, false, 0.001f, 0.0f, 0.90f, 0.55f, 2.2f, 1.0f,
                   1.0f, 1.0f, 0.22f, 300.0f, 1.4f, 0.65f, 8, 0.75f, 0.75f,
                   0.10f, 0.80f, 0.55f, 0.25f, 0.70f, 0.55f, 3.0f),

    // 34. Asahi Super Takumar
    PROFILE_NO_ANA("Asahi Super Takumar", EVIDENCE_VENDOR_DESCRIBED, 1.0f,
                   -0.009f, 0.0018f, 0.0f, 0.0f, 0.0f, 0.65f, 0.26f, 0.60f,
                   10.0f, 1.1f, 0.80f, 1.9f, false, 0.001f, 0.0008f, 0.92f,
                   0.50f, 2.2f, 1.02f, 1.0f, 0.98f, 0.24f, 315.0f, 1.3f, 0.62f,
                   6, 0.88f, 0.55f, 0.10f, 0.22f, 0.52f, 0.30f, 0.68f, 0.50f,
                   3.5f),

    // 35. Canon FD
    PROFILE_NO_ANA("Canon FD", EVIDENCE_VENDOR_DESCRIBED, 0.95f, -0.0085f,
                   0.0016f, 0.0f, 0.0f, 0.0f, 0.60f, 0.24f, 0.58f, 12.0f, 1.2f,
                   0.75f, 2.0f, false, 0.0015f, 0.001f, 0.95f, 0.48f, 2.1f,
                   1.02f, 1.0f, 0.98f, 0.22f, 315.0f, 1.2f, 0.6f, 8, 0.90f,
                   0.45f, 0.10f, 0.18f, 0.58f, 0.24f, 0.74f, 0.58f, 2.8f),

    // 36. Voigtlander Nokton
    PROFILE_NO_ANA("Voigtlander Nokton", EVIDENCE_HEURISTIC, 0.85f, -0.0075f,
                   0.0014f, 0.0f, 0.0f, 0.0f, 0.55f, 0.22f, 0.55f, 10.0f, 1.2f,
                   0.70f, 2.0f, false, 0.001f, 0.0008f, 0.98f, 0.45f, 2.1f,
                   1.01f, 1.0f, 0.99f, 0.20f, 310.0f, 1.1f, 0.55f, 10, 0.94f,
                   0.35f, 0.10f, 0.12f, 0.62f, 0.20f, 0.78f, 0.62f, 2.2f),

    // 37. LOMO Spherical
    PROFILE_NO_ANA("LOMO Spherical", EVIDENCE_HEURISTIC, 1.1f, -0.010f, 0.0022f,
                   0.0f, 0.0f, 0.0f, 0.75f, 0.30f, 0.68f, 15.0f, 1.0f, 0.95f,
                   1.8f, false, 0.001f, 0.0005f, 0.90f, 0.65f, 2.3f, 1.0f,
                   0.99f, 1.02f, 0.30f, 300.0f, 1.5f, 0.7f, 8, 0.85f, 0.65f,
                   0.12f, 0.30f, 0.48f, 0.35f, 0.62f, 0.44f, 4.0f),

    // 38. DZOFilm Arles
    PROFILE_NO_ANA("DZOFilm Arles", EVIDENCE_HEURISTIC, 0.7f, -0.006f, 0.0011f,
                   0.0f, 0.0f, 0.0f, 0.45f, 0.18f, 0.50f, 5.0f, 1.3f, 0.60f,
                   2.1f, false, 0.001f, 0.0006f, 1.05f, 0.32f, 2.0f, 1.01f,
                   1.0f, 0.99f, 0.18f, 305.0f, 1.1f, 0.55f, 16, 0.96f, 0.30f,
                   0.08f, 0.08f, 0.72f, 0.12f, 0.82f, 0.72f, 1.8f),

    // 39. Cooke 14mm look reference
    PROFILE_NO_ANA("Cooke 14mm look reference", EVIDENCE_HEURISTIC, 1.4f,
                   -0.013f, 0.0030f, 0.0f, 0.0f, 0.0f, 0.70f, 0.30f, 0.65f,
                   15.0f, 1.0f, 1.10f, 1.7f, false, 0.002f, 0.0015f, 0.95f,
                   0.50f, 2.2f, 1.02f, 1.0f, 0.98f, 0.28f, 320.0f, 1.4f, 0.7f,
                   8, 0.90f, 0.35f, 0.10f, 0.18f, 0.60f, 0.20f, 0.78f, 0.58f,
                   2.0f),

    // 40. Canon K35
    PROFILE_NO_ANA("Canon K35", EVIDENCE_VENDOR_DESCRIBED, 0.8f, -0.007f,
                   0.0010f, 0.0f, 0.0f, 0.0f, 0.55f, 0.22f, 0.55f, 25.0f, 1.2f,
                   0.70f, 2.0f, false, 0.003f, 0.002f, 0.95f, 0.45f, 2.1f,
                   1.05f, 0.98f, 1.03f, 0.20f, 310.0f, 1.2f, 0.55f, 15, 0.98f,
                   0.35f, 0.12f, 0.12f, 0.62f, 0.22f, 0.80f, 0.70f, 2.5f),

    // 41. Angenieux 10X25HP 25-250
    PROFILE_NO_ANA("Angenieux 10X25HP 25-250", EVIDENCE_HEURISTIC, 0.95f,
                   -0.0085f, 0.0016f, 0.0f, 0.0f, 0.0f, 0.60f, 0.24f, 0.58f,
                   10.0f, 1.2f, 0.75f, 2.0f, false, 0.0008f, 0.0005f, 1.00f,
                   0.45f, 2.1f, 1.0f, 1.0f, 1.0f, 0.22f, 300.0f, 1.2f, 0.6f, 8,
                   0.84f, 0.40f, 0.10f, 0.12f, 0.62f, 0.18f, 0.78f, 0.62f,
                   2.8f),

    // 42. Kowa vintage heavy flare preset
    PROFILE_ANA("Kowa vintage heavy flare preset", EVIDENCE_HEURISTIC, 1.8f,
                -0.017f, 0.0042f, 0.0f, 0.0f, 0.0f, 1.1f, 0.45f, 0.85f, 25.0f,
                0.85f, 1.25f, 1.55f, false, 0.001f, 0.0008f, 0.82f, 0.90f, 2.6f,
                1.0f, 0.98f, 1.03f, 0.45f, 305.0f, 1.9f, 0.88f, 11, 0.82f,
                0.92f, 0.24f, 0.38f, 0.38f, 0.50f, 0.55f, 0.32f, 6.5f, 2.0f,
                0.9f, 0.95f, 0.7f),

    // 43. MIR / Soviet character preset
    PROFILE_NO_ANA("MIR / Soviet character preset", EVIDENCE_HEURISTIC, 1.3f,
                   -0.012f, 0.0028f, 0.0f, 0.0f, 0.0f, 0.85f, 0.35f, 0.75f,
                   15.0f, 0.95f, 1.0f, 1.7f, false, 0.001f, 0.0005f, 0.88f,
                   0.75f, 2.4f, 1.0f, 1.0f, 1.0f, 0.35f, 300.0f, 1.6f, 0.75f, 8,
                   0.72f, 0.85f, 0.10f, 0.75f, 0.32f, 0.55f, 0.50f, 0.28f,
                   5.0f),

    // 44. Rehoused stills character kit (FD/K35/Takumar)
    PROFILE_NO_ANA("Rehoused stills character kit (FD/K35/Takumar)",
                   EVIDENCE_HEURISTIC, 1.0f, -0.009f, 0.0018f, 0.0f, 0.0f, 0.0f,
                   0.65f, 0.26f, 0.62f, 15.0f, 1.1f, 0.85f, 1.9f, false, 0.002f,
                   0.0015f, 0.92f, 0.65f, 2.3f, 1.03f, 1.00f, 0.98f, 0.26f,
                   315.0f, 1.4f, 0.7f, 12, 0.96f, 0.55f, 0.14f, 0.22f, 0.45f,
                   0.40f, 0.65f, 0.45f, 3.8f),

    // 45. Zeiss Planar 50mm f/0.7
    PROFILE_NO_ANA("Zeiss Planar 50mm f/0.7", EVIDENCE_PUBLISHED, 0.8f, -0.007f,
                   0.0012f, 0.0f, 0.0f, 0.0f, 0.55f, 0.28f, 0.60f, 10.0f, 1.1f,
                   0.70f, 2.0f, false, 0.001f, 0.0005f, 0.80f, 0.80f, 2.5f,
                   1.0f, 1.0f, 1.0f, 0.30f, 300.0f, 1.6f, 0.7f, 10, 0.95f,
                   0.40f, 0.18f, 0.10f, 0.40f, 0.55f, 0.60f, 0.38f, 4.5f),

    // 46. Canon Rangefinder 50mm f/0.95
    PROFILE_NO_ANA("Canon Rangefinder 50mm f/0.95", EVIDENCE_HEURISTIC, 0.9f,
                   -0.008f, 0.0015f, 0.0f, 0.0f, 0.0f, 0.70f, 0.32f, 0.62f,
                   15.0f, 1.0f, 0.85f, 1.9f, false, 0.002f, 0.0015f, 0.82f,
                   0.85f, 2.6f, 1.02f, 1.0f, 0.98f, 0.32f, 315.0f, 1.7f, 0.75f,
                   10, 0.96f, 0.45f, 0.20f, 0.15f, 0.35f, 0.60f, 0.55f, 0.32f,
                   5.0f),

    // 47. Ultra Panavision 70 vibe (1.25x)
    PROFILE_ANA("Ultra Panavision 70 vibe (1.25x)", EVIDENCE_HEURISTIC, 0.8f,
                -0.007f, 0.0014f, 0.0f, 0.0f, 0.0f, 0.60f, 0.25f, 0.60f, 15.0f,
                1.1f, 0.65f, 2.0f, false, 0.001f, 0.0005f, 0.92f, 0.55f, 2.2f,
                1.0f, 0.98f, 1.06f, 0.25f, 305.0f, 1.4f, 0.7f, 11, 0.92f, 0.60f,
                0.10f, 0.15f, 0.60f, 0.25f, 0.75f, 0.60f, 2.5f, 1.25f, 0.7f,
                0.55f, 0.6f),

    // 48. Petzval design vibe
    PROFILE_NO_ANA("Petzval design vibe", EVIDENCE_HEURISTIC, 1.5f, -0.014f,
                   0.0032f, 0.0f, 0.0f, 0.0f, 0.85f, 0.36f, 0.75f, 15.0f, 1.0f,
                   1.10f, 1.6f, false, 0.001f, 0.0003f, 0.90f, 0.70f, 2.4f,
                   1.0f, 1.0f, 1.0f, 0.34f, 300.0f, 1.7f, 0.8f, 8, 0.70f, 0.90f,
                   0.10f, 0.90f, 0.35f, 0.55f, 0.55f, 0.30f, 4.0f),

    // 49. Projector lens extreme vibe
    PROFILE_NO_ANA("Projector lens extreme vibe", EVIDENCE_HEURISTIC, 2.0f,
                   -0.020f, 0.0060f, 0.0f, 0.0f, 0.0f, 1.20f, 0.55f, 0.90f,
                   20.0f, 0.8f, 1.30f, 1.5f, false, 0.001f, 0.0f, 0.80f, 0.95f,
                   2.6f, 1.0f, 1.0f, 1.0f, 0.50f, 300.0f, 2.0f, 0.9f, 6, 0.60f,
                   0.95f, 0.15f, 0.95f, 0.25f, 0.70f, 0.45f, 0.20f, 6.0f),

    // 50. LOMO Roundfronts extra flare preset
    PROFILE_ANA("LOMO Roundfronts extra flare preset", EVIDENCE_HEURISTIC, 1.7f,
                -0.016f, 0.0038f, 0.0f, 0.0f, 0.0f, 1.10f, 0.42f, 0.84f, 25.0f,
                0.85f, 1.20f, 1.6f, false, 0.001f, 0.0005f, 0.85f, 0.85f, 2.6f,
                1.0f, 0.98f, 1.02f, 0.42f, 300.0f, 1.9f, 0.86f, 11, 0.80f,
                0.92f, 0.22f, 0.40f, 0.40f, 0.45f, 0.55f, 0.35f, 5.5f, 2.0f,
                0.92f, 0.92f, 0.7f),

}; // end s_profiles

// Compile-time check: exactly 50 presets
static_assert(sizeof(s_profiles) / sizeof(s_profiles[0]) == kLensProfileCount,
              "Lens profile database must contain exactly 50 presets");

// ── Public API ──────────────────────────────────────────────────

const LensProfile *getLensProfiles() { return s_profiles; }

const LensProfile *getLensProfile(int index) {
  if (index < 0 || index >= kLensProfileCount)
    return nullptr;
  return &s_profiles[index];
}

const LensProfile *getLensProfileByName(const char *name) {
  if (!name)
    return nullptr;
  for (int i = 0; i < kLensProfileCount; ++i) {
    if (std::strcmp(s_profiles[i].name, name) == 0)
      return &s_profiles[i];
  }
  return nullptr;
}

int getLensProfileCount() { return kLensProfileCount; }

} // namespace glasspectrum
