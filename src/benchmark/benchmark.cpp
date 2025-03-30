#include <benchmark/benchmark.h>
#include <cpioo/managed_entity.hpp>
#include <vector>
#include <memory>
#include <random>
#include <thread>
#include <numeric>
#include <atomic>
#include <mutex>

// Maximum age before wrapping back to 0
const size_t MAX_AGE = 100;

// Forward declaration for TestObjectManaged for use in storage type
struct TestObjectManaged;

// Define storage type and reference type directly
using testobj_storage = cpioo::managed_entity::storage<TestObjectManaged, 32 - 6, int>;
using testobj_ref = cpioo::managed_entity::reference<testobj_storage>;

// Test object using managed_entity for references
struct TestObjectManaged {
  size_t age;
  std::array<std::optional<testobj_ref>, 2> children;
  
  TestObjectManaged(size_t age, 
                  std::optional<testobj_ref> child_1, 
                  std::optional<testobj_ref> child_2)
      : age(age), children{child_1, child_2} {}
};

// Test object using shared_ptr for references
struct TestObjectSharedPtr {
  size_t age;
  std::array<std::optional<std::shared_ptr<const TestObjectSharedPtr>>, 2> children;
  
  TestObjectSharedPtr(size_t age, 
                     std::optional<std::shared_ptr<const TestObjectSharedPtr>> child_1, 
                     std::optional<std::shared_ptr<const TestObjectSharedPtr>> child_2)
      : age(age), children{child_1, child_2} {}
};

// Create a deeply nested tree using shared_ptr
std::optional<std::shared_ptr<const TestObjectSharedPtr>>
createSharedPtrTree(size_t depth, size_t& current_age) {
    if (depth == 0) {
        return std::nullopt;
    }
    // Create parent with current age
    current_age = (current_age + 1) % MAX_AGE;
    
    // First create children
    auto left_child = createSharedPtrTree(depth - 1, current_age);
    auto right_child = createSharedPtrTree(depth - 1, current_age);
    
    return std::make_shared<TestObjectSharedPtr>(current_age, left_child, right_child);
}

// Create a deeply nested tree using ManagedEntity
std::optional<testobj_ref> 
createManagedEntityTree(size_t depth, size_t& current_age) {
    if (depth == 0) {
        return std::nullopt;
    }
    // Create parent with current age
    current_age = (current_age + 1) % MAX_AGE;
    
    // First create children
    auto left_child = createManagedEntityTree(depth - 1, current_age);
    auto right_child = createManagedEntityTree(depth - 1, current_age);
    
    return testobj_storage::make_entity({ current_age, left_child, right_child });
}

// Simulate one tick using shared_ptr implementation
std::optional<std::shared_ptr<const TestObjectSharedPtr>>
simulateSharedPtrTick(std::optional<std::shared_ptr<const TestObjectSharedPtr>> node) {
        if (!node) {
        return std::nullopt;
    }
    
    auto new_left = simulateSharedPtrTick(node.value()->children[0]);
    auto new_right = simulateSharedPtrTick(node.value()->children[1]);
    
    // Calculate new age
    size_t new_age = (node.value()->age + 1) % MAX_AGE;
    
    return std::make_shared<TestObjectSharedPtr>(new_age, new_left, new_right);
}

// Simulate one tick using ManagedEntity implementation
std::optional<testobj_ref> 
simulateManagedEntityTick(std::optional<testobj_ref> node) {
        if (!node) {
        return std::nullopt;
    }
    
    // Update children first (depth-first)
    auto new_left = simulateManagedEntityTick(node.value()->children[0]);
    auto new_right = simulateManagedEntityTick(node.value()->children[1]);
    
    // Calculate new age
    size_t new_age = (node.value()->age + 1) % MAX_AGE;
    
    // Always create a new node since we're using const objects
    return testobj_storage::make_entity({new_age, new_left, new_right});
}

thread_local size_t observable = 0;
void visitSharedPtrTreeNode(std::optional<std::shared_ptr<const TestObjectSharedPtr>> node) {
        if (!node) return;
    
    observable = node.value()->age;
    // Visit children
    visitSharedPtrTreeNode(node.value()->children[0]);
    visitSharedPtrTreeNode(node.value()->children[1]);
}

