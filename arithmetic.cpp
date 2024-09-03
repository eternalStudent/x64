struct UDivByConstantParams {
	uint64 M; // Magic number,
	int64 s;  // shift amount
	bool a;   // "add" indicator,
}; 

UDivByConstantParams GetUDivByConstantParams(uint64 d) {
	/* 
	 * given u64 d, finds u64 m, s64 s such that for each unsigned n
	 *    2^64*high + low = m*n
	 *    n/d =  high >> s
	 *
	 *  if (MAX_UINT64 < m) define M := m - 2^64
	 *  and 2^64*high + low = M*n
	 *  and n/d = (high + n) >> s = (high + (n - high) >> 1) >> (s-1)
	 * 
	 */
	uint64 delta;
	UDivByConstantParams params;
	params.a = false; // Initialize "add" indicator.
	int64 p = 63; // Initialize p.
	uint64 q = 0x7FFFFFFFFFFFFFFF/d; // Initialize q = (2**p - 1)/d.
	uint64 r = 0x7FFFFFFFFFFFFFFF - q*d; // Init. r = rem(2**p - 1, d).
	uint64 p64 = 1;
	do {
		p++;
		if (p == 64) p64 = 1; // Set p64 = 2**(p-64).
		else p64 = 2*p64;
		if (r + 1 >= d - r) {
			if (q >= 0x7FFFFFFFFFFFFFFF) params.a = true;
			q = 2*q + 1; // Update q.
			r = 2*r + 1 - d; // Update r.
		}
		else {
			if (q >= 0x8000000000000000) params.a = true;
			q = 2*q;
			r = 2*r + 1;
		}
		delta = d - 1 - r;
	} while (p < 128 && p64 < delta);
	params.M = q + 1; // Magic number and
	params.s = p - 64; // shift amount to return
	return params; // (params.a was set above).
}

enum UMulMethod {
	UMul_IMul,
	UMul_Shift,
	UMul_Lea,
	UMul_LeaAndShift,
	UMul_LeaAndLea
};

struct UMulByConstantParams {
	UMulMethod method;
	int32 shift;
	int32 lea1;
	int32 lea2;
};

UMulByConstantParams GetUMulByConstantParams(uint64 m) {
	ASSERT(m > 1);

	if ((m & (m - 1)) == 0) {
		return {UMul_Shift, (int32)HighBit(m, -1)};
	}

	int32 lea;
	if (m%9 == 0)
		lea = 9;
	else if (m%5 == 0)
		lea = 5;
	else if (m%3 == 0)
		lea = 3;
	else lea = 0;
	
	if (lea) {
		if (m == lea) return {UMul_Lea, 0, lea};

		m /= lea;
		if ((m & (m - 1)) == 0) {
			return {UMul_LeaAndShift, (int32)HighBit(m, -1), lea};
		}

		if (m == 9 || m == 5 || m == 3) {
			return {UMul_LeaAndLea, 0, lea, (int32)m};
		}
	}

	return {UMul_IMul};
	
}