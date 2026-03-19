#include "laplace/transport.h"

#include <string.h>

#if LAPLACE_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static void transport_init_header(laplace_transport_mapping_header_t* header) {
    memset(header, 0, sizeof(*header));
    header->magic               = LAPLACE_TRANSPORT_MAGIC;
    header->abi_version         = LAPLACE_TRANSPORT_ABI_VERSION;
    header->endian              = LAPLACE_TRANSPORT_NATIVE_ENDIAN;
    header->ingress_capacity    = LAPLACE_TRANSPORT_INGRESS_CAPACITY;
    header->egress_capacity     = LAPLACE_TRANSPORT_EGRESS_CAPACITY;
    header->command_record_size = LAPLACE_TRANSPORT_COMMAND_RECORD_SIZE;
    header->event_record_size   = LAPLACE_TRANSPORT_EVENT_RECORD_SIZE;
    header->total_mapping_size  = (uint32_t)LAPLACE_TRANSPORT_TOTAL_MAPPING_SIZE;
    header->flags               = 0u;
#if LAPLACE_PLATFORM_WINDOWS
    header->creator_pid         = (uint32_t)GetCurrentProcessId();
#else
    header->creator_pid         = 0u;
#endif
}

static void transport_init_ring(laplace_transport_ring_header_t* ring) {
    memset(ring, 0, sizeof(*ring));
    ring->head = 0u;
    ring->tail = 0u;
}

static bool transport_validate_header_fields(const laplace_transport_mapping_header_t* header) {
    if (header->magic != LAPLACE_TRANSPORT_MAGIC) {
        return false;
    }
    if (header->abi_version != LAPLACE_TRANSPORT_ABI_VERSION) {
        return false;
    }
    if (header->endian != LAPLACE_TRANSPORT_NATIVE_ENDIAN) {
        return false;
    }
    if (header->ingress_capacity != LAPLACE_TRANSPORT_INGRESS_CAPACITY) {
        return false;
    }
    if (header->egress_capacity != LAPLACE_TRANSPORT_EGRESS_CAPACITY) {
        return false;
    }
    if (header->command_record_size != LAPLACE_TRANSPORT_COMMAND_RECORD_SIZE) {
        return false;
    }
    if (header->event_record_size != LAPLACE_TRANSPORT_EVENT_RECORD_SIZE) {
        return false;
    }
    if (header->total_mapping_size != (uint32_t)LAPLACE_TRANSPORT_TOTAL_MAPPING_SIZE) {
        return false;
    }
    return true;
}

#if LAPLACE_PLATFORM_WINDOWS

/*
 * Convert UTF-8 name to wide-string for Win32 APIs.
 * Returns 0 on success, non-zero on failure.
 * The caller provides a fixed-size wide buffer.
 */
#define TRANSPORT_WIN32_NAME_MAX 256

static int transport_utf8_to_wide(const char* utf8_name, wchar_t* wide_buf, int wide_buf_count) {
    if (utf8_name == NULL || wide_buf == NULL || wide_buf_count <= 0) {
        return -1;
    }
    const int result = MultiByteToWideChar(CP_UTF8, 0, utf8_name, -1, wide_buf, wide_buf_count);
    return (result > 0) ? 0 : -1;
}

