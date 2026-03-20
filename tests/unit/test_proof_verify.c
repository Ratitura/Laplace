#include <string.h>

#include "test_harness.h"
#include "laplace/proof.h"
#include "laplace/proof_verify.h"
#include "laplace/derive.h"

#define TEST_VERIFY_ARENA_SIZE (32u * 1024u * 1024u) /* 32 MB */

static _Alignas(64) uint8_t g_verify_arena_buf[TEST_VERIFY_ARENA_SIZE];

typedef struct test_verify_fixture {
    laplace_arena_t                    arena;
    laplace_proof_store_t              store;
    laplace_proof_verify_context_t     verifier;
} test_verify_fixture_t;

static laplace_error_t test_verify_fixture_init(test_verify_fixture_t* f) {
    memset(f, 0, sizeof(*f));
    laplace_error_t err = laplace_arena_init(&f->arena,
        g_verify_arena_buf, sizeof(g_verify_arena_buf));
    if (err != LAPLACE_OK) return err;
    err = laplace_proof_store_init(&f->store, &f->arena);
    if (err != LAPLACE_OK) return err;
    return laplace_proof_verify_init(&f->verifier, &f->store);
}

typedef struct mini_corpus {
    laplace_proof_symbol_id_t c_wff;
    laplace_proof_symbol_id_t c_turnstile;
    laplace_proof_symbol_id_t c_implies;
    laplace_proof_symbol_id_t c_not;
    laplace_proof_symbol_id_t c_lparen;
    laplace_proof_symbol_id_t c_rparen;

    laplace_proof_symbol_id_t v_ph;
    laplace_proof_symbol_id_t v_ps;
    laplace_proof_symbol_id_t v_ch;

    laplace_proof_expr_id_t e_ph;       /* wff ph */
    laplace_proof_expr_id_t e_ps;       /* wff ps */
    laplace_proof_expr_id_t e_ch;       /* wff ch */
    laplace_proof_expr_id_t e_ph_impl_ps; /* wff ( ph -> ps ) */
    laplace_proof_expr_id_t e_ps_impl_ph; /* wff ( ps -> ph ) */
    laplace_proof_expr_id_t e_ph_impl_ps_impl_ph; /* wff ( ph -> ( ps -> ph ) ) */

    laplace_proof_frame_id_t frame;

    laplace_proof_hyp_id_t hyp_ph;
    laplace_proof_hyp_id_t hyp_ps;
    laplace_proof_hyp_id_t hyp_ch;

    laplace_proof_hyp_id_t ess_ph;
    laplace_proof_hyp_id_t ess_ph_impl_ps;

    laplace_proof_dv_id_t dv_ph_ps;

    laplace_proof_assertion_id_t ax_id;      /* ax-id: float(ph) -> wff ph */
    laplace_proof_assertion_id_t ax_1;       /* ax-1: float(ph,ps) -> wff ( ph -> ( ps -> ph ) ) */
    laplace_proof_assertion_id_t ax_mp;      /* ax-mp: float(ph,ps) ess(ph, ph->ps) -> wff ps */
    laplace_proof_assertion_id_t ax_id_dv;   /* like ax_id but with DV(ph,ps) */
} mini_corpus_t;

