#!/usr/bin/env python3

import sys
import json
import re
import os
from collections import defaultdict
import matplotlib.pyplot as plt
import numpy as np
import io

def parse_benchmark_json(json_data):
    results = defaultdict(dict)
    
    # Parse JSON data
    data = json.loads(json_data)
    
    for benchmark in data['benchmarks']:
        # Parse the name to extract simulation type and parameters
        name = benchmark['name']
        # Updated regex to match iterations:100 pattern
        match = re.match(r'BM_(\w+)Simulation/(\d+)/(\d+)/iterations:\d+/real_time', name)
        
        if not match:
            continue
            
        simulation_type, depth, ticks = match.groups()
        
        # Use depth/ticks as the key
        key = f"{depth}/{ticks}"
        
        # Store results with raw values for charting
        results[key][simulation_type] = {
            'tick_rate': benchmark.get('Tick_Rate', 0),
            'visit_rate': benchmark.get('Visit_Rate', 0),
            'objects_created': benchmark.get('Objects_Creation_Rate', 0),
            'tick_rate_formatted': format_value(benchmark.get('Tick_Rate', 0)),
            'visit_rate_formatted': format_value(benchmark.get('Visit_Rate', 0)),
            'objects_created_formatted': format_value(benchmark.get('Objects_Creation_Rate', 0))
        }
    
    return results

def format_value(value):
    """Format numeric values with k or M suffix based on size"""
    if value >= 1000000:
        return f"{value/1000000:.2f}M"
    elif value >= 1000:
        return f"{value/1000:.2f}k"
    else:
        return f"{value:.2f}"

def calculate_percent_change(old_value, new_value):
    # Parse values that might have 'k' or 'M' suffix
    def parse_value(val):
        if isinstance(val, (int, float)):
            return float(val)
        if isinstance(val, str):
            if val.endswith('k'):
                return float(val[:-1]) * 1000
            if val.endswith('M'):
                return float(val[:-1]) * 1000000
            return float(val)
        return 0.0
    
    old_val = parse_value(old_value)
    new_val = parse_value(new_value)
    
    if old_val == 0:
        return "N/A"  # Avoid division by zero
    
    percent_change = ((new_val - old_val) / old_val) * 100
    
    # Format with plus sign for positive values
    return f"+{percent_change:.2f}%" if percent_change > 0 else f"{percent_change:.2f}%"

def generate_markdown_table(results):
    # Table header
    table = "| Depth/Ticks | Tick/s (SharedPtr) | Tick/s (ManagedEntity) | %-Change (Tick/s) | Visit/s (SharedPtr) | Visit/s (ManagedEntity) | %-Change (Visit/s) | Objects/s (SharedPtr) | Objects/s (ManagedEntity) | %-Change (Objects/s) |\n"
    table += "|-------------|--------------------|-----------------------|-------------------|---------------------|-------------------------|--------------------|----------------------|--------------------------|----------------------|"
    
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
        objects_created_change = calculate_percent_change(shared_ptr['objects_created'], managed_entity['objects_created'])
        
        # Add row to table
        table += f"\n| {key} | {shared_ptr['tick_rate_formatted']} | {managed_entity['tick_rate_formatted']} | {tick_rate_change} | {shared_ptr['visit_rate_formatted']} | {managed_entity['visit_rate_formatted']} | {visit_rate_change} | {shared_ptr['objects_created_formatted']} | {managed_entity['objects_created_formatted']} | {objects_created_change} |"
    
    return table

def generate_bar_chart(results, metric, title, output_file):
    """Generate a bar chart comparing SharedPtr vs ManagedEntity for a specific metric"""
    # Sort keys to ensure consistent ordering
    def sort_key(k):
        depth, ticks = map(int, k.split('/'))
        return (depth, ticks)
    
    sorted_keys = sorted(results.keys(), key=sort_key)
    valid_keys = [k for k in sorted_keys if 'SharedPtr' in results[k] and 'ManagedEntity' in results[k]]
    
    if not valid_keys:
        return None
    
    # Set up the plot
    fig, ax = plt.subplots(figsize=(12, 6))
    
    # Set width of bars
    bar_width = 0.35
    
    # Set position of bars on x axis
    indices = np.arange(len(valid_keys))
    
    # Create bars
    shared_ptr_values = [results[k]['SharedPtr'][metric] for k in valid_keys]
    managed_entity_values = [results[k]['ManagedEntity'][metric] for k in valid_keys]
    
    # Create bars
    ax.bar(indices - bar_width/2, shared_ptr_values, bar_width, label='SharedPtr')
    ax.bar(indices + bar_width/2, managed_entity_values, bar_width, label='ManagedEntity')
    
    # Add labels, title and axis ticks
    ax.set_xlabel('Configuration (Depth/Ticks)')
    ax.set_ylabel('Operations per second')
    ax.set_title(title)
    ax.set_xticks(indices)
    ax.set_xticklabels(valid_keys)
    
    # Add a legend
    ax.legend()
    
    # Show values on top of bars
    for i, v in enumerate(shared_ptr_values):
        ax.text(i - bar_width/2, v * 1.02, format_value(v), ha='center')
    
    for i, v in enumerate(managed_entity_values):
        ax.text(i + bar_width/2, v * 1.02, format_value(v), ha='center')
    
    # Format y-axis with K and M suffixes
    ax.get_yaxis().set_major_formatter(
        plt.FuncFormatter(lambda x, p: format_value(x).replace('k', 'K'))
    )
    
    # Adjust layout to prevent clipping of labels
    plt.tight_layout()
    
    # Save to file
    plt.savefig(output_file, format='svg')
    plt.close(fig)
    
    return output_file