laplace_error_t laplace_transport_create(laplace_transport_mapping_t* out_mapping,
                                          const char* name) {
    if (out_mapping == NULL || name == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    memset(out_mapping, 0, sizeof(*out_mapping));

    wchar_t wide_name[TRANSPORT_WIN32_NAME_MAX];
    if (transport_utf8_to_wide(name, wide_name, TRANSPORT_WIN32_NAME_MAX) != 0) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    const DWORD total_size = (DWORD)LAPLACE_TRANSPORT_TOTAL_MAPPING_SIZE;

    HANDLE hMapping = CreateFileMappingW(
        INVALID_HANDLE_VALUE,  /* backed by page file */
        NULL,                  /* default security */
        PAGE_READWRITE,
        0u,                    /* high-order DWORD of size */
        total_size,            /* low-order DWORD of size */
        wide_name
    );

    if (hMapping == NULL) {
        return LAPLACE_ERR_INTERNAL;
    }

    void* view = MapViewOfFile(
        hMapping,
        FILE_MAP_ALL_ACCESS,
        0u, 0u,
        total_size
    );

    if (view == NULL) {
        CloseHandle(hMapping);
        return LAPLACE_ERR_INTERNAL;
    }

    /* Zero entire region first */
    memset(view, 0, total_size);

    out_mapping->view           = view;
    out_mapping->backend_handle = (void*)hMapping;
    out_mapping->total_size     = total_size;
    out_mapping->is_creator     = true;

    laplace_transport_mapping_header_t* header = laplace_transport_get_header(out_mapping);
    transport_init_header(header);

    laplace_transport_ring_header_t* ingress_ring = laplace_transport_get_ingress_ring(out_mapping);
    transport_init_ring(ingress_ring);

    laplace_transport_ring_header_t* egress_ring = laplace_transport_get_egress_ring(out_mapping);
    transport_init_ring(egress_ring);

    return LAPLACE_OK;
}

laplace_error_t laplace_transport_open(laplace_transport_mapping_t* out_mapping,
                                        const char* name) {
    if (out_mapping == NULL || name == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    memset(out_mapping, 0, sizeof(*out_mapping));

    wchar_t wide_name[TRANSPORT_WIN32_NAME_MAX];
    if (transport_utf8_to_wide(name, wide_name, TRANSPORT_WIN32_NAME_MAX) != 0) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    HANDLE hMapping = OpenFileMappingW(
        FILE_MAP_ALL_ACCESS,
        FALSE,
        wide_name
    );

    if (hMapping == NULL) {
        return LAPLACE_ERR_INTERNAL;
    }

    void* view = MapViewOfFile(
        hMapping,
        FILE_MAP_ALL_ACCESS,
        0u, 0u,
        (DWORD)LAPLACE_TRANSPORT_TOTAL_MAPPING_SIZE
    );

    if (view == NULL) {
        CloseHandle(hMapping);
        return LAPLACE_ERR_INTERNAL;
    }

    out_mapping->view           = view;
    out_mapping->backend_handle = (void*)hMapping;
    out_mapping->total_size     = (uint32_t)LAPLACE_TRANSPORT_TOTAL_MAPPING_SIZE;
    out_mapping->is_creator     = false;

    const laplace_transport_mapping_header_t* header = laplace_transport_get_header(out_mapping);
    if (!transport_validate_header_fields(header)) {
        laplace_transport_close(out_mapping);
        return LAPLACE_ERR_INVALID_STATE;
    }

    return LAPLACE_OK;
}

void laplace_transport_close(laplace_transport_mapping_t* mapping) {
    if (mapping == NULL) {
        return;
    }

    if (mapping->view != NULL) {
        UnmapViewOfFile(mapping->view);
        mapping->view = NULL;
    }

    if (mapping->backend_handle != NULL) {
        CloseHandle((HANDLE)mapping->backend_handle);
        mapping->backend_handle = NULL;
    }

    mapping->total_size = 0u;
    mapping->is_creator = false;
}

#else /* !LAPLACE_PLATFORM_WINDOWS */

/*
 * Non-Windows stub: returns error.  POSIX / FPGA support is deferred.
 */
laplace_error_t laplace_transport_create(laplace_transport_mapping_t* out_mapping,
                                          const char* name) {
    (void)out_mapping;
    (void)name;
    return LAPLACE_ERR_NOT_SUPPORTED;
}

laplace_error_t laplace_transport_open(laplace_transport_mapping_t* out_mapping,
                                        const char* name) {
    (void)out_mapping;
    (void)name;
    return LAPLACE_ERR_NOT_SUPPORTED;
}

void laplace_transport_close(laplace_transport_mapping_t* mapping) {
    (void)mapping;
}

#endif /* LAPLACE_PLATFORM_WINDOWS */

bool laplace_transport_validate_header(const laplace_transport_mapping_t* mapping) {
    if (mapping == NULL || mapping->view == NULL) {
        return false;
    }
    const laplace_transport_mapping_header_t* header = laplace_transport_get_header(mapping);
    return transport_validate_header_fields(header);
}
