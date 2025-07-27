#pragma once
// #define USE_LIBXSMM
#ifdef USE_LIBXSMM
#include <aten/Linear.h>
#include <dyndisp/DispatchStub.h>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <unordered_map>
#include "csrc/cpu/tpp/kernels/TPPGEMMKrnl.h"
#include "csrc/cpu/tpp/woq/tla.h"
#include "utils.h"
#include "woq_defines.h"
#include "woq_dynamic_quant.h"
#include "woq_utils.h"

inline void ipex_quantize_reshape_64_cols(
    uint8_t* matrix_b,
    int8_t* matrix_o,
    int rows,
    int colsb,
    int8_t* zps,
    int qw_type,
    bool sym_quant_w,
    int32_t* compensate,
    int quant_a_mode) {
  __mmask64 mask = 0xFFFFFFFFFFFFFFFFULL;
  const __m512i REPEAT_4X_INDICES = _mm512_set_epi8(
      15,
      15,
      15,
      15,
      14,
      14,
      14,
      14,
      13,
      13,
      13,
      13,
      12,
      12,
      12,
      12,
      11,
      11,
      11,
      11,
      10,
      10,
      10,
      10,
      9,
      9,
      9,
      9,
      8,
      8,
      8,
      8,
      7,
      7,
      7,
      7,
      6,
      6,
      6,
      6,
      5,
      5,
      5,
      5,
      4,
      4,
      4,
      4,
      3,
      3,
      3,
      3,
      2,
      2,
      2,
      2,
      1,
      1,
      1,
      1,
      0,
      0,
      0,
      0);
  __m512i low_mask = _mm512_set1_epi8(0x0F);
  __m512i ones = _mm512_set1_epi32(0x01010101);
#pragma unroll
  for (int j = 0; j < colsb; j += 64) {
    int u = j / 64 * 32;
    __m512i compensate_vec1 = _mm512_setzero_epi32();
    __m512i compensate_vec2 = _mm512_setzero_epi32();
    // __m128i data1 = _mm_loadu_epi8(zps + u);
    // __m128i data2 = _mm_loadu_epi8(zps + u + 16);
    // __m512i broadcast1 = _mm512_broadcast_i32x4(data1);
    // __m512i broadcast2 = _mm512_broadcast_i32x4(data2);
    __m512i broadcast1 = _mm512_set1_epi64(*reinterpret_cast<long long*>(zps));
    __m512i broadcast1_2 =
        _mm512_set1_epi64(*reinterpret_cast<long long*>(zps + 8));
    __m512i broadcast2 =
        _mm512_set1_epi64(*reinterpret_cast<long long*>(zps + 16));
    __m512i broadcast2_2 =
        _mm512_set1_epi64(*reinterpret_cast<long long*>(zps + 24));
    broadcast1 = _mm512_unpacklo_epi64(broadcast1, broadcast1_2);
    broadcast2 = _mm512_unpacklo_epi64(broadcast2, broadcast2_2);
    // __m512i zps_vec1 = _mm512_permutexvar_epi8(REPEAT_4X_INDICES,
    // broadcast1);
    __m512i zps_vec1 = _mm512_shuffle_epi8(broadcast1, REPEAT_4X_INDICES);
    // __m512i zps_vec2 = _mm512_permutexvar_epi8(REPEAT_4X_INDICES,
    // broadcast2);
    __m512i zps_vec2 = _mm512_shuffle_epi8(broadcast2, REPEAT_4X_INDICES);
    for (int i = 0; i < rows; i += 4) {
      __m512i r0 = _mm512_loadu_epi8(matrix_b + i * colsb + j);
      __m512i r1 = _mm512_loadu_epi8(matrix_b + (i + 1) * colsb + j);
      __m512i r2 = _mm512_loadu_epi8(matrix_b + (i + 2) * colsb + j);
      __m512i r3 = _mm512_loadu_epi8(matrix_b + (i + 3) * colsb + j);
      // _mm512_mask_storeu_epi8(tmp, mask | mask1, r0);
      // print_buffer((uint8_t*)tmp, 1, 64);
      // __m512i high_bits = _mm512_and_si512(r0, _mm512_set1_epi8(0xF0));
      __m512i high_bits_0 = _mm512_srli_epi16(r0, 4);
      __m512i high_bits_1 = _mm512_srli_epi16(r1, 4);
      __m512i high_bits_2 = _mm512_srli_epi16(r2, 4);
      __m512i high_bits_3 = _mm512_srli_epi16(r3, 4);
      high_bits_0 = _mm512_and_si512(high_bits_0, low_mask);
      high_bits_1 = _mm512_and_si512(high_bits_1, low_mask);
      high_bits_2 = _mm512_and_si512(high_bits_2, low_mask);
      high_bits_3 = _mm512_and_si512(high_bits_3, low_mask);
      __m512i low_bits_0 = _mm512_and_si512(r0, low_mask);
      __m512i low_bits_1 = _mm512_and_si512(r1, low_mask);
      __m512i low_bits_2 = _mm512_and_si512(r2, low_mask);
      __m512i low_bits_3 = _mm512_and_si512(r3, low_mask);
      // _mm512_mask_storeu_epi8(tmp, mask | mask1, low_bits);
      // __m512i combine_low_0 =
      // _mm512_inserti64x4(_mm512_castsi256_si512(_mm512_extracti64x4_epi64(low_bits_0,
      // 0)), _mm512_extracti64x4_epi64(high_bits_0, 0), 1);
      // __m512i combine_high_0 =
      // _mm512_inserti64x4(_mm512_castsi256_si512(_mm512_extracti64x4_epi64(low_bits_0,
      // 1)), _mm512_extracti64x4_epi64(high_bits_0, 1), 1);
      // __m512i combine_low_1 =
      // _mm512_inserti64x4(_mm512_castsi256_si512(_mm512_extracti64x4_epi64(low_bits_1,
      // 0)), _mm512_extracti64x4_epi64(high_bits_1, 0), 1);
      // __m512i combine_high_1 =
      // _mm512_inserti64x4(_mm512_castsi256_si512(_mm512_extracti64x4_epi64(low_bits_1,
      // 1)), _mm512_extracti64x4_epi64(high_bits_1, 1), 1);
      // __m512i combine_low_2 =
      // _mm512_inserti64x4(_mm512_castsi256_si512(_mm512_extracti64x4_epi64(low_bits_2,
      // 0)), _mm512_extracti64x4_epi64(high_bits_2, 0), 1);
      // __m512i combine_high_2 =
      // _mm512_inserti64x4(_mm512_castsi256_si512(_mm512_extracti64x4_epi64(low_bits_2,
      // 1)), _mm512_extracti64x4_epi64(high_bits_2, 1), 1);
      // __m512i combine_low_3 =
      // _mm512_inserti64x4(_mm512_castsi256_si512(_mm512_extracti64x4_epi64(low_bits_3,
      // 0)), _mm512_extracti64x4_epi64(high_bits_3, 0), 1);
      // __m512i combine_high_3 =
      // _mm512_inserti64x4(_mm512_castsi256_si512(_mm512_extracti64x4_epi64(low_bits_3,
      // 1)), _mm512_extracti64x4_epi64(high_bits_3, 1), 1);
      __m512i combine_low_0 =
          _mm512_shuffle_i64x2(low_bits_0, high_bits_0, 0x44);
      __m512i combine_high_0 =
          _mm512_shuffle_i64x2(low_bits_0, high_bits_0, 0xEE);
      __m512i combine_low_1 =
          _mm512_shuffle_i64x2(low_bits_1, high_bits_1, 0x44);
      __m512i combine_high_1 =
          _mm512_shuffle_i64x2(low_bits_1, high_bits_1, 0xEE);
      __m512i combine_low_2 =
          _mm512_shuffle_i64x2(low_bits_2, high_bits_2, 0x44);
      __m512i combine_high_2 =
          _mm512_shuffle_i64x2(low_bits_2, high_bits_2, 0xEE);
      __m512i combine_low_3 =
          _mm512_shuffle_i64x2(low_bits_3, high_bits_3, 0x44);
      __m512i combine_high_3 =
          _mm512_shuffle_i64x2(low_bits_3, high_bits_3, 0xEE);
      // __m512i combine_low =
      // _mm512_or_si512(_mm512_castsi256_si512(_mm512_extracti64x4_epi64(low_bits,
      // 0)), _mm512_slli_epi16(high_bits, 256));
      // __m512i combine_high =
      // _mm512_or_si512(_mm512_castsi256_si512(_mm512_extracti64x4_epi64(low_bits,
      // 1)), _mm512_slli_epi16(high_bits, 256)); print_buffer(tmp, 1, 64); if
      // (qw_type == WOQ_DTYPE_NF4) {
      //     __m512i nf4_lut = _mm512_set_epi8(
      //         -127, -88, -67, -50, -36, -23, -12, 0,
      //         0, 10, 20, 31, 43, 56, 71, 92,
      //         -127, -88, -67, -50, -36, -23, -12, 0,
      //         0, 10, 20, 31, 43, 56, 71, 92,
      //         -127, -88, -67, -50, -36, -23, -12, 0,
      //         0, 10, 20, 31, 43, 56, 71, 92,
      //         -127, -88, -67, -50, -36, -23, -12, 0,
      //         0, 10, 20, 31, 43, 56, 71, 92
      //     );
      //     combine_high = _mm512_permutexvar_epi8(combine_high, nf4_lut);
      //     combine_low = _mm512_permutexvar_epi8(combine_low, nf4_lut);
      // } else if (qw_type == WOQ_DTYPE_INT4 && sym_quant_w) {
      //     // high_bits -= 8;
      //     // low_bits -= 8;
      //     __m512i zps_vec = _mm512_set1_epi8(8);
      //     combine_high -= zps_vec;
      //     combine_low -= zps_vec;
      // } else
      {
        // if (!sym_quant_w) {
        combine_low_0 = _mm512_sub_epi8(combine_low_0, zps_vec1);
        combine_high_0 = _mm512_sub_epi8(combine_high_0, zps_vec2);
        combine_low_1 = _mm512_sub_epi8(combine_low_1, zps_vec1);
        combine_high_1 = _mm512_sub_epi8(combine_high_1, zps_vec2);
        combine_low_2 = _mm512_sub_epi8(combine_low_2, zps_vec1);
        combine_high_2 = _mm512_sub_epi8(combine_high_2, zps_vec2);
        combine_low_3 = _mm512_sub_epi8(combine_low_3, zps_vec1);
        combine_high_3 = _mm512_sub_epi8(combine_high_3, zps_vec2);
        // }
      }
      // if (!sym_quant_w && is_asymmetric_quant_a(quant_a_mode)) {
      compensate_vec1 =
          _mm512_dpbusd_epi32(compensate_vec1, ones, combine_low_0);
      compensate_vec2 =
          _mm512_dpbusd_epi32(compensate_vec2, ones, combine_high_0);
      compensate_vec1 =
          _mm512_dpbusd_epi32(compensate_vec1, ones, combine_low_1);
      compensate_vec2 =
          _mm512_dpbusd_epi32(compensate_vec2, ones, combine_high_1);
      compensate_vec1 =
          _mm512_dpbusd_epi32(compensate_vec1, ones, combine_low_2);
      compensate_vec2 =
          _mm512_dpbusd_epi32(compensate_vec2, ones, combine_high_2);
      compensate_vec1 =
          _mm512_dpbusd_epi32(compensate_vec1, ones, combine_low_3);
      compensate_vec2 =
          _mm512_dpbusd_epi32(compensate_vec2, ones, combine_high_3);
      // }
      _mm512_storeu_epi8(matrix_o + i * colsb * 2 + j * 2, combine_low_0);
      _mm512_storeu_epi8(matrix_o + i * colsb * 2 + j * 2 + 64, combine_high_0);
      _mm512_storeu_epi8(matrix_o + (i + 1) * colsb * 2 + j * 2, combine_low_1);
      _mm512_storeu_epi8(
          matrix_o + (i + 1) * colsb * 2 + j * 2 + 64, combine_high_1);
      _mm512_storeu_epi8(matrix_o + (i + 2) * colsb * 2 + j * 2, combine_low_2);
      _mm512_storeu_epi8(
          matrix_o + (i + 2) * colsb * 2 + j * 2 + 64, combine_high_2);
      _mm512_storeu_epi8(matrix_o + (i + 3) * colsb * 2 + j * 2, combine_low_3);
      _mm512_storeu_epi8(
          matrix_o + (i + 3) * colsb * 2 + j * 2 + 64, combine_high_3);
    }
    _mm512_storeu_epi32(compensate + u, compensate_vec1);
    _mm512_storeu_epi32(compensate + u + 16, compensate_vec2);
  }
  // std::cerr << "**************" << std::endl;
}
namespace torch_ipex {
namespace cpu {
using namespace tpp;
using TensorList = std::vector<at::Tensor>;

static int SMALL_BATCH_THRESHOLD = env2int("IPEX_WOQ_SMALL_BATCH_THRESHOLD", 5);
constexpr long LOOP_K_UNROLL = 4; // TODO(jgong5): do not hard-code
constexpr long PREFETCH_K_DIST = 64; // TODO(jgong5): do not hard-code

namespace {

// Get mask for last column
// template <int EXPANDED_N, int col>
// constexpr inline unsigned short get_mask(unsigned short mask) {
//   // Not last column, return 0xffffff indicating load/store all 16 floats
//   if constexpr (col < EXPANDED_N / 16 - 1)
//     return (unsigned short)0xffff;
//   else
//     return mask;
// }
// template <int EXPANDED_N>
// constexpr inline unsigned short get_mask(int col, unsigned short mask) {
//   // Not last column, return 0xffffff indicating load/store all 16 floats
//   if (col < EXPANDED_N / 16 - 1)
//     return (unsigned short)0xffff;
//   else
//     return mask;
// }

// const int BLOCK_N = 64, BLOCK_K = 96, PREFETCH_K = 64;

struct DotMicroKernelKey {
  bool trans_a;
  bool trans_b;
  int lda;
  int ldb;
  int ldc;

  DotMicroKernelKey(bool trans_a, bool trans_b, int lda, int ldb, int ldc)
      : trans_a(trans_a), trans_b(trans_b), lda(lda), ldb(ldb), ldc(ldc) {}

  bool operator==(const DotMicroKernelKey& other) const {
    return trans_a == other.trans_a && trans_b == other.trans_b &&
        lda == other.lda && ldb == other.ldb && ldc == other.ldc;
  }
};

template <int BLOCK_M, int BLOCK_N, int BLOCK_K>
class DotMicroKernel {
 public:
  DotMicroKernel(bool trans_a, bool trans_b, int lda, int ldb, int ldc) {
    libxsmm_gemm_shape brshape = libxsmm_create_gemm_shape(
        BLOCK_M,
        BLOCK_N,
        BLOCK_K,
        lda,
        ldb,
        ldc,
        /*type A*/ LIBXSMM_DATATYPE_F32,
        /*type B*/ LIBXSMM_DATATYPE_F32,
        /*type C*/ LIBXSMM_DATATYPE_F32,
        /*acctype*/ LIBXSMM_DATATYPE_F32);
    libxsmm_bitfield brflags =
        (trans_a ? LIBXSMM_GEMM_FLAG_TRANS_A : LIBXSMM_GEMM_FLAG_NONE) |
        (trans_b ? LIBXSMM_GEMM_FLAG_TRANS_B : LIBXSMM_GEMM_FLAG_NONE);
    libxsmm_gemm_batch_reduce_config brconfig;
    std::fill_n(
        reinterpret_cast<char*>(&brconfig),
        sizeof(libxsmm_gemm_batch_reduce_config),
        0);
    brconfig.br_type = LIBXSMM_GEMM_BATCH_REDUCE_NONE;

    kernel_func_ = libxsmm_dispatch_brgemm_v2(
        brshape, brflags, /*prefetch_flags=*/0, brconfig);
    std::fill_n(
        reinterpret_cast<char*>(&gemm_param_), sizeof(libxsmm_gemm_param), 0);
  }

  void operator()(void* A, void* B, void* C) {
    gemm_param_.a.primary = (void*)A;
    gemm_param_.b.primary = (void*)B;
    gemm_param_.c.primary = (void*)C;
    kernel_func_(&gemm_param_);
  }

