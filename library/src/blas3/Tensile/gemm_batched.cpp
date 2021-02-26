/* ************************************************************************
 * Copyright 2016-2021 Advanced Micro Devices, Inc.
 * ************************************************************************ */
#include "gemm_batched.hpp"
#include "gemm.hpp"
#include "logging.hpp"

namespace
{
    template <typename>
    constexpr char rocblas_gemm_batched_name[] = "unknown";
    template <>
    constexpr char rocblas_gemm_batched_name<rocblas_half>[] = "rocblas_hgemm_batched";
    template <>
    constexpr char rocblas_gemm_batched_name<float>[] = "rocblas_sgemm_batched";
    template <>
    constexpr char rocblas_gemm_batched_name<double>[] = "rocblas_dgemm_batched";
    template <>
    constexpr char rocblas_gemm_batched_name<rocblas_float_complex>[] = "rocblas_cgemm_batched";
    template <>
    constexpr char rocblas_gemm_batched_name<rocblas_double_complex>[] = "rocblas_zgemm_batched";

    /*******************************************************************************
    * Batched GEMM implementation
    ******************************************************************************/
    template <typename T>
    rocblas_status rocblas_gemm_batched_impl(rocblas_handle    handle,
                                             rocblas_operation trans_a,
                                             rocblas_operation trans_b,
                                             rocblas_int       m,
                                             rocblas_int       n,
                                             rocblas_int       k,
                                             const T*          alpha,
                                             const T* const    A[],
                                             ptrdiff_t         ld_a,
                                             const T* const    B[],
                                             ptrdiff_t         ld_b,
                                             const T*          beta,
                                             T* const          C[],
                                             ptrdiff_t         ld_c,
                                             rocblas_int       b_c)
    {
        if(!handle)
            return rocblas_status_invalid_handle;
        RETURN_ZERO_DEVICE_MEMORY_SIZE_IF_QUERIED(handle);

        // Copy alpha and beta to host if on device
        T alpha_h, beta_h;
        RETURN_IF_ROCBLAS_ERROR(
            copy_alpha_beta_to_host_if_on_device(handle, alpha, beta, alpha_h, beta_h, k));
        auto saved_pointer_mode = handle->push_pointer_mode(rocblas_pointer_mode_host);

        // Perform logging
        auto layer_mode     = handle->layer_mode;
        auto check_numerics = handle->check_numerics;
        if(layer_mode
           & (rocblas_layer_mode_log_trace | rocblas_layer_mode_log_bench
              | rocblas_layer_mode_log_profile))
        {
            auto trans_a_letter = rocblas_transpose_letter(trans_a);
            auto trans_b_letter = rocblas_transpose_letter(trans_b);

            if(layer_mode & rocblas_layer_mode_log_trace)
                log_trace(handle,
                          rocblas_gemm_batched_name<T>,
                          trans_a,
                          trans_b,
                          m,
                          n,
                          k,
                          LOG_TRACE_SCALAR_VALUE(handle, alpha),
                          A,
                          ld_a,
                          B,
                          ld_b,
                          LOG_TRACE_SCALAR_VALUE(handle, beta),
                          C,
                          ld_c,
                          b_c);

            if(layer_mode & rocblas_layer_mode_log_bench)
                log_bench(handle,
                          "./rocblas-bench -f gemm_batched -r",
                          rocblas_precision_string<T>,
                          "--transposeA",
                          trans_a_letter,
                          "--transposeB",
                          trans_b_letter,
                          "-m",
                          m,
                          "-n",
                          n,
                          "-k",
                          k,
                          LOG_BENCH_SCALAR_VALUE(handle, alpha),
                          "--lda",
                          ld_a,
                          "--ldb",
                          ld_b,
                          LOG_BENCH_SCALAR_VALUE(handle, beta),
                          "--ldc",
                          ld_c,
                          "--batch_count",
                          b_c);

            if(layer_mode & rocblas_layer_mode_log_profile)
                log_profile(handle,
                            rocblas_gemm_batched_name<T>,
                            "transA",
                            trans_a_letter,
                            "transB",
                            trans_b_letter,
                            "M",
                            m,
                            "N",
                            n,
                            "K",
                            k,
                            "alpha",
                            value_category(*alpha),
                            "lda",
                            ld_a,
                            "ldb",
                            ld_b,
                            "beta",
                            value_category(*beta),
                            "ldc",
                            ld_c,
                            "batch_count",
                            b_c);
        }

        auto validArgs = validateArgs(
            handle, trans_a, trans_b, m, n, k, alpha, A, ld_a, B, ld_b, beta, C, ld_c, b_c);

        if(validArgs != rocblas_status_continue)
            return validArgs;

        if(check_numerics)
        {
            bool           is_input = true;
            rocblas_status gemm_check_numerics_status
                = rocblas_gemm_check_numerics(rocblas_gemm_batched_name<T>,
                                              handle,
                                              trans_a,
                                              trans_b,
                                              m,
                                              n,
                                              k,
                                              A,
                                              ld_a,
                                              0,
                                              B,
                                              ld_b,
                                              0,
                                              C,
                                              ld_c,
                                              0,
                                              b_c,
                                              check_numerics,
                                              is_input);
            if(gemm_check_numerics_status != rocblas_status_success)
                return gemm_check_numerics_status;
        }

        rocblas_status status = rocblas_status_success;
        status                = rocblas_gemm_template<true>(handle,
                                             trans_a,
                                             trans_b,
                                             m,
                                             n,
                                             k,
                                             alpha,
                                             A,
                                             0,
                                             ld_a,
                                             0,
                                             B,
                                             0,
                                             ld_b,
                                             0,
                                             beta,
                                             C,
                                             0,
                                             ld_c,
                                             0,
                                             b_c);
        if(status != rocblas_status_success)
            return status;

        if(check_numerics)
        {
            bool           is_input = false;
            rocblas_status gemm_check_numerics_status
                = rocblas_gemm_check_numerics(rocblas_gemm_batched_name<T>,
                                              handle,
                                              trans_a,
                                              trans_b,
                                              m,
                                              n,
                                              k,
                                              A,
                                              ld_a,
                                              0,
                                              B,
                                              ld_b,
                                              0,
                                              C,
                                              ld_c,
                                              0,
                                              b_c,
                                              check_numerics,
                                              is_input);
            if(gemm_check_numerics_status != rocblas_status_success)
                return gemm_check_numerics_status;
        }
        return status;
    }
}

