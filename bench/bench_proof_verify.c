#include <stdint.h>
#include <string.h>

#include "bench_harness.h"
#include "laplace/arena.h"
#include "laplace/proof.h"
#include "laplace/proof_verify.h"
#include "laplace/derive.h"

#define BENCH_VERIFY_ARENA_SIZE (32u * 1024u * 1024u) /* 32 MB */

static _Alignas(64) uint8_t g_bench_verify_buf[BENCH_VERIFY_ARENA_SIZE];

typedef struct bench_verify_ctx {
    laplace_arena_t                arena;
    laplace_proof_store_t          store;
    laplace_proof_verify_context_t verifier;

    laplace_proof_theorem_id_t     thm_trivial;
    laplace_proof_theorem_id_t     thm_ax1;
    laplace_proof_theorem_id_t     thm_mp;
    laplace_proof_theorem_id_t     thm_dv;

    laplace_derive_context_t       derive;

    volatile uint32_t sink_u32;
} bench_verify_ctx_t;

static bench_verify_ctx_t g_bench_verify;

static void bench_verify_setup(void) {
    bench_verify_ctx_t* const ctx = &g_bench_verify;
    memset(ctx, 0, sizeof(*ctx));

    (void)laplace_arena_init(&ctx->arena,
        g_bench_verify_buf, sizeof(g_bench_verify_buf));
    (void)laplace_proof_store_init(&ctx->store, &ctx->arena);
    (void)laplace_proof_verify_init(&ctx->verifier, &ctx->store);

    laplace_proof_symbol_id_t c_wff, c_lparen, c_rparen, c_implies;
    laplace_proof_symbol_id_t v_ph, v_ps;
    (void)laplace_proof_import_symbol(&ctx->store,
        LAPLACE_PROOF_SYMBOL_CONSTANT, 0, 0, &c_wff);
    (void)laplace_proof_import_symbol(&ctx->store,
        LAPLACE_PROOF_SYMBOL_CONSTANT, 1, 0, &c_lparen);
    (void)laplace_proof_import_symbol(&ctx->store,
        LAPLACE_PROOF_SYMBOL_CONSTANT, 2, 0, &c_rparen);
    (void)laplace_proof_import_symbol(&ctx->store,
        LAPLACE_PROOF_SYMBOL_CONSTANT, 3, 0, &c_implies);
    (void)laplace_proof_import_symbol(&ctx->store,
        LAPLACE_PROOF_SYMBOL_VARIABLE, 4, 0, &v_ph);
    (void)laplace_proof_import_symbol(&ctx->store,
        LAPLACE_PROOF_SYMBOL_VARIABLE, 5, 0, &v_ps);

    laplace_proof_expr_id_t e_ph, e_ps, e_ph_impl_ps, e_ph_impl_ps_impl_ph;
    {
        laplace_proof_symbol_id_t t[] = {v_ph};
        (void)laplace_proof_import_expr(&ctx->store, c_wff, t, 1, &e_ph);
    }
    {
        laplace_proof_symbol_id_t t[] = {v_ps};
        (void)laplace_proof_import_expr(&ctx->store, c_wff, t, 1, &e_ps);
    }
    {
        laplace_proof_symbol_id_t t[] = {c_lparen, v_ph, c_implies, v_ps, c_rparen};
        (void)laplace_proof_import_expr(&ctx->store, c_wff, t, 5, &e_ph_impl_ps);
    }
    {
        laplace_proof_symbol_id_t t[] = {
            c_lparen, v_ph, c_implies,
            c_lparen, v_ps, c_implies, v_ph, c_rparen,
            c_rparen
        };
        (void)laplace_proof_import_expr(&ctx->store, c_wff, t, 9, &e_ph_impl_ps_impl_ph);
    }

    laplace_proof_frame_desc_t fdesc = {0};
    laplace_proof_symbol_id_t mvars[] = {v_ph, v_ps};
    fdesc.mandatory_vars = mvars;
    fdesc.mandatory_var_count = 2;
    laplace_proof_frame_id_t frame;
    (void)laplace_proof_import_frame(&ctx->store, &fdesc, &frame);

    laplace_proof_hyp_id_t hyp_ph, hyp_ps;
    (void)laplace_proof_import_float_hyp(&ctx->store, frame, e_ph, &hyp_ph);
    (void)laplace_proof_import_float_hyp(&ctx->store, frame, e_ps, &hyp_ps);

    laplace_proof_hyp_id_t ess_ph, ess_ph_impl_ps;
    (void)laplace_proof_import_essential_hyp(&ctx->store, frame, e_ph, &ess_ph);
    (void)laplace_proof_import_essential_hyp(&ctx->store, frame, e_ph_impl_ps, &ess_ph_impl_ps);

    laplace_proof_dv_id_t dv_ph_ps;
    (void)laplace_proof_import_dv_pair(&ctx->store, frame, v_ph, v_ps, &dv_ph_ps);

    laplace_proof_assertion_id_t ax_id;
    {
        laplace_proof_assertion_desc_t d = {
            .kind = LAPLACE_PROOF_ASSERTION_AXIOM,
            .frame_id = frame,
            .conclusion_id = e_ph,
            .mand_float_offset = hyp_ph,
            .mand_float_count = 1,
        };
        (void)laplace_proof_import_assertion(&ctx->store, &d, &ax_id);
    }

    laplace_proof_assertion_id_t ax_1;
    {
        laplace_proof_assertion_desc_t d = {
            .kind = LAPLACE_PROOF_ASSERTION_AXIOM,
            .frame_id = frame,
            .conclusion_id = e_ph_impl_ps_impl_ph,
            .mand_float_offset = hyp_ph,
            .mand_float_count = 2,
        };
        (void)laplace_proof_import_assertion(&ctx->store, &d, &ax_1);
    }

    laplace_proof_assertion_id_t ax_mp;
    {
        laplace_proof_assertion_desc_t d = {
            .kind = LAPLACE_PROOF_ASSERTION_AXIOM,
            .frame_id = frame,
            .conclusion_id = e_ps,
            .mand_float_offset = hyp_ph,
            .mand_float_count = 2,
            .mand_ess_offset = ess_ph,
            .mand_ess_count = 2,
        };
        (void)laplace_proof_import_assertion(&ctx->store, &d, &ax_mp);
    }

    laplace_proof_assertion_id_t ax_dv;
    {
        laplace_proof_assertion_desc_t d = {
            .kind = LAPLACE_PROOF_ASSERTION_AXIOM,
            .frame_id = frame,
            .conclusion_id = e_ps,
            .mand_float_offset = hyp_ph,
            .mand_float_count = 2,
            .dv_offset = dv_ph_ps,
            .dv_count = 1,
        };
        (void)laplace_proof_import_assertion(&ctx->store, &d, &ax_dv);
    }

    {
        laplace_proof_assertion_desc_t td = {
            .kind = LAPLACE_PROOF_ASSERTION_THEOREM,
            .frame_id = frame,
            .conclusion_id = e_ph,
            .mand_float_offset = hyp_ph,
            .mand_float_count = 1,
        };
        laplace_proof_assertion_id_t ta;
        (void)laplace_proof_import_assertion(&ctx->store, &td, &ta);

        laplace_proof_step_t steps[] = {
            {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = hyp_ph},
            {.kind = LAPLACE_PROOF_STEP_ASSERTION,  .ref_id = ax_id},
        };
        laplace_proof_theorem_desc_t d = {
            .assertion_id = ta, .steps = steps, .step_count = 2,
        };
        (void)laplace_proof_import_theorem(&ctx->store, &d, &ctx->thm_trivial);
    }

    {
        laplace_proof_assertion_desc_t td = {
            .kind = LAPLACE_PROOF_ASSERTION_THEOREM,
            .frame_id = frame,
            .conclusion_id = e_ph_impl_ps_impl_ph,
            .mand_float_offset = hyp_ph,
            .mand_float_count = 2,
        };
        laplace_proof_assertion_id_t ta;
        (void)laplace_proof_import_assertion(&ctx->store, &td, &ta);

        laplace_proof_step_t steps[] = {
            {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = hyp_ph},
            {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = hyp_ps},
            {.kind = LAPLACE_PROOF_STEP_ASSERTION,  .ref_id = ax_1},
        };
        laplace_proof_theorem_desc_t d = {
            .assertion_id = ta, .steps = steps, .step_count = 3,
        };
        (void)laplace_proof_import_theorem(&ctx->store, &d, &ctx->thm_ax1);
    }

    {
        laplace_proof_assertion_desc_t td = {
            .kind = LAPLACE_PROOF_ASSERTION_THEOREM,
            .frame_id = frame,
            .conclusion_id = e_ps,
            .mand_float_offset = hyp_ph,
            .mand_float_count = 2,
        };
        laplace_proof_assertion_id_t ta;
        (void)laplace_proof_import_assertion(&ctx->store, &td, &ta);

        laplace_proof_step_t steps[] = {
            {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = hyp_ph},
            {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = hyp_ps},
            {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = hyp_ph},
            {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = ess_ph_impl_ps},
            {.kind = LAPLACE_PROOF_STEP_ASSERTION,  .ref_id = ax_mp},
        };
        laplace_proof_theorem_desc_t d = {
            .assertion_id = ta, .steps = steps, .step_count = 5,
        };
        (void)laplace_proof_import_theorem(&ctx->store, &d, &ctx->thm_mp);
    }

    {
        laplace_proof_assertion_desc_t td = {
            .kind = LAPLACE_PROOF_ASSERTION_THEOREM,
            .frame_id = frame,
            .conclusion_id = e_ps,
            .mand_float_offset = hyp_ph,
            .mand_float_count = 2,
        };
        laplace_proof_assertion_id_t ta;
        (void)laplace_proof_import_assertion(&ctx->store, &td, &ta);

        laplace_proof_step_t steps[] = {
            {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = hyp_ph},
            {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = hyp_ps},
            {.kind = LAPLACE_PROOF_STEP_ASSERTION,  .ref_id = ax_dv},
        };
        laplace_proof_theorem_desc_t d = {
            .assertion_id = ta, .steps = steps, .step_count = 3,
        };
        (void)laplace_proof_import_theorem(&ctx->store, &d, &ctx->thm_dv);
    }

    laplace_derive_context_init(&ctx->derive,
        NULL, NULL, NULL, NULL,
        (struct laplace_proof_store*)&ctx->store,
        (struct laplace_proof_verify_context*)&ctx->verifier,
        NULL);
}