 private:
  libxsmm_gemmfunction kernel_func_;
  libxsmm_gemm_param gemm_param_;
};

template <int BLOCK_M, int BLOCK_N, int BLOCK_K>
using DotMicroKernelRef =
    std::shared_ptr<DotMicroKernel<BLOCK_M, BLOCK_N, BLOCK_K>>;

template <int BLOCK_M, int BLOCK_N, int BLOCK_K>
DotMicroKernelRef<BLOCK_M, BLOCK_N, BLOCK_K> create_or_get_dot_microkernel(
    bool trans_a,
    bool trans_b,
    int lda,
    int ldb,
    int ldc) {
  thread_local std::unordered_map<
      DotMicroKernelKey,
      DotMicroKernelRef<BLOCK_M, BLOCK_N, BLOCK_K>>
      cache;
  DotMicroKernelKey key(trans_a, trans_b, lda, ldb, ldc);
  auto search = cache.find(key);
  if (search != cache.end()) {
    return search->second;
  } else {
    cache.insert(
        {key,
         std::make_shared<DotMicroKernel<BLOCK_M, BLOCK_N, BLOCK_K>>(
             trans_a, trans_b, lda, ldb, ldc)});
    return cache[key];
  }
}
} // namespace

#if defined(CPU_CAPABILITY_AVX512)
// negate elements in a according to b's sign
static inline __m512i _mm512_sign_epi8(__m512i a, __m512i b) {
  __m512i zero = _mm512_setzero_si512();
  __mmask64 blt0 = _mm512_movepi8_mask(b);
  return _mm512_mask_sub_epi8(a, blt0, zero, a);
}
#endif

template <long N_GROUP_SIZE, bool sym_quant>
struct load_dequant_zp_only_4bit {
  template <typename LUT, typename VAT>
  static inline VAT call(uint8_t* p, LUT lut, VAT vzps) {
    TLA_ASSERT(false, "not implemented");
  }
};

template <bool sym_quant>
struct load_dequant_zp_only_4bit<64, sym_quant> {
// TODO(jgong5): further simplify the dequant intrinsics below with VecOps
#if defined(CPU_CAPABILITY_AVX512)
  static inline std::array<__m512, 4> call(
      uint8_t* p,
      __m512 lut,
      std::array<__m512, 4> vzps) {
    using T = float;
    using VA = VecArray<64, T>;
    using VAT = typename VA::type;
    constexpr long COLS = VA::num_vec;
    auto packed = _mm256_loadu_si256((__m256i*)p);
    __m512i int32[COLS];
    {
      auto low_4bit = _mm512_cvtepu8_epi32(_mm256_castsi256_si128(packed));
      auto high_4bit = _mm512_srli_epi32(low_4bit, 4);
      int32[0] = low_4bit;
      int32[2] = high_4bit;
    }
    {
      auto low_4bit = _mm512_cvtepu8_epi32(_mm256_extracti128_si256(packed, 1));
      auto high_4bit = _mm512_srli_epi32(low_4bit, 4);
      int32[1] = low_4bit;
      int32[3] = high_4bit;
    }
    VAT vbs;
    compile_time_for<COLS>::op([&](auto idx) {
      vbs[idx] = _mm512_permutexvar_ps(int32[idx], lut);
      if constexpr (!sym_quant) {
        vbs[idx] = _mm512_sub_ps(vbs[idx], vzps[idx]);
      }
    });
    return vbs;
  }
#endif

#if defined(CPU_CAPABILITY_AVX512_FP16)
  static inline std::array<__m512h, 2> call(
      uint8_t* p,
      __m512h lut,
      std::array<__m512h, 2> vzps) {
    using T = tpp::half;
    using VA = VecArray<64, T>;
    using VAT = typename VA::type;
    constexpr long COLS = VA::num_vec;
    auto packed = _mm256_loadu_si256((__m256i*)p);
    __m512i int32[COLS];
    {
      auto low_4bit = _mm512_cvtepu8_epi16(packed);
      auto high_4bit = _mm512_srli_epi16(low_4bit, 4);
      int32[0] = low_4bit;
      int32[1] = high_4bit;
    }
    VAT vbs;
    compile_time_for<COLS>::op([&](auto idx) {
      vbs[idx] = _mm512_permutexvar_ph(int32[idx], lut);
      if constexpr (!sym_quant) {
        vbs[idx] = _mm512_sub_ph(vbs[idx], vzps[idx]);
      }
    });
    return vbs;
  }
#endif
};

template <bool sym_quant>
struct load_dequant_zp_only_4bit<32, sym_quant> {
#if defined(CPU_CAPABILITY_AVX512)
  static inline std::array<__m512, 2> call(
      uint8_t* p,
      __m512 lut,
      std::array<__m512, 2> vzps) {
    using T = float;
    using VA = VecArray<32, T>;
    using VAT = typename VA::type;
    constexpr long COLS = VA::num_vec;
    auto packed = _mm_loadu_si128((__m128i*)p);
    __m512i int32[COLS];
    {
      auto low_4bit = _mm512_cvtepu8_epi32(packed);
      auto high_4bit = _mm512_srli_epi32(low_4bit, 4);
      int32[0] = low_4bit;
      int32[1] = high_4bit;
    }
    VAT vbs;
    compile_time_for<COLS>::op([&](auto idx) {
      vbs[idx] = _mm512_permutexvar_ps(int32[idx], lut);
      if constexpr (!sym_quant) {
        vbs[idx] = _mm512_sub_ps(vbs[idx], vzps[idx]);
      }
    });
    return vbs;
  }
#endif

#if defined(CPU_CAPABILITY_AVX512_FP16)
  static inline std::array<__m512h, 1> call(
      uint8_t* p,
      __m512h lut,
      std::array<__m512h, 1> vzps) {
    using T = tpp::half;
    using VA = VecArray<32, T>;
    using VAT = typename VA::type;
    constexpr long COLS = VA::num_vec;
    auto packed = _mm_loadu_si128((__m128i*)p);
    __m512i int32[COLS];
    {
      auto low_4bit = _mm256_cvtepu8_epi16(packed);
      auto high_4bit = _mm256_srli_epi16(low_4bit, 4);
      // combine low_4bit and high_4bit into __m512i
      int32[0] =
          _mm512_inserti64x4(_mm512_castsi256_si512(low_4bit), high_4bit, 1);
    }
    VAT vbs;
    compile_time_for<COLS>::op([&](auto idx) {
      vbs[idx] = _mm512_permutexvar_ph(int32[idx], lut);
      if constexpr (!sym_quant) {
        vbs[idx] = _mm512_sub_ph(vbs[idx], vzps[idx]);
      }
    });
    return vbs;
  }
#endif
};

template <bool sym_quant>
struct load_dequant_zp_only_4bit<16, sym_quant> {
#if defined(CPU_CAPABILITY_AVX512)
  static inline std::array<__m512, 1> call(
      uint8_t* p,
      __m512 lut,
      std::array<__m512, 1> vzps) {
    using T = float;
    using VA = VecArray<16, T>;
    using VAT = typename VA::type;
    constexpr long COLS = VA::num_vec;
    static_assert(COLS == 1, "COLS must be 1");
    uint64_t packed = reinterpret_cast<uint64_t*>(p)[0];
    uint64_t high = packed >> 4;
    __m128i packed_128 = _mm_set_epi64x(high, packed);
    __m512i int32 = _mm512_cvtepu8_epi32(packed_128);
    VAT vbs;
    vbs[0] = _mm512_permutexvar_ps(int32, lut);
    if constexpr (!sym_quant) {
      vbs[0] = _mm512_sub_ps(vbs[0], vzps[0]);
    }
    return vbs;
  }
#endif

#if defined(CPU_CAPABILITY_AVX512_FP16)
  static inline std::array<__m512h, 0> call(
      uint8_t* p,
      __m512h lut,
      std::array<__m512h, 0> vzps) {
    TLA_ASSERT(false, "not implemented");
  }
#endif
};

template <long N_GROUP_SIZE, bool sym_quant>
struct load_dequant_zp_only_int8 {
  template <typename VAT>
  static inline VAT call(uint8_t* p, VAT vzps) {
    TLA_ASSERT(false, "not implemented");
  }
};

template <bool sym_quant>
struct load_dequant_zp_only_int8<64, sym_quant> {
// TODO(jgong5): further simplify the dequant intrinsics below with VecOps
#if defined(CPU_CAPABILITY_AVX512)
  static inline std::array<__m512, 4> call(
      uint8_t* p,
      std::array<__m512, 4> vzps) {
    using T = float;
    using VA = VecArray<64, T>;
    using VAT = typename VA::type;
    constexpr long COLS = VA::num_vec;
    auto packed = _mm512_loadu_si512((__m512i*)p);
    VAT vbs;
    compile_time_for<COLS>::op([&](auto i) {
      constexpr long imm = i;
      auto int8 = _mm512_extracti32x4_epi32(packed, imm);
      vbs[i] = _mm512_cvtepi32_ps(_mm512_cvtepi8_epi32(int8));
      if constexpr (!sym_quant) {
        vbs[i] = _mm512_sub_ps(vbs[i], vzps[i]);
      }
    });
    return vbs;
  }
#endif

#if defined(CPU_CAPABILITY_AVX512_FP16)
  static inline std::array<__m512h, 2> call(
      uint8_t* p,
      std::array<__m512h, 2> vzps) {
    using T = tpp::half;
    using VA = VecArray<64, T>;
    using VAT = typename VA::type;
    constexpr long COLS = VA::num_vec;
    auto packed = _mm512_loadu_si512((__m512i*)p);
    VAT vbs;
    compile_time_for<COLS>::op([&](auto i) {
      constexpr long imm = i;
      auto int8 = _mm512_extracti64x4_epi64(packed, imm);
      vbs[i] = _mm512_cvtepi16_ph(_mm512_cvtepi8_epi16(int8));
      if constexpr (!sym_quant) {
        vbs[i] = _mm512_sub_ph(vbs[i], vzps[i]);
      }
    });
    return vbs;
  }
#endif
};

template <bool sym_quant>
struct load_dequant_zp_only_int8<32, sym_quant> {
#if defined(CPU_CAPABILITY_AVX512)
  static inline std::array<__m512, 2> call(
      uint8_t* p,
      std::array<__m512, 2> vzps) {
    using T = float;
    using VA = VecArray<32, T>;
    using VAT = typename VA::type;
    constexpr long COLS = VA::num_vec;
    auto packed = _mm256_loadu_si256((__m256i*)p);
    VAT vbs;
    compile_time_for<COLS>::op([&](auto i) {
      constexpr long imm = i;
      auto int8 = _mm256_extracti128_si256(packed, imm);
      vbs[i] = _mm512_cvtepi32_ps(_mm512_cvtepi8_epi32(int8));
      if constexpr (!sym_quant) {
        vbs[i] = _mm512_sub_ps(vbs[i], vzps[i]);
      }
    });
    return vbs;
  }
#endif

#if defined(CPU_CAPABILITY_AVX512_FP16)
  static inline std::array<__m512h, 1> call(
      uint8_t* p,
      std::array<__m512h, 1> vzps) {
    using T = tpp::half;
    using VA = VecArray<32, T>;
    using VAT = typename VA::type;
    constexpr long COLS = VA::num_vec;
    auto packed = _mm256_loadu_si256((__m256i*)p);
    VAT vbs;
    compile_time_for<COLS>::op([&](auto i) {
      constexpr long imm = i;
      vbs[i] = _mm512_cvtepi16_ph(_mm512_cvtepi8_epi16(packed));
      if constexpr (!sym_quant) {
        vbs[i] = _mm512_sub_ph(vbs[i], vzps[i]);
      }
    });
    return vbs;
  }
#endif
};

template <bool sym_quant>
struct load_dequant_zp_only_int8<16, sym_quant> {
#if defined(CPU_CAPABILITY_AVX512)
  static inline std::array<__m512, 1> call(
      uint8_t* p,
      std::array<__m512, 1> vzps) {
    using T = float;
    using VA = VecArray<16, T>;
    using VAT = typename VA::type;
    constexpr long COLS = VA::num_vec;
    static_assert(COLS == 1);
    auto packed = _mm_loadu_si128((__m128i*)p);
    VAT vbs;
    vbs[0] = _mm512_cvtepi32_ps(_mm512_cvtepi8_epi32(packed));
    if constexpr (!sym_quant) {
      vbs[0] = _mm512_sub_ps(vbs[0], vzps[0]);
    }
    return vbs;
  }
#endif

#if defined(CPU_CAPABILITY_AVX512_FP16)
  static inline std::array<__m512h, 0> call(
      uint8_t* p,
      std::array<__m512h, 0> vzps) {
    TLA_ASSERT(false, "not implemented");
  }
#endif
};

#if defined(CPU_CAPABILITY_AVX512)
inline __m512i combine_m256i(__m256i a, __m256i b) {
  __m512i c = _mm512_castsi256_si512(a);
  return _mm512_inserti64x4(c, b, 1);
}

inline __m512i combine_m256i(std::array<__m256i, 2> two_256) {
  return combine_m256i(two_256[0], two_256[1]);
}

inline std::array<__m256i, 2> load_zps_4vnni(int8_t* zps) {
  // broadcast 01234567 to
  // 01234567012345670123456701234567
  __m256i vzps_low = _mm256_set1_epi64x(*reinterpret_cast<long*>(zps));
  __m256i vzps_high = _mm256_set1_epi64x(*reinterpret_cast<long*>(zps + 8));
  // shuffle from
  // 01234567012345670123456701234567
  // to
  // 00001111222233334444555566667777
  __m256i shuffle_mask = _mm256_set_epi8(
      7,
      7,
      7,
      7,
      6,
      6,
      6,
      6,
      5,
      5,
      5,
      5,
      4,
      4,
      4,
      4,
      3,
      3,
      3,
      3,
      2,
      2,
      2,
      2,
      1,
      1,
      1,
      1,
      0,
      0,
      0,
      0);
  vzps_low = _mm256_shuffle_epi8(vzps_low, shuffle_mask);
  vzps_high = _mm256_shuffle_epi8(vzps_high, shuffle_mask);
  return {vzps_low, vzps_high};
}

// load unsigned int4
inline std::array<__m256i, 2> load_uint4_as_int8(uint8_t* qB) {
  __m256i packed = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(qB));
  const __m256i low_mask = _mm256_set1_epi8(0x0f);
  __m256i high = _mm256_srli_epi16(packed, 4);
  high = _mm256_and_si256(high, low_mask);
  __m256i low = _mm256_and_si256(packed, low_mask);
  return {low, high};
}

// load signed int4: load as unsigned int4 then minus 8
inline std::array<__m256i, 2> load_sint4_as_int8(uint8_t* qB) {
  __m256i packed = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(qB));
  const __m256i bias = _mm256_set1_epi8(0x8);
  const __m256i low_mask = _mm256_set1_epi8(0x0f);
  __m256i high = _mm256_srli_epi16(packed, 4);
  high = _mm256_and_si256(high, low_mask);
  high = _mm256_subs_epi8(high, bias);
  __m256i low = _mm256_and_si256(packed, low_mask);
  low = _mm256_subs_epi8(low, bias);
  return {low, high};
}

// load nf4
inline std::array<__m256i, 2> load_nf4_as_int8(uint8_t* qB) {
  __m256i packed = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(qB));
  const __m256i low_mask = _mm256_set1_epi8(0x0f);
  __m256i high = _mm256_srli_epi16(packed, 4);
  high = _mm256_and_si256(high, low_mask);
  __m256i low = _mm256_and_si256(packed, low_mask);
  const __m256i lut = _mm256_set_epi8(
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      127,
      92,
      71,
      56,
      43,
      31,
      20,
      10,
      0,
      -12,
      -23,
      -36,
      -50,
      -67,
      -88,
      -127);
  low = _mm256_permutexvar_epi8(low, lut);
  high = _mm256_permutexvar_epi8(high, lut);
  return {low, high};
}

#else
inline std::array<__m256i, 2> load_zps_4vnni(int8_t* zps) {
  TLA_ASSERT(false, "not implemented");
  return std::array<__m256i, 2>();
}

inline std::array<__m256i, 2> load_uint4_as_int8(uint8_t* qB) {
  TLA_ASSERT(false, "not implemented");
  return std::array<__m256i, 2>();
}

inline std::array<__m256i, 2> load_sint4_as_int8(uint8_t* qB) {
  TLA_ASSERT(false, "not implemented");
  return std::array<__m256i, 2>();
}

inline std::array<__m256i, 2> load_nf4_as_int8(uint8_t* qB) {
  TLA_ASSERT(false, "not implemented");
  return std::array<__m256i, 2>();
}

#endif

template <long N, bool sym_quant, typename T>
struct load_dequant_4bit {
  using VT = typename VecType<T>::type;
  using V = VecOps<VT>;
  using VA = VecArray<N, T>;
  using VAT = typename VA::type;
  constexpr static long COLS = VA::num_vec;

  static inline VAT call(uint8_t* p, VAT vscales, VT lut, VAT vzps) {
    auto vbs = load_dequant_zp_only_4bit<N, sym_quant>::call(p, lut, vzps);
    compile_time_for<COLS>::op(
        [&](auto idx) { vbs[idx] = V::mul(vbs[idx], vscales[idx]); });
    return vbs;
  }
};

template <long N, bool sym_quant, typename T>
struct load_dequant_int8 {
  using VT = typename VecType<T>::type;
  using V = VecOps<VT>;
  using VA = VecArray<N, T>;
  using VAT = typename VA::type;
  constexpr static long COLS = VA::num_vec;

  static inline VAT call(uint8_t* p, VAT vscales, VAT vzps) {
    auto vbs = load_dequant_zp_only_int8<N, sym_quant>::call(p, vzps);
    compile_time_for<COLS>::op(
        [&](auto idx) { vbs[idx] = V::mul(vbs[idx], vscales[idx]); });
    return vbs;
  }
};

constexpr int get_n_group_size(int N) {
  return N == 16 ? 16 : (N == 32 ? 32 : 64);
}

template <
    typename T,
    typename Tout,
    typename TScale,
    typename TZero,
    long M,
    long N,
    long ldb,
    bool transA = false,
    bool ACC = false,
    int quant_a_mode = -1,
    long PREFETCH_K_DIST = 0,
    typename Enabled = void>
struct GemmMicroKernel {
  template <int qw_type, bool sym_quant_w>
  static inline void call(
      long K,
      T* A,
      long lda,
      uint8_t* B,
      Tout* C,
      long ldc,
      TScale* scales,
      TZero* zps) {
    TLA_ASSERT(false, "Not implemented");
  }
};

template <
    typename T,
    long M,
    long N,
    long ldb,
    bool transA,
    bool ACC,
    int quant_a_mode,
    long PREFETCH_K_DIST>
struct GemmMicroKernel<
    T,
    T,
    T,
    T,
    M,
    N,
    ldb,
    transA,
    ACC,
    quant_a_mode,
    PREFETCH_K_DIST,
    typename std::enable_if_t<
        std::is_same<T, float>::value || std::is_same<T, half>::value>> {
  // TODO(jgong5): generalize this with pre/post op handlers
  template <int qw_type, bool sym_quant_w>
  static inline void call(
      long K,
      T* A,
      long lda,
      uint8_t* B,
      T* C,
      long ldc,
      T* scales,
      T* zps) {
    static_assert(N % 16 == 0, "N must be a multiple of 16");
    constexpr const int N_GROUP_SIZE = get_n_group_size(N);

    using VT = typename VecType<T>::type;
    using V = VecOps<VT>;
    using ST = typename V::ST;
    using VArray = VecArray<N_GROUP_SIZE, T>;
    using VArrayT = typename VArray::type;

    constexpr const int COLS = N / V::VLEN;
    constexpr const int CBLOCK = N_GROUP_SIZE / V::VLEN;
    constexpr const int CNBLOCKS = N / N_GROUP_SIZE;
    VT va[M];
    VArrayT vb[CNBLOCKS];
    VT vc[M * COLS];
    VArrayT vscales[CNBLOCKS];
    VArrayT vzps[CNBLOCKS];

    VT lut;
    constexpr bool is_4bit_flag = is_4bit(qw_type);
    if constexpr (is_4bit_flag) {
      if constexpr (qw_type == WOQ_DTYPE_NF4) {
        lut = V::set_nf4_lut();
      } else if constexpr (sym_quant_w) {
        lut = V::set_neg_8_to_7();
      } else {
        lut = V::set_0_to_15();
      }
    }

    // Load scales and zps
    compile_time_for<CNBLOCKS>::op([&](auto i) {
      constexpr const int col = i * CBLOCK;
      vscales[i] = VArray::load1d(scales + col * V::VLEN);
      if constexpr (!sym_quant_w) {
        vzps[i] = VArray::load1d(zps + col * V::VLEN);
      }
    });

    // NB: For fp16 in int8 woq, we do not delay the scale to the post-op but
    // leave it to the dequant otherwise the weight value might be too large to
    // overflow fp16 range.
    constexpr bool scale_as_post_op = !std::is_same<T, half>() || is_4bit_flag;

    compile_time_for<M * COLS>::op([&](auto i) { vc[i] = V::setzero(); });

    auto compute = [&](auto i, int k) {
      constexpr const int row = i / CNBLOCKS;
      constexpr const int cbidx = i % CNBLOCKS;

      if constexpr (cbidx == 0) {
        if constexpr (transA) {
          va[row] = V::set1(*(ST*)ADDRESS(A, k, row, lda));
        } else {
          va[row] = V::set1(*(ST*)ADDRESS(A, row, k, lda));
        }
      }

      if constexpr (row == 0) {
        constexpr const int col = cbidx * CBLOCK;
        if constexpr (scale_as_post_op) {
          if constexpr (is_4bit_flag) {
            vb[cbidx] =
                load_dequant_zp_only_4bit<N_GROUP_SIZE, sym_quant_w>::call(
                    ADDRESS(B, k, col * V::VLEN / 2, ldb / 2),
                    lut,
                    vzps[cbidx]);
          } else {
            vb[cbidx] =
                load_dequant_zp_only_int8<N_GROUP_SIZE, sym_quant_w>::call(
                    ADDRESS(B, k, col * V::VLEN, ldb), vzps[cbidx]);
          }
        } else {
          if constexpr (is_4bit_flag) {
            vb[cbidx] = load_dequant_4bit<N_GROUP_SIZE, sym_quant_w, T>::call(
                ADDRESS(B, k, col * V::VLEN / 2, ldb / 2),
                vscales[cbidx],
                lut,
                vzps[cbidx]);
          } else {
            vb[cbidx] = load_dequant_int8<N_GROUP_SIZE, sym_quant_w, T>::call(
                ADDRESS(B, k, col * V::VLEN, ldb), vscales[cbidx], vzps[cbidx]);
          }
        }
        if constexpr (PREFETCH_K_DIST > 0) {
          if constexpr (is_4bit_flag) {
            _mm_prefetch(
                ADDRESS(B, k + PREFETCH_K_DIST, col * V::VLEN / 2, ldb / 2),
                _MM_HINT_T0);
          } else {
            _mm_prefetch(
                ADDRESS(B, k + PREFETCH_K_DIST, col * V::VLEN, ldb),
                _MM_HINT_T0);
          }
        }
      }

      compile_time_for<CBLOCK>::op([&](auto col) {
        constexpr const int idx = INDEX(row, INDEX(cbidx, col, CBLOCK), COLS);
        vc[idx] = V::fmadd(va[row], vb[cbidx][col], vc[idx]);
      });
    };

    // Accumulate along k
    constexpr const int unroll = LOOP_K_UNROLL;
    int k = 0;
    for (; k < K / unroll; k++) {
      compile_time_for<unroll>::op([&](auto i) {
        compile_time_for<M * CNBLOCKS>::op(compute, k * unroll + i);
      });
    }
    k *= unroll;
    for (; k < K; k++) {
      compile_time_for<M * CNBLOCKS>::op(compute, k);
    }

    // Store to C
    auto store = [&](auto i) {
      constexpr const int row = i / COLS;
      constexpr const int col = i % COLS;
      if constexpr (ACC) {
        auto vc_old = V::loadu(ADDRESS(C, row, col * V::VLEN, ldc));
        if constexpr (scale_as_post_op) {
          vc[i] = V::fmadd(vscales[col / CBLOCK][col % CBLOCK], vc[i], vc_old);
        } else {
          vc[i] = V::fmadd(V::set1(1.0f), vc[i], vc_old);
        }
      } else if constexpr (scale_as_post_op) {
        vc[i] = V::mul(vscales[col / CBLOCK][col % CBLOCK], vc[i]);
      }
      V::storeu(ADDRESS(C, row, col * V::VLEN, ldc), vc[i]);
    };

    compile_time_for<M * COLS>::op(store);
  }
};