static laplace_error_t build_mini_corpus(
    laplace_proof_store_t* store,
    mini_corpus_t* mc) {

    memset(mc, 0, sizeof(*mc));
    laplace_error_t err;

    err = laplace_proof_import_symbol(store, LAPLACE_PROOF_SYMBOL_CONSTANT, 0, 0, &mc->c_wff);
    if (err != LAPLACE_OK) return err;
    err = laplace_proof_import_symbol(store, LAPLACE_PROOF_SYMBOL_CONSTANT, 1, 0, &mc->c_turnstile);
    if (err != LAPLACE_OK) return err;
    err = laplace_proof_import_symbol(store, LAPLACE_PROOF_SYMBOL_CONSTANT, 2, 0, &mc->c_implies);
    if (err != LAPLACE_OK) return err;
    err = laplace_proof_import_symbol(store, LAPLACE_PROOF_SYMBOL_CONSTANT, 3, 0, &mc->c_not);
    if (err != LAPLACE_OK) return err;
    err = laplace_proof_import_symbol(store, LAPLACE_PROOF_SYMBOL_CONSTANT, 4, 0, &mc->c_lparen);
    if (err != LAPLACE_OK) return err;
    err = laplace_proof_import_symbol(store, LAPLACE_PROOF_SYMBOL_CONSTANT, 5, 0, &mc->c_rparen);
    if (err != LAPLACE_OK) return err;

    err = laplace_proof_import_symbol(store, LAPLACE_PROOF_SYMBOL_VARIABLE, 6, 0, &mc->v_ph);
    if (err != LAPLACE_OK) return err;
    err = laplace_proof_import_symbol(store, LAPLACE_PROOF_SYMBOL_VARIABLE, 7, 0, &mc->v_ps);
    if (err != LAPLACE_OK) return err;
    err = laplace_proof_import_symbol(store, LAPLACE_PROOF_SYMBOL_VARIABLE, 8, 0, &mc->v_ch);
    if (err != LAPLACE_OK) return err;

    {
        laplace_proof_symbol_id_t tokens[] = {mc->v_ph};
        err = laplace_proof_import_expr(store, mc->c_wff, tokens, 1, &mc->e_ph);
        if (err != LAPLACE_OK) return err;
    }
    {
        laplace_proof_symbol_id_t tokens[] = {mc->v_ps};
        err = laplace_proof_import_expr(store, mc->c_wff, tokens, 1, &mc->e_ps);
        if (err != LAPLACE_OK) return err;
    }
    {
        laplace_proof_symbol_id_t tokens[] = {mc->v_ch};
        err = laplace_proof_import_expr(store, mc->c_wff, tokens, 1, &mc->e_ch);
        if (err != LAPLACE_OK) return err;
    }
    {
        laplace_proof_symbol_id_t tokens[] = {
            mc->c_lparen, mc->v_ph, mc->c_implies, mc->v_ps, mc->c_rparen
        };
        err = laplace_proof_import_expr(store, mc->c_wff, tokens, 5, &mc->e_ph_impl_ps);
        if (err != LAPLACE_OK) return err;
    }
    {
        laplace_proof_symbol_id_t tokens[] = {
            mc->c_lparen, mc->v_ps, mc->c_implies, mc->v_ph, mc->c_rparen
        };
        err = laplace_proof_import_expr(store, mc->c_wff, tokens, 5, &mc->e_ps_impl_ph);
        if (err != LAPLACE_OK) return err;
    }
    {
        laplace_proof_symbol_id_t tokens[] = {
            mc->c_lparen, mc->v_ph, mc->c_implies,
            mc->c_lparen, mc->v_ps, mc->c_implies, mc->v_ph, mc->c_rparen,
            mc->c_rparen
        };
        err = laplace_proof_import_expr(store, mc->c_wff, tokens, 9, &mc->e_ph_impl_ps_impl_ph);
        if (err != LAPLACE_OK) return err;
    }

    {
        laplace_proof_frame_desc_t fdesc;
        memset(&fdesc, 0, sizeof(fdesc));
        laplace_proof_symbol_id_t mvars[] = {mc->v_ph, mc->v_ps, mc->v_ch};
        fdesc.mandatory_vars = mvars;
        fdesc.mandatory_var_count = 3;
        err = laplace_proof_import_frame(store, &fdesc, &mc->frame);
        if (err != LAPLACE_OK) return err;
    }

    err = laplace_proof_import_float_hyp(store, mc->frame, mc->e_ph, &mc->hyp_ph);
    if (err != LAPLACE_OK) return err;
    err = laplace_proof_import_float_hyp(store, mc->frame, mc->e_ps, &mc->hyp_ps);
    if (err != LAPLACE_OK) return err;
    err = laplace_proof_import_float_hyp(store, mc->frame, mc->e_ch, &mc->hyp_ch);
    if (err != LAPLACE_OK) return err;

    err = laplace_proof_import_essential_hyp(store, mc->frame, mc->e_ph, &mc->ess_ph);
    if (err != LAPLACE_OK) return err;
    err = laplace_proof_import_essential_hyp(store, mc->frame, mc->e_ph_impl_ps, &mc->ess_ph_impl_ps);
    if (err != LAPLACE_OK) return err;

    err = laplace_proof_import_dv_pair(store, mc->frame, mc->v_ph, mc->v_ps, &mc->dv_ph_ps);
    if (err != LAPLACE_OK) return err;

    {
        laplace_proof_assertion_desc_t desc;
        memset(&desc, 0, sizeof(desc));
        desc.kind           = LAPLACE_PROOF_ASSERTION_AXIOM;
        desc.frame_id       = mc->frame;
        desc.conclusion_id  = mc->e_ph;
        desc.mand_float_offset = mc->hyp_ph;
        desc.mand_float_count  = 1;
        err = laplace_proof_import_assertion(store, &desc, &mc->ax_id);
        if (err != LAPLACE_OK) return err;
    }

    {
        laplace_proof_assertion_desc_t desc;
        memset(&desc, 0, sizeof(desc));
        desc.kind           = LAPLACE_PROOF_ASSERTION_AXIOM;
        desc.frame_id       = mc->frame;
        desc.conclusion_id  = mc->e_ph_impl_ps_impl_ph;
        desc.mand_float_offset = mc->hyp_ph;
        desc.mand_float_count  = 2;
        err = laplace_proof_import_assertion(store, &desc, &mc->ax_1);
        if (err != LAPLACE_OK) return err;
    }

    {
        laplace_proof_assertion_desc_t desc;
        memset(&desc, 0, sizeof(desc));
        desc.kind           = LAPLACE_PROOF_ASSERTION_AXIOM;
        desc.frame_id       = mc->frame;
        desc.conclusion_id  = mc->e_ps;
        desc.mand_float_offset = mc->hyp_ph;
        desc.mand_float_count  = 2;
        desc.mand_ess_offset   = mc->ess_ph;
        desc.mand_ess_count    = 2;
        err = laplace_proof_import_assertion(store, &desc, &mc->ax_mp);
        if (err != LAPLACE_OK) return err;
    }

    {
        laplace_proof_assertion_desc_t desc;
        memset(&desc, 0, sizeof(desc));
        desc.kind           = LAPLACE_PROOF_ASSERTION_AXIOM;
        desc.frame_id       = mc->frame;
        desc.conclusion_id  = mc->e_ph;
        desc.mand_float_offset = mc->hyp_ph;
        desc.mand_float_count  = 1;
        desc.dv_offset         = mc->dv_ph_ps;
        desc.dv_count          = 1;
        err = laplace_proof_import_assertion(store, &desc, &mc->ax_id_dv);
        if (err != LAPLACE_OK) return err;
    }

    return LAPLACE_OK;
}

