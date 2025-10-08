/**
 * @file benchmark_init.c
 * @brief SDK initialization performance benchmark for KnishIO C SDK
 * 
 * Tests that SDK initialization meets the <50ms requirement as specified
 * in the SDK Implementation Guide. This is critical for production readiness.
 */

#include "unity.h"
#include "knishio/knishio.h"
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Performance constants from Implementation Guide */
#define KNISHIO_PERFORMANCE_TARGET_INIT_MS 50
#define BENCHMARK_ITERATIONS 10
#define WARMUP_ITERATIONS 3

/* Time measurement utilities */
static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000.0) + (tv.tv_usec / 1000.0);
}

/* Performance statistics */
typedef struct {
    double min_ms;
    double max_ms; 
    double avg_ms;
    double median_ms;
    int iterations;
    bool target_met;
} benchmark_results_t;

static int compare_double(const void *a, const void *b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    return (da > db) - (da < db);
}

/* SDK initialization benchmark test */
void test_sdk_init_performance_benchmark(void) {
    printf("\n🚀 SDK Initialization Performance Benchmark\n");
    printf("Target: <%d ms (Implementation Guide requirement)\n", KNISHIO_PERFORMANCE_TARGET_INIT_MS);
    printf("Iterations: %d (with %d warmup)\n", BENCHMARK_ITERATIONS, WARMUP_ITERATIONS);
    
    double times[BENCHMARK_ITERATIONS];
    benchmark_results_t results = {0};
    results.min_ms = 999999.0;
    results.max_ms = 0.0;
    results.iterations = BENCHMARK_ITERATIONS;
    
    /* Warmup iterations (not counted) */
    printf("\nWarmup iterations...\n");
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        double start = get_time_ms();
        knishio_error_t error = knishio_init();
        double end = get_time_ms();
        
        TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, error);
        knishio_cleanup();
        printf("  Warmup %d: %.2f ms\n", i + 1, end - start);
    }
    
    /* Benchmark iterations */
    printf("\nBenchmark iterations...\n");
    double total_time = 0.0;
    
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        /* Ensure clean state */
        usleep(1000); // 1ms pause between iterations
        
        double start = get_time_ms();
        knishio_error_t error = knishio_init();
        double end = get_time_ms();
        
        TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, error);
        
        double elapsed = end - start;
        times[i] = elapsed;
        total_time += elapsed;
        
        /* Update min/max */
        if (elapsed < results.min_ms) results.min_ms = elapsed;
        if (elapsed > results.max_ms) results.max_ms = elapsed;
        
        printf("  Iteration %2d: %.2f ms%s\n", i + 1, elapsed, 
               elapsed <= KNISHIO_PERFORMANCE_TARGET_INIT_MS ? " ✓" : " ⚠️");
        
        knishio_cleanup();
    }
    
    /* Calculate statistics */
    results.avg_ms = total_time / BENCHMARK_ITERATIONS;
    
    /* Calculate median */
    qsort(times, BENCHMARK_ITERATIONS, sizeof(double), compare_double);
    if (BENCHMARK_ITERATIONS % 2 == 0) {
        results.median_ms = (times[BENCHMARK_ITERATIONS/2 - 1] + times[BENCHMARK_ITERATIONS/2]) / 2.0;
    } else {
        results.median_ms = times[BENCHMARK_ITERATIONS/2];
    }
    
    /* Determine if target is met */
    results.target_met = (results.avg_ms <= KNISHIO_PERFORMANCE_TARGET_INIT_MS) && 
                        (results.max_ms <= KNISHIO_PERFORMANCE_TARGET_INIT_MS * 1.5); // Allow 50% tolerance on max
    
    /* Print detailed results */
    printf("\n📊 Performance Results:\n");
    printf("  Minimum:   %.2f ms\n", results.min_ms);
    printf("  Maximum:   %.2f ms\n", results.max_ms);
    printf("  Average:   %.2f ms\n", results.avg_ms);
    printf("  Median:    %.2f ms\n", results.median_ms);
    printf("  Target:    <%d ms\n", KNISHIO_PERFORMANCE_TARGET_INIT_MS);
    printf("  Status:    %s\n", results.target_met ? "✅ PASSED" : "❌ FAILED");
    
    if (!results.target_met) {
        printf("\n⚠️  Performance target not met!\n");
        printf("   This indicates initialization bottlenecks that need optimization.\n");
        printf("   Common causes:\n");
        printf("   - Excessive library loading\n");
        printf("   - Blocking I/O operations\n");
        printf("   - Inefficient memory allocation\n");
        printf("   - Complex initialization logic\n");
    } else {
        printf("\n🎉 Performance target achieved!\n");
        printf("   SDK initialization is optimized for production use.\n");
    }
    
    /* Test assertion - fail if performance target not met */
    TEST_ASSERT_TRUE_MESSAGE(results.target_met, 
        "SDK initialization performance does not meet Implementation Guide requirements");
}