#if defined(CPU_CAPABILITY_AVX512_VNNI)
template <
    long M,
    long N,
    long ldb,
    bool transA,
    bool ACC,
    int quant_a_mode,
    long PREFETCH_K_DIST>
struct GemmMicroKernel<
    /*Tin*/ uint8_t,
    /*Tout*/ float,
    /*TScale*/ float,
    /*TZero*/ int8_t,
    M,
    N,
    ldb,
    transA,
    ACC,
    quant_a_mode,
    PREFETCH_K_DIST> {
  template <int qw_type, bool sym_quant_w>
  static inline void call(
      long K,
      uint8_t* A,
      long lda,
      uint8_t* B,
      float* C,
      long ldc,
      float* scales,
      int8_t* zps,
      float* scale_a,
      int32_t* zp_a,
      int32_t k_groups) {
    if constexpr (!sym_quant_w) {
      TLA_ASSERT(zps, "Zero points must be given for asymmetric quantization");
    }
    auto pqB = GetVLAPtr<uint8_t>(B, {ldb, 2}); // [K/4,N,4] packed in 4-bit

    static_assert(N % 16 == 0, "N must be a multiple of 16");
    constexpr const int COLS = N / 16;
    // std::clog << "Test11111114541" << std::endl;
    // printf("M: %d\n", M);
    __m512i ones = _mm512_set1_epi8(1); // used for computing compensation
    __m512i va;
    __m512i vb[COLS];
    __m512i vc[M * COLS];
    __m512 vscales[COLS];
    __m512i vzps[COLS];
    __m512i vcompensate[COLS];

    // Load scales and zps
    compile_time_for<COLS>::op([&](auto i) {
      vscales[i] = _mm512_loadu_ps(scales + i * 16);
      if constexpr (qw_type == WOQ_DTYPE_NF4) {
        const __m512 factor = _mm512_set1_ps(1.0f / 127.0f);
        vscales[i] = _mm512_mul_ps(vscales[i], factor);
      }
      // TODO(jgong5): should we use 512 or two 256 here?
      if constexpr (!sym_quant_w) {
        vzps[i] = combine_m256i(load_zps_4vnni(zps + i * 16));
      }
      if constexpr (is_asymmetric_quant_a(quant_a_mode)) {
        vcompensate[i] = _mm512_setzero_epi32();
      }
    });

    compile_time_for<M * COLS>::op(
        [&](auto i) { vc[i] = _mm512_setzero_epi32(); });

    auto compute = [&](auto i, int k) {
      constexpr const int row = i / COLS;
      constexpr const int col = i % COLS;

      if constexpr (col == 0) {
        if constexpr (transA) {
          va = _mm512_set1_epi32(*(int32_t*)ADDRESS(A, k, row, lda));
        } else {
          va = _mm512_set1_epi32(*(int32_t*)ADDRESS(A, row, k, lda));
        }
      }

      if constexpr (row == 0) {
        if constexpr (!sym_quant_w) {
          vb[col] = combine_m256i(load_uint4_as_int8(pqB[k / 4][col * 16]));
          vb[col] = _mm512_sub_epi8(vb[col], vzps[col]);
        } else if constexpr (qw_type == WOQ_DTYPE_INT4) {
          vb[col] = combine_m256i(load_sint4_as_int8(pqB[k / 4][col * 16]));
        } else {
          vb[col] = combine_m256i(load_nf4_as_int8(pqB[k / 4][col * 16]));
        }
        if constexpr (is_asymmetric_quant_a(quant_a_mode)) {
          vcompensate[col] =
              _mm512_dpbusd_epi32(vcompensate[col], ones, vb[col]);
        }
        if constexpr (PREFETCH_K_DIST > 0) {
          _mm_prefetch(pqB[(k + PREFETCH_K_DIST) / 4][col * 16], _MM_HINT_T0);
        }
      }
      if constexpr (is_asymmetric_quant_a(quant_a_mode)) {
        vc[i] = _mm512_dpbusd_epi32(vc[i], va, vb[col]);
      } else {
        auto vsb = _mm512_sign_epi8(vb[col], va);
        auto vabsa = _mm512_sign_epi8(va, va);
        vc[i] = _mm512_dpbusds_epi32(vc[i], vabsa, vsb);
      }
    };

    // Accumulate along k
    constexpr const int unroll = LOOP_K_UNROLL;
    int k = 0;
    for (; k < K / 4 / unroll; k++) {
      compile_time_for<unroll>::op([&](auto i) {
        compile_time_for<M * COLS>::op(compute, 4 * (k * unroll + i));
      });
    }
    k *= 4 * unroll;
    for (; k < K; k += 4) {
      compile_time_for<M * COLS>::op(compute, k);
    }

    // Store to C
    auto store = [&](auto i) {
      constexpr const int row = i / COLS;
      constexpr const int col = i % COLS;
      // compute (qC - compensate * zp_a) * scale_a * scale_b
      // where compensate = sum(qB)
      __m512 vc_float;
      if constexpr (
          quant_a_mode == QUANT_A_PER_TENSOR ||
          quant_a_mode == QUANT_A_PER_K_BLOCK ||
          quant_a_mode == QUANT_A_PER_TENSOR_SYM ||
          quant_a_mode == QUANT_A_PER_K_BLOCK_SYM) {
        if constexpr (
            quant_a_mode == QUANT_A_PER_TENSOR ||
            quant_a_mode == QUANT_A_PER_K_BLOCK) {
          vc[i] = _mm512_sub_epi32(
              vc[i],
              _mm512_mullo_epi32(vcompensate[col], _mm512_set1_epi32(*zp_a)));
        }
        vc_float = _mm512_cvtepi32_ps(vc[i]);
        vc_float = _mm512_mul_ps(vc_float, _mm512_set1_ps(*scale_a));
      } else if constexpr (
          quant_a_mode == QUANT_A_PER_M || quant_a_mode == QUANT_A_PER_M_SYM) {
        if constexpr (quant_a_mode == QUANT_A_PER_M) {
          vc[i] = _mm512_sub_epi32(
              vc[i],
              _mm512_mullo_epi32(
                  vcompensate[col], _mm512_set1_epi32(*(zp_a + row))));
        }
        vc_float = _mm512_cvtepi32_ps(vc[i]);
        vc_float = _mm512_mul_ps(vc_float, _mm512_set1_ps(*(scale_a + row)));
      } else {
        if constexpr (is_asymmetric_quant_a(quant_a_mode)) {
          vc[i] = _mm512_sub_epi32(
              vc[i],
              _mm512_mullo_epi32(
                  vcompensate[col],
                  _mm512_set1_epi32(*(zp_a + row * k_groups))));
        }
        vc_float = _mm512_cvtepi32_ps(vc[i]);
        vc_float = _mm512_mul_ps(
            vc_float, _mm512_set1_ps(*(scale_a + row * k_groups)));
      }

      vc_float = _mm512_mul_ps(vc_float, vscales[col]);
      if constexpr (ACC) {
        auto vc_old = _mm512_loadu_ps(C + row * ldc + col * 16);
        vc_float = _mm512_add_ps(vc_float, vc_old);
      }
      _mm512_storeu_ps(C + row * ldc + col * 16, vc_float);
    };
    compile_time_for<M * COLS>::op(store);
  }
};
#endif

// a dequant function the requires N to be a multiple of N_GROUP_SIZE
template <
    typename Tin,
    long ldb,
    long N_GROUP_SIZE,
    int qw_type,
    bool sym_quant_w>
struct dequant_n_grouped {
  template <typename Lambda1, typename Lambda2, typename Lambda3>
  static inline void call(
      uint8_t* qB,
      long K,
      long N,
      Tin* scales,
      Tin* zps,
      Tin* B,
      const Lambda1& load_qparam,
      const Lambda2& load_qint_as_fp,
      const Lambda3& store) {
    using VA = VecArray<N_GROUP_SIZE, Tin>;
    using VAT = typename VA::type;
    constexpr bool is_4bit_flag = is_4bit(qw_type);
    for (int n = 0; n < N; n += N_GROUP_SIZE) {
      // load scales and zps
      auto vscales = load_qparam(scales + n);
      VAT vzps;
      if constexpr (!sym_quant_w) {
        vzps = load_qparam(zps + n);
      }
      for (int k = 0; k < K; k++) {
        // load and dequant qB to vb
        auto vbs = load_qint_as_fp(
            is_4bit_flag ? &qB[k * ldb / 2 + n / 2] : &qB[k * ldb + n],
            vscales,
            vzps);
        // prefetch qB data
        if constexpr (PREFETCH_K_DIST > 0) {
          auto prefetch_addr = is_4bit_flag
              ? &qB[(k + PREFETCH_K_DIST) * ldb / 2 + n / 2]
              : &qB[(k + PREFETCH_K_DIST) * ldb + n];
          _mm_prefetch(prefetch_addr, _MM_HINT_T0);
        }
        // store vb to B
        store(B + k * N + n, vbs);
      }
    }
  }

  template <typename Lambda1, typename Lambda2, typename Lambda3>
  static inline void call_with_g_idx(
      uint8_t* qB,
      long K,
      long N,
      Tin* scales,
      Tin* zps,
      Tin* B,
      long k_start,
      int* g_idx,
      const Lambda1& load_qparam,
      const Lambda2& load_qint_as_fp,
      const Lambda3& store) {
    using VA = VecArray<N_GROUP_SIZE, Tin>;
    using VAT = typename VA::type;
    constexpr bool is_4bit_flag = is_4bit(qw_type);
    for (int n = 0; n < N; n += N_GROUP_SIZE) {
      for (int k = 0; k < K; k++) {
        // load scales and zps
        int g = g_idx[k_start + k];
        auto vscales = load_qparam(scales + g * N + n);
        VAT vzps;
        if constexpr (!sym_quant_w) {
          vzps = load_qparam(zps + g * N + n);
        }
        // load and dequant qB to vb
        auto vbs = load_qint_as_fp(
            is_4bit_flag ? &qB[k * ldb / 2 + n / 2] : &qB[k * ldb + n],
            vscales,
            vzps);
        // prefetch qB data
        if constexpr (PREFETCH_K_DIST > 0) {
          auto prefetch_addr = is_4bit_flag
              ? &qB[(k + PREFETCH_K_DIST) * ldb / 2 + n / 2]
              : &qB[(k + PREFETCH_K_DIST) * ldb + n];
          _mm_prefetch(prefetch_addr, _MM_HINT_T0);
        }
        // store vb to B
        store(B + k * N + n, vbs);
      }
    }
  }
};

#if defined(CPU_CAPABILITY_AVX512)
template <long ldb, long N_GROUP_SIZE, int qw_type, bool sym_quant_w>
struct dequant_n_grouped<bfloat16, ldb, N_GROUP_SIZE, qw_type, sym_quant_w> {
  // For vnni, we need to interleave the 2 bfloat16 values
  static inline std::array<__m512, 2> interleave(__m512 v0, __m512 v1) {
    __m512i idx_low = _mm512_set_epi32(
        0x17,
        0x07,
        0x16,
        0x06,
        0x15,
        0x05,
        0x14,
        0x04,
        0x13,
        0x03,
        0x12,
        0x02,
        0x11,
        0x01,
        0x10,
        0x00);
    __m512i idx_high = _mm512_set_epi32(
        0x1f,
        0x0f,
        0x1e,
        0x0e,
        0x1d,
        0x0d,
        0x1c,
        0x0c,
        0x1b,
        0x0b,
        0x1a,
        0x0a,
        0x19,
        0x09,
        0x18,
        0x08);
    return std::array<__m512, 2>(
        {_mm512_permutex2var_ps(v0, idx_low, v1),
         _mm512_permutex2var_ps(v0, idx_high, v1)});
  };

  template <typename Lambda1, typename Lambda2, typename Lambda3>
  static inline void call(
      uint8_t* qB,
      long K,
      long N,
      bfloat16* scales,
      bfloat16* zps,
      bfloat16* B,
      const Lambda1& load_qparam,
      const Lambda2& load_qint_as_fp,
      const Lambda3& store) {
    using VA = VecArray<N_GROUP_SIZE, float>;
    using VAT = typename VA::type;
    constexpr long COLS = VA::num_vec;
    constexpr bool is_4bit_flag = is_4bit(qw_type);

    for (int n = 0; n < N; n += N_GROUP_SIZE) {
      // load scales and zps
      auto vscales = load_qparam(scales + n);
      VAT vzps;
      if constexpr (!sym_quant_w) {
        vzps = load_qparam(zps + n);
      }
      // convert to vnni: [K/2, N, 2]
      for (int k = 0; k < K; k += 2) {
        // load and dequant qB to vb
        auto vbs_k0 = load_qint_as_fp(
            is_4bit_flag ? &qB[k * ldb / 2 + n / 2] : &qB[k * ldb + n],
            vscales,
            vzps);
        auto vbs_k1 = load_qint_as_fp(
            is_4bit_flag ? &qB[(k + 1) * ldb / 2 + n / 2]
                         : &qB[(k + 1) * ldb + n],
            vscales,
            vzps);
        // prefetch qB data
        if constexpr (PREFETCH_K_DIST > 0) {
          auto prefetch_addr = is_4bit_flag
              ? &qB[(k + PREFETCH_K_DIST) * ldb / 2 + n / 2]
              : &qB[(k + PREFETCH_K_DIST) * ldb + n];
          _mm_prefetch(prefetch_addr, _MM_HINT_T0);
        }
        typename VA::type vbs[2];
        compile_time_for<COLS>::op([&](auto i) {
          auto [low, high] = interleave(vbs_k0[i], vbs_k1[i]);
          vbs[i * 2 / COLS][i * 2 % COLS] = low;
          vbs[(i * 2 + 1) / COLS][(i * 2 + 1) % COLS] = high;
        });
        // store vb to B: low: [k + n*2 / N, n*2 % N], high: [k +
        // (n*2+N_GROUP_SIZE) / N, (n*2+N_GROUP_SIZE) % N]
        store(ADDRESS(B, k + (n * 2) / N, (n * 2) % N, N), vbs[0]);
        store(
            ADDRESS(
                B,
                k + (n * 2 + N_GROUP_SIZE) / N,
                (n * 2 + N_GROUP_SIZE) % N,
                N),
            vbs[1]);
      }
    }
  }

  template <typename Lambda1, typename Lambda2, typename Lambda3>
  static inline void call_with_g_idx(
      uint8_t* qB,
      long K,
      long N,
      bfloat16* scales,
      bfloat16* zps,
      bfloat16* B,
      long k_start,
      int* g_idx,
      const Lambda1& load_qparam,
      const Lambda2& load_qint_as_fp,
      const Lambda3& store) {
    using VA = VecArray<N_GROUP_SIZE, float>;
    using VAT = typename VA::type;
    constexpr long COLS = VA::num_vec;
    constexpr bool is_4bit_flag = is_4bit(qw_type);

    for (int n = 0; n < N; n += N_GROUP_SIZE) {
      // convert to vnni: [K/2, N, 2]
      for (int k = 0; k < K; k += 2) {
        // load scales and zps
        int g = g_idx[k_start + k];
        auto vscales = load_qparam(scales + g * N + n);
        VAT vzps;
        if constexpr (!sym_quant_w) {
          vzps = load_qparam(zps + g * N + n);
        }
        // load and dequant qB to vb
        auto vbs_k0 = load_qint_as_fp(
            is_4bit_flag ? &qB[k * ldb / 2 + n / 2] : &qB[k * ldb + n],
            vscales,
            vzps);
        g = g_idx[k_start + k + 1];
        vscales = load_qparam(scales + g * N + n);
        if constexpr (!sym_quant_w) {
          vzps = load_qparam(zps + g * N + n);
        }
        auto vbs_k1 = load_qint_as_fp(
            is_4bit_flag ? &qB[(k + 1) * ldb / 2 + n / 2]
                         : &qB[(k + 1) * ldb + n],
            vscales,
            vzps);
        // prefetch qB data
        if constexpr (PREFETCH_K_DIST > 0) {
          auto prefetch_addr = is_4bit_flag
              ? &qB[(k + PREFETCH_K_DIST) * ldb / 2 + n / 2]
              : &qB[(k + PREFETCH_K_DIST) * ldb + n];
          _mm_prefetch(prefetch_addr, _MM_HINT_T0);
        }
        typename VA::type vbs[2];
        compile_time_for<COLS>::op([&](auto i) {
          auto [low, high] = interleave(vbs_k0[i], vbs_k1[i]);
          vbs[i * 2 / COLS][i * 2 % COLS] = low;
          vbs[(i * 2 + 1) / COLS][(i * 2 + 1) % COLS] = high;
        });
        // store vb to B: low: [k + n*2 / N, n*2 % N], high: [k +
        // (n*2+N_GROUP_SIZE) / N, (n*2+N_GROUP_SIZE) % N]
        store(ADDRESS(B, k + (n * 2) / N, (n * 2) % N, N), vbs[0]);
        store(
            ADDRESS(
                B,
                k + (n * 2 + N_GROUP_SIZE) / N,
                (n * 2 + N_GROUP_SIZE) % N,
                N),
            vbs[1]);
      }
    }
  }
};
#endif