static int test_verify_init(void) {
    test_verify_fixture_t f;
    LAPLACE_TEST_ASSERT(test_verify_fixture_init(&f) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(f.verifier.initialized == true);
    LAPLACE_TEST_ASSERT(f.verifier.store == &f.store);
    LAPLACE_TEST_ASSERT(f.verifier.stack_depth == 0u);
    return 0;
}

static int test_verify_init_null(void) {
    laplace_proof_verify_context_t ctx;
    laplace_proof_store_t store;
    memset(&store, 0, sizeof(store));
    LAPLACE_TEST_ASSERT(laplace_proof_verify_init(NULL, &store) == LAPLACE_ERR_INVALID_ARGUMENT);
    LAPLACE_TEST_ASSERT(laplace_proof_verify_init(&ctx, NULL) == LAPLACE_ERR_INVALID_ARGUMENT);
    return 0;
}

static int test_verify_trivial_theorem(void) {
    test_verify_fixture_t f;
    LAPLACE_TEST_ASSERT(test_verify_fixture_init(&f) == LAPLACE_OK);

    mini_corpus_t mc;
    LAPLACE_TEST_ASSERT(build_mini_corpus(&f.store, &mc) == LAPLACE_OK);

    laplace_proof_assertion_desc_t thm_desc;
    memset(&thm_desc, 0, sizeof(thm_desc));
    thm_desc.kind           = LAPLACE_PROOF_ASSERTION_THEOREM;
    thm_desc.frame_id       = mc.frame;
    thm_desc.conclusion_id  = mc.e_ph;
    thm_desc.mand_float_offset = mc.hyp_ph;
    thm_desc.mand_float_count  = 1;

    laplace_proof_assertion_id_t thm_assn_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_assertion(
        &f.store, &thm_desc, &thm_assn_id) == LAPLACE_OK);

    laplace_proof_step_t steps[] = {
        {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = mc.hyp_ph},
        {.kind = LAPLACE_PROOF_STEP_ASSERTION,  .ref_id = mc.ax_id},
    };
    laplace_proof_theorem_desc_t tdesc = {
        .assertion_id = thm_assn_id,
        .steps        = steps,
        .step_count   = 2,
        .flags        = 0,
    };

    laplace_proof_theorem_id_t thm_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_theorem(
        &f.store, &tdesc, &thm_id) == LAPLACE_OK);

    laplace_proof_verify_result_t result;
    LAPLACE_TEST_ASSERT(laplace_proof_verify_theorem(
        &f.verifier, thm_id, &result) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(result.status == LAPLACE_PROOF_VERIFY_OK);
    LAPLACE_TEST_ASSERT(result.steps_processed == 2u);

    return 0;
}

static int test_verify_typecode_mismatch(void) {
    test_verify_fixture_t f;
    LAPLACE_TEST_ASSERT(test_verify_fixture_init(&f) == LAPLACE_OK);

    mini_corpus_t mc;
    LAPLACE_TEST_ASSERT(build_mini_corpus(&f.store, &mc) == LAPLACE_OK);

    laplace_proof_symbol_id_t c_set;
    LAPLACE_TEST_ASSERT(laplace_proof_import_symbol(
        &f.store, LAPLACE_PROOF_SYMBOL_CONSTANT, 10, 0, &c_set) == LAPLACE_OK);
    laplace_proof_expr_id_t e_set_ph;
    {
        laplace_proof_symbol_id_t tokens[] = {mc.v_ph};
        LAPLACE_TEST_ASSERT(laplace_proof_import_expr(
            &f.store, c_set, tokens, 1, &e_set_ph) == LAPLACE_OK);
    }

    laplace_proof_hyp_id_t hyp_set_ph;
    LAPLACE_TEST_ASSERT(laplace_proof_import_float_hyp(
        &f.store, mc.frame, e_set_ph, &hyp_set_ph) == LAPLACE_OK);

    laplace_proof_assertion_desc_t thm_desc;
    memset(&thm_desc, 0, sizeof(thm_desc));
    thm_desc.kind           = LAPLACE_PROOF_ASSERTION_THEOREM;
    thm_desc.frame_id       = mc.frame;
    thm_desc.conclusion_id  = mc.e_ph;  /* doesn't matter, will fail before */
    thm_desc.mand_float_offset = mc.hyp_ph;
    thm_desc.mand_float_count  = 1;

    laplace_proof_assertion_id_t thm_assn_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_assertion(
        &f.store, &thm_desc, &thm_assn_id) == LAPLACE_OK);

    laplace_proof_step_t steps[] = {
        {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = hyp_set_ph},
        {.kind = LAPLACE_PROOF_STEP_ASSERTION,  .ref_id = mc.ax_id},
    };
    laplace_proof_theorem_desc_t tdesc = {
        .assertion_id = thm_assn_id,
        .steps        = steps,
        .step_count   = 2,
        .flags        = 0,
    };
    laplace_proof_theorem_id_t thm_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_theorem(
        &f.store, &tdesc, &thm_id) == LAPLACE_OK);

    laplace_proof_verify_result_t result;
    LAPLACE_TEST_ASSERT(laplace_proof_verify_theorem(
        &f.verifier, thm_id, &result) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(result.status == LAPLACE_PROOF_VERIFY_TYPECODE_MISMATCH);
    LAPLACE_TEST_ASSERT(result.failure_step == 1u);

    return 0;
}

