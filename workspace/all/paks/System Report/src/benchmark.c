/**
 * benchmark.c - CPU benchmark for frequency scaling
 *
 * Simple CPU-bound workload using xorshift PRNG.
 * Used to discover hardware frequency steps and measure relative performance.
 *
 * Usage: benchmark <duration_ms> [warmup_ms]
 * Output: iterations duration_ms
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

// Tuning: iterations between time checks
// Higher = less overhead, but coarser time boundary
// 100 × 1000 ops = ~0.4ms between checks = ~0.25% overhead
#define BATCH_SIZE 100

// Prevent dead code elimination
volatile uint32_t sink = 0;

static uint64_t get_time_ms(void) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// CPU-bound workload: xorshift PRNG iterations
static uint32_t compute(uint32_t x) {
	for (int i = 0; i < 1000; i++) {
		x ^= (x << 13);
		x ^= (x >> 17);
		x ^= (x << 5);
	}
	return x;
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <duration_ms> [warmup_ms]\n", argv[0]);
		return 1;
	}

	int duration_ms = atoi(argv[1]);
	int warmup_ms = (argc >= 3) ? atoi(argv[2]) : 0;

	if (duration_ms <= 0) {
		fprintf(stderr, "Duration must be positive\n");
		return 1;
	}

	uint32_t acc = 12345;

	// Warmup phase (lets CPU frequency stabilize)
	if (warmup_ms > 0) {
		uint64_t end = get_time_ms() + warmup_ms;
		while (get_time_ms() < end) {
			acc = compute(acc);
		}
	}

	// Measurement phase
	uint64_t iterations = 0;
	uint64_t start = get_time_ms();
	uint64_t end_time = start + duration_ms;

	while (get_time_ms() < end_time) {
		for (int i = 0; i < BATCH_SIZE; i++) {
			acc = compute(acc);
		}
		iterations += BATCH_SIZE;
	}

	uint64_t actual = get_time_ms() - start;
	sink = acc;

	printf("%llu %llu\n", (unsigned long long)iterations, (unsigned long long)actual);
	return 0;
}