template <
    typename Tin,
    long ldb,
    long N_GROUP_SIZE,
    int qw_type,
    bool sym_quant_w,
    bool use_g_idx>
struct Dequantize {
  static void call(
      uint8_t* qB,
      long K,
      long N,
      Tin* scales,
      Tin* zps,
      Tin* B,
      int k_start = 0,
      int* g_idx = nullptr);
};

template <
    long ldb,
    long N_GROUP_SIZE,
    int qw_type,
    bool sym_quant_w,
    bool use_g_idx>
struct Dequantize<float, ldb, N_GROUP_SIZE, qw_type, sym_quant_w, use_g_idx> {
  static inline void call(
      uint8_t* qB,
      long K,
      long N,
      float* scales,
      float* zps,
      float* B,
      int k_start = 0,
      int* g_idx = nullptr) {
#if defined(__AVX512F__)
    using T = float;
    using VT = typename VecType<T>::type;
    using V = VecOps<VT>;
    using VA = VecArray<N_GROUP_SIZE, T>;
    constexpr int VLEN = VA::vec_ops::VLEN;
    constexpr long COLS = VA::num_vec;

    VT lut;
    constexpr bool is_4bit_flag = is_4bit(qw_type);
    if constexpr (is_4bit_flag) {
      if constexpr (qw_type == WOQ_DTYPE_NF4) {
        lut = V::set_nf4_lut();
      } else if constexpr (sym_quant_w) {
        lut = V::set_neg_8_to_7();
      } else {
        lut = V::set_0_to_15();
      }
    }

    auto load_qparam = [&](float* p) { return VA::load1d(p); };
    auto load_qint_as_fp = [&](uint8_t* p, auto vscales, auto vzps) {
      if constexpr (is_4bit_flag) {
        return load_dequant_4bit<N_GROUP_SIZE, sym_quant_w, T>::call(
            p, vscales, lut, vzps);
      } else {
        return load_dequant_int8<N_GROUP_SIZE, sym_quant_w, T>::call(
            p, vscales, vzps);
      }
    };
    auto store = [&](auto p, auto vbs) {
      compile_time_for<COLS>::op(
          [&](auto idx) { _mm512_storeu_ps(p + idx * VLEN, vbs[idx]); });
    };

    if constexpr (use_g_idx) {
      dequant_n_grouped<float, ldb, N_GROUP_SIZE, qw_type, sym_quant_w>::
          call_with_g_idx(
              qB,
              K,
              N,
              scales,
              zps,
              B,
              k_start,
              g_idx,
              load_qparam,
              load_qint_as_fp,
              store);
    } else {
      dequant_n_grouped<float, ldb, N_GROUP_SIZE, qw_type, sym_quant_w>::call(
          qB, K, N, scales, zps, B, load_qparam, load_qint_as_fp, store);
    }
#else
    TLA_ASSERT(false, "not implemented");
#endif
  }
};

template <
    long ldb,
    long N_GROUP_SIZE,
    int qw_type,
    bool sym_quant_w,
    bool use_g_idx>
struct Dequantize<
    bfloat16,
    ldb,
    N_GROUP_SIZE,
    qw_type,
    sym_quant_w,
    use_g_idx> {
  static inline void call(
      uint8_t* qB,
      long K,
      long N,
      bfloat16* scales,
      bfloat16* zps,
      bfloat16* B,
      int k_start = 0,
      int* g_idx = nullptr) {
#if defined(CPU_CAPABILITY_AVX512)
    using T = bfloat16;
    using VT = typename VecType<T>::type;
    using V = VecOps<VT>;
    using VA = VecArray<N_GROUP_SIZE, T>;
    constexpr long COLS = VA::num_vec;

    // lookup table converting uint8 to float, 15.0f - 0.0f
    // _mm512_permutexvar_ph needs 5 bits while we only need 4 bits, init the
    // table to honor the lower 4 bits regardless of the the highest bit, thus
    // saving an "and" op
    VT lut;
    constexpr bool is_4bit_flag = is_4bit(qw_type);
    if constexpr (is_4bit_flag) {
      if constexpr (qw_type == WOQ_DTYPE_NF4) {
        lut = V::set_nf4_lut();
      } else if constexpr (sym_quant_w) {
        lut = V::set_neg_8_to_7();
      } else {
        lut = V::set_0_to_15();
      }
    }

    auto load_qparam = [&](bfloat16* p) { return VA::load1d(p); };
    auto load_qint_as_fp = [&](uint8_t* p, auto vscales, auto vzps) {
      if constexpr (is_4bit_flag) {
        return load_dequant_4bit<N_GROUP_SIZE, sym_quant_w, float>::call(
            p, vscales, lut, vzps);
      } else {
        return load_dequant_int8<N_GROUP_SIZE, sym_quant_w, float>::call(
            p, vscales, vzps);
      }
    };
    auto store = [&](auto p, auto vbs) {
      compile_time_for<COLS / 2>::op([&](auto idx) {
        _vec_store_two_floats_as_bfloat16(
            p + idx * 32, vbs[idx * 2], vbs[idx * 2 + 1]);
      });
    };

    if constexpr (use_g_idx) {
      dequant_n_grouped<bfloat16, ldb, N_GROUP_SIZE, qw_type, sym_quant_w>::
          call_with_g_idx(
              qB,
              K,
              N,
              scales,
              zps,
              B,
              k_start,
              g_idx,
              load_qparam,
              load_qint_as_fp,
              store);
    } else {
      dequant_n_grouped<bfloat16, ldb, N_GROUP_SIZE, qw_type, sym_quant_w>::
          call(qB, K, N, scales, zps, B, load_qparam, load_qint_as_fp, store);
    }
#else
    TLA_ASSERT(false, "not implemented");
#endif
  }
};

template <
    long ldb,
    long N_GROUP_SIZE,
    int qw_type,
    bool sym_quant_w,
    bool use_g_idx>
struct Dequantize<half, ldb, N_GROUP_SIZE, qw_type, sym_quant_w, use_g_idx> {
  static inline void call(
      uint8_t* qB,
      long K,
      long N,
      half* scales,
      half* zps,
      half* B,
      int k_start = 0,
      int* g_idx = nullptr) {
#if defined(CPU_CAPABILITY_AVX512_FP16)
    using T = half;
    using VT = typename VecType<T>::type;
    using V = VecOps<VT>;
    using VA = VecArray<N_GROUP_SIZE, T>;
    constexpr int VLEN = VA::vec_ops::VLEN;
    constexpr long COLS = VA::num_vec;

    // lookup table converting uint8 to float, 15.0f - 0.0f
    // _mm512_permutexvar_ph needs 5 bits while we only need 4 bits, init the
    // table to honor the lower 4 bits regardless of the the highest bit, thus
    // saving an "and" op
    VT lut;
    constexpr bool is_4bit_flag = is_4bit(qw_type);
    if constexpr (is_4bit_flag) {
      if constexpr (qw_type == WOQ_DTYPE_NF4) {
        lut = V::set_nf4_lut();
      } else if constexpr (sym_quant_w) {
        lut = V::set_neg_8_to_7();
      } else {
        lut = V::set_0_to_15();
      }
    }

    auto load_qparam = [&](half* p) { return VA::load1d(p); };
    auto load_qint_as_fp = [&](uint8_t* p, auto vscales, auto vzps) {
      if constexpr (is_4bit_flag) {
        return load_dequant_4bit<N_GROUP_SIZE, sym_quant_w, T>::call(
            p, vscales, lut, vzps);
      } else {
        return load_dequant_int8<N_GROUP_SIZE, sym_quant_w, T>::call(
            p, vscales, vzps);
      }
    };
    auto store = [&](auto p, auto vbs) {
      compile_time_for<COLS>::op(
          [&](auto idx) { _mm512_storeu_ph(p + idx * VLEN, vbs[idx]); });
    };

    if constexpr (use_g_idx) {
      dequant_n_grouped<half, ldb, N_GROUP_SIZE, qw_type, sym_quant_w>::
          call_with_g_idx(
              qB,
              K,
              N,
              scales,
              zps,
              B,
              k_start,
              g_idx,
              load_qparam,
              load_qint_as_fp,
              store);
    } else {
      dequant_n_grouped<half, ldb, N_GROUP_SIZE, qw_type, sym_quant_w>::call(
          qB, K, N, scales, zps, B, load_qparam, load_qint_as_fp, store);
    }
#else
    TLA_ASSERT(false, "not implemented");
#endif
  }
};

template <long ldb, int qw_type, bool sym_quant_w>
struct Dequantize<
    int8_t,
    ldb,
    /*N_GROUP_SIZE*/ 16,
    qw_type,
    sym_quant_w,
    /*use_g_idx*/ false> {
  template <int quant_a_mode>
  static inline void call(
      uint8_t* qB,
      long K,
      long N,
      int8_t* zps,
      int8_t* B,
      int32_t* compensation) {
#if defined(CPU_CAPABILITY_AVX512_VNNI)
    auto pqB = GetVLAPtr<uint8_t>(qB, {ldb, 2}); // [K/4,N,4] packed in 4-bit
    auto pB = GetVLAPtr<int8_t>(B, {ldb, 4}); // [K/4,N,4]
    __m256i ones = _mm256_set1_epi8(1);
    for (int n = 0; n < N; n += 16) {
      __m256i vzps_low, vzps_high;
      if constexpr (!sym_quant_w) {
        auto [zps_low, zps_high] = load_zps_4vnni(&zps[n]);
        vzps_low = zps_low;
        vzps_high = zps_high;
      }
      __m256i vcompensate[2];
      if constexpr (is_asymmetric_quant_a(quant_a_mode)) {
        vcompensate[0] = _mm256_setzero_si256();
        vcompensate[1] = _mm256_setzero_si256();
      }
      // TODO(jgong5): unroll k?
      for (int k = 0; k < K / 4; k++) {
        // TODO(jgong5): consider optimize the instruction sequence below, e.g,
        // use avx512? load 64 (N:16, K:4) int4 values from qB
        __m256i vb_low, vb_high;
        if constexpr (!sym_quant_w) {
          auto [low, high] = load_uint4_as_int8(pqB[k][n]);
          vb_high = _mm256_sub_epi8(high, vzps_high);
          vb_low = _mm256_sub_epi8(low, vzps_low);
        } else if constexpr (qw_type == WOQ_DTYPE_INT4) {
          auto [low, high] = load_sint4_as_int8(pqB[k][n]);
          vb_low = low;
          vb_high = high;
        } else {
          auto [low, high] = load_nf4_as_int8(pqB[k][n]);
          vb_low = low;
          vb_high = high;
        }
        if constexpr (is_asymmetric_quant_a(quant_a_mode)) {
          vcompensate[0] = _mm256_dpbusd_epi32(vcompensate[0], ones, vb_low);
          vcompensate[1] = _mm256_dpbusd_epi32(vcompensate[1], ones, vb_high);
        }
        // store vb to B
        _mm256_storeu_si256(reinterpret_cast<__m256i_u*>(pB[k][n]), vb_low);
        _mm256_storeu_si256(
            reinterpret_cast<__m256i_u*>(pB[k][n + 8]), vb_high);
      }
      if constexpr (is_asymmetric_quant_a(quant_a_mode)) {
        _mm256_storeu_si256(
            reinterpret_cast<__m256i_u*>(&compensation[n]), vcompensate[0]);
        _mm256_storeu_si256(
            reinterpret_cast<__m256i_u*>(&compensation[n + 8]), vcompensate[1]);
      }
    }
#else
    TLA_ASSERT(false, "not implemented");
#endif
  }
};

// TODO(jgong5): move to tpp.h
template <
    typename Tin,
    typename Tout,
    typename TScale,
    typename TZero,
    long BLOCK_M,
    long N,
    long ldb,
    bool transA,
    bool ACC,
    int qw_type,
    int quant_a_mode,
    int quant_w_mode,
    long PREFETCH_K_DIST = 0,
    bool use_g_idx = false>
class DequantGemmTPP {
 public:
  DequantGemmTPP(
      long M,
      long K,
      long lda,
      long ldc,
      int unroll_hint = 1,
      long str_a = 1) {}

  void operator()(
      Tin* A,
      uint8_t* qB,
      TScale* scales,
      TZero* zps,
      Tout* C,
      bool no_tile_cfg = true,
      float* scale_a = nullptr,
      int32_t* zp_a = nullptr,
      int32_t k_groups = -1,
      int32_t count = 64,
      int kc_start = 0,
      int quant_block_multiple = 1,
      int* g_idx = nullptr) {
    TLA_ASSERT(false, "not implemented");
  }

  void config() {
    TLA_ASSERT(false, "not implemented");
  }

  void release() {
    TLA_ASSERT(false, "not implemented");
  }
};

template <
    typename Tin,
    typename Tout,
    long BLOCK_M,
    long N,
    long ldb,
    bool transA,
    bool ACC,
    int qw_type,
    int quant_a_mode,
    int quant_w_mode,
    long PREFETCH_K_DIST,
    bool use_g_idx>
class DequantGemmTPP<
    Tin,
    Tout,
    Tin,
    Tin,
    BLOCK_M,
    N,
    ldb,
    transA,
    ACC,
    qw_type,
    quant_a_mode,
    quant_w_mode,
    PREFETCH_K_DIST,
    use_g_idx> {
 public:
  DequantGemmTPP(
      long M,
      long K,
      long lda,
      long ldc,
      int unroll_hint = 1,
      long str_a = 1)
      : M(M), K(K), lda(lda), ldc(ldc) {
    static_assert(N % 16 == 0, "N must be a multiple of 16");
    if (std::is_same<Tin, bfloat16>())
      TLA_ASSERT(K % 2 == 0, "Kb must be a multiple of 2 for bfloat16");
    pgemm = new BrgemmTPP<Tin, Tout>(
        M,
        N,
        K,
        str_a,
        N * K,
        lda,
        ldb,
        ldc,
        ACC ? 1 : 0,
        transA,
        unroll_hint,
        /*b_vnni*/ std::is_same<Tin, bfloat16>());
    // pgemm = nullptr;
  }

  ~DequantGemmTPP() {
    if (pgemm) {
      delete pgemm;
    }
  }

  inline void operator()(
      Tin* A,
      uint8_t* qB,
      Tin* scales,
      Tin* zps,
      Tout* C,
      bool no_tile_cfg = true,
      float* scale_a = nullptr,
      int32_t* zp_a = nullptr,
      int32_t k_groups = -1,
      int32_t count = 64,
      int kc_start = 0,
      int quant_block_multiple = 1,
      int* g_idx = nullptr) {
    constexpr bool sym_quant_w = quant_w_mode >= QUANT_W_PER_CHANNEL_SYM;
    // if (M < SMALL_BATCH_THRESHOLD && g_idx == nullptr &&
    //     ((std::is_same<Tin, half>() && std::is_same<Tout, half>()) ||
    //      (std::is_same<Tin, float>() && std::is_same<Tout, float>()))) {
    for (long m = 0; m < M; m += BLOCK_M) {
      long block_m = std::min(M - m, BLOCK_M);
      enumerate_dispatcher<long, 4, BLOCK_M>::call(
          block_m,
          [&](auto i) {
            GemmMicroKernel<
                Tin,
                Tin,
                Tin,
                Tin,
                i,
                N,
                ldb,
                transA,
                ACC,
                quant_a_mode,
                PREFETCH_K_DIST>::
                template call<qw_type, sym_quant_w>(
                    K,
                    transA ? (Tin*)A + m : (Tin*)A + m * lda,
                    lda,
                    qB,
                    (Tin*)C + m * ldc,
                    ldc,
                    scales,
                    zps);
          },
          [&](auto i) {
            range_dispatcher<long, 1, BLOCK_M - 1>::call(
                i,
                [&](auto j) {
                  GemmMicroKernel<
                      Tin,
                      Tin,
                      Tin,
                      Tin,
                      j,
                      N,
                      ldb,
                      transA,
                      ACC,
                      quant_a_mode,
                      PREFETCH_K_DIST>::
                      template call<qw_type, sym_quant_w>(
                          K,
                          transA ? (Tin*)A + m : (Tin*)A + m * lda,
                          lda,
                          qB,
                          (Tin*)C + m * ldc,
                          ldc,
                          scales,
                          zps);
                },
                [&](auto j) { failing_fallback(); });
          });
    }
    // } else {
    //   constexpr const int N_GROUP_SIZE = get_n_group_size(N);
    //   Tin B[count][K][N];
    //   // TODO(jgong5): add prefetch
    //   for (int cnt = 0; cnt < count; cnt++) {
    //     int32_t quant_offset = quant_w_mode == QUANT_W_PER_CHANNEL ||
    //             quant_w_mode == QUANT_W_PER_CHANNEL_SYM || g_idx
    //         ? 0
    //         : (kc_start + cnt) / quant_block_multiple -
    //             kc_start / quant_block_multiple;
    //     Dequantize<Tin, ldb, N_GROUP_SIZE, qw_type, sym_quant_w, use_g_idx>::
    //         call(
    //             qB + K * N * cnt,
    //             K,
    //             N,
    //             scales + N * quant_offset,
    //             sym_quant_w ? nullptr : zps + N * quant_offset,
    //             B[cnt][0],
    //             (kc_start + cnt) * K,
    //             g_idx);
    //   }
    //   (*pgemm)(A, B[0][0], C, count, no_tile_cfg);
    // }
  }

  void config() {
    if (pgemm) {
      pgemm->config();
    }
  }

  void release() {
    if (pgemm) {
      pgemm->release();
    }
  }

 private:
  BrgemmTPP<Tin, Tout>* pgemm;
  long M;
  long K;
  long lda;
  long ldc;
};

template <
    long BLOCK_M,
    long N,
    long ldb,
    bool transA,
    bool ACC,
    int qw_type,
    int quant_a_mode,
    int quant_w_mode,
    long PREFETCH_K_DIST>