def update_readme(readme_path, table, chart_paths):
    """Update README.md with the new table and charts"""
    # Check if README.md exists
    if not os.path.exists(readme_path):
        print(f"README file not found at {readme_path}")
        return False
    
    # Read the current README
    with open(readme_path, 'r') as f:
        content = f.read()
    
    # Find the benchmark table section
    # Look for a marker or create a pattern to identify the section
    table_pattern = r'(## Benchmark Results\s*\n\s*\|[^\n]*\|[^\n]*\n\s*\|[-:\s|]*\|[^\n]*(?:\n\s*\|[^\n]*\|[^\n]*)*)'
    
    # Check if the pattern exists
    benchmark_section_match = re.search(table_pattern, content)
    
    # If the benchmark section exists, replace it
    if benchmark_section_match:
        updated_content = content.replace(benchmark_section_match.group(1), f"## Benchmark Results\n\n{table}")
    else:
        # If no benchmark section is found, append to the end
        updated_content = content + f"\n\n## Benchmark Results\n\n{table}\n"
    
    # Add charts section
    charts_section = "\n\n## Benchmark Charts\n\n"
    
    # Add each chart with its title
    charts_section += "### Tick Rate Comparison\n\n"
    charts_section += f"![Tick Rate Comparison](charts/{os.path.basename(chart_paths['tick_rate'])})\n\n"
    
    charts_section += "### Visit Rate Comparison\n\n"
    charts_section += f"![Visit Rate Comparison](charts/{os.path.basename(chart_paths['visit_rate'])})\n\n"
    
    charts_section += "### Object Creation Rate Comparison\n\n"
    charts_section += f"![Object Creation Rate Comparison](charts/{os.path.basename(chart_paths['objects_created'])})\n\n"
    
    # Check if charts section already exists
    charts_pattern = r'## Benchmark Charts\s*\n\s*'
    charts_section_match = re.search(charts_pattern, updated_content)
    
    if charts_section_match:
        # Replace the charts section
        end_of_charts = re.search(r'##(?! Benchmark Charts)', updated_content[charts_section_match.end():])
        if end_of_charts:
            end_idx = charts_section_match.end() + end_of_charts.start()
            updated_content = updated_content[:charts_section_match.start()] + charts_section + updated_content[end_idx:]
        else:
            updated_content = updated_content[:charts_section_match.start()] + charts_section
    else:
        # Append charts section
        updated_content += charts_section
    
    # Write the updated content back to the README
    with open(readme_path, 'w') as f:
        f.write(updated_content)
    
    print(f"Updated {readme_path} with benchmark results and charts")
    return True

def main():
    # Read input from stdin if no file is provided
    if len(sys.argv) > 1:
        benchmark_file = sys.argv[1]
        with open(benchmark_file, 'r') as f:
            json_data = f.read()
    else:
        json_data = sys.stdin.read()
    
    results = parse_benchmark_json(json_data)
    markdown_table = generate_markdown_table(results)
    
    # Print table to stdout
    print(markdown_table)
    
    # Determine project root directory (assuming script is in project/scripts/)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    
    # Define output paths
    readme_path = os.path.join(project_root, 'README.md')
    charts_dir = os.path.join(project_root, 'charts')
    
    # Create charts directory if it doesn't exist
    os.makedirs(charts_dir, exist_ok=True)
    
    # Generate charts
    chart_paths = {
        'tick_rate': os.path.join(charts_dir, 'tick_rate_comparison.svg'),
        'visit_rate': os.path.join(charts_dir, 'visit_rate_comparison.svg'),
        'objects_created': os.path.join(charts_dir, 'objects_created_comparison.svg')
    }
    
    generate_bar_chart(results, 'tick_rate', 'Tick Rate Comparison (ops/sec)', chart_paths['tick_rate'])
    generate_bar_chart(results, 'visit_rate', 'Visit Rate Comparison (ops/sec)', chart_paths['visit_rate'])
    generate_bar_chart(results, 'objects_created', 'Object Creation Rate Comparison (ops/sec)', chart_paths['objects_created'])
    
    # Update README with new table and charts
    update_readme(readme_path, markdown_table, chart_paths)

if __name__ == "__main__":
    main()