void visitManagedEntityTreeNode(std::optional<testobj_ref> const& node) {
        if (!node) return;
    
    observable = node.value()->age;
    // Visit children
    visitManagedEntityTreeNode(node.value()->children[0]);
    visitManagedEntityTreeNode(node.value()->children[1]);
}

// Benchmark for shared_ptr implementation
static void BM_SharedPtrSimulation(benchmark::State& state) {

  size_t sharedptr_tick_count = 0;
  size_t sharedptr_visit_count = 0;
  for (auto _ : state) {
    state.PauseTiming();
    
    
    const size_t depth = state.range(0);
    const size_t ticks = state.range(1);
    size_t current_age = 0;
    auto root = createSharedPtrTree(depth, current_age).value();
    std::atomic<bool> running{true};
    std::mutex root_mutex;

    auto get_root_ref = [&]() {
      std::lock_guard<std::mutex> lock(root_mutex);
      return root;
    };
    auto set_root_ref = [&](std::shared_ptr<const TestObjectSharedPtr> new_root) {
      std::lock_guard<std::mutex> lock(root_mutex);
      root = new_root;
    };

    state.ResumeTiming();

    // Start consumer thread
    std::thread consumer_thread([&]() {
      while (running.load()) {}
        sharedptr_visit_count++;
        visitSharedPtrTreeNode(get_root_ref());
      });


    // Run simulation for a fixed number of ticks
    for (size_t i = 0; i < ticks; ++i) {
      sharedptr_tick_count++;
      set_root_ref(simulateSharedPtrTick(get_root_ref()).value());
    }
    testobj_storage::return_free_pool_to_global();
    
    // Stop consumer thread
    running.store(false);
    consumer_thread.join();
    
  }
  // Add rate metrics with display flags to show as columns
  state.counters["Tick_Rate"] = benchmark::Counter(
    sharedptr_tick_count, 
    benchmark::Counter::kIsRate | benchmark::Counter::kAvgThreads
  );
  state.counters["Visit_Rate"] = benchmark::Counter(
    sharedptr_visit_count, 
    benchmark::Counter::kIsRate | benchmark::Counter::kAvgThreads
  );
}

// Benchmark for ManagedEntity implementation
static void BM_ManagedEntitySimulation(benchmark::State& state) {
  
  size_t managed_entity_tick_count = 0;
  size_t managed_entity_visit_count = 0;

  for (auto _ : state) {
    state.PauseTiming();
    
    
    const size_t depth = state.range(0);
    const size_t ticks = state.range(1);
    size_t current_age = 0;
    // Setup tree
    auto root = std::make_unique<testobj_ref>(createManagedEntityTree(depth, current_age).value());
    std::atomic<bool> running{true};
    std::mutex root_mutex;

    auto get_root_ref = [&]() {
      std::lock_guard<std::mutex> lock(root_mutex);
      return *root;
    };
    auto set_root_ref = [&](const testobj_ref& new_root) {
      std::lock_guard<std::mutex> lock(root_mutex);
      root = std::make_unique<testobj_ref>(new_root);
    };
    
    state.ResumeTiming();
    
    // Start consumer thread
    std::thread consumer_thread([&]() {
      while (running.load()) {
        managed_entity_visit_count++;
        visitManagedEntityTreeNode(get_root_ref());
      }
    });
    
    // Run simulation for a fixed number of ticks
    for (size_t i = 0; i < ticks; ++i) {
      managed_entity_tick_count++;
      set_root_ref(simulateManagedEntityTick(get_root_ref()).value());
    }
    
    // Stop consumer thread
    running.store(false);
    consumer_thread.join();
    
  }
  // Add rate metrics with display flags to show as columns
  state.counters["Tick_Rate"] = benchmark::Counter(
    managed_entity_tick_count, 
    benchmark::Counter::kIsRate | benchmark::Counter::kAvgThreads
  );
  state.counters["Visit_Rate"] = benchmark::Counter(
    managed_entity_visit_count, 
    benchmark::Counter::kIsRate | benchmark::Counter::kAvgThreads
  );
}

// Register benchmarks with different tree depths
BENCHMARK(BM_ManagedEntitySimulation)
  ->Ranges({{1, 10}, {1, 200}})
  ->UseRealTime()
  ->DisplayAggregatesOnly(true);
BENCHMARK(BM_SharedPtrSimulation)
  ->Ranges({{1, 10}, {1, 200}})
  ->UseRealTime()
  ->DisplayAggregatesOnly(true);

BENCHMARK_MAIN();