class DequantGemmTPP<
    /*Tin*/ uint8_t,
    /*Tout*/ float,
    /*TScale*/ float,
    /*TZero*/ int8_t,
    BLOCK_M,
    N,
    ldb,
    transA,
    ACC,
    qw_type,
    quant_a_mode,
    quant_w_mode,
    PREFETCH_K_DIST,
    /*use_g_idx*/ false> {
  using TBrgemmTPP = BrgemmTPP<int8_t, int32_t>;

 public:
  DequantGemmTPP(
      long M,
      long K,
      long lda,
      long ldc,
      int unroll_hint = 1,
      long str_a = 1)
      : M(M), K(K), lda(lda), ldc(ldc) {
    static_assert(N % 16 == 0, "N must be a multiple of 16");
    TLA_ASSERT(K % 4 == 0, "Kb must be a multiple of 4 for int8 VNNI");
    // TODO(jgong5): output fp32 directly
    // set is_sym_quant true if quant_a_mode is larger than
    // QUANT_A_PER_TENSOR_SYM
    constexpr bool is_sym_quant = quant_a_mode >= QUANT_A_PER_TENSOR_SYM;
    pgemm = new TBrgemmTPP(
        M > 16 ? 16 : M,
        N,
        K,
        K,
        K * N,
        lda,
        N,
        N,
        /*ACC*/ 0,
        /*transA*/ false,
        unroll_hint,
        /*b_vnni*/ true,
        is_sym_quant);
    // pgemm = nullptr;
  }

  ~DequantGemmTPP() {
    if (pgemm)
      delete pgemm;
  }

  inline void operator()(
      uint8_t* A,
      uint8_t* qB,
      float* scales,
      int8_t* zps,
      float* C,
      bool no_tile_cfg = true,
      float* scale_a = nullptr,
      int32_t* zp_a = nullptr,
      int32_t k_groups = -1,
      int32_t count = 1,
      int kc_start = 0,
      int quant_block_multiple = 1,
      int* g_idx = nullptr) {
    constexpr bool sym_quant_w = !is_asymmetric_quant_w(quant_w_mode);

    auto qA = GetVLAPtr<uint8_t>(A, {lda});
#if defined(CPU_CAPABILITY_AVX512_VNNI)
    if (M < SMALL_BATCH_THRESHOLD) {
      // printf("BLOCK_M %ld, M %ld\n", BLOCK_M, M);
      constexpr long PREFERRED_BLOCK_M =
          BLOCK_M * N / 16 >= 16 ? BLOCK_M / 2 : BLOCK_M;
      for (long m = 0; m < M; m += PREFERRED_BLOCK_M) {
        
        long block_m = std::min(M - m, PREFERRED_BLOCK_M);
        // printf("block_m %ld, m %ld\n", block_m, m);
        float* scale_a_m;
        int32_t* zp_a_m;
        if constexpr (
            quant_a_mode == QUANT_A_PER_M ||
            quant_a_mode == QUANT_A_PER_M_K_BLOCK ||
            quant_a_mode == QUANT_A_PER_M_SYM ||
            quant_a_mode == QUANT_A_PER_M_K_BLOCK_SYM) {
          scale_a_m = scale_a + m * k_groups;
          if constexpr (
              quant_a_mode == QUANT_A_PER_M ||
              quant_a_mode == QUANT_A_PER_M_K_BLOCK) {
            zp_a_m = zp_a + m * k_groups;
          }
        } else {
          scale_a_m = scale_a;
          if constexpr (
              quant_a_mode == QUANT_A_PER_K_BLOCK ||
              quant_a_mode == QUANT_A_PER_TENSOR) {
            zp_a_m = zp_a;
          }
        }
        enumerate_dispatcher<long, 4, PREFERRED_BLOCK_M>::call(
            block_m,
            [&](auto i) {
              GemmMicroKernel<
                  /*Tin*/ uint8_t,
                  /*Tout*/ float,
                  /*TScale*/ float,
                  /*TZero*/ int8_t,
                  /*M*/ i,
                  N,
                  ldb,
                  /*transA*/ false,
                  ACC,
                  quant_a_mode,
                  PREFETCH_K_DIST>::
                  template call<qw_type, sym_quant_w>(
                      K,
                      qA[m],
                      lda,
                      qB,
                      C + m * ldc,
                      ldc,
                      scales,
                      zps,
                      scale_a_m,
                      zp_a_m,
                      k_groups);
            },
            [&](auto i) {
              range_dispatcher<long, 1, PREFERRED_BLOCK_M - 1>::call(
                  i,
                  [&](auto j) {
                    GemmMicroKernel<
                        /*Tin*/ uint8_t,
                        /*Tout*/ float,
                        /*TScale*/ float,
                        /*TZero*/ int8_t,
                        /*M*/ j,
                        N,
                        ldb,
                        /*transA*/ false,
                        ACC,
                        quant_a_mode,
                        PREFETCH_K_DIST>::
                        template call<qw_type, sym_quant_w>(
                            K,
                            qA[m],
                            lda,
                            qB,
                            C + m * ldc,
                            ldc,
                            scales,
                            zps,
                            scale_a_m,
                            zp_a_m,
                            k_groups);
                   
                  },
                  [&](auto j) { failing_fallback(); });
            });
      }
    } else
#endif
    if (M < 16) {
      int8_t B[K / 4][N][4];
      int32_t qC[M][N];
      int32_t compensation[N];
      // int omp_id = omp_get_thread_num();
      // amx[omp_id].ipex_init(M, N, K);
      ipex_quantize_reshape_64_cols(
          qB,
          B[0][0],
          K / 4,
          N * 2,
          zps,
          qw_type,
          sym_quant_w,
          compensation,
          quant_a_mode);
      // for (long m = 0; m < M - 15; m += 16) {
        // long m_end = m + 16;
        // if (M != pre_M){
        //   // printf("m_block_size %ld, m %ld, AMX selected\n", m_end - m, m);
        //   pre_M = M;
        // }
        
      (*pgemm)((int8_t*)qA[0], B[0][0], qC[0], 1, no_tile_cfg);

      if constexpr (PREFETCH_K_DIST > 0) {
        _mm_prefetch(qB + N * K / 2, _MM_HINT_T0);
        _mm_prefetch(A + K, _MM_HINT_T0);
      }
      // post-op and convert back to C
      auto convert_one_row = [&](long m) {
#pragma omp simd
        for (long n = 0; n < N; ++n) {
          float* scale_a_m;
          int32_t* zp_a_m;
          if constexpr (
              quant_a_mode == QUANT_A_PER_M ||
              quant_a_mode == QUANT_A_PER_M_K_BLOCK ||
              quant_a_mode == QUANT_A_PER_M_SYM ||
              quant_a_mode == QUANT_A_PER_M_K_BLOCK_SYM) {
            scale_a_m = scale_a + m * k_groups;
            if constexpr (is_asymmetric_quant_a(quant_a_mode)) {
              zp_a_m = zp_a + m * k_groups;
            }
          } else {
            scale_a_m = scale_a;
            if constexpr (is_asymmetric_quant_a(quant_a_mode)) {
              zp_a_m = zp_a;
            }
          }
          float c = 0;
          auto scale = scales[n];
          if constexpr (qw_type == WOQ_DTYPE_NF4) {
            scale *= (1.0f / 127.0f);
          }
          if constexpr (is_asymmetric_quant_a(quant_a_mode)) {
            c = (qC[m][n] - compensation[n] * (*zp_a_m)) * (*scale_a_m) * scale;
          } else {
            c = (qC[m][n]) * (*scale_a_m) * scale;
          }
          if constexpr (ACC) {
            C[m * ldc + n] += c;
          } else {
            C[m * ldc + n] = c;
          }
        }
      };
      long m_idx = 0;
      for (; m_idx < M - 4; m_idx += 4) {
        convert_one_row(m_idx);
        convert_one_row(m_idx + 1);
        convert_one_row(m_idx + 2);
        convert_one_row(m_idx + 3);
      }
      for (; m_idx < M; ++m_idx) {
        convert_one_row(m_idx);
      }
    } else 
    {
      int8_t B[K / 4][N][4];
      int32_t qC[M][N];
      int32_t compensation[N];
      // int omp_id = omp_get_thread_num();
      // amx[omp_id].ipex_init(M, N, K);
      ipex_quantize_reshape_64_cols(
          qB,
          B[0][0],
          K / 4,
          N * 2,
          zps,
          qw_type,
          sym_quant_w,
          compensation,
          quant_a_mode);
      // for (long m = 0; m < M - 15; m += 16) {
        // long m_end = m + 16;
        // if (M != pre_M){
        //   // printf("m_block_size %ld, m %ld, AMX selected\n", m_end - m, m);
        //   pre_M = M;
        // }
        

      int8_t* Bp = B[0][0];
      int32_t* Cp = qC[0];
      for (int i = 0; i < M - 15; i += 16) {
        for (int j = 0; j < N; j += 32) {
          _tile_zero(6);
          _tile_zero(7);
          for (int k = 0; k < K; k += 128) {
            _tile_loadd(2, Bp + k * N + j * 4, N * 4);
            _tile_loadd(3, Bp + k * N + (j + 16) * 4, N * 4);
            _tile_loadd(4, Bp + (k + 64) * N + j * 4, N * 4);
            _tile_loadd(5, Bp + (k + 64) * N + (j + 16) * 4, N * 4);
            _tile_loadd(0, (uint8_t*)(qA[i]) + k, lda);
            _tile_loadd(1, (uint8_t*)(qA[i]) + k + 64, lda);

            _tile_dpbusd(6, 0, 2);
            _tile_dpbusd(6, 1, 4);
            _tile_dpbusd(7, 0, 3);
            _tile_dpbusd(7, 1, 5);
          }
          _tile_stored(6, Cp + i * N + j, N * 4);
          _tile_stored(7, Cp + i * N + (j + 16), N * 4);
        }
      }
      // }
      long m_start = 16 * (M / 16);
      long m_end = M;
      if (m_start != m_end){
        long m_end_start = m_end - 16 < 0 ? 0 : m_end - 16;
        for (int i = m_end_start; i < m_end; i += 16) {
          for (int j = 0; j < N; j += 32) {
            _tile_zero(6);
            _tile_zero(7);
            for (int k = 0; k < K; k += 128) {
              _tile_loadd(2, Bp + k * N + j * 4, N * 4);
              _tile_loadd(3, Bp + k * N + (j + 16) * 4, N * 4);
              _tile_loadd(4, Bp + (k + 64) * N + j * 4, N * 4);
              _tile_loadd(5, Bp + (k + 64) * N + (j + 16) * 4, N * 4);
              _tile_loadd(0, (uint8_t*)(qA[i]) + k, lda);
              _tile_loadd(1, (uint8_t*)(qA[i]) + k + 64, lda);

              _tile_dpbusd(6, 0, 2);
              _tile_dpbusd(6, 1, 4);
              _tile_dpbusd(7, 0, 3);
              _tile_dpbusd(7, 1, 5);
            }
            _tile_stored(6, Cp + i * N + j, N * 4);
            _tile_stored(7, Cp + i * N + (j + 16), N * 4);
          }
        }
      }
      if constexpr (PREFETCH_K_DIST > 0) {
        _mm_prefetch(qB + N * K / 2, _MM_HINT_T0);
        _mm_prefetch(A + K, _MM_HINT_T0);
      }
      // post-op and convert back to C
      auto convert_one_row = [&](long m) {
#pragma omp simd
        for (long n = 0; n < N; ++n) {
          float* scale_a_m;
          int32_t* zp_a_m;
          if constexpr (
              quant_a_mode == QUANT_A_PER_M ||
              quant_a_mode == QUANT_A_PER_M_K_BLOCK ||
              quant_a_mode == QUANT_A_PER_M_SYM ||
              quant_a_mode == QUANT_A_PER_M_K_BLOCK_SYM) {
            scale_a_m = scale_a + m * k_groups;
            if constexpr (is_asymmetric_quant_a(quant_a_mode)) {
              zp_a_m = zp_a + m * k_groups;
            }
          } else {
            scale_a_m = scale_a;
            if constexpr (is_asymmetric_quant_a(quant_a_mode)) {
              zp_a_m = zp_a;
            }
          }
          float c = 0;
          auto scale = scales[n];
          if constexpr (qw_type == WOQ_DTYPE_NF4) {
            scale *= (1.0f / 127.0f);
          }
          if constexpr (is_asymmetric_quant_a(quant_a_mode)) {
            c = (qC[m][n] - compensation[n] * (*zp_a_m)) * (*scale_a_m) * scale;
          } else {
            c = (qC[m][n]) * (*scale_a_m) * scale;
          }
          if constexpr (ACC) {
            C[m * ldc + n] += c;
          } else {
            C[m * ldc + n] = c;
          }
        }
      };
      long m_idx = 0;
      for (; m_idx < M - 4; m_idx += 4) {
        convert_one_row(m_idx);
        convert_one_row(m_idx + 1);
        convert_one_row(m_idx + 2);
        convert_one_row(m_idx + 3);
      }
      for (; m_idx < M; ++m_idx) {
        convert_one_row(m_idx);
      }
    }
  }

  void config() {
    if (pgemm) {
      pgemm->config();
    }
  }

  void release() {
    if (pgemm) {
      pgemm->release();
    }
  }

 private:
  TBrgemmTPP* pgemm;
  long M;
  long K;
  long lda;
  long ldc;
};

template <
    long BLOCK_M,
    long N,
    long ldb,
    bool transA,
    bool ACC,
    int quant_a_mode,
    int quant_w_mode,
    long PREFETCH_K_DIST>
class NoDequantGemmTPP {
  using TBrgemmTPP = BrgemmTPP<int8_t, int32_t>;

 public:
  NoDequantGemmTPP(
      long M,
      long K,
      long lda,
      long ldc,
      int unroll_hint = 1,
      long str_a = 1)
      : M(M), K(K), lda(lda), ldc(ldc) {
    static_assert(N % 16 == 0, "N must be a multiple of 16");
    TLA_ASSERT(K % 4 == 0, "Kb must be a multiple of 4 for int8 VNNI");
    // set is_sym_quant true if quant_a_mode is larger than
    // QUANT_A_PER_TENSOR_SYM
    constexpr bool is_sym_quant = quant_a_mode >= QUANT_A_PER_TENSOR_SYM;
    pgemm = new TBrgemmTPP(
        M,
        N,
        K,
        K,
        K * N,
        lda,
        N,
        N,
        /*ACC*/ 0,
        /*transA*/ false,
        unroll_hint,
        /*b_vnni*/ true,
        is_sym_quant);
    // pgemm = nullptr;
  }

  ~NoDequantGemmTPP() {
    if (pgemm)
      delete pgemm;
  }

  inline void operator()(
      uint8_t* A,
      int8_t* B,
      int32_t* compensation,
      float* scales,
      int8_t* zps,
      float* C,
      bool no_tile_cfg = true,
      float* scale_a = nullptr,
      int32_t* zp_a = nullptr,
      int32_t k_groups = -1,
      int32_t count = 1,
      int kc_start = 0,
      int quant_block_multiple = 1) {
    auto qA = GetVLAPtr<uint8_t>(A, {lda});
    int32_t qC[M][N];
    (*pgemm)((int8_t*)qA[0], B, qC[0], 1, no_tile_cfg);
    if constexpr (PREFETCH_K_DIST > 0) {
      _mm_prefetch(B + N * K, _MM_HINT_T0);
      _mm_prefetch(A + K, _MM_HINT_T0);
    }
    // post-op and convert back to C
    auto convert_one_row = [&](long m) {
#pragma omp simd
      for (long n = 0; n < N; ++n) {
        float* scale_a_m;
        int32_t* zp_a_m;
        if constexpr (
            quant_a_mode == QUANT_A_PER_M ||
            quant_a_mode == QUANT_A_PER_M_K_BLOCK ||
            quant_a_mode == QUANT_A_PER_M_SYM ||
            quant_a_mode == QUANT_A_PER_M_K_BLOCK_SYM) {
          scale_a_m = scale_a + m * k_groups;
          if constexpr (is_asymmetric_quant_a(quant_a_mode)) {
            zp_a_m = zp_a + m * k_groups;
          }
        } else {
          scale_a_m = scale_a;
          if constexpr (is_asymmetric_quant_a(quant_a_mode)) {
            zp_a_m = zp_a;
          }
        }
        float c = 0;
        if constexpr (is_asymmetric_quant_a(quant_a_mode)) {
          c = (qC[m][n] - compensation[n] * (*zp_a_m)) * (*scale_a_m) *
              scales[n];
        } else {
          c = (qC[m][n]) * (*scale_a_m) * scales[n];
        }
        if constexpr (ACC) {
          C[m * ldc + n] += c;
        } else {
          C[m * ldc + n] = c;
        }
      }
    };
    long m = 0;
    for (; m < M - 4; m += 4) {
      convert_one_row(m);
      convert_one_row(m + 1);
      convert_one_row(m + 2);
      convert_one_row(m + 3);
    }
    for (; m < M; ++m) {
      convert_one_row(m);
    }
  }

  void config() {
    if (pgemm) {
      pgemm->config();
    }
  }

  void release() {
    if (pgemm) {
      pgemm->release();
    }
  }

 private:
  TBrgemmTPP* pgemm;
  long M;
  long K;
  long lda;
  long ldc;
};

static int IPEX_KCB_BLOCK_SIZE = env2int("IPEX_KCB_BLOCK_SIZE", 64);
#define DEQUANT_UPFRONT_THRESHOLD 1024
#define PARALLEL_M_THRESHOLD 128

// Compared to qlinear_woq_affine_impl,
// this function dequantize weight upfront before gemm to improve the
// performance for the first token.
template <
    typename T,
    typename TComp,
    typename TGemmOut,
    typename Tout,
    typename TScale,
    typename TZero,
    int quant_a_mode = -1,
    int quant_w_mode = 0>
