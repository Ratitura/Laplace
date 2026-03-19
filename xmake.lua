set_project("laplace")
set_version("0.1.0")

set_languages("c23")
set_toolchains("clang")

add_rules("mode.debug", "mode.release")

option("sanitizer")
    set_default("none")
    set_showmenu(true)
    set_values("none", "address", "undefined")
    set_description("Optional clang sanitizer for debug builds")
option_end()

option("ispc")
    set_default(false)
    set_showmenu(true)
    set_description("Enable ISPC backend for HV microkernels (Phase 03)")
option_end()

option("ispc_path")
    set_default("C:\\tools\\ispc\\bin\\ispc.exe")
    set_showmenu(true)
    set_description("Path to ISPC compiler executable")
option_end()

option("ispc_target")
    set_default("avx2-i32x8")
    set_showmenu(true)
    set_description("ISPC target ISA (e.g., avx2-i32x8, sse4-i32x4, avx512skx-i32x16)")
option_end()

option("ispc_arch")
    set_default("x86-64")
    set_showmenu(true)
    set_description("ISPC architecture (e.g., x86-64, aarch64)")
option_end()

option("llvm_ar_path")
    set_default("C:\\Program Files\\LLVM\\bin\\llvm-ar.exe")
    set_showmenu(true)
    set_description("Path to llvm-ar for ISPC object archival")
option_end()

local function laplace_apply_common_warnings()
    set_warnings("all", "extra", "error")
    add_cflags("-Wpedantic", {tools = "clang"})
    add_cflags("-Wconversion", {tools = "clang"})
    add_cflags("-Wshadow", {tools = "clang"})
    add_cflags("-Wstrict-prototypes", {tools = "clang"})
end

local function laplace_apply_common_defines()
    if is_mode("debug") then
        add_defines("LAPLACE_DEBUG=1")
    else
        add_defines("LAPLACE_DEBUG=0")
    end
end

local function laplace_apply_optional_sanitizer()
    if not is_mode("debug") then
        return
    end

    local sanitizer = get_config("sanitizer")
    if sanitizer == "address" then
        add_cflags("-fsanitize=address", {tools = "clang"})
        add_ldflags("-fsanitize=address", {tools = "clang"})
    elseif sanitizer == "undefined" then
        add_cflags("-fsanitize=undefined", {tools = "clang"})
        add_ldflags("-fsanitize=undefined", {tools = "clang"})
    end
end

-- =========================================================================
-- ISPC backend integration (Phase 03 / Phase 03.1 hardening)
--
-- Generated-file layout (Phase 03.1):
--   build/generated/ispc/
--     hv_kernels.obj        -- compiled ISPC object
--     hv_kernels_ispc.h     -- ISPC-generated C header
--
-- Configuration options:
--   --ispc=y               -- enable ISPC backend
--   --ispc_path=<path>     -- ISPC compiler path
--   --ispc_target=<target> -- ISPC target ISA (default: avx2-i32x8)
--   --ispc_arch=<arch>     -- ISPC architecture (default: x86-64)
--   --llvm_ar_path=<path>  -- llvm-ar path for object archival
--
-- When "ispc" is disabled (default), all HV operations use the scalar
-- reference backend (LAPLACE_HV_BACKEND=0).  The scalar backend is
-- always compiled regardless of this option.
--
-- When "ispc" is enabled, first-wave HV microkernels (bind, distance,
-- popcount, similarity) dispatch to ISPC-generated code via the C bridge
-- layer.  The scalar backend remains available for parity testing.
--
-- LAPLACE_HV_BACKEND is defined as a PRIVATE compile flag on laplace_core.
-- External targets (tests, benchmarks) use laplace_hv_backend_name() for
-- runtime backend identification, not compile-time macros.
--
-- Archive injection workaround (Phase 03.1 documentation):
--   xmake does not natively support ISPC as a first-class language.
--   The ISPC object is compiled externally and injected into the static
--   archive via llvm-ar in the after_build hook.  This is a known
--   build-system limitation, not an architectural deficiency.
-- =========================================================================

