AI generated quaternion benchmark comparing two methods: direct quaternion rotation of a vector vs. converting the quaternion into a matrix (Mat3) first and rotating via a sequence of dot products (result: Mat3 is faster):

### 📊 Benchmark Results (N = 2,000,000)

| Implementation | Varying `q` (ns/vec) | Reusing Same `q` (ns/vec) |
| :--- | :---: | :---: |
| **mat3 (precomputed / build+mul)** | 5.33900 | **4.72819** |
| **user_fast_inline** | 9.72915 | 9.72917 |
| **user_original (operator^)** | 9.76867 | 9.74288 |
| **quat xprod** | 10.34308 | 10.37802 |