void qlinear_woq_affine_dequant_upfront_impl(
    const at::Tensor& x,
    const at::Tensor& qw_packed,
    const at::Tensor& scales, // dtype is TComp
    const at::Tensor& b, // dtype is TGemmOut
    at::Tensor y,
    const int qw_type,
    int fusion_type,
    const TensorList& others_list,
    int64_t quant_block_k,
    const at::Tensor& zps = at::Tensor(), // dtype is TComp
    const c10::optional<at::Tensor>& g_idx = c10::nullopt) {
  const bool asym_quant_w = is_asymmetric_quant_w(quant_w_mode);
  auto x_sizes = x.sizes();
  auto w_sizes = qw_packed.sizes();
  auto Nc = w_sizes[0];
  auto Nb = w_sizes[3];
  auto Kc = w_sizes[1];
  auto Kb = w_sizes[2];
  auto N = Nc * Nb;
  auto K = Kc * Kb;
  TLA_ASSERT(
      !(std::is_same<T, uint8_t>() || std::is_same<TComp, uint8_t>()),
      "WOQ dequant upfront does not support uint8_t computation");

  auto quant_block_multiple = quant_block_k == 0 ? 1 : quant_block_k / Kb;
  auto quant_k_blocks =
      quant_block_k == 0 ? 1 : (K + quant_block_k - 1) / quant_block_k;
  int scales_kc = quant_w_mode == QUANT_W_PER_CHANNEL ||
          quant_w_mode == QUANT_W_PER_CHANNEL_SYM
      ? 1
      : quant_k_blocks;

  auto pw = GetVLAPtr<uint8_t>((uint8_t*)qw_packed.data_ptr(), {Kc, Kb * Nb});
  auto ldy = N;

  torch::Tensor dqw =
      torch::empty({Nc, Kc, Kb, Nb}, c10::CppTypeToScalarType<TComp>::value);
  int b_vnni = 0;
  if (std::is_same<TComp, bfloat16>()) {
    // reshape to VNNI format
    dqw = dqw.view({Nc, Kc, Kb / 2, Nb, 2});
    b_vnni = 1;
  }
  auto dqw_ptr = GetVLAPtr<TComp>(dqw, {Kc, Kb, Nb});
  auto tin0 = others_list.size() > 0
      ? others_list[0].to(c10::CppTypeToScalarType<T>::value)
      : at::Tensor{};
  auto tin1 = others_list.size() > 1
      ? others_list[1].to(c10::CppTypeToScalarType<T>::value)
      : at::Tensor{};
  auto pscales = GetVLAPtr<TComp>(scales, {scales_kc, Nb});
  auto pzps = asym_quant_w ? GetVLAPtr<TComp>(zps, {scales_kc, Nb})
                           : GetVLAPtr<TComp>(nullptr, {1, 1});
  product_dispatcher<
      std::tuple</*qw_type*/ int, /*BLOCK_N*/ long, /*use g_idx*/ bool>,
      std::tuple<
          enumerate_dispatcher<
              int,
              WOQ_DTYPE_INT8,
              WOQ_DTYPE_INT4,
              WOQ_DTYPE_NF4>,
          enumerate_dispatcher<long, WOQ_N_BLOCK_SIZE>,
          enumerate_dispatcher<bool, false, true>>>::
      call(
          std::make_tuple(qw_type, Nb, g_idx.has_value()),
          [&](auto tuple) {
            auto qw_type_ = std::get<0>(tuple);
            auto block_n = std::get<1>(tuple);
            auto use_g_idx = std::get<2>(tuple);
            auto loop_scheme = "bA";
            auto dequant_loop =
                ThreadedLoop<2>({{Nc}, {0, Kc, Kc}}, loop_scheme);
            constexpr const int N_GROUP_SIZE = get_n_group_size(block_n);
            constexpr bool sym_quant_w = !is_asymmetric_quant_w(quant_w_mode);
            int* g_idx_ptr =
                use_g_idx ? g_idx.value().data_ptr<int>() : nullptr;
            dequant_loop(
                [&](int* idx) {
                  int nc = idx[0];
                  int kc_start = idx[1];
                  int kc_end = kc_start + Kc;
                  for (int kc = kc_start; kc < kc_end; kc++) {
                    int32_t k_groups = -1;
                    int32_t quant_offset = kc / quant_block_multiple;
                    TScale* scale_w = nullptr;
                    TZero* zp_w = nullptr;
                    if constexpr (
                        quant_w_mode == QUANT_W_PER_CHANNEL ||
                        quant_w_mode == QUANT_W_PER_CHANNEL_SYM || use_g_idx) {
                      scale_w = pscales[nc][0];
                      if constexpr (asym_quant_w) {
                        zp_w = pzps[nc][0];
                      }
                    } else {
                      scale_w = pscales[nc][quant_offset];
                      if constexpr (asym_quant_w) {
                        zp_w = pzps[nc][quant_offset];
                      }
                    }
                    int k_start = use_g_idx ? kc * Kb : 0;
                    Dequantize<
                        TComp,
                        block_n,
                        N_GROUP_SIZE,
                        qw_type_,
                        sym_quant_w,
                        use_g_idx>::
                        call(
                            pw[nc][kc],
                            Kb,
                            block_n,
                            scale_w,
                            zp_w,
                            dqw_ptr[nc][kc][0],
                            k_start,
                            g_idx_ptr);
                  }
                },
                [&]() {},
                [&]() {});

            auto x_reshaped = x.dim() == 3 ? x : x.view({x_sizes[0], 1, K});
            auto M = y.numel() / y.size(-1);
            auto block_m = 64L;
            auto rem = M % block_m;
            auto ldy = N;
            if (Nc % 4 == 0) {
              Nc /= 4;
              Nb *= 4;
            } else if (Nc % 2 == 0) {
              Nc /= 2;
              Nb *= 2;
            }
            auto gelu_fwd_tpp_ptr = fusion_type == WOQ_FUSE_GELU_ERF
                ? std::make_shared<GeluFwdTPP<T>>(
                      GeluFwdTPP<T>(block_m, Nb, ldy, ldy))
                : nullptr;
            auto gelu_fwd_tpp_rem_ptr = fusion_type == WOQ_FUSE_GELU_ERF
                ? std::make_shared<GeluFwdTPP<T>>(
                      GeluFwdTPP<T>(rem, Nb, ldy, ldy))
                : nullptr;
            bool has_add_post_op =
                fusion_type == WOQ_FUSE_ADD || fusion_type == WOQ_FUSE_ADD_ADD;
            auto add_tpp_ptr = has_add_post_op
                ? std::make_shared<AddTPP<T, T>>(
                      AddTPP<T, T>(block_m, Nb, ldy, ldy))
                : nullptr;
            auto add_tpp_rem_ptr = has_add_post_op
                ? std::make_shared<AddTPP<T, T>>(
                      AddTPP<T, T>(rem, Nb, ldy, ldy))
                : nullptr;
            auto gelu_tanh_fwd_tpp_ptr = fusion_type == WOQ_FUSE_GELU_TANH
                ? std::make_shared<GeluTanhFwdTPP<T>>(
                      GeluTanhFwdTPP<T>(block_m, Nb, ldy, ldy))
                : nullptr;
            auto gelu_tanh_fwd_tpp_rem_ptr = fusion_type == WOQ_FUSE_GELU_TANH
                ? std::make_shared<GeluTanhFwdTPP<T>>(
                      GeluTanhFwdTPP<T>(rem, Nb, ldy, ldy))
                : nullptr;
            auto relu_fwd_tpp_ptr = fusion_type == WOQ_FUSE_RELU
                ? std::make_shared<ReLUFwdTPP<T>>(
                      ReLUFwdTPP<T>(block_m, Nb, ldy, ldy, false))
                : nullptr;
            auto relu_fwd_tpp_rem_ptr = fusion_type == WOQ_FUSE_RELU
                ? std::make_shared<ReLUFwdTPP<T>>(
                      ReLUFwdTPP<T>(rem, Nb, ldy, ldy, false))
                : nullptr;
            auto silu_fwd_tpp_ptr = fusion_type == WOQ_FUSE_SILU
                ? std::make_shared<SiLUFwdTPP<T>>(
                      SiLUFwdTPP<T>(block_m, Nb, ldy, ldy))
                : nullptr;
            auto silu_fwd_tpp_rem_ptr = fusion_type == WOQ_FUSE_SILU
                ? std::make_shared<SiLUFwdTPP<T>>(
                      SiLUFwdTPP<T>(rem, Nb, ldy, ldy))
                : nullptr;
            auto mul_tpp_ptr = fusion_type == WOQ_FUSE_MUL
                ? std::make_shared<MulTPP<T>>(MulTPP<T>(block_m, Nb, ldy, ldy))
                : nullptr;
            auto mul_tpp_rem_ptr = fusion_type == WOQ_FUSE_MUL
                ? std::make_shared<MulTPP<T>>(MulTPP<T>(rem, Nb, ldy, ldy))
                : nullptr;
            auto in0_ptr = GetVLAPtr<T>(tin0, {Nc, Nb});
            auto in1_ptr = GetVLAPtr<T>(tin1, {Nc, Nb});

            // For add/add_add, using aten add is faster than TPP fused kernel
            auto tpp_linear_with_post_op = [&](at::Tensor& in,
                                               at::Tensor& out,
                                               int fuse_type = 0) {
              if (fuse_type == WOQ_FUSE_GELU_ERF) {
                tpp_linear_gelu<TComp, Tout>(in, dqw, b, out, b_vnni);
              } else if (fuse_type == WOQ_FUSE_GELU_TANH) {
                tpp_linear_gelu_tanh<TComp, Tout>(in, dqw, b, out, b_vnni);
              } else if (fuse_type == WOQ_FUSE_RELU) {
                tpp_linear_relu<TComp, Tout>(in, dqw, b, out, b_vnni);
              } else if (fuse_type == WOQ_FUSE_SILU) {
                tpp_linear_silu<TComp, Tout>(in, dqw, b, out, b_vnni);
              } else if (fuse_type == WOQ_FUSE_MUL) {
                tpp_linear_mul<TComp, Tout>(in, tin0, dqw, b, out, b_vnni);
              } else if (
                  fuse_type == WOQ_FUSE_ADD || fuse_type == WOQ_FUSE_ADD_ADD) {
                TLA_ASSERT(
                    false,
                    "fuse_type should not be ADD or ADD_ADD since it's slower than aten add");
              } else {
                tpp_linear_bias<TComp, TGemmOut>(in, dqw, b, out, b_vnni);
              }
            };

            // Maybe convert x to TComp and then call tpp_linear
            // We can only call tpp linear with post op when Tout == TGemmOut
            // Otherwise, we need to convert the output to Tout then apply post
            // op ourselves
            auto maybe_cvt_x_and_compute = [&](at::Tensor& y,
                                               int fuse_type = 0) {
              if constexpr (!std::is_same<T, TComp>()) {
                auto x_comp = at::empty(
                    x_reshaped.sizes(),
                    x_reshaped.options().dtype(
                        c10::CppTypeToScalarType<TComp>::value));
                auto cvt_x_tpp = ConvertTPP<T, TComp>(block_m, Kb, K, K);
                auto cvt_x_rem_tpp = ConvertTPP<T, TComp>(rem, Kb, K, K);
                auto cvt_loop = torch_ipex::tpp::ThreadedLoop<2>(
                    {{0, M, block_m}, {Kc}}, "AB");
                auto in_ptr = GetVLAPtr<T>(x, {Kc, Kb});
                auto out_ptr = GetVLAPtr<TComp>(x_comp, {Kc, Kb});
                cvt_loop([&](int* ind) {
                  int m = ind[0], kc = ind[1];
                  if (m + block_m <= M) {
                    cvt_x_tpp(in_ptr[m][kc], out_ptr[m][kc]);
                  } else {
                    cvt_x_rem_tpp(in_ptr[m][kc], out_ptr[m][kc]);
                  }
                });
                tpp_linear_with_post_op(x_comp, y, fuse_type);
              } else {
                tpp_linear_with_post_op(x_reshaped, y, fuse_type);
              }
            };

            // If Tout != TGemmOut, such as the lowp-mode=bf16 case, we need a
            // buffer for output
            if constexpr (!std::is_same<Tout, TGemmOut>()) {
              auto y_gemm = at::empty(
                  {M, y.size(-1)},
                  y.options().dtype(c10::CppTypeToScalarType<TGemmOut>::value));
              maybe_cvt_x_and_compute(y_gemm);
              auto cvt_y_tpp =
                  ConvertTPP<TGemmOut, Tout>(block_m, Nb, ldy, ldy);
              auto cvt_y_rem_tpp =
                  ConvertTPP<TGemmOut, Tout>(rem, Nb, ldy, ldy);
              auto post_loop = torch_ipex::tpp::ThreadedLoop<2>(
                  {{0, M, block_m}, {Nc}}, "AB");
              auto in_ptr = GetVLAPtr<TGemmOut>(y_gemm, {Nc, Nb});
              auto out_ptr = GetVLAPtr<Tout>(y, {Nc, Nb});
              // Convert y to T and handle post ops
              if (fusion_type == WOQ_FUSE_NONE) {
                post_loop([&](int* ind) {
                  int m = ind[0], nc = ind[1];
                  if (m + block_m <= M) {
                    cvt_y_tpp(in_ptr[m][nc], out_ptr[m][nc]);
                  } else {
                    cvt_y_rem_tpp(in_ptr[m][nc], out_ptr[m][nc]);
                  }
                });
              } else if (fusion_type == WOQ_FUSE_GELU_ERF) {
                post_loop([&](int* ind) {
                  int m = ind[0], nc = ind[1];
                  if (m + block_m <= M) {
                    cvt_y_tpp(in_ptr[m][nc], out_ptr[m][nc]);
                    (*gelu_fwd_tpp_ptr)(out_ptr[m][nc], out_ptr[m][nc]);
                  } else {
                    cvt_y_rem_tpp(in_ptr[m][nc], out_ptr[m][nc]);
                    (*gelu_fwd_tpp_rem_ptr)(out_ptr[m][nc], out_ptr[m][nc]);
                  }
                });
              } else if (fusion_type == WOQ_FUSE_GELU_TANH) {
                post_loop([&](int* ind) {
                  int m = ind[0], nc = ind[1];
                  if (m + block_m <= M) {
                    cvt_y_tpp(in_ptr[m][nc], out_ptr[m][nc]);
                    (*gelu_tanh_fwd_tpp_ptr)(out_ptr[m][nc], out_ptr[m][nc]);
                  } else {
                    cvt_y_rem_tpp(in_ptr[m][nc], out_ptr[m][nc]);
                    (*gelu_tanh_fwd_tpp_rem_ptr)(
                        out_ptr[m][nc], out_ptr[m][nc]);
                  }
                });
              } else if (fusion_type == WOQ_FUSE_RELU) {
                post_loop([&](int* ind) {
                  int m = ind[0], nc = ind[1];
                  if (m + block_m <= M) {
                    cvt_y_tpp(in_ptr[m][nc], out_ptr[m][nc]);
                    (*relu_fwd_tpp_ptr)(out_ptr[m][nc], out_ptr[m][nc]);
                  } else {
                    cvt_y_rem_tpp(in_ptr[m][nc], out_ptr[m][nc]);
                    (*relu_fwd_tpp_rem_ptr)(out_ptr[m][nc], out_ptr[m][nc]);
                  }
                });
              } else if (fusion_type == WOQ_FUSE_SILU) {
                post_loop([&](int* ind) {
                  int m = ind[0], nc = ind[1];
                  if (m + block_m <= M) {
                    cvt_y_tpp(in_ptr[m][nc], out_ptr[m][nc]);
                    (*silu_fwd_tpp_ptr)(out_ptr[m][nc], out_ptr[m][nc]);
                  } else {
                    cvt_y_rem_tpp(in_ptr[m][nc], out_ptr[m][nc]);
                    (*silu_fwd_tpp_rem_ptr)(out_ptr[m][nc], out_ptr[m][nc]);
                  }
                });
              } else if (fusion_type == WOQ_FUSE_ADD) {
                post_loop([&](int* ind) {
                  int m = ind[0], nc = ind[1];
                  if (m + block_m <= M) {
                    cvt_y_tpp(in_ptr[m][nc], out_ptr[m][nc]);
                    (*add_tpp_ptr)(
                        out_ptr[m][nc], in0_ptr[m][nc], out_ptr[m][nc]);
                  } else {
                    cvt_y_rem_tpp(in_ptr[m][nc], out_ptr[m][nc]);
                    (*add_tpp_rem_ptr)(
                        out_ptr[m][nc], in0_ptr[m][nc], out_ptr[m][nc]);
                  }
                });
              } else if (fusion_type == WOQ_FUSE_ADD_ADD) {
                post_loop([&](int* ind) {
                  int m = ind[0], nc = ind[1];
                  if (m + block_m <= M) {
                    cvt_y_tpp(in_ptr[m][nc], out_ptr[m][nc]);
                    (*add_tpp_ptr)(
                        out_ptr[m][nc], in0_ptr[m][nc], out_ptr[m][nc]);
                    (*add_tpp_ptr)(
                        out_ptr[m][nc], in1_ptr[m][nc], out_ptr[m][nc]);
                  } else {
                    cvt_y_rem_tpp(in_ptr[m][nc], out_ptr[m][nc]);
                    (*add_tpp_rem_ptr)(
                        out_ptr[m][nc], in0_ptr[m][nc], out_ptr[m][nc]);
                    (*add_tpp_rem_ptr)(
                        out_ptr[m][nc], in1_ptr[m][nc], out_ptr[m][nc]);
                  }
                });
              } else if (fusion_type == WOQ_FUSE_MUL) {
                post_loop([&](int* ind) {
                  int m = ind[0], nc = ind[1];
                  if (m + block_m <= M) {
                    cvt_y_tpp(in_ptr[m][nc], out_ptr[m][nc]);
                    (*mul_tpp_ptr)(
                        out_ptr[m][nc], in0_ptr[m][nc], out_ptr[m][nc]);
                  } else {
                    cvt_y_rem_tpp(in_ptr[m][nc], out_ptr[m][nc]);
                    (*mul_tpp_rem_ptr)(
                        out_ptr[m][nc], in0_ptr[m][nc], out_ptr[m][nc]);
                  }
                });
              }
            } else { // Tout == TGemmOut
              // For add/add_add, using aten add is faster than TPP fused kernel
              if (fusion_type == WOQ_FUSE_ADD ||
                  fusion_type == WOQ_FUSE_ADD_ADD) {
                maybe_cvt_x_and_compute(y, 0);
                for (auto& tin : others_list) {
                  y.add_(tin.view(y.sizes()));
                }
              } else {
                maybe_cvt_x_and_compute(y, fusion_type);
              }
            }
          },
          [](auto tuple) { failing_fallback(); });
}

// If T != TComp
//   T -> TComp -> GEMM -> TComp -> bias/PostOp -> Tout
// If T == TComp (we can save intermediate output buffer and schedule M/N/K
// loops together)
//   T -> GEMM -> T -> bias/PostOp -> Tout
template <
    typename T,
    typename TComp,
    typename TGemmOut,
    typename Tout,
    typename TScale,
    typename TZero,
    int quant_a_mode = -1,
    int quant_w_mode = 0>