static int test_verify_essential_hyp_mismatch(void) {
    test_verify_fixture_t f;
    LAPLACE_TEST_ASSERT(test_verify_fixture_init(&f) == LAPLACE_OK);

    mini_corpus_t mc;
    LAPLACE_TEST_ASSERT(build_mini_corpus(&f.store, &mc) == LAPLACE_OK);

    laplace_proof_assertion_desc_t thm_desc;
    memset(&thm_desc, 0, sizeof(thm_desc));
    thm_desc.kind           = LAPLACE_PROOF_ASSERTION_THEOREM;
    thm_desc.frame_id       = mc.frame;
    thm_desc.conclusion_id  = mc.e_ps;
    thm_desc.mand_float_offset = mc.hyp_ph;
    thm_desc.mand_float_count  = 1;

    laplace_proof_assertion_id_t thm_assn_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_assertion(
        &f.store, &thm_desc, &thm_assn_id) == LAPLACE_OK);

    laplace_proof_step_t steps[] = {
        {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = mc.hyp_ph},  /* push e_ph */
        {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = mc.hyp_ps},  /* push e_ps */
        {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = mc.hyp_ps},  /* WRONG: push e_ps instead of e_ph */
        {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = mc.hyp_ph},  /* push e_ph for impl, wrong for ess */
        {.kind = LAPLACE_PROOF_STEP_ASSERTION,  .ref_id = mc.ax_mp},
    };
    laplace_proof_theorem_desc_t tdesc = {
        .assertion_id = thm_assn_id,
        .steps        = steps,
        .step_count   = 5,
        .flags        = 0,
    };
    laplace_proof_theorem_id_t thm_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_theorem(
        &f.store, &tdesc, &thm_id) == LAPLACE_OK);

    laplace_proof_verify_result_t result;
    LAPLACE_TEST_ASSERT(laplace_proof_verify_theorem(
        &f.verifier, thm_id, &result) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(result.status == LAPLACE_PROOF_VERIFY_ESSENTIAL_HYP_MISMATCH);

    return 0;
}

static int test_verify_dv_violation(void) {
    test_verify_fixture_t f;
    LAPLACE_TEST_ASSERT(test_verify_fixture_init(&f) == LAPLACE_OK);

    mini_corpus_t mc;
    LAPLACE_TEST_ASSERT(build_mini_corpus(&f.store, &mc) == LAPLACE_OK);

    laplace_proof_assertion_desc_t dv_desc;
    memset(&dv_desc, 0, sizeof(dv_desc));
    dv_desc.kind           = LAPLACE_PROOF_ASSERTION_AXIOM;
    dv_desc.frame_id       = mc.frame;
    dv_desc.conclusion_id  = mc.e_ps;
    dv_desc.mand_float_offset = mc.hyp_ph;
    dv_desc.mand_float_count  = 2;   /* hyp_ph, hyp_ps */
    dv_desc.dv_offset         = mc.dv_ph_ps;
    dv_desc.dv_count          = 1;

    laplace_proof_assertion_id_t ax_dv;
    LAPLACE_TEST_ASSERT(laplace_proof_import_assertion(
        &f.store, &dv_desc, &ax_dv) == LAPLACE_OK);

    laplace_proof_assertion_desc_t thm_desc;
    memset(&thm_desc, 0, sizeof(thm_desc));
    thm_desc.kind           = LAPLACE_PROOF_ASSERTION_THEOREM;
    thm_desc.frame_id       = mc.frame;
    thm_desc.conclusion_id  = mc.e_ph;
    thm_desc.mand_float_offset = mc.hyp_ph;
    thm_desc.mand_float_count  = 1;

    laplace_proof_assertion_id_t thm_assn_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_assertion(
        &f.store, &thm_desc, &thm_assn_id) == LAPLACE_OK);

    laplace_proof_step_t steps[] = {
        {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = mc.hyp_ph},  /* push e_ph */
        {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = mc.hyp_ph},  /* push e_ph (for ps slot too) */
        {.kind = LAPLACE_PROOF_STEP_ASSERTION,  .ref_id = ax_dv},
    };
    laplace_proof_theorem_desc_t tdesc = {
        .assertion_id = thm_assn_id,
        .steps        = steps,
        .step_count   = 3,
        .flags        = 0,
    };
    laplace_proof_theorem_id_t thm_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_theorem(
        &f.store, &tdesc, &thm_id) == LAPLACE_OK);

    laplace_proof_verify_result_t result;
    LAPLACE_TEST_ASSERT(laplace_proof_verify_theorem(
        &f.verifier, thm_id, &result) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(result.status == LAPLACE_PROOF_VERIFY_DV_VIOLATION);
    LAPLACE_TEST_ASSERT(result.failure_step == 2u);

    return 0;
}

static int test_verify_invalid_step_ref(void) {
    test_verify_fixture_t f;
    LAPLACE_TEST_ASSERT(test_verify_fixture_init(&f) == LAPLACE_OK);

    mini_corpus_t mc;
    LAPLACE_TEST_ASSERT(build_mini_corpus(&f.store, &mc) == LAPLACE_OK);

    laplace_proof_assertion_desc_t thm_desc;
    memset(&thm_desc, 0, sizeof(thm_desc));
    thm_desc.kind           = LAPLACE_PROOF_ASSERTION_THEOREM;
    thm_desc.frame_id       = mc.frame;
    thm_desc.conclusion_id  = mc.e_ph;
    thm_desc.mand_float_offset = mc.hyp_ph;
    thm_desc.mand_float_count  = 1;

    laplace_proof_assertion_id_t thm_assn_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_assertion(
        &f.store, &thm_desc, &thm_assn_id) == LAPLACE_OK);

    laplace_proof_step_t steps[] = {
        {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = 9999u},
    };
    laplace_proof_theorem_desc_t tdesc = {
        .assertion_id = thm_assn_id,
        .steps        = steps,
        .step_count   = 1,
        .flags        = 0,
    };

    laplace_proof_verify_result_t result;
    LAPLACE_TEST_ASSERT(laplace_proof_verify_theorem(
        &f.verifier, 9999u, &result) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(result.status == LAPLACE_PROOF_VERIFY_INVALID_ARTIFACT);

    (void)tdesc; /* suppress unused warning */
    return 0;
}

