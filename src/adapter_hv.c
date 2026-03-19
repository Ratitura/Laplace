#include "laplace/adapter_hv.h"

#include <string.h>

laplace_adapter_status_t laplace_adapter_validate_hv_header(
    const laplace_adapter_hv_header_t* const header)
{
    if (header == NULL) {
        return LAPLACE_ADAPTER_ERR_NULL_ARGUMENT;
    }

    if (header->abi_version != LAPLACE_ADAPTER_ABI_VERSION) {
        return LAPLACE_ADAPTER_ERR_INVALID_VERSION;
    }

    if (header->hv_dimension != LAPLACE_HV_DIM) {
        return LAPLACE_ADAPTER_ERR_DIMENSION_MISMATCH;
    }

    if (header->hv_words != LAPLACE_HV_WORDS) {
        return LAPLACE_ADAPTER_ERR_DIMENSION_MISMATCH;
    }

    return LAPLACE_ADAPTER_OK;
}

laplace_adapter_status_t laplace_adapter_hv_ingest(
    const laplace_adapter_hv_header_t* const header,
    const uint64_t* const words,
    laplace_hv_t* const out_hv)
{
    if (header == NULL || words == NULL || out_hv == NULL) {
        return LAPLACE_ADAPTER_ERR_NULL_ARGUMENT;
    }

    const laplace_adapter_status_t hdr_status =
        laplace_adapter_validate_hv_header(header);
    if (hdr_status != LAPLACE_ADAPTER_OK) {
        return hdr_status;
    }

    memcpy(out_hv->words, words, (size_t)LAPLACE_HV_WORDS * sizeof(uint64_t));

    return LAPLACE_ADAPTER_OK;
}
