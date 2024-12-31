import numpy as np

def octahedron_to_unit_vector(oct):
    N = np.array([oct[0], oct[1], 1 - np.sum(np.abs(oct))])
    t = max(-N[2], 0)
    if N[0] >= 0:
        N[0] -= t
    else:
        N[0] += t
    if N[1] >= 0:
        N[1] -= t
    else:
        N[1] += t
    return N / np.linalg.norm(N)

def unpack_unorm16x2(packed):
    return np.array([
        (packed & 0xFFFF) / 65535.0,
        (packed >> 16) / 65535.0
    ])

def get_normal_from_packed_uint(value):
    return octahedron_to_unit_vector(unpack_unorm16x2(value) * 2.0 - 1.0)

print(get_normal_from_packed_uint(4286611200))