void qlinear_woq_affine_impl(
    const at::Tensor& x,
    const at::Tensor& qw_packed,
    const at::Tensor& scales, // dtype is TComp
    const at::Tensor& b, // dtype is TComp
    at::Tensor y,
    const int qw_type,
    int k_splits,
    int fusion_type,
    const TensorList& others_list,
    int64_t quant_block_k,
    const at::Tensor& zps = at::Tensor(), // dtype is TComp
    float* scales_a_ptr = nullptr,
    int32_t* zps_a_ptr = nullptr,
    const c10::optional<at::Tensor>& compensation = c10::nullopt,
    const c10::optional<at::Tensor>& g_idx = c10::nullopt) {
  const bool is_4bit_flag = is_4bit(qw_type);
  constexpr bool asym_quant_w = is_asymmetric_quant_w(quant_w_mode);
  bool no_dequant_weight = compensation.has_value();
  if (no_dequant_weight) {
    TLA_ASSERT(
        (std::is_same<TComp, uint8_t>() && k_splits <= 1),
        "WOQ: TComp should be uint8 and k_splits should be <= 1 if no_dequant_weight is true");
  }
  constexpr bool is_int8_linear =
      (std::is_same<T, int8_t>() || std::is_same<T, uint8_t>()) &&
      std::is_same<TComp, uint8_t>();
  auto w_sizes = qw_packed.sizes();
  auto Nc = w_sizes[0];
  auto Nb = (is_4bit_flag && !no_dequant_weight) ? w_sizes[3] * 2 : w_sizes[3];
  auto Kc = w_sizes[1];
  auto Kb = w_sizes[2];
  auto N = Nc * Nb;
  auto K = Kc * Kb;
  auto M = x.numel() / K;
  assert(quant_block_k % Kb == 0);
  auto quant_block_multiple = quant_block_k == 0 ? 1 : quant_block_k / Kb;
  auto quant_k_blocks =
      quant_block_k == 0 ? 1 : (K + quant_block_k - 1) / quant_block_k;

  TLA_ASSERT(Nb % 16 == 0, "Nb must be a multiple of 16");

  // For first token with large M, go to the dequant upfront path
  // Now it only supports INT8 weight
  if constexpr (!std::is_same<TComp, uint8_t>()) {
    if (M >= DEQUANT_UPFRONT_THRESHOLD && !is_4bit_flag) {
      qlinear_woq_affine_dequant_upfront_impl<
          T,
          TComp,
          TGemmOut,
          Tout,
          TScale,
          TZero,
          quant_a_mode,
          quant_w_mode>(
          x,
          qw_packed,
          scales,
          b,
          y,
          qw_type,
          fusion_type,
          others_list,
          quant_block_k,
          zps,
          g_idx);
      return;
    }
  } else {
    TLA_ASSERT(
        !g_idx.has_value(), "WOQ: g_idx is not supported for int8 computation");
  }

  // select BLOCK_M according to M
  // TODO(jgong5): improve the heuristic
  auto BLOCK_M = [&]() -> long {
    if (M <= 48) {
      return M;
    } else if (M < 64) {
      return 32;
    } else if (M < 96) {
      return 48;
    } else {
      return 64;
    }
  }();

  auto BLOCK_M_rem = M % BLOCK_M;

  // TODO(jgong5): use heuristics to decide k_splits
  if (k_splits <= 0 || M >= 32 || BLOCK_M_rem) {
    k_splits = 1;
  }
  TLA_ASSERT(Kc % k_splits == 0, "Kc must be a multiple of k_splits");
  TLA_ASSERT(
      !(std::is_same<T, uint8_t>()) || (std::is_same<T, TComp>()),
      "T must be TComp if T is uint8_t");

  bool no_x_buf = std::is_same<T, TComp>() || std::is_same<T, int8_t>();
  bool no_y_buf = std::is_same<T, TComp>() && std::is_same<Tout, TGemmOut>() &&
      k_splits == 1;

  auto lda = no_x_buf ? K : Kb;
  auto ldy = N;
  auto ldc = (no_y_buf || k_splits > 1) ? ldy : Nb;
  auto str_a = no_x_buf == true ? Kb : BLOCK_M * Kb;
  auto Kcb = Kc;
  if (M < PARALLEL_M_THRESHOLD) {
    Kcb = 1;
  } else if (
      is_4bit_flag || !std::is_same<T, TComp>() ||
      std::is_same<TComp, uint8_t>()) {
    Kcb = 1;
  } else if (M >= PARALLEL_M_THRESHOLD) {
    Kcb = IPEX_KCB_BLOCK_SIZE;
  }
  auto px = GetVLAPtr<T>(x, {Kc, Kb});
  auto pw = GetVLAPtr<uint8_t>(
      (uint8_t*)qw_packed.data_ptr(),
      {Kc, Kb * ((is_4bit_flag && !no_dequant_weight) ? Nb / 2 : Nb)});
  auto py = GetVLAPtr<Tout>(y, {Nc, Nb}); /*[M, Nc, Nb]*/
  int scales_kc = quant_w_mode == QUANT_W_PER_CHANNEL ||
          quant_w_mode == QUANT_W_PER_CHANNEL_SYM
      ? 1
      : quant_k_blocks;
  if (g_idx.has_value()) {
    TLA_ASSERT(
        scales.dim() == 3,
        "scale is expected to be a 3D tensor if g_idx is used");
    scales_kc = scales.sizes()[1];
  }
  auto pscales = GetVLAPtr<TScale>(scales, {scales_kc, Nb});
  auto pzps = asym_quant_w ? GetVLAPtr<TZero>(zps, {scales_kc, Nb})
                           : GetVLAPtr<TZero>(nullptr, {1, 1});
  auto pb = GetVLAPtr<TGemmOut>(b, {Nb});
  auto tin0 = others_list.size() > 0 ? others_list[0] : at::Tensor{};
  auto pin0 = GetVLAPtr<Tout>(tin0, {Nc, Nb}); /*[M, Nc, Nb]*/
  auto tin1 = others_list.size() > 1 ? others_list[1] : at::Tensor{};
  auto pin1 = GetVLAPtr<Tout>(tin1, {Nc, Nb}); /*[M, Nc, Nb]*/
  auto pcomp = no_dequant_weight
      ? GetVLAPtr<int32_t>(compensation.value(), {Kc, Nb})
      : /*[Nc, Kc, Nb]*/
      GetVLAPtr<int32_t>(nullptr, {1, 1});
  auto g_idx_ptr = g_idx.has_value() ? g_idx.value().data_ptr<int>() : nullptr;

  auto copy_bias_out_tpp = CpyBiasTPP<TGemmOut>(BLOCK_M, Nb, ldy);
  auto copy_bias_buf_tpp = CpyBiasTPP<TGemmOut>(BLOCK_M, Nb, Nb);
  auto copy_bias_out_rem_tpp = CpyBiasTPP<TGemmOut>(BLOCK_M_rem, Nb, ldy);
  auto copy_bias_buf_rem_tpp = CpyBiasTPP<TGemmOut>(BLOCK_M_rem, Nb, Nb);
  auto zero_out_tpp = SetZeroTPP<TGemmOut>(BLOCK_M, Nb, ldy);
  auto zero_buf_tpp = SetZeroTPP<TGemmOut>(BLOCK_M, Nb, Nb);
  auto zero_out_rem_tpp = SetZeroTPP<TGemmOut>(BLOCK_M_rem, Nb, ldy);
  auto zero_buf_rem_tpp = SetZeroTPP<TGemmOut>(BLOCK_M_rem, Nb, Nb);
  auto gelu_erf_fwd_tpp = GeluFwdTPP<Tout>(BLOCK_M, Nb, ldy, ldy);
  auto gelu_erf_fwd_rem_tpp = GeluFwdTPP<Tout>(BLOCK_M_rem, Nb, ldy, ldy);
  auto gelu_tanh_fwd_tpp = GeluTanhFwdTPP<Tout>(BLOCK_M, Nb, ldy, ldy);
  auto gelu_tanh_fwd_rem_tpp = GeluTanhFwdTPP<Tout>(BLOCK_M_rem, Nb, ldy, ldy);
  auto relu_fwd_tpp = ReLUFwdTPP<Tout>(BLOCK_M, Nb, ldy, ldy, false);
  auto relu_fwd_rem_tpp = ReLUFwdTPP<Tout>(BLOCK_M_rem, Nb, ldy, ldy, false);
  auto silu_fwd_tpp = SiLUFwdTPP<Tout>(BLOCK_M, Nb, ldy, ldy);
  auto silu_fwd_rem_tpp = SiLUFwdTPP<Tout>(BLOCK_M_rem, Nb, ldy, ldy);
  auto add_tpp = AddTPP<Tout>(BLOCK_M, Nb, ldy, ldy);
  auto add_rem_tpp = AddTPP<Tout>(BLOCK_M_rem, Nb, ldy, ldy);
  auto mul_tpp = MulTPP<Tout>(BLOCK_M, Nb, ldy, ldy);
  auto mul_rem_tpp = MulTPP<Tout>(BLOCK_M_rem, Nb, ldy, ldy);
  bool has_extra_input = fusion_type == WOQ_FUSE_ADD ||
      fusion_type == WOQ_FUSE_ADD_ADD || fusion_type == WOQ_FUSE_MUL;
  auto post_ops_fn = [&](int m, int nc) {
    Tout* y_ptr = (Tout*)py[m][nc];
    Tout* tin0_ptr = has_extra_input ? (Tout*)pin0[m][nc] : nullptr;
    Tout* tin1_ptr =
        fusion_type == WOQ_FUSE_ADD_ADD ? (Tout*)pin1[m][nc] : nullptr;
    if (fusion_type == WOQ_FUSE_GELU_ERF) {
      gelu_erf_fwd_tpp(y_ptr, y_ptr);
    } else if (fusion_type == WOQ_FUSE_GELU_TANH) {
      gelu_tanh_fwd_tpp(y_ptr, y_ptr);
    } else if (fusion_type == WOQ_FUSE_RELU) {
      relu_fwd_tpp(y_ptr, y_ptr);
    } else if (fusion_type == WOQ_FUSE_SILU) {
      silu_fwd_tpp(y_ptr, y_ptr);
    } else if (fusion_type == WOQ_FUSE_ADD) {
      add_tpp(y_ptr, tin0_ptr, y_ptr);
    } else if (fusion_type == WOQ_FUSE_ADD_ADD) {
      add_tpp(y_ptr, tin0_ptr, y_ptr);
      add_tpp(y_ptr, tin1_ptr, y_ptr);
    } else if (fusion_type == WOQ_FUSE_MUL) {
      mul_tpp(y_ptr, tin0_ptr, y_ptr);
    }
  };
  auto post_ops_rem_fn = [&](int m, int nc) {
    Tout* y_ptr = (Tout*)py[m][nc];
    Tout* tin0_ptr = has_extra_input ? (Tout*)pin0[m][nc] : nullptr;
    Tout* tin1_ptr =
        fusion_type == WOQ_FUSE_ADD_ADD ? (Tout*)pin1[m][nc] : nullptr;
    if (fusion_type == WOQ_FUSE_GELU_ERF) {
      gelu_erf_fwd_rem_tpp(y_ptr, y_ptr);
    } else if (fusion_type == WOQ_FUSE_GELU_TANH) {
      gelu_tanh_fwd_rem_tpp(y_ptr, y_ptr);
    } else if (fusion_type == WOQ_FUSE_RELU) {
      relu_fwd_rem_tpp(y_ptr, y_ptr);
    } else if (fusion_type == WOQ_FUSE_SILU) {
      silu_fwd_rem_tpp(y_ptr, y_ptr);
    } else if (fusion_type == WOQ_FUSE_ADD) {
      add_rem_tpp(y_ptr, tin0_ptr, y_ptr);
    } else if (fusion_type == WOQ_FUSE_ADD_ADD) {
      add_rem_tpp(y_ptr, tin0_ptr, y_ptr);
      add_rem_tpp(y_ptr, tin1_ptr, y_ptr);
    } else if (fusion_type == WOQ_FUSE_MUL) {
      mul_rem_tpp(y_ptr, tin0_ptr, y_ptr);
    }
  };

#define GET_DEQUANT_GEMM_TPP(prefetch_dist, block_m) \
  DequantGemmTPP<                                    \
      TComp,                                         \
      TGemmOut,                                      \
      TScale,                                        \
      TZero,                                         \
      MICRO_BLOCK_M,                                 \
      BLOCK_N,                                       \
      /*ldb*/ BLOCK_N,                               \
      /*transA*/ false,                              \
      /*ACC*/ true,                                  \
      qw_type,                                       \
      quant_a_mode,                                  \
      quant_w_mode,                                  \
      prefetch_dist,                                 \
      use_g_idx>(                                    \
      /*M*/ block_m, /*K*/ Kb, lda, ldc, IPEX_KCB_BLOCK_SIZE, str_a);

#define GET_NO_DEQUANT_GEMM_TPP(prefetch_dist, block_m) \
  NoDequantGemmTPP<                                     \
      MICRO_BLOCK_M,                                    \
      BLOCK_N,                                          \
      /*ldb*/ BLOCK_N,                                  \
      /*transA*/ false,                                 \
      /*ACC*/ true,                                     \
      quant_a_mode,                                     \
      quant_w_mode,                                     \
      prefetch_dist>(                                   \
      /*M*/ block_m, /*K*/ Kb, lda, ldc, IPEX_KCB_BLOCK_SIZE, str_a);

#define RUN_DEQUANT_GEMM_TPP(                                 \
    gemm_kernel, no_tile_cfg, kc_start, quant_block_multiple) \
  gemm_kernel(                                                \
      x_ptr,                                                  \
      pw[nc][kc],                                             \
      scale_w,                                                \
      zp_w,                                                   \
      y_ptr,                                                  \
      no_tile_cfg,                                            \
      scale_a,                                                \
      zp_a,                                                   \
      k_groups,                                               \
      count,                                                  \
      kc_start,                                               \
      quant_block_multiple,                                   \
      g_idx_ptr);

