// q_orig_bench.cpp
// Single-file microbench including the user's original code (unchanged) +
// comparisons. Indent: 2 spaces.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using f32 = float;

struct Vec3 {
  f32 x, y, z;
  inline Vec3() : x(0), y(0), z(0) {}
  inline Vec3(f32 X, f32 Y, f32 Z) : x(X), y(Y), z(Z) {}
};

struct Quat4 {
  f32 x, y, z, w;  // (x,y,z,w)
  inline Quat4() : x(0), y(0), z(0), w(1) {}
  inline Quat4(f32 X, f32 Y, f32 Z, f32 W) : x(X), y(Y), z(Z), w(W) {}
};

// ---------------- basic helpers ----------------
static inline f32 dot(const Vec3& a, const Vec3& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
static inline Vec3 normalize(const Vec3& v) {
  f32 n2 = std::max(dot(v, v), 1e-30f);
  f32 inv = 1.0f / std::sqrt(n2);
  return Vec3(v.x * inv, v.y * inv, v.z * inv);
}
static inline Quat4 q_conjugate(const Quat4& q) {
  return Quat4(-q.x, -q.y, -q.z, q.w);
}
static inline Quat4 q_mul(const Quat4& A, const Quat4& B) {
  return Quat4(A.w * B.x + A.x * B.w + A.y * B.z - A.z * B.y,
               A.w * B.y - A.x * B.z + A.y * B.w + A.z * B.x,
               A.w * B.z + A.x * B.y - A.y * B.x + A.z * B.w,
               A.w * B.w - A.x * B.x - A.y * B.y - A.z * B.z);
}

// ---------------- YOUR ORIGINAL CODE (unchanged) ----------------

// Normal: 32 muls, 24 adds. Optimized: 24 muls, 17 adds
inline Quat4 mulQuat4AwIs0(const Vec3& a, const Quat4& b) {
  return Quat4(
      b.w * a.x + b.y * a.z - b.z * a.y, b.w * a.y - b.x * a.z + b.z * a.x,
      b.w * a.z + b.x * a.y - b.y * a.x, -b.x * a.x - b.y * a.y - b.z * a.z);
}  // mulQuat4AwIs0

inline Quat4 mulQuat4BwIs0(const Quat4& a, const Vec3& b) {
  return Quat4(
      b.x * a.w + b.y * a.z - b.z * a.y, b.y * a.w - b.x * a.z + b.z * a.x,
      b.x * a.y - b.y * a.x + b.z * a.w, -b.x * a.x - b.y * a.y - b.z * a.z);
}  // mulQuat4BwIs0

// Minimal companion to make the original operator^ compile.
// By construction tmp = mulQuat4AwIs0(p,q) = (0,p) * q  OR  q * (0,p)?  (See
// algebra.) The provided AwIs0 form matches  b*(0,a)  when called as (a=Vec3,
// b=Quat4), i.e.,  q*(0,p). We therefore finish rotation as:  R = (q*(0,p)) *
// q*  and return vec(R).
inline Vec3 mulQuat4InvAtoVec3(const Quat4& q, const Quat4& tmp /*= q*(0,p)*/) {
  Quat4 R = q_mul(tmp, q_conjugate(q));
  return Vec3(R.x, R.y, R.z);
}

// Rotate a vector with a Quaternion  (your original API)
inline Vec3 operator^(const Quat4& q, const Vec3& p) {
  return mulQuat4InvAtoVec3(q, mulQuat4AwIs0(p, q));
}  // Quat4 ^

// ---------------- alternative kernels for comparison ----------------

// Fully inlined fast sandwich: t=(0,v)*q* ; r=q*t
static inline Vec3 rotate_sandwich_fast(const Quat4& q, const Vec3& v) {
  const f32 qx = q.x, qy = q.y, qz = q.z, qw = q.w;
  const f32 vx = v.x, vy = v.y, vz = v.z;
  const f32 qcx = -qx, qcy = -qy, qcz = -qz, qcw = qw;  // q*
  const f32 tx = qcw * vx + qcy * vz - qcz * vy;
  const f32 ty = qcw * vy - qcx * vz + qcz * vx;
  const f32 tz = qcw * vz + qcx * vy - qcy * vx;
  const f32 tw = -(qcx * vx + qcy * vy + qcz * vz);
  return Vec3(qw * tx + qx * tw + qy * tz - qz * ty,
              qw * ty - qx * tz + qy * tw + qz * tx,
              qw * tz + qx * ty - qy * tx + qz * tw);
}

// Cross-product formula
static inline Vec3 rotate_cross(const Quat4& q, const Vec3& v) {
  const f32 qx = q.x, qy = q.y, qz = q.z, qw = q.w;
  const f32 tx = 2.0f * (qy * v.z - qz * v.y);
  const f32 ty = 2.0f * (qz * v.x - qx * v.z);
  const f32 tz = 2.0f * (qx * v.y - qy * v.x);
  return Vec3(v.x + qw * tx + (qy * tz - qz * ty),
              v.y + qw * ty + (qz * tx - qx * tz),
              v.z + qw * tz + (qx * ty - qy * tx));
}

// Matrix path
struct Mat3 {
  f32 m[9];
};

static inline Mat3 mat3_from_quat(const Quat4& q) {
  const f32 x = q.x, y = q.y, z = q.z, w = q.w;
  const f32 xx = x * x, yy = y * y, zz = z * z;
  const f32 xy = x * y, xz = x * z, yz = y * z;
  const f32 wx = w * x, wy = w * y, wz = w * z;
  Mat3 R;
  R.m[0] = 1.0f - 2.0f * (yy + zz);
  R.m[1] = 2.0f * (xy - wz);
  R.m[2] = 2.0f * (xz + wy);
  R.m[3] = 2.0f * (xy + wz);
  R.m[4] = 1.0f - 2.0f * (xx + zz);
  R.m[5] = 2.0f * (yz - wx);
  R.m[6] = 2.0f * (xz - wy);
  R.m[7] = 2.0f * (yz + wx);
  R.m[8] = 1.0f - 2.0f * (xx + yy);
  return R;
}
static inline Vec3 rotate_mat3(const Mat3& R, const Vec3& v) {
  return Vec3(R.m[0] * v.x + R.m[1] * v.y + R.m[2] * v.z,
              R.m[3] * v.x + R.m[4] * v.y + R.m[5] * v.z,
              R.m[6] * v.x + R.m[7] * v.y + R.m[8] * v.z);
}

// ---------------- bench harness (tight loops) ----------------
struct Result {
  std::string name;
  double ns_per_vec;
};

template <typename F>
static double run_loop(F&& f, const Quat4* __restrict qs,
                       const Vec3* __restrict vs, Vec3* __restrict out,
                       size_t N) {
  using clk = std::chrono::steady_clock;
  volatile f32 sink = 0.0f;

  for (size_t i = 0; i < N; i++) {
    Vec3 r = f(qs[i], vs[i]);
    sink += r.x + r.y + r.z;
    out[i] = r;
  }

  auto t0 = clk::now();
  for (size_t i = 0; i < N; i++) {
    Vec3 r = f(qs[i], vs[i]);
    sink += r.x + r.y + r.z;
    out[i] = r;
  }
  auto t1 = clk::now();

  std::chrono::duration<double> dt = t1 - t0;
  if (sink == 1234567.0f) std::fprintf(stderr, "ignore\n");
  return (dt.count() * 1e9) / double(N);
}

static void bench_case(size_t N, bool reuseSameQ) {
  std::mt19937 rng(1337u);
  std::uniform_real_distribution<f32> U(-1.0f, 1.0f);
  std::uniform_real_distribution<f32> A(0.0f, 1.0f);

  std::vector<Vec3> vs(N), out(N);
  std::vector<Quat4> qs(N);

  for (size_t i = 0; i < N; i++) {
    vs[i] = normalize({U(rng), U(rng), U(rng)});
    Vec3 axis = normalize({U(rng), U(rng), U(rng)});
    f32 theta = A(rng) * 3.14159265358979323846f;
    f32 s = std::sin(0.5f * theta), c = std::cos(0.5f * theta);
    qs[i] = Quat4(axis.x * s, axis.y * s, axis.z * s, c);  // unit
  }
  if (reuseSameQ) {
    Quat4 q0 = qs[0];
    std::fill(qs.begin(), qs.end(), q0);
  }

  // Precompute matrices (reuse case)
  std::vector<Mat3> mats;
  if (reuseSameQ) {
    mats.resize(N);
    for (size_t i = 0; i < N; i++) mats[i] = mat3_from_quat(qs[i]);
  }

  // Kernels
  auto k_user_original = [](const Quat4& q, const Vec3& v) -> Vec3 {
    return (q ^ v);  // your operator^ path
  };
  auto k_user_fast = [](const Quat4& q, const Vec3& v) -> Vec3 {
    return rotate_sandwich_fast(q, v);
  };
  auto k_cross = [](const Quat4& q, const Vec3& v) -> Vec3 {
    return rotate_cross(q, v);
  };
  auto k_mat_build = [](const Quat4& q, const Vec3& v) -> Vec3 {
    return rotate_mat3(mat3_from_quat(q), v);
  };

  std::printf("N=%zu %s\n", N, reuseSameQ ? "(reuse same q)" : "(vary q)");

  double ns_user_orig =
      run_loop(k_user_original, qs.data(), vs.data(), out.data(), N);
  std::printf("user_original (operator^): %.5f ns/vec\n", ns_user_orig);

  double ns_user_fast =
      run_loop(k_user_fast, qs.data(), vs.data(), out.data(), N);
  std::printf("user_fast_inline: %.5f ns/vec\n", ns_user_fast);

  double ns_cross = run_loop(k_cross, qs.data(), vs.data(), out.data(), N);
  std::printf("quat xprod: %.5f ns/vec\n", ns_cross);

  if (reuseSameQ) {
    // explicit loop for precomputed matrix
    volatile f32 sink = 0.0f;
    using clk = std::chrono::steady_clock;
    // warmup
    for (size_t i = 0; i < N; i++) {
      Vec3 r = rotate_mat3(mats[i], vs[i]);
      sink += r.x + r.y + r.z;
      out[i] = r;
    }
    auto t0 = clk::now();
    for (size_t i = 0; i < N; i++) {
      Vec3 r = rotate_mat3(mats[i], vs[i]);
      sink += r.x + r.y + r.z;
      out[i] = r;
    }
    auto t1 = clk::now();
    if (sink == 1234567.0f) std::fprintf(stderr, "ignore\n");
    double ns =
        std::chrono::duration<double>(t1 - t0).count() * 1e9 / double(N);
    std::printf("mat3 (precomputed): %.5f ns/vec\n\n", ns);
  } else {
    double ns_build =
        run_loop(k_mat_build, qs.data(), vs.data(), out.data(), N);
    std::printf("mat3 (build+mul): %.5f ns/vec\n\n", ns_build);
  }
}

int main() {
  const size_t N = 2'000'000;
  bench_case(N, /*reuseSameQ=*/false);
  bench_case(N, /*reuseSameQ=*/true);
  return 0;
}