static void bench_verify_trivial(void* const context) {
    bench_verify_ctx_t* const ctx = (bench_verify_ctx_t*)context;
    laplace_proof_verify_result_t result;
    (void)laplace_proof_verify_theorem(&ctx->verifier, ctx->thm_trivial, &result);
    ctx->sink_u32 = (uint32_t)result.status;
}

static void bench_verify_ax1(void* const context) {
    bench_verify_ctx_t* const ctx = (bench_verify_ctx_t*)context;
    laplace_proof_verify_result_t result;
    (void)laplace_proof_verify_theorem(&ctx->verifier, ctx->thm_ax1, &result);
    ctx->sink_u32 = (uint32_t)result.status;
}

static void bench_verify_mp(void* const context) {
    bench_verify_ctx_t* const ctx = (bench_verify_ctx_t*)context;
    laplace_proof_verify_result_t result;
    (void)laplace_proof_verify_theorem(&ctx->verifier, ctx->thm_mp, &result);
    ctx->sink_u32 = (uint32_t)result.status;
}

static void bench_verify_dv(void* const context) {
    bench_verify_ctx_t* const ctx = (bench_verify_ctx_t*)context;
    laplace_proof_verify_result_t result;
    (void)laplace_proof_verify_theorem(&ctx->verifier, ctx->thm_dv, &result);
    ctx->sink_u32 = (uint32_t)result.status;
}

