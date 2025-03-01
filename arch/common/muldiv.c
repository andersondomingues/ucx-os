#include <hal.h>

/*
software implementation of multiply/divide and 64-bit routines
*/

uint32_t __mulsi3(uint32_t a, uint32_t b)
{
	uint32_t answer = 0;

	while (b) {
		if (b & 1)
			answer += a;
		a <<= 1;
		b >>= 1;
	}
	
	return answer;
}

uint64_t __muldsi3(uint32_t a, uint32_t b)
{
	uint64_t aa = a, bb = b;
	uint64_t answer = 0;

	while (bb) {
		if (bb & 1)
			answer += aa;
		aa <<= 1;
		bb >>= 1;
	}
	
	return answer;
}

uint64_t __muldi3(uint64_t a, uint64_t b)
{
	uint32_t al = (uint32_t)a;
	uint32_t ah = (uint32_t)(a >> 32);
	uint32_t bl = (uint32_t)b;
	uint32_t bh = (uint32_t)(b >> 32);
	uint64_t v;

	v = (uint64_t)__muldsi3(al, bl);
	v += (uint64_t)(__mulsi3(al, bh) + __mulsi3(ah, bl)) << 32;

	return v;
}

uint32_t __udivmodsi4(uint32_t num, uint32_t den, int8_t mod)
{
	uint32_t quot = 0, qbit = 1;

	if (den == 0)
		return 0;

	while ((int32_t)den >= 0) {
		den <<= 1;
		qbit <<= 1;
	}

	while (qbit) {
		if (den <= num) {
			num -= den;
			quot += qbit;
		}
		den >>= 1;
		qbit >>= 1;
	}

	if (mod)
		return num;

	return quot;
}

int32_t __divsi3(int32_t num, int32_t den)
{
	int32_t neg = 0;
	int32_t v;

	if (num < 0) {
		num = -num;
		neg = 1;
	}
	
	if (den < 0) {
		den = -den;
		neg ^= 1;
	}

	v = __udivmodsi4(num, den, 0);
	
	if (neg)
		v = -v;

	return v;
}

int32_t __modsi3(int32_t num, int32_t den)
{
	int32_t neg = 0;
	int32_t v;

	if (num < 0) {
		num = -num;
		neg = 1;
	}
	
	if (den < 0) {
		den = -den;
		neg ^= 1;
	}

	v = __udivmodsi4(num, den, 1);
	
	if (neg)
		v = -v;

	return v;
}

uint32_t __udivsi3(uint32_t num, uint32_t den)
{
	return __udivmodsi4(num, den, 0);
}

uint32_t __umodsi3(uint32_t num, uint32_t den)
{
	return __udivmodsi4(num, den, 1);
}

uint64_t __ashldi3(uint64_t v, int32_t cnt)
{
	int32_t c = cnt & 31;
	uint32_t vl = (uint32_t)v;
	uint32_t vh = (uint32_t)(v >> 32);

	if (cnt & 32) {
		vh = (vl << c);
		vl = 0;
	} else {
		vh = (vh << c) + (vl >> (32 - c));
		vl = (vl << c);
	}

	return ((uint64_t)vh << 32) + vl;
}

uint64_t __ashrdi3(uint64_t v, int32_t cnt)
{
	int32_t c = cnt & 31;
	uint32_t vl = (uint32_t)v;
	uint32_t vh = (uint32_t)(v >> 32);

	if (cnt & 32) {
		vl = (int32_t)vh >> c;
		vh = (int32_t)vh >> 31;
	} else {
		vl = (vl >> c) + (vh << (32 - c));
		vh = (int32_t)vh >> c;
	}

	return ((uint64_t)vh << 32) + vl;
}

uint64_t __lshrdi3(uint64_t v, int32_t cnt)
{
	int32_t c = cnt & 31;
	uint32_t vl = (uint32_t)v;
	uint32_t vh = (uint32_t)(v >> 32);

	if (cnt & 32) {
		vl = (vh >> c);
		vh = 0;
	} else {
		vl = (vl >> c) + (vh << (32 - c));
		vh = (vh >> c);
	}

	return ((uint64_t)vh << 32) + vl;
}

uint64_t __udivmoddi4(uint64_t num, uint64_t den, int8_t mod)
{
	uint64_t quot = 0, qbit = 1;

	if (den == 0)
		return 0;

	while ((int64_t) den >= 0) {
		den <<= 1;
		qbit <<= 1;
	}

	while (qbit) {
		if (den <= num) {
			num -= den;
			quot += qbit;
		}
		den >>= 1;
		qbit >>= 1;
	}

	if (mod)
		return num;

	return quot;
}

uint64_t __umoddi3(uint64_t num, uint64_t den)
{
	return __udivmoddi4(num, den, 1);
}

uint64_t __udivdi3(uint64_t num, uint64_t den)
{
	return __udivmoddi4(num, den, 0);
}

int64_t __moddi3(int64_t num, int64_t den)
{
	int32_t neg = 0;
	int64_t v;

	if (num < 0) {
		num = -num;
		neg = 1;
	}
	
	if (den < 0) {
		den = -den;
		neg ^= 1;
	}

	v = __udivmoddi4(num, den, 1);
	
	if (neg)
		v = -v;

	return v;
}

int64_t __divdi3(int64_t num, int64_t den)
{
	int32_t neg = 0;
	int64_t v;

	if (num < 0) {
		num = -num;
		neg = 1;
	}
	
	if (den < 0) {
		den = -den;
		neg ^= 1;
	}

	v = __udivmoddi4(num, den, 0);
	
	if (neg)
		v = -v;

	return v;
}
