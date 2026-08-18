AI generated quaternion benchmark comparing two methods: direct quaternion rotation of a vector vs. converting the quaternion into a matrix (Mat3) first and rotating via a sequence of dot products (result: Mat3 is faster):

N=2000000 (vary q)
user_original (operator^): 9.76867 ns/vec
user_fast_inline: 9.72915 ns/vec
quat xprod: 10.34308 ns/vec
mat3 (build+mul): 5.33900 ns/vec

N=2000000 (reuse same q)
user_original (operator^): 9.74288 ns/vec
user_fast_inline: 9.72917 ns/vec
quat xprod: 10.37802 ns/vec
mat3 (precomputed): 4.72819 ns/vec
