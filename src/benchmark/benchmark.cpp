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
  size_t birth_tick; // Changed from age to birth_tick
  std::array<std::optional<testobj_ref>, 2> children;
  
  TestObjectManaged(size_t birth_tick, 
                  std::optional<testobj_ref> child_1, 
                  std::optional<testobj_ref> child_2)
      : birth_tick(birth_tick), children{child_1, child_2} {}
};

// Test object using shared_ptr for references
struct TestObjectSharedPtr {
  size_t birth_tick; // Changed from age to birth_tick
  std::array<std::optional<std::shared_ptr<const TestObjectSharedPtr>>, 2> children;
  
  TestObjectSharedPtr(size_t birth_tick, 
                     std::optional<std::shared_ptr<const TestObjectSharedPtr>> child_1, 
                     std::optional<std::shared_ptr<const TestObjectSharedPtr>> child_2)
      : birth_tick(birth_tick), children{child_1, child_2} {}
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
simulateSharedPtrTick(std::optional<std::shared_ptr<const TestObjectSharedPtr>> node, size_t current_tick, size_t& objects_created) {
    if (!node) {
        return std::nullopt;
    }
    
    // Calculate age based on birth_tick and current_tick
    size_t age = (current_tick - node.value()->birth_tick) % MAX_AGE;
    
    // Process children
    auto new_left = simulateSharedPtrTick(node.value()->children[0], current_tick, objects_created);
    auto new_right = simulateSharedPtrTick(node.value()->children[1], current_tick, objects_created);
        
    bool needs_replacement = (age >= MAX_AGE - 1); // Replace if at max age
    
    if ((new_left.has_value() != node.value()->children[0].has_value() || 
         (new_left.has_value() && *new_left != *node.value()->children[0])) ||
        (new_right.has_value() != node.value()->children[1].has_value() || 
         (new_right.has_value() && *new_right != *node.value()->children[1])) ||
        needs_replacement) {
        // Create a new object with the current tick as birth_tick if the object reached max age
        size_t new_birth_tick = needs_replacement ? current_tick : node.value()->birth_tick;
        objects_created++; // Increment the passed counter instead of the thread_local
        return std::make_shared<TestObjectSharedPtr>(new_birth_tick, new_left, new_right);
    }
    
    // Return the same object if no changes are needed
    return node;
}

// Simulate one tick using ManagedEntity implementation
std::optional<testobj_ref> 
simulateManagedEntityTick(std::optional<testobj_ref> node, size_t current_tick, size_t& objects_created) {
    if (!node) {
        return std::nullopt;
    }
    
    // Calculate age based on birth_tick and current_tick
    size_t age = (current_tick - node.value()->birth_tick) % MAX_AGE;
    
    // Process children
    auto new_left = simulateManagedEntityTick(node.value()->children[0], current_tick, objects_created);
    auto new_right = simulateManagedEntityTick(node.value()->children[1], current_tick, objects_created);
        
    bool needs_replacement = (age >= MAX_AGE - 1); // Replace if at max age
    
    if ((new_left.has_value() != node.value()->children[0].has_value() || 
         (new_left.has_value() && new_left.value() != node.value()->children[0].value())) ||
        (new_right.has_value() != node.value()->children[1].has_value() || 
         (new_right.has_value() && new_right.value() != node.value()->children[1].value())) ||
        needs_replacement) {
        // Create a new object with the current tick as birth_tick if the object reached max age
        size_t new_birth_tick = needs_replacement ? current_tick : node.value()->birth_tick;
        objects_created++; // Increment the passed counter instead of the thread_local
        return testobj_storage::make_entity({new_birth_tick, new_left, new_right});
    }

    // No changes needed, return the same object
    return node;
}

thread_local size_t observable = 0;
void visitSharedPtrTreeNode(std::optional<std::shared_ptr<const TestObjectSharedPtr>> node) {
        if (!node) return;
    
    // Calculate age based on birth_tick and current tick
    observable = node.value()->birth_tick;
    
    // Visit children
    visitSharedPtrTreeNode(node.value()->children[0]);
    visitSharedPtrTreeNode(node.value()->children[1]);
}

void visitManagedEntityTreeNode(std::optional<testobj_ref> const& node) {
        if (!node) return;
    
    observable = node.value()->birth_tick;
    // Visit children
    visitManagedEntityTreeNode(node.value()->children[0]);
    visitManagedEntityTreeNode(node.value()->children[1]);
}

// Benchmark for shared_ptr implementation
static void BM_SharedPtrSimulation(benchmark::State& state) {

  size_t sharedptr_tick_count = 0;
  size_t sharedptr_visit_count = 0;
  size_t total_objects_created = 0;
  
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
      while (running.load()) {
        sharedptr_visit_count++;
        visitSharedPtrTreeNode(get_root_ref());
      }
    });


    // Run simulation for a fixed number of ticks
    for (size_t i = 0; i < ticks; ++i) {
      sharedptr_tick_count++;
      set_root_ref(simulateSharedPtrTick(get_root_ref(), MAX_AGE + i, total_objects_created).value());
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
  state.counters["Objects_Created"] = benchmark::Counter(
    total_objects_created,
    benchmark::Counter::kIsRate | benchmark::Counter::kAvgThreads
  );
}

// Benchmark for ManagedEntity implementation
static void BM_ManagedEntitySimulation(benchmark::State& state) {
  
  size_t managed_entity_tick_count = 0;
  size_t managed_entity_visit_count = 0;
  size_t total_objects_created = 0;

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
      set_root_ref(simulateManagedEntityTick(get_root_ref(), MAX_AGE + i, total_objects_created).value());
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
  state.counters["Objects_Created"] = benchmark::Counter(
    total_objects_created,
    benchmark::Counter::kIsRate | benchmark::Counter::kAvgThreads
  );
}

// Register benchmarks with different tree depths
BENCHMARK(BM_ManagedEntitySimulation)
  ->Ranges({{1, 15}, {1, 2000}})
  ->UseRealTime()
  ->DisplayAggregatesOnly(true);
BENCHMARK(BM_SharedPtrSimulation)
  ->Ranges({{1, 15}, {1, 2000}})
  ->UseRealTime()
  ->DisplayAggregatesOnly(true);

BENCHMARK_MAIN();