static int test_verify_stack_underflow(void) {
    test_verify_fixture_t f;
    LAPLACE_TEST_ASSERT(test_verify_fixture_init(&f) == LAPLACE_OK);

    mini_corpus_t mc;
    LAPLACE_TEST_ASSERT(build_mini_corpus(&f.store, &mc) == LAPLACE_OK);

    laplace_proof_assertion_desc_t thm_desc;
    memset(&thm_desc, 0, sizeof(thm_desc));
    thm_desc.kind           = LAPLACE_PROOF_ASSERTION_THEOREM;
    thm_desc.frame_id       = mc.frame;
    thm_desc.conclusion_id  = mc.e_ph;
    thm_desc.mand_float_offset = mc.hyp_ph;
    thm_desc.mand_float_count  = 1;

    laplace_proof_assertion_id_t thm_assn_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_assertion(
        &f.store, &thm_desc, &thm_assn_id) == LAPLACE_OK);

    laplace_proof_step_t steps[] = {
        {.kind = LAPLACE_PROOF_STEP_ASSERTION,  .ref_id = mc.ax_id},
    };
    laplace_proof_theorem_desc_t tdesc = {
        .assertion_id = thm_assn_id,
        .steps        = steps,
        .step_count   = 1,
        .flags        = 0,
    };
    laplace_proof_theorem_id_t thm_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_theorem(
        &f.store, &tdesc, &thm_id) == LAPLACE_OK);

    laplace_proof_verify_result_t result;
    LAPLACE_TEST_ASSERT(laplace_proof_verify_theorem(
        &f.verifier, thm_id, &result) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(result.status == LAPLACE_PROOF_VERIFY_STACK_UNDERFLOW);
    LAPLACE_TEST_ASSERT(result.failure_step == 0u);

    return 0;
}

static int test_verify_final_mismatch(void) {
    test_verify_fixture_t f;
    LAPLACE_TEST_ASSERT(test_verify_fixture_init(&f) == LAPLACE_OK);

    mini_corpus_t mc;
    LAPLACE_TEST_ASSERT(build_mini_corpus(&f.store, &mc) == LAPLACE_OK);

    laplace_proof_assertion_desc_t thm_desc;
    memset(&thm_desc, 0, sizeof(thm_desc));
    thm_desc.kind           = LAPLACE_PROOF_ASSERTION_THEOREM;
    thm_desc.frame_id       = mc.frame;
    thm_desc.conclusion_id  = mc.e_ps;  /* WRONG: proof will produce e_ph */
    thm_desc.mand_float_offset = mc.hyp_ph;
    thm_desc.mand_float_count  = 1;

    laplace_proof_assertion_id_t thm_assn_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_assertion(
        &f.store, &thm_desc, &thm_assn_id) == LAPLACE_OK);

    laplace_proof_step_t steps[] = {
        {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = mc.hyp_ph},
        {.kind = LAPLACE_PROOF_STEP_ASSERTION,  .ref_id = mc.ax_id},
    };
    laplace_proof_theorem_desc_t tdesc = {
        .assertion_id = thm_assn_id,
        .steps        = steps,
        .step_count   = 2,
        .flags        = 0,
    };
    laplace_proof_theorem_id_t thm_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_theorem(
        &f.store, &tdesc, &thm_id) == LAPLACE_OK);

    laplace_proof_verify_result_t result;
    LAPLACE_TEST_ASSERT(laplace_proof_verify_theorem(
        &f.verifier, thm_id, &result) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(result.status == LAPLACE_PROOF_VERIFY_FINAL_MISMATCH);

    return 0;
}

static int test_verify_deterministic_repeat(void) {
    test_verify_fixture_t f;
    LAPLACE_TEST_ASSERT(test_verify_fixture_init(&f) == LAPLACE_OK);

    mini_corpus_t mc;
    LAPLACE_TEST_ASSERT(build_mini_corpus(&f.store, &mc) == LAPLACE_OK);

    laplace_proof_assertion_desc_t thm_desc;
    memset(&thm_desc, 0, sizeof(thm_desc));
    thm_desc.kind           = LAPLACE_PROOF_ASSERTION_THEOREM;
    thm_desc.frame_id       = mc.frame;
    thm_desc.conclusion_id  = mc.e_ph;
    thm_desc.mand_float_offset = mc.hyp_ph;
    thm_desc.mand_float_count  = 1;

    laplace_proof_assertion_id_t thm_assn_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_assertion(
        &f.store, &thm_desc, &thm_assn_id) == LAPLACE_OK);

    laplace_proof_step_t steps[] = {
        {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = mc.hyp_ph},
        {.kind = LAPLACE_PROOF_STEP_ASSERTION,  .ref_id = mc.ax_id},
    };
    laplace_proof_theorem_desc_t tdesc = {
        .assertion_id = thm_assn_id,
        .steps        = steps,
        .step_count   = 2,
        .flags        = 0,
    };
    laplace_proof_theorem_id_t thm_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_theorem(
        &f.store, &tdesc, &thm_id) == LAPLACE_OK);

    for (int i = 0; i < 10; ++i) {
        laplace_proof_verify_result_t result;
        LAPLACE_TEST_ASSERT(laplace_proof_verify_theorem(
            &f.verifier, thm_id, &result) == LAPLACE_OK);
        LAPLACE_TEST_ASSERT(result.status == LAPLACE_PROOF_VERIFY_OK);
        LAPLACE_TEST_ASSERT(result.steps_processed == 2u);
    }

    return 0;
}