/*******************************************************************************
 * Batched GEMM APIs
 ******************************************************************************/

extern "C" {
rocblas_status rocblas_hgemm_batched(rocblas_handle            handle,
                                     rocblas_operation         trans_a,
                                     rocblas_operation         trans_b,
                                     rocblas_int               m,
                                     rocblas_int               n,
                                     rocblas_int               k,
                                     const rocblas_half*       alpha,
                                     const rocblas_half* const A[],
                                     rocblas_int               ld_a,
                                     const rocblas_half* const B[],
                                     rocblas_int               ld_b,
                                     const rocblas_half*       beta,
                                     rocblas_half* const       C[],
                                     rocblas_int               ld_c,
                                     rocblas_int               b_c)
try
{
    return rocblas_gemm_batched_impl<rocblas_half>(handle,
                                                   trans_a,
                                                   trans_b,
                                                   m,
                                                   n,
                                                   k,
                                                   alpha,
                                                   A,
                                                   ptrdiff_t(ld_a),
                                                   B,
                                                   ptrdiff_t(ld_b),
                                                   beta,
                                                   C,
                                                   ptrdiff_t(ld_c),
                                                   b_c);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_sgemm_batched(rocblas_handle     handle,
                                     rocblas_operation  trans_a,
                                     rocblas_operation  trans_b,
                                     rocblas_int        m,
                                     rocblas_int        n,
                                     rocblas_int        k,
                                     const float*       alpha,
                                     const float* const A[],
                                     rocblas_int        ld_a,
                                     const float* const B[],
                                     rocblas_int        ld_b,
                                     const float*       beta,
                                     float* const       C[],
                                     rocblas_int        ld_c,
                                     rocblas_int        b_c)
try
{
    return rocblas_gemm_batched_impl<float>(handle,
                                            trans_a,
                                            trans_b,
                                            m,
                                            n,
                                            k,
                                            alpha,
                                            A,
                                            ptrdiff_t(ld_a),
                                            B,
                                            ptrdiff_t(ld_b),
                                            beta,
                                            C,
                                            ptrdiff_t(ld_c),
                                            b_c);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_dgemm_batched(rocblas_handle      handle,
                                     rocblas_operation   trans_a,
                                     rocblas_operation   trans_b,
                                     rocblas_int         m,
                                     rocblas_int         n,
                                     rocblas_int         k,
                                     const double*       alpha,
                                     const double* const A[],
                                     rocblas_int         ld_a,
                                     const double* const B[],
                                     rocblas_int         ld_b,
                                     const double*       beta,
                                     double* const       C[],
                                     rocblas_int         ld_c,
                                     rocblas_int         b_c)
try
{
    return rocblas_gemm_batched_impl<double>(handle,
                                             trans_a,
                                             trans_b,
                                             m,
                                             n,
                                             k,
                                             alpha,
                                             A,
                                             ptrdiff_t(ld_a),
                                             B,
                                             ptrdiff_t(ld_b),
                                             beta,
                                             C,
                                             ptrdiff_t(ld_c),
                                             b_c);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_cgemm_batched(rocblas_handle                     handle,
                                     rocblas_operation                  trans_a,
                                     rocblas_operation                  trans_b,
                                     rocblas_int                        m,
                                     rocblas_int                        n,
                                     rocblas_int                        k,
                                     const rocblas_float_complex*       alpha,
                                     const rocblas_float_complex* const A[],
                                     rocblas_int                        ld_a,
                                     const rocblas_float_complex* const B[],
                                     rocblas_int                        ld_b,
                                     const rocblas_float_complex*       beta,
                                     rocblas_float_complex* const       C[],
                                     rocblas_int                        ld_c,
                                     rocblas_int                        b_c)
try
{
    return rocblas_gemm_batched_impl<rocblas_float_complex>(handle,
                                                            trans_a,
                                                            trans_b,
                                                            m,
                                                            n,
                                                            k,
                                                            alpha,
                                                            A,
                                                            ptrdiff_t(ld_a),
                                                            B,
                                                            ptrdiff_t(ld_b),
                                                            beta,
                                                            C,
                                                            ptrdiff_t(ld_c),
                                                            b_c);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_zgemm_batched(rocblas_handle                      handle,
                                     rocblas_operation                   trans_a,
                                     rocblas_operation                   trans_b,
                                     rocblas_int                         m,
                                     rocblas_int                         n,
                                     rocblas_int                         k,
                                     const rocblas_double_complex*       alpha,
                                     const rocblas_double_complex* const A[],
                                     rocblas_int                         ld_a,
                                     const rocblas_double_complex* const B[],
                                     rocblas_int                         ld_b,
                                     const rocblas_double_complex*       beta,
                                     rocblas_double_complex* const       C[],
                                     rocblas_int                         ld_c,
                                     rocblas_int                         b_c)
try
{
    return rocblas_gemm_batched_impl<rocblas_double_complex>(handle,
                                                             trans_a,
                                                             trans_b,
                                                             m,
                                                             n,
                                                             k,
                                                             alpha,
                                                             A,
                                                             ptrdiff_t(ld_a),
                                                             B,
                                                             ptrdiff_t(ld_b),
                                                             beta,
                                                             C,
                                                             ptrdiff_t(ld_c),
                                                             b_c);
}
catch(...)
{
    return exception_to_rocblas_status();
}

/*******************************************************************************
 * Batched GEMM Kernel name APIs
 ******************************************************************************/
rocblas_status rocblas_hgemm_batched_kernel_name(rocblas_handle      handle,
                                                 rocblas_operation   trans_a,
                                                 rocblas_operation   trans_b,
                                                 rocblas_int         m,
                                                 rocblas_int         n,
                                                 rocblas_int         k,
                                                 const rocblas_half* alpha,
                                                 const rocblas_half* A[],
                                                 rocblas_int         ld_a,
                                                 const rocblas_half* B[],
                                                 rocblas_int         ld_b,
                                                 const rocblas_half* beta,
                                                 rocblas_half*       C[],
                                                 rocblas_int         ld_c,
                                                 rocblas_int         b_c)
{
    return rocblas_status_not_implemented;
}

rocblas_status rocblas_sgemm_batched_kernel_name(rocblas_handle    handle,
                                                 rocblas_operation trans_a,
                                                 rocblas_operation trans_b,
                                                 rocblas_int       m,
                                                 rocblas_int       n,
                                                 rocblas_int       k,
                                                 const float*      alpha,
                                                 const float*      A[],
                                                 rocblas_int       ld_a,
                                                 const float*      B[],
                                                 rocblas_int       ld_b,
                                                 const float*      beta,
                                                 float*            C[],
                                                 rocblas_int       ld_c,
                                                 rocblas_int       b_c)
{
    return rocblas_status_not_implemented;
}

rocblas_status rocblas_dgemm_batched_kernel_name(rocblas_handle    handle,
                                                 rocblas_operation trans_a,
                                                 rocblas_operation trans_b,
                                                 rocblas_int       m,
                                                 rocblas_int       n,
                                                 rocblas_int       k,
                                                 const double*     alpha,
                                                 const double*     A[],
                                                 rocblas_int       ld_a,
                                                 const double*     B[],
                                                 rocblas_int       ld_b,
                                                 const double*     beta,
                                                 double*           C[],
                                                 rocblas_int       ld_c,
                                                 rocblas_int       b_c)
{
    return rocblas_status_not_implemented;
}

} // extern "C"