target("laplace_core")
    set_kind("static")
    add_includedirs("include", {public = true})
    add_includedirs("src")
    add_headerfiles("include/(laplace/**.h)")
    add_files("src/*.c")

    laplace_apply_common_warnings()
    laplace_apply_common_defines()
    laplace_apply_optional_sanitizer()

    if has_config("ispc") then
        -- Backend define is PRIVATE to laplace_core compilation units.
        -- External targets (tests, benchmarks) must not inspect this macro.
        -- Use laplace_hv_backend_name() for runtime backend identification.
        add_defines("LAPLACE_HV_BACKEND=1")

        before_build(function (target)
            local ispc_exe    = get_config("ispc_path")   or "C:\\tools\\ispc\\bin\\ispc.exe"
            local ispc_target = get_config("ispc_target") or "avx2-i32x8"
            local ispc_arch   = get_config("ispc_arch")   or "x86-64"
            local ispc_src    = path.join(os.projectdir(), "src", "ispc", "hv_kernels.ispc")
            local outputdir   = path.join(os.projectdir(), "build", "generated", "ispc")
            local objectfile  = path.join(outputdir, "hv_kernels.obj")
            local headerfile  = path.join(outputdir, "hv_kernels_ispc.h")

            os.mkdir(outputdir)

            -- Always log backend status (concise summary)
            print("[ispc] enabled | target=%s | arch=%s", ispc_target, ispc_arch)

            -- Only recompile if source is newer than object
            if not os.isfile(objectfile) or os.mtime(ispc_src) > os.mtime(objectfile) then
                print("[ispc] compiling %s", ispc_src)
                os.execv(ispc_exe, {
                    ispc_src,
                    "-o", objectfile,
                    "-h", headerfile,
                    "--arch=" .. ispc_arch,
                    "--target=" .. ispc_target,
                    "--opt=fast-math",
                    "--pic"
                })
                print("[ispc] generated: %s", objectfile)
                print("[ispc] generated: %s", headerfile)
            end

            -- Add generated header dir to include path for hv_ispc_bridge.c
            target:add("includedirs", outputdir)
        end)

        after_build(function (target)
            -- Inject the ISPC object into the static library archive.
            -- See comment block above for workaround rationale.
            local outputdir  = path.join(os.projectdir(), "build", "generated", "ispc")
            local objectfile = path.join(outputdir, "hv_kernels.obj")
            local targetfile = target:targetfile()
            local ar         = get_config("llvm_ar_path") or "C:\\Program Files\\LLVM\\bin\\llvm-ar.exe"

            if os.isfile(objectfile) and os.isfile(targetfile) then
                print("[ispc] archiving %s -> %s", objectfile, targetfile)
                os.execv(ar, {"r", targetfile, objectfile})
            else
                if not os.isfile(objectfile) then
                    print("[ispc] WARNING: object not found: %s", objectfile)
                end
            end
        end)
    else
        -- Scalar-only backend (default).
        -- Define is PRIVATE to laplace_core.
        add_defines("LAPLACE_HV_BACKEND=0")

        before_build(function (target)
            print("[backend] scalar-only (ISPC not enabled)")
        end)
    end

target("laplace_tests")
    set_kind("binary")
    add_deps("laplace_core")
    add_includedirs("include", "tests")
    add_files("tests/test_main.c", "tests/unit/*.c")

    laplace_apply_common_warnings()
    laplace_apply_common_defines()
    laplace_apply_optional_sanitizer()

target("laplace_bench")
    set_kind("binary")
    add_deps("laplace_core")
    add_includedirs("include", "bench")
    add_files("bench/*.c")

    laplace_apply_common_warnings()
    laplace_apply_common_defines()
    laplace_apply_optional_sanitizer()
