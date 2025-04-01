#!/usr/bin/env python3

import sys
import re
from collections import defaultdict

def parse_benchmark_output(lines):
    results = defaultdict(dict)
    
    for line in lines:
        line = line.strip()
        if not (line.startswith("BM_ManagedEntitySimulation") or line.startswith("BM_SharedPtrSimulation")):
            continue
            
        # Parse benchmark line
        match = re.match(r'BM_(\w+)Simulation/(\d+)/(\d+)/real_time\s+\d+ ns\s+\d+ ns\s+\d+\s+Tick_Rate=([^/]+)/s\s+Visit_Rate=([^/]+)/s', line)
        if match:
            simulation_type, depth, ticks, tick_rate, visit_rate = match.groups()
            
            # Use depth/ticks as the key
            key = f"{depth}/{ticks}"
            
            # Store results
            results[key][simulation_type] = {
                'tick_rate': tick_rate,
                'visit_rate': visit_rate
            }
    
    return results

def calculate_percent_change(old_value, new_value):
    # Parse values that might have 'k' suffix
    def parse_value(val):
        if val.endswith('k'):
            return float(val[:-1]) * 1000
        if val.endswith('M'):
            return float(val[:-1]) * 1000000
        return float(val)
    
    old_val = parse_value(old_value)
    new_val = parse_value(new_value)
    
    percent_change = ((new_val - old_val) / old_val) * 100
    
    # Format with plus sign for positive values
    return f"+{percent_change:.2f}%" if percent_change > 0 else f"{percent_change:.2f}%"

def generate_markdown_table(results):
    # Table header
    table = "| Depth/Ticks | Tick/s (SharedPtr) | Tick/s (ManagedEntity) | %-Change (Tick/s) | Visit/s (SharedPtr) | Visit/s (ManagedEntity) | %-Change (Visit/s) |\n"
    table += "|-------------|--------------------|------------------------|-------------------|---------------------|-------------------------|--------------------|"
    
    # Sort keys to ensure consistent ordering
    # First by depth, then by ticks
    def sort_key(k):
        depth, ticks = map(int, k.split('/'))
        return (depth, ticks)
    
    sorted_keys = sorted(results.keys(), key=sort_key)
    
    for key in sorted_keys:
        if 'SharedPtr' not in results[key] or 'ManagedEntity' not in results[key]:
            continue
            
        shared_ptr = results[key]['SharedPtr']
        managed_entity = results[key]['ManagedEntity']
        
        # Calculate percentage changes
        tick_rate_change = calculate_percent_change(shared_ptr['tick_rate'], managed_entity['tick_rate'])
        visit_rate_change = calculate_percent_change(shared_ptr['visit_rate'], managed_entity['visit_rate'])
        
        # Add row to table
        table += f"\n| {key} | {shared_ptr['tick_rate']} | {managed_entity['tick_rate']} | {tick_rate_change} | {shared_ptr['visit_rate']} | {managed_entity['visit_rate']} | {visit_rate_change} |"
    
    return table

def main():
    # Read input from stdin if no file is provided
    if len(sys.argv) > 1:
        with open(sys.argv[1], 'r') as f:
            lines = f.readlines()
    else:
        lines = sys.stdin.readlines()
    
    results = parse_benchmark_output(lines)
    markdown_table = generate_markdown_table(results)
    print(markdown_table)

if __name__ == "__main__":
    main()