static int test_verify_ax1_application(void) {
    test_verify_fixture_t f;
    LAPLACE_TEST_ASSERT(test_verify_fixture_init(&f) == LAPLACE_OK);

    mini_corpus_t mc;
    LAPLACE_TEST_ASSERT(build_mini_corpus(&f.store, &mc) == LAPLACE_OK);

    laplace_proof_assertion_desc_t thm_desc;
    memset(&thm_desc, 0, sizeof(thm_desc));
    thm_desc.kind           = LAPLACE_PROOF_ASSERTION_THEOREM;
    thm_desc.frame_id       = mc.frame;
    thm_desc.conclusion_id  = mc.e_ph_impl_ps_impl_ph;
    thm_desc.mand_float_offset = mc.hyp_ph;
    thm_desc.mand_float_count  = 2; /* hyp_ph, hyp_ps */

    laplace_proof_assertion_id_t thm_assn_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_assertion(
        &f.store, &thm_desc, &thm_assn_id) == LAPLACE_OK);

    laplace_proof_step_t steps[] = {
        {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = mc.hyp_ph},
        {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = mc.hyp_ps},
        {.kind = LAPLACE_PROOF_STEP_ASSERTION,  .ref_id = mc.ax_1},
    };
    laplace_proof_theorem_desc_t tdesc = {
        .assertion_id = thm_assn_id,
        .steps        = steps,
        .step_count   = 3,
        .flags        = 0,
    };
    laplace_proof_theorem_id_t thm_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_theorem(
        &f.store, &tdesc, &thm_id) == LAPLACE_OK);

    laplace_proof_verify_result_t result;
    LAPLACE_TEST_ASSERT(laplace_proof_verify_theorem(
        &f.verifier, thm_id, &result) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(result.status == LAPLACE_PROOF_VERIFY_OK);
    LAPLACE_TEST_ASSERT(result.steps_processed == 3u);

    return 0;
}

static int test_verify_modus_ponens(void) {
    test_verify_fixture_t f;
    LAPLACE_TEST_ASSERT(test_verify_fixture_init(&f) == LAPLACE_OK);

    mini_corpus_t mc;
    LAPLACE_TEST_ASSERT(build_mini_corpus(&f.store, &mc) == LAPLACE_OK);

    laplace_proof_assertion_desc_t thm_desc;
    memset(&thm_desc, 0, sizeof(thm_desc));
    thm_desc.kind           = LAPLACE_PROOF_ASSERTION_THEOREM;
    thm_desc.frame_id       = mc.frame;
    thm_desc.conclusion_id  = mc.e_ps;
    thm_desc.mand_float_offset = mc.hyp_ph;
    thm_desc.mand_float_count  = 2;

    laplace_proof_assertion_id_t thm_assn_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_assertion(
        &f.store, &thm_desc, &thm_assn_id) == LAPLACE_OK);

    laplace_proof_step_t steps[] = {
        {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = mc.hyp_ph},       /* push e_ph */
        {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = mc.hyp_ps},       /* push e_ps */
        {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = mc.hyp_ph},       /* push e_ph (for ess[0]) */
        {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = mc.ess_ph_impl_ps}, /* push ess hyp expr */
        {.kind = LAPLACE_PROOF_STEP_ASSERTION,  .ref_id = mc.ax_mp},
    };

    laplace_proof_theorem_desc_t tdesc = {
        .assertion_id = thm_assn_id,
        .steps        = steps,
        .step_count   = 5,
        .flags        = 0,
    };
    laplace_proof_theorem_id_t thm_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_theorem(
        &f.store, &tdesc, &thm_id) == LAPLACE_OK);

    laplace_proof_verify_result_t result;
    LAPLACE_TEST_ASSERT(laplace_proof_verify_theorem(
        &f.verifier, thm_id, &result) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(result.status == LAPLACE_PROOF_VERIFY_OK);
    LAPLACE_TEST_ASSERT(result.steps_processed == 5u);

    return 0;
}

static int test_verify_dv_success(void) {
    test_verify_fixture_t f;
    LAPLACE_TEST_ASSERT(test_verify_fixture_init(&f) == LAPLACE_OK);

    mini_corpus_t mc;
    LAPLACE_TEST_ASSERT(build_mini_corpus(&f.store, &mc) == LAPLACE_OK);

    laplace_proof_assertion_desc_t dv_desc;
    memset(&dv_desc, 0, sizeof(dv_desc));
    dv_desc.kind           = LAPLACE_PROOF_ASSERTION_AXIOM;
    dv_desc.frame_id       = mc.frame;
    dv_desc.conclusion_id  = mc.e_ps;
    dv_desc.mand_float_offset = mc.hyp_ph;
    dv_desc.mand_float_count  = 2;
    dv_desc.dv_offset         = mc.dv_ph_ps;
    dv_desc.dv_count          = 1;

    laplace_proof_assertion_id_t ax_dv;
    LAPLACE_TEST_ASSERT(laplace_proof_import_assertion(
        &f.store, &dv_desc, &ax_dv) == LAPLACE_OK);

    laplace_proof_assertion_desc_t thm_desc;
    memset(&thm_desc, 0, sizeof(thm_desc));
    thm_desc.kind           = LAPLACE_PROOF_ASSERTION_THEOREM;
    thm_desc.frame_id       = mc.frame;
    thm_desc.conclusion_id  = mc.e_ps;
    thm_desc.mand_float_offset = mc.hyp_ph;
    thm_desc.mand_float_count  = 2;

    laplace_proof_assertion_id_t thm_assn_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_assertion(
        &f.store, &thm_desc, &thm_assn_id) == LAPLACE_OK);

    laplace_proof_step_t steps[] = {
        {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = mc.hyp_ph},
        {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = mc.hyp_ps},
        {.kind = LAPLACE_PROOF_STEP_ASSERTION,  .ref_id = ax_dv},
    };
    laplace_proof_theorem_desc_t tdesc = {
        .assertion_id = thm_assn_id,
        .steps        = steps,
        .step_count   = 3,
        .flags        = 0,
    };
    laplace_proof_theorem_id_t thm_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_theorem(
        &f.store, &tdesc, &thm_id) == LAPLACE_OK);

    laplace_proof_verify_result_t result;
    LAPLACE_TEST_ASSERT(laplace_proof_verify_theorem(
        &f.verifier, thm_id, &result) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(result.status == LAPLACE_PROOF_VERIFY_OK);

    return 0;
}