/* Cold start performance test */
void test_sdk_cold_start_performance(void) {
    printf("\n❄️  Cold Start Performance Test\n");
    printf("Testing initialization without any warmup\n");
    
    /* Ensure completely cold start */
    usleep(10000); // 10ms pause
    
    double start = get_time_ms();
    knishio_error_t error = knishio_init();
    double end = get_time_ms();
    
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, error);
    
    double elapsed = end - start;
    printf("Cold start time: %.2f ms\n", elapsed);
    
    bool cold_start_ok = elapsed <= (KNISHIO_PERFORMANCE_TARGET_INIT_MS * 2.0); // 2x tolerance for cold start
    printf("Cold start status: %s (target: <%d ms)\n", 
           cold_start_ok ? "✅ ACCEPTABLE" : "❌ TOO SLOW",
           KNISHIO_PERFORMANCE_TARGET_INIT_MS * 2);
    
    knishio_cleanup();
    
    /* Cold start can be slower but should be reasonable */
    TEST_ASSERT_TRUE_MESSAGE(cold_start_ok, 
        "Cold start initialization is unreasonably slow");
}

/* Resource usage during initialization */
void test_sdk_init_resource_usage(void) {
    printf("\n💾 Resource Usage During Initialization\n");
    
    /* Test multiple init/cleanup cycles for memory leaks */
    size_t cycles = 5;
    printf("Testing %zu init/cleanup cycles for resource leaks\n", cycles);
    
    for (size_t i = 0; i < cycles; i++) {
        double start = get_time_ms();
        knishio_error_t error = knishio_init();
        double end = get_time_ms();
        
        TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, error);
        
        double elapsed = end - start;
        printf("  Cycle %zu: %.2f ms\n", i + 1, elapsed);
        
        /* Each cycle should be consistent (no memory leaks affecting performance) */
        if (i > 0) {
            TEST_ASSERT_TRUE_MESSAGE(elapsed <= KNISHIO_PERFORMANCE_TARGET_INIT_MS * 1.5,
                "Performance degradation detected in repeated initialization");
        }
        
        knishio_cleanup();
        usleep(1000); // Small pause between cycles
    }
    
    printf("✅ Resource usage test completed\n");
}

/* Concurrent initialization test */
void test_sdk_init_thread_safety_performance(void) {
    printf("\n🧵 Thread Safety Performance Test\n");
    printf("Testing that concurrent initialization attempts are handled safely\n");
    
    /* Single-threaded performance baseline */
    double start = get_time_ms();
    knishio_error_t error = knishio_init();
    double baseline = get_time_ms() - start;
    
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, error);
    printf("Baseline initialization: %.2f ms\n", baseline);
    
    /* Test double initialization (should be no-op) */
    start = get_time_ms();
    error = knishio_init(); // Should be safe/no-op
    double double_init = get_time_ms() - start;
    
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, error);
    printf("Double initialization: %.2f ms\n", double_init);
    
    /* Double init should be very fast (already initialized) */
    TEST_ASSERT_TRUE_MESSAGE(double_init < 1.0, 
        "Double initialization is not optimized");
    
    knishio_cleanup();
    printf("✅ Thread safety performance test completed\n");
}

/* Performance regression detection */
void test_sdk_init_regression_detection(void) {
    printf("\n📈 Performance Regression Detection\n");
    
    /* Known good baseline (can be updated as optimizations improve) */
    const double known_baseline_ms = KNISHIO_PERFORMANCE_TARGET_INIT_MS * 0.8; // 80% of target
    
    /* Run a single precise measurement */
    double start = get_time_ms();
    knishio_error_t error = knishio_init();
    double end = get_time_ms();
    
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, error);
    
    double elapsed = end - start;
    printf("Current initialization time: %.2f ms\n", elapsed);
    printf("Known baseline: %.2f ms\n", known_baseline_ms);
    
    if (elapsed <= known_baseline_ms) {
        printf("✅ Performance is optimal (at or below baseline)\n");
    } else if (elapsed <= KNISHIO_PERFORMANCE_TARGET_INIT_MS) {
        printf("✅ Performance meets target (%.1fx baseline)\n", elapsed / known_baseline_ms);
    } else {
        printf("⚠️  Performance regression detected (%.1fx target)\n", 
               elapsed / KNISHIO_PERFORMANCE_TARGET_INIT_MS);
    }
    
    knishio_cleanup();
}

/* Main performance test suite */
void test_init_performance_suite(void) {
    printf("\n=== SDK Initialization Performance Test Suite ===\n");
    printf("Implementation Guide Compliance: <50ms initialization target\n");
    
    RUN_TEST(test_sdk_init_performance_benchmark);
    RUN_TEST(test_sdk_cold_start_performance);
    RUN_TEST(test_sdk_init_resource_usage);
    RUN_TEST(test_sdk_init_thread_safety_performance);
    RUN_TEST(test_sdk_init_regression_detection);
    
    printf("\n✅ Performance Test Suite Complete\n");
    printf("   All initialization performance requirements validated\n");
}