#define RUN_NO_DEQUANT_GEMM_TPP(                              \
    gemm_kernel, no_tile_cfg, kc_start, quant_block_multiple) \
  if constexpr (is_int8_linear)                               \
    gemm_kernel(                                              \
        x_ptr,                                                \
        (int8_t*)pw[nc][kc],                                  \
        pcomp[nc][kc],                                        \
        scale_w,                                              \
        zp_w,                                                 \
        y_ptr,                                                \
        no_tile_cfg,                                          \
        scale_a,                                              \
        zp_a,                                                 \
        k_groups,                                             \
        count,                                                \
        kc_start,                                             \
        quant_block_multiple);

  constexpr long MICRO_BLOCK_M = 8;
  product_dispatcher<
      std::tuple<
          /*BLOCK_N*/ long,
          /*qw_type*/ int,
          /*no_dequant_weight*/ bool,
          /*use_g_idx*/ bool>,
      std::tuple<
          enumerate_dispatcher<long, WOQ_N_BLOCK_SIZE>,
          enumerate_dispatcher<
              int,
              WOQ_DTYPE_INT8,
              WOQ_DTYPE_INT4,
              WOQ_DTYPE_NF4>,
          enumerate_dispatcher<bool, false, true>,
          enumerate_dispatcher<bool, false, true>>>::
      call(
          std::make_tuple(Nb, qw_type, no_dequant_weight, g_idx.has_value()),
          [&](auto tuple) {
            auto BLOCK_N = std::get<0>(tuple);
            auto qw_type = std::get<1>(tuple);
            auto no_dequant_w = std::get<2>(tuple);
            auto use_g_idx = std::get<3>(tuple);
            auto dequant_gemm_tpp =
                GET_DEQUANT_GEMM_TPP(PREFETCH_K_DIST, BLOCK_M);
            auto dequant_gemm_no_prefetch_tpp =
                GET_DEQUANT_GEMM_TPP(0, BLOCK_M);
            auto dequant_gemm_rem_tpp =
                GET_DEQUANT_GEMM_TPP(PREFETCH_K_DIST, BLOCK_M_rem);
            auto dequant_gemm_no_prefetch_rem_tpp =
                GET_DEQUANT_GEMM_TPP(0, BLOCK_M_rem);

            auto no_dequant_gemm_tpp =
                GET_NO_DEQUANT_GEMM_TPP(PREFETCH_K_DIST, BLOCK_M);
            auto no_dequant_gemm_no_prefetch_tpp =
                GET_NO_DEQUANT_GEMM_TPP(0, BLOCK_M);
            auto no_dequant_gemm_rem_tpp =
                GET_NO_DEQUANT_GEMM_TPP(PREFETCH_K_DIST, BLOCK_M_rem);
            auto no_dequant_gemm_no_prefetch_rem_tpp =
                GET_NO_DEQUANT_GEMM_TPP(0, BLOCK_M_rem);

            auto pcvt_x_tpp =
                std::is_same<T, uint8_t>() || std::is_same<T, int8_t>()
                ? nullptr
                : std::make_shared<ConvertTPP<T, TComp>>(BLOCK_M, Kb, K, Kb);
            auto pcvt_x_rem_tpp =
                std::is_same<T, uint8_t>() || std::is_same<T, int8_t>()
                ? nullptr
                : std::make_shared<ConvertTPP<T, TComp>>(
                      BLOCK_M_rem, Kb, K, Kb);
            auto cvt_y_tpp = ConvertTPP<TGemmOut, Tout>(BLOCK_M, Nb, Nb, ldy);
            auto cvt_y_rem_tpp =
                ConvertTPP<TGemmOut, Tout>(BLOCK_M_rem, Nb, Nb, ldy);
            auto cvt_y_private_tpp =
                ConvertTPP<TGemmOut, Tout>(BLOCK_M, Nb, N, N);
            auto add_y_tpp = BinaryTPP(
                BLOCK_M, /*row*/
                Nb, /*col*/
                N, /*ldi0*/
                N, /*ldi1*/
                N, /*ldo*/
                XsmmDtype<TGemmOut>(), /*dt_in0*/
                XsmmDtype<Tout>(), /*dt_in1*/
                XsmmDtype<Tout>(), /*dt_out*/
                XsmmDtype<float>(), /*dt_compute*/
                LIBXSMM_MELTW_FLAG_BINARY_NONE,
                LIBXSMM_MELTW_TYPE_BINARY_ADD);

            // TODO(jgong5): parallelize over M on large BS
            if (no_y_buf) {
              auto loop_scheme = M >= PARALLEL_M_THRESHOLD ? "ACb" : "aCb";
              auto gemm_loop = ThreadedLoop<3>(
                  {{0, M, BLOCK_M, false}, {0, Kc, Kcb, false}, {Nc}},
                  loop_scheme);
              gemm_loop(
                  [&](int* idx) {
                    int m = idx[0];
                    int kc = idx[1];
                    int nc = idx[2];
                    auto count = kc + Kcb < Kc ? Kcb : Kc - kc;
                    float* scale_a = nullptr;
                    int32_t* zp_a = nullptr;
                    int32_t k_groups = -1;
                    int32_t quant_offset =
                        g_idx_ptr ? 0 : kc / quant_block_multiple;
                    if constexpr (std::is_same<TComp, uint8_t>()) {
                      if constexpr (
                          quant_a_mode == QUANT_A_PER_TENSOR ||
                          quant_a_mode == QUANT_A_PER_TENSOR_SYM) {
                        scale_a = scales_a_ptr;
                        if constexpr (quant_a_mode == QUANT_A_PER_TENSOR) {
                          zp_a = zps_a_ptr;
                        }
                      } else if constexpr (
                          quant_a_mode == QUANT_A_PER_K_BLOCK ||
                          quant_a_mode == QUANT_A_PER_K_BLOCK_SYM) {
                        scale_a = scales_a_ptr + quant_offset;
                        if constexpr (quant_a_mode == QUANT_A_PER_K_BLOCK) {
                          zp_a = zps_a_ptr + quant_offset;
                        }
                      } else if constexpr (
                          quant_a_mode == QUANT_A_PER_M ||
                          quant_a_mode == QUANT_A_PER_M_SYM) {
                        scale_a = scales_a_ptr + m;
                        if constexpr (quant_a_mode == QUANT_A_PER_M) {
                          zp_a = zps_a_ptr + m;
                        }
                        k_groups = 1;
                      } else {
                        scale_a =
                            scales_a_ptr + m * quant_k_blocks + quant_offset;
                        if constexpr (quant_a_mode == QUANT_A_PER_M_K_BLOCK) {
                          zp_a = zps_a_ptr + m * quant_k_blocks + quant_offset;
                        }
                        k_groups = quant_k_blocks;
                      }
                    }
                    TScale* scale_w = nullptr;
                    TZero* zp_w = nullptr;
                    if constexpr (
                        quant_w_mode == QUANT_W_PER_CHANNEL ||
                        quant_w_mode == QUANT_W_PER_CHANNEL_SYM) {
                      scale_w = pscales[nc][0];
                      if constexpr (asym_quant_w) {
                        zp_w = pzps[nc][0];
                      }
                    } else {
                      scale_w = pscales[nc][quant_offset];
                      if constexpr (asym_quant_w) {
                        zp_w = pzps[nc][quant_offset];
                      }
                    }
                    bool is_rem = (m + BLOCK_M > M);
                    TGemmOut* y_ptr = (TGemmOut*)py[m][nc];
                    if (!is_rem) {
                      if (kc == 0) {
                        if (b.defined()) {
                          copy_bias_out_tpp(pb[nc], y_ptr);
                        } else {
                          zero_out_tpp(y_ptr);
                        }
                      }
                      TComp* x_ptr = (TComp*)px[m][kc];
                      if (kc < Kc - 1) {
                        if constexpr (no_dequant_w) {
                          RUN_NO_DEQUANT_GEMM_TPP(
                              no_dequant_gemm_tpp, true, kc, 1);
                        } else {
                          RUN_DEQUANT_GEMM_TPP(dequant_gemm_tpp, true, kc, 1);
                        }
                      } else {
                        if constexpr (no_dequant_w) {
                          RUN_NO_DEQUANT_GEMM_TPP(
                              no_dequant_gemm_no_prefetch_tpp, true, kc, 1);
                        } else {
                          RUN_DEQUANT_GEMM_TPP(
                              dequant_gemm_no_prefetch_tpp, true, kc, 1);
                        }
                        if (fusion_type > 0) {
                          post_ops_fn(m, nc);
                        }
                      }
                    } else {
                      if (kc == 0) {
                        if (b.defined()) {
                          copy_bias_out_rem_tpp(pb[nc], y_ptr);
                        } else {
                          zero_out_rem_tpp(y_ptr);
                        }
                      }
                      TComp* x_ptr = (TComp*)px[m][kc];
                      if (kc < Kc - 1) {
                        if constexpr (no_dequant_w) {
                          RUN_NO_DEQUANT_GEMM_TPP(
                              no_dequant_gemm_rem_tpp, false, kc, 1);
                          no_dequant_gemm_tpp.config();
                        } else {
                          RUN_DEQUANT_GEMM_TPP(
                              dequant_gemm_rem_tpp, false, kc, 1);
                          dequant_gemm_tpp.config();
                        }
                      } else {
                        if constexpr (no_dequant_w) {
                          RUN_NO_DEQUANT_GEMM_TPP(
                              no_dequant_gemm_no_prefetch_rem_tpp,
                              false,
                              kc,
                              1);
                          no_dequant_gemm_no_prefetch_tpp.config();
                        } else {
                          RUN_DEQUANT_GEMM_TPP(
                              dequant_gemm_no_prefetch_rem_tpp, false, kc, 1);
                          dequant_gemm_no_prefetch_tpp.config();
                        }
                        if (fusion_type > 0) {
                          post_ops_rem_fn(m, nc);
                        }
                      }
                    }
                    // TODO(jgong5): post-op fusion
                  },
                  [&]() {
                    if constexpr (no_dequant_w) {
                      no_dequant_gemm_tpp.config();
                    } else {
                      dequant_gemm_tpp.config();
                    }
                  },
                  [&]() {
                    if constexpr (no_dequant_w) {
                      no_dequant_gemm_tpp.release();
                    } else {
                      dequant_gemm_tpp.release();
                    }
                  });
            } else { // no_y_buf is false
              auto num_threads = omp_get_max_threads();
              TGemmOut* y_private = nullptr;
              bool* y_private_valid = nullptr;
              if (k_splits > 1) {
                // TODO(jgong5): if we know the thread decomposition, we can
                // allocate a smaller buffer
                y_private = (TGemmOut*)std::aligned_alloc(
                    64, num_threads * M * N * sizeof(TGemmOut));
                y_private_valid = (bool*)std::aligned_alloc(
                    64, num_threads * (M / BLOCK_M) * Nc * sizeof(bool));
                std::fill_n(
                    y_private_valid, num_threads * (M / BLOCK_M) * Nc, false);
              }
              auto y_private_ptr = GetVLAPtr<TGemmOut>(y_private, {M, Nc, Nb});
              auto y_private_valid_ptr =
                  GetVLAPtr<bool>(y_private_valid, {M / BLOCK_M, Nc});
              static const char* SCHEME_LARGE_M =
                  getenv("IPEX_WOQ_GEMM_LOOP_SCHEME")
                  ? getenv("IPEX_WOQ_GEMM_LOOP_SCHEME")
                  : "CAB";
              auto loop_scheme =
                  M >= PARALLEL_M_THRESHOLD ? SCHEME_LARGE_M : "ABc";
              auto gemm_loop = ThreadedLoop<3>(
                  {{Nc}, {0, Kc, Kc / k_splits, true}, {0, M, BLOCK_M, false}},
                  loop_scheme);
              gemm_loop(
                  [&](int* idx) {
                    int my_id = omp_get_thread_num();
                    int nc = idx[0];
                    int kc_start = idx[1];
                    int kc_end = kc_start + Kc / k_splits;
                    int m = idx[2];
                    bool is_rem = (m + BLOCK_M > M);
                    auto y_out_ptr = py[m][nc];
                    alignas(64) TGemmOut y_buf[BLOCK_M][Nb];
                    TGemmOut* y_ptr = y_private_ptr[my_id][m][nc];
                    if (k_splits > 1) {
                      if (!y_private_valid_ptr[my_id][m / BLOCK_M][nc]) {
                        if (kc_start == 0 && b.defined()) {
                          copy_bias_out_tpp(pb[nc], y_ptr);
                        } else {
                          zero_out_tpp(y_ptr);
                        }
                        y_private_valid_ptr[my_id][m / BLOCK_M][nc] = true;
                      }
                    } else {
                      y_ptr = y_buf[0];
                      if (b.defined()) {
                        if (!is_rem) {
                          copy_bias_buf_tpp(pb[nc], y_buf[0]);
                        } else {
                          copy_bias_buf_rem_tpp(pb[nc], y_buf[0]);
                        }
                      } else {
                        if (!is_rem) {
                          zero_buf_tpp(y_buf[0]);
                        } else {
                          zero_buf_rem_tpp(y_buf[0]);
                        }
                      }
                    }
                    for (int kc = kc_start; kc < kc_end; kc += Kcb) {
                      auto count = kc + Kcb < Kc ? Kcb : Kc - kc;
                      TComp* x_ptr = (TComp*)px[m][kc];
                      float* scale_a = nullptr;
                      int32_t* zp_a = nullptr;
                      int32_t k_groups = -1;
                      int32_t quant_offset =
                          g_idx_ptr ? 0 : kc / quant_block_multiple;
                      if constexpr (std::is_same<TComp, uint8_t>()) {
                        if constexpr (
                            quant_a_mode == QUANT_A_PER_TENSOR ||
                            quant_a_mode == QUANT_A_PER_TENSOR_SYM) {
                          scale_a = scales_a_ptr;
                          if constexpr (quant_a_mode == QUANT_A_PER_TENSOR) {
                            zp_a = zps_a_ptr;
                          }
                        } else if constexpr (
                            quant_a_mode == QUANT_A_PER_K_BLOCK ||
                            quant_a_mode == QUANT_A_PER_K_BLOCK_SYM) {
                          scale_a = scales_a_ptr + quant_offset;
                          if constexpr (quant_a_mode == QUANT_A_PER_K_BLOCK) {
                            zp_a = zps_a_ptr + quant_offset;
                          }
                        } else if constexpr (
                            quant_a_mode == QUANT_A_PER_M ||
                            quant_a_mode == QUANT_A_PER_M_SYM) {
                          scale_a = scales_a_ptr + m;
                          if constexpr (quant_a_mode == QUANT_A_PER_M) {
                            zp_a = zps_a_ptr + m;
                          }
                          k_groups = 1;
                        } else {
                          scale_a =
                              scales_a_ptr + m * quant_k_blocks + quant_offset;
                          if constexpr (quant_a_mode == QUANT_A_PER_M_K_BLOCK) {
                            zp_a =
                                zps_a_ptr + m * quant_k_blocks + quant_offset;
                          }
                          k_groups = quant_k_blocks;
                        }
                      }
                      TScale* scale_w = nullptr;
                      TZero* zp_w = nullptr;
                      if constexpr (
                          quant_w_mode == QUANT_W_PER_CHANNEL ||
                          quant_w_mode == QUANT_W_PER_CHANNEL_SYM) {
                        scale_w = pscales[nc][0];
                        if constexpr (asym_quant_w) {
                          zp_w = pzps[nc][0];
                        }
                      } else {
                        scale_w = pscales[nc][quant_offset];
                        if constexpr (asym_quant_w) {
                          zp_w = pzps[nc][quant_offset];
                        }
                      }
                      if (!is_rem) {
                        alignas(64) TComp x_buf[count][BLOCK_M][Kb];
                        if (!no_x_buf) {
                          for (int cnt = 0; cnt < count; cnt++) {
                            (*pcvt_x_tpp)(px[m][kc + cnt], x_buf[cnt][0]);
                          }
                          x_ptr = x_buf[0][0];
                        }

                        if (kc < Kc - 1) {
                          if constexpr (no_dequant_w) {
                            RUN_NO_DEQUANT_GEMM_TPP(
                                no_dequant_gemm_tpp,
                                true,
                                kc,
                                quant_block_multiple);
                          } else {
                            RUN_DEQUANT_GEMM_TPP(
                                dequant_gemm_tpp,
                                true,
                                kc,
                                quant_block_multiple);
                          }
                        } else {
                          if constexpr (no_dequant_w) {
                            RUN_NO_DEQUANT_GEMM_TPP(
                                no_dequant_gemm_no_prefetch_tpp,
                                true,
                                kc,
                                quant_block_multiple);
                          } else {
                            RUN_DEQUANT_GEMM_TPP(
                                dequant_gemm_no_prefetch_tpp,
                                true,
                                kc,
                                quant_block_multiple);
                          }
                        }
                      } else {
                        alignas(64) TComp x_buf[count][BLOCK_M][Kb];
                        if (!no_x_buf) {
                          for (int cnt = 0; cnt < count; cnt++) {
                            (*pcvt_x_rem_tpp)(px[m][kc + cnt], x_buf[cnt][0]);
                          }
                          x_ptr = x_buf[0][0];
                        }
                        if (kc < Kc - 1) {
                          if constexpr (no_dequant_w) {
                            RUN_NO_DEQUANT_GEMM_TPP(
                                no_dequant_gemm_rem_tpp,
                                false,
                                kc,
                                quant_block_multiple);
                            no_dequant_gemm_tpp.config();
                          } else {
                            RUN_DEQUANT_GEMM_TPP(
                                dequant_gemm_rem_tpp,
                                false,
                                kc,
                                quant_block_multiple);
                            dequant_gemm_tpp.config();
                          }
                        } else {
                          if constexpr (no_dequant_w) {
                            RUN_NO_DEQUANT_GEMM_TPP(
                                no_dequant_gemm_no_prefetch_rem_tpp,
                                false,
                                kc,
                                quant_block_multiple);
                            no_dequant_gemm_no_prefetch_tpp.config();
                          } else {
                            RUN_DEQUANT_GEMM_TPP(
                                dequant_gemm_no_prefetch_rem_tpp,
                                false,
                                kc,
                                quant_block_multiple);
                            dequant_gemm_no_prefetch_tpp.config();
                          }
                        }
                      }
                    }
                    // TODO(jgong5): post-op fusion
                    if (k_splits <= 1) {
                      if (!is_rem) {
                        cvt_y_tpp(y_buf[0], y_out_ptr);
                        if (fusion_type > 0) {
                          post_ops_fn(m, nc);
                        }
                      } else {
                        cvt_y_rem_tpp(y_buf[0], y_out_ptr);
                        if (fusion_type > 0) {
                          post_ops_rem_fn(m, nc);
                        }
                      }
                    }
                  },
                  [&]() {
                    if constexpr (no_dequant_w) {
                      no_dequant_gemm_tpp.config();
                    } else {
                      dequant_gemm_tpp.config();
                    }
                  },
                  [&]() {
                    if constexpr (no_dequant_w) {
                      no_dequant_gemm_tpp.release();
                    } else {
                      dequant_gemm_tpp.release();
                    }
                  });
              if (k_splits > 1) {
                TLA_ASSERT(
                    M % BLOCK_M == 0,
                    "M must be divisible by BLOCK_M for k_splits > 1");
                auto reduce_loop =
                    ThreadedLoop<2>({{0, M, BLOCK_M, true}, {Nc}}, "AB");
                reduce_loop([&](int* idx) {
                  int m = idx[0];
                  int nc = idx[1];
                  bool init = false;
                  for (int id = 0; id < num_threads; id++) {
                    if (y_private_valid_ptr[id][m / BLOCK_M][nc]) {
                      if (!init) {
                        cvt_y_private_tpp(y_private_ptr[id][m][nc], py[m][nc]);
                        init = true;
                      } else {
                        add_y_tpp(
                            y_private_ptr[id][m][nc], py[m][nc], py[m][nc]);
                      }
                    }
                  }
                  if (fusion_type > 0) {
                    post_ops_fn(m, nc);
                  }
                });
                std::free(y_private);
                std::free(y_private_valid);
              }
            }
          },
          [](auto tuple) { failing_fallback(); });
}

static at::Tensor woq_gemm_ref_impl(
    const at::Tensor& x,
    const at::Tensor& qw,
    const TensorList& scales_list,
    const TensorList& zp_list,
    const TensorList& bias_list,
    const int qw_type,
    at::ScalarType compute_dtype,
    int64_t fusion_type,
    const TensorList& others_list,
    int64_t quant_w_mode = 0,
    int64_t quant_block_k = 0,
    const c10::optional<at::Tensor>& g_idx = c10::nullopt) {
  constexpr size_t fp32_idx = 0, fp16_idx = 1, bf16_idx = 2, int8_idx = 3;
  auto biases = bias_list.empty()
      ? TensorList({at::Tensor(), at::Tensor(), at::Tensor()})
      : bias_list;
  TLA_ASSERT(
      qw.dim() == 2,
      "weight must be in 2D plain format for reference impl, got ",
      qw.dim(),
      "D");
  auto K = x.size(-1);
  auto M = x.numel() / K;
  auto N = qw.size(0);
  at::Tensor scale, zp;
  if (g_idx.has_value()) {
    TORCH_CHECK(
        g_idx.value().numel() == K,
        "WOQ: g_idx must have the same number of elements as K, got ",
        g_idx.value().numel(),
        " elements, expected ",
        K,
        " elements");
    TORCH_CHECK(scales_list[fp32_idx].dim() == 2);
    using namespace at::indexing;
    scale = at::empty({N, K}, scales_list[fp32_idx].options());
    for (int k = 0; k < K; ++k) {
      auto g = g_idx.value()[k].item<int32_t>();
      scale.index_put_({Slice(), k}, scales_list[fp32_idx].index({Slice(), g}));
    }
    if (is_asymmetric_quant_w(quant_w_mode)) {
      zp = at::empty({N, K}, zp_list[fp32_idx].options());
      for (int k = 0; k < K; ++k) {
        auto g = g_idx.value()[k].item<int32_t>();
        zp.index_put_({Slice(), k}, zp_list[fp32_idx].index({Slice(), g}));
      }
    }
  } else {
    scale = scales_list[fp32_idx].unsqueeze(-1);
    if (is_asymmetric_quant_w(quant_w_mode)) {
      zp = zp_list[fp32_idx].unsqueeze(-1);
    }
  }
  auto w = torch_ipex::cpu::dequantize_woq_weight(
               qw, {N, K}, scale, zp, qw_type, quant_block_k)
               .to(compute_dtype);
  auto x_reshape = x.reshape({M, K});
  auto x_fp = x_reshape.to(compute_dtype);
  auto y = at::linear(x_fp, w);
  if (biases[0].defined()) {
    auto b_index = compute_dtype == at::kFloat ? fp32_idx
        : compute_dtype == at::kHalf           ? fp16_idx
                                               : bf16_idx;
    y = at::add(y, biases[b_index]);
  }
  if (fusion_type == WOQ_FUSE_GELU_ERF) {
    at::gelu_(y);
  } else if (fusion_type == WOQ_FUSE_GELU_TANH) {
    at::gelu_(y, "tanh");
  } else if (fusion_type == WOQ_FUSE_RELU) {
    at::relu_(y);
  } else if (fusion_type == WOQ_FUSE_SILU) {
    at::silu_(y);
  } else if (fusion_type == WOQ_FUSE_ADD || fusion_type == WOQ_FUSE_ADD_ADD) {
    for (auto& tin : others_list) {
      auto tin_view = tin.view({-1, y.size(-1)});
      if (tin_view.size(0) < y.size(0)) {
        tin_view = at::pad(
            tin_view, {0, 0, 0, y.size(0) - tin_view.size(0)}, "constant", 0);
      }
      y = at::add(y, tin_view);
    }
  } else if (fusion_type == WOQ_FUSE_MUL) {
    for (auto& tin : others_list) {
      auto tin_view = tin.view({-1, y.size(-1)});
      if (tin_view.size(0) < y.size(0)) {
        tin_view = at::pad(
            tin_view, {0, 0, 0, y.size(0) - tin_view.size(0)}, "constant", 0);
      }
      y = at::mul(y, tin_view);
    }
  } else {
    TORCH_CHECK(
        fusion_type == WOQ_FUSE_NONE,
        "WOQ: Unexpected fusion type: ",
        fusion_type);
  }
  auto out_sizes = x.sizes().vec();
  out_sizes.back() = N;
  y = y.view(out_sizes);
  return y.to(x.scalar_type());
}

} // namespace cpu
} // namespace torch_ipex

namespace std {
template <>
struct hash<torch_ipex::cpu::DotMicroKernelKey> {
  std::size_t operator()(const torch_ipex::cpu::DotMicroKernelKey& key) const {
    std::size_t h = std::hash<bool>()(key.trans_a);
    h = std::hash<bool>()(key.trans_b) ^ (h << 1);
    h = std::hash<int>()(key.lda) ^ (h << 1);
    h = std::hash<int>()(key.ldb) ^ (h << 1);
    h = std::hash<int>()(key.ldc) ^ (h << 1);
    return h;
  }
};
} // namespace std

#endif