static int test_verify_derive_dispatch(void) {
    test_verify_fixture_t f;
    LAPLACE_TEST_ASSERT(test_verify_fixture_init(&f) == LAPLACE_OK);

    mini_corpus_t mc;
    LAPLACE_TEST_ASSERT(build_mini_corpus(&f.store, &mc) == LAPLACE_OK);

    laplace_proof_assertion_desc_t thm_desc;
    memset(&thm_desc, 0, sizeof(thm_desc));
    thm_desc.kind           = LAPLACE_PROOF_ASSERTION_THEOREM;
    thm_desc.frame_id       = mc.frame;
    thm_desc.conclusion_id  = mc.e_ph;
    thm_desc.mand_float_offset = mc.hyp_ph;
    thm_desc.mand_float_count  = 1;

    laplace_proof_assertion_id_t thm_assn_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_assertion(
        &f.store, &thm_desc, &thm_assn_id) == LAPLACE_OK);

    laplace_proof_step_t steps[] = {
        {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = mc.hyp_ph},
        {.kind = LAPLACE_PROOF_STEP_ASSERTION,  .ref_id = mc.ax_id},
    };
    laplace_proof_theorem_desc_t tdesc = {
        .assertion_id = thm_assn_id,
        .steps        = steps,
        .step_count   = 2,
        .flags        = 0,
    };
    laplace_proof_theorem_id_t thm_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_theorem(
        &f.store, &tdesc, &thm_id) == LAPLACE_OK);

    laplace_derive_context_t dctx;
    laplace_derive_context_init(&dctx,
        NULL, NULL, NULL, NULL,
        &f.store, (struct laplace_proof_verify_context*)&f.verifier, NULL);

    laplace_derive_action_t action;
    memset(&action, 0, sizeof(action));
    action.api_version = LAPLACE_DERIVE_API_VERSION;
    action.kernel      = LAPLACE_KERNEL_PROOF;
    action.action      = LAPLACE_DERIVE_ACTION_PROOF_VERIFY;
    action.payload.proof_verify.theorem_id = thm_id;

    laplace_derive_result_t result;
    laplace_derive_dispatch(&dctx, &action, &result);

    LAPLACE_TEST_ASSERT(result.status == LAPLACE_DERIVE_STATUS_OK);
    LAPLACE_TEST_ASSERT(result.result_kind == LAPLACE_DERIVE_RESULT_PROOF_VERIFY);
    LAPLACE_TEST_ASSERT(result.payload.proof_verify.verify_status ==
                         (uint32_t)LAPLACE_PROOF_VERIFY_OK);

    return 0;
}

static int test_verify_derive_dispatch_fail(void) {
    test_verify_fixture_t f;
    LAPLACE_TEST_ASSERT(test_verify_fixture_init(&f) == LAPLACE_OK);

    mini_corpus_t mc;
    LAPLACE_TEST_ASSERT(build_mini_corpus(&f.store, &mc) == LAPLACE_OK);

    laplace_proof_assertion_desc_t thm_desc;
    memset(&thm_desc, 0, sizeof(thm_desc));
    thm_desc.kind           = LAPLACE_PROOF_ASSERTION_THEOREM;
    thm_desc.frame_id       = mc.frame;
    thm_desc.conclusion_id  = mc.e_ps;  /* WRONG conclusion */
    thm_desc.mand_float_offset = mc.hyp_ph;
    thm_desc.mand_float_count  = 1;

    laplace_proof_assertion_id_t thm_assn_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_assertion(
        &f.store, &thm_desc, &thm_assn_id) == LAPLACE_OK);

    laplace_proof_step_t steps[] = {
        {.kind = LAPLACE_PROOF_STEP_HYP,       .ref_id = mc.hyp_ph},
        {.kind = LAPLACE_PROOF_STEP_ASSERTION,  .ref_id = mc.ax_id},
    };
    laplace_proof_theorem_desc_t tdesc = {
        .assertion_id = thm_assn_id,
        .steps        = steps,
        .step_count   = 2,
        .flags        = 0,
    };
    laplace_proof_theorem_id_t thm_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_theorem(
        &f.store, &tdesc, &thm_id) == LAPLACE_OK);

    laplace_derive_context_t dctx;
    laplace_derive_context_init(&dctx,
        NULL, NULL, NULL, NULL,
        &f.store, (struct laplace_proof_verify_context*)&f.verifier, NULL);

    laplace_derive_action_t action;
    memset(&action, 0, sizeof(action));
    action.api_version = LAPLACE_DERIVE_API_VERSION;
    action.kernel      = LAPLACE_KERNEL_PROOF;
    action.action      = LAPLACE_DERIVE_ACTION_PROOF_VERIFY;
    action.payload.proof_verify.theorem_id = thm_id;

    laplace_derive_result_t result;
    laplace_derive_dispatch(&dctx, &action, &result);

    LAPLACE_TEST_ASSERT(result.status == LAPLACE_DERIVE_STATUS_VALIDATION_FAILED);
    LAPLACE_TEST_ASSERT(result.result_kind == LAPLACE_DERIVE_RESULT_PROOF_VERIFY);
    LAPLACE_TEST_ASSERT(result.payload.proof_verify.verify_status ==
                         (uint32_t)LAPLACE_PROOF_VERIFY_FINAL_MISMATCH);

    return 0;
}