static void bench_verify_derive(void* const context) {
    bench_verify_ctx_t* const ctx = (bench_verify_ctx_t*)context;

    laplace_derive_action_t action;
    memset(&action, 0, sizeof(action));
    action.api_version = LAPLACE_DERIVE_API_VERSION;
    action.kernel      = LAPLACE_KERNEL_PROOF;
    action.action      = LAPLACE_DERIVE_ACTION_PROOF_VERIFY;
    action.payload.proof_verify.theorem_id = ctx->thm_trivial;

    laplace_derive_result_t result;
    laplace_derive_dispatch(&ctx->derive, &action, &result);
    ctx->sink_u32 = result.payload.proof_verify.verify_status;
}

static void bench_verify_reset(void* const context) {
    bench_verify_ctx_t* const ctx = (bench_verify_ctx_t*)context;
    laplace_proof_verify_reset(&ctx->verifier);
    ctx->sink_u32 = ctx->verifier.stack_depth;
}

void laplace_bench_proof_verify(void) {
    bench_verify_setup();

    const laplace_bench_case_t benches[] = {
        {"proof_verify_trivial", bench_verify_trivial,
         &g_bench_verify, 2000000u},
        {"proof_verify_ax1", bench_verify_ax1,
         &g_bench_verify, 1000000u},
        {"proof_verify_mp", bench_verify_mp,
         &g_bench_verify, 1000000u},
        {"proof_verify_dv", bench_verify_dv,
         &g_bench_verify, 1000000u},
        {"proof_verify_derive", bench_verify_derive,
         &g_bench_verify, 1000000u},
        {"proof_verify_reset", bench_verify_reset,
         &g_bench_verify, 5000000u},
    };

    const size_t count = sizeof(benches) / sizeof(benches[0]);
    for (size_t i = 0u; i < count; ++i) {
        laplace_bench_run_case(&benches[i]);
    }
}