static int test_verify_derive_unsupported_action(void) {
    test_verify_fixture_t f;
    LAPLACE_TEST_ASSERT(test_verify_fixture_init(&f) == LAPLACE_OK);

    laplace_derive_context_t dctx;
    laplace_derive_context_init(&dctx,
        NULL, NULL, NULL, NULL,
        &f.store, (struct laplace_proof_verify_context*)&f.verifier, NULL);

    laplace_derive_action_t action;
    memset(&action, 0, sizeof(action));
    action.api_version = LAPLACE_DERIVE_API_VERSION;
    action.kernel      = LAPLACE_KERNEL_PROOF;
    action.action      = LAPLACE_DERIVE_ACTION_PROOF_STEP; /* not implemented */

    laplace_derive_result_t result;
    laplace_derive_dispatch(&dctx, &action, &result);

    LAPLACE_TEST_ASSERT(result.status == LAPLACE_DERIVE_STATUS_UNSUPPORTED_ACTION);
    LAPLACE_TEST_ASSERT(result.result_kind == LAPLACE_DERIVE_RESULT_UNSUPPORTED_ACTION);

    return 0;
}

static int test_verify_status_strings(void) {
    LAPLACE_TEST_ASSERT(strcmp(
        laplace_proof_verify_status_string(LAPLACE_PROOF_VERIFY_OK),
        "ok") == 0);
    LAPLACE_TEST_ASSERT(strcmp(
        laplace_proof_verify_status_string(LAPLACE_PROOF_VERIFY_STACK_UNDERFLOW),
        "stack_underflow") == 0);
    LAPLACE_TEST_ASSERT(strcmp(
        laplace_proof_verify_status_string(LAPLACE_PROOF_VERIFY_DV_VIOLATION),
        "dv_violation") == 0);
    LAPLACE_TEST_ASSERT(strcmp(
        laplace_proof_verify_status_string(LAPLACE_PROOF_VERIFY_FINAL_MISMATCH),
        "final_mismatch") == 0);
    LAPLACE_TEST_ASSERT(strcmp(
        laplace_proof_verify_status_string(99u),
        "unknown") == 0);
    return 0;
}

static int test_verify_empty_proof(void) {
    test_verify_fixture_t f;
    LAPLACE_TEST_ASSERT(test_verify_fixture_init(&f) == LAPLACE_OK);

    mini_corpus_t mc;
    LAPLACE_TEST_ASSERT(build_mini_corpus(&f.store, &mc) == LAPLACE_OK);

    laplace_proof_assertion_desc_t thm_desc;
    memset(&thm_desc, 0, sizeof(thm_desc));
    thm_desc.kind           = LAPLACE_PROOF_ASSERTION_THEOREM;
    thm_desc.frame_id       = mc.frame;
    thm_desc.conclusion_id  = mc.e_ph;
    thm_desc.mand_float_offset = mc.hyp_ph;
    thm_desc.mand_float_count  = 1;

    laplace_proof_assertion_id_t thm_assn_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_assertion(
        &f.store, &thm_desc, &thm_assn_id) == LAPLACE_OK);

    laplace_proof_theorem_desc_t tdesc = {
        .assertion_id = thm_assn_id,
        .steps        = NULL,
        .step_count   = 0,
        .flags        = 0,
    };
    laplace_proof_theorem_id_t thm_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_theorem(
        &f.store, &tdesc, &thm_id) == LAPLACE_OK);

    laplace_proof_verify_result_t result;
    LAPLACE_TEST_ASSERT(laplace_proof_verify_theorem(
        &f.verifier, thm_id, &result) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(result.status == LAPLACE_PROOF_VERIFY_FINAL_MISMATCH);
    LAPLACE_TEST_ASSERT(result.steps_processed == 0u);

    return 0;
}

static int test_verify_result_layout(void) {
    LAPLACE_TEST_ASSERT(sizeof(laplace_proof_verify_result_t) == 28u);
    return 0;
}

typedef int (*test_fn)(void);
typedef struct {
    const char* name;
    test_fn     fn;
} verify_subtest_t;

static const verify_subtest_t verify_subtests[] = {
    {"init",                       test_verify_init},
    {"init_null",                  test_verify_init_null},
    {"trivial_theorem",            test_verify_trivial_theorem},
    {"typecode_mismatch",          test_verify_typecode_mismatch},
    {"essential_hyp_mismatch",     test_verify_essential_hyp_mismatch},
    {"dv_violation",               test_verify_dv_violation},
    {"dv_success",                 test_verify_dv_success},
    {"invalid_step_ref",           test_verify_invalid_step_ref},
    {"stack_underflow",            test_verify_stack_underflow},
    {"final_mismatch",             test_verify_final_mismatch},
    {"deterministic_repeat",       test_verify_deterministic_repeat},
    {"ax1_application",            test_verify_ax1_application},
    {"modus_ponens",               test_verify_modus_ponens},
    {"derive_dispatch",            test_verify_derive_dispatch},
    {"derive_dispatch_fail",       test_verify_derive_dispatch_fail},
    {"derive_unsupported_action",  test_verify_derive_unsupported_action},
    {"status_strings",             test_verify_status_strings},
    {"empty_proof",                test_verify_empty_proof},
    {"result_layout",              test_verify_result_layout},
};

int laplace_test_proof_verify(void) {
    const size_t count = sizeof(verify_subtests) / sizeof(verify_subtests[0]);
    int failed = 0;

    for (size_t i = 0; i < count; ++i) {
        const int result = verify_subtests[i].fn();
        if (result != 0) {
            fprintf(stderr, "  proof_verify subtest FAIL: %s\n",
                    verify_subtests[i].name);
            ++failed;
        }
    }
    return failed;
}
