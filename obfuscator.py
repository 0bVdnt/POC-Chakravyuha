#!/usr/bin/env python3
"""
Simple LLVM-based Code Obfuscator - Working Version
Fixed path issues and simplified approach
"""

import os
import sys
import subprocess
import argparse
import json
import random
import tempfile
from datetime import datetime
from pathlib import Path

class SimpleObfuscator:
    def __init__(self):
        self.stats = {
            'original_size': 0,
            'obfuscated_size': 0,
            'bogus_functions': 0,
            'string_encryptions': 0,
            'fake_calls': 0,
            'cycles': 0
        }
        
    def find_clang(self):
        """Find clang compiler"""
        possible_paths = [
            'clang',
            '/usr/bin/clang',
            '/usr/local/bin/clang',
            'clang-15',
            'clang-14',
            'clang-13',
            '/home/siddhesh/dev/llvm-project/build/bin/clang'
        ]
        
        for clang_path in possible_paths:
            try:
                result = subprocess.run([clang_path, '--version'], 
                                      capture_output=True, text=True)
                if result.returncode == 0:
                    print(f"Found clang: {clang_path}")
                    return clang_path
            except FileNotFoundError:
                continue
        
        raise RuntimeError("Clang compiler not found!")
    
    def generate_random_name(self, prefix="obf"):
        """Generate random identifier name"""
        return f"_{prefix}_{random.randint(1000, 9999)}"
    
    def add_bogus_functions(self, source, count=3):
        """Add bogus functions that do nothing useful"""
        bogus_code = "\n// === BOGUS FUNCTIONS ===\n"
        
        for i in range(count):
            func_name = self.generate_random_name("bogus_func")
            
            bogus_func = f"""
static int {func_name}(int x) {{
    int result = x;
    for(int i = 0; i < 5; i++) {{
        result = result * 2 + 1;
        result = result % 1000;
    }}
    return result;
}}
"""
            bogus_code += bogus_func
            self.stats['bogus_functions'] += 1
        
        # Insert before main function
        if 'int main(' in source:
            parts = source.split('int main(')
            return parts[0] + bogus_code + '\nint main(' + parts[1]
        else:
            return bogus_code + source
    
    def add_fake_calls(self, source):
        """Add calls to bogus functions"""
        lines = source.split('\n')
        bogus_funcs = []
        
        for line in lines:
            if 'static int _bogus_func_' in line and '(' in line:
                func_name = line.split('static int ')[1].split('(')[0].strip()
                bogus_funcs.append(func_name)
        
        if not bogus_funcs:
            return source
        
        # Add fake calls in main function
        fake_call_code = f"""
    // Fake calls to confuse analysis
    volatile int _fake_result = 0;
"""
        
        for func in bogus_funcs[:2]:  # Only use first 2 functions
            fake_call_code += f"    _fake_result += {func}({random.randint(1, 10)});\n"
            self.stats['fake_calls'] += 1
        
        # Insert after first opening brace of main
        if 'int main(' in source and '{' in source:
            main_start = source.find('int main(')
            brace_pos = source.find('{', main_start)
            if brace_pos != -1:
                source = source[:brace_pos+1] + fake_call_code + source[brace_pos+1:]
        
        return source
    
    def obfuscate_strings(self, source):
        """Obfuscate string literals to hide them from 'strings' command"""
        import re
        
        def encrypt_string(match):
            original_string = match.group(1)
            
            # Skip empty strings
            if len(original_string) == 0:
                return match.group(0)
            
            # Skip printf format strings (containing %)
            if '%' in original_string:
                return match.group(0)
            
            # Skip very short strings that might be important
            if len(original_string) < 2:
                return match.group(0)
            
            # Generate a random key for XOR encryption
            key = random.randint(1, 255)
            
            # Encrypt each character
            encrypted_bytes = []
            for char in original_string:
                encrypted_bytes.append(ord(char) ^ key)
            
            # Generate a unique variable name
            var_name = self.generate_random_name("str")
            
            # Create the decryption code
            decrypt_code = f'''({{
    unsigned char {var_name}[] = {{{', '.join(str(b) for b in encrypted_bytes)}, 0}};
    for(int _i = 0; _i < {len(original_string)}; _i++) {{
        {var_name}[_i] ^= {key};
    }}
    (char*){var_name};
}})'''
            
            self.stats['string_encryptions'] += 1
            print(f"    Encrypted string: '{original_string}' -> key {key}")
            
            return decrypt_code
        
        # Simple approach: find all string literals, but skip include statements
        lines = source.split('\n')
        result_lines = []
        
        for line in lines:
            # Skip #include lines completely
            if line.strip().startswith('#include'):
                result_lines.append(line)
                continue
            
            # Apply string obfuscation to other lines
            pattern = r'"([^"]*)"'
            obfuscated_line = re.sub(pattern, encrypt_string, line)
            result_lines.append(obfuscated_line)
        
        return '\n'.join(result_lines)

    def obfuscate_source(self, input_file, cycles=1, enable_string_obfuscation=True):
        """Main obfuscation function"""
        print(f"Reading: {input_file}")
        
        with open(input_file, 'r') as f:
            source = f.read()
        
        self.stats['original_size'] = len(source)
        
        # Add required headers
        headers = '''#include <stdio.h>
#include <stdlib.h>
#include <string.h>

'''
        
        # Only add headers if not already present
        if '#include <stdio.h>' not in source:
            source = headers + source
        
        print(f"Starting obfuscation with {cycles} cycles...")
        
        # Apply string obfuscation first (before adding bogus functions)
        if enable_string_obfuscation:
            print("  Applying string obfuscation...")
            source = self.obfuscate_strings(source)
        
        for cycle in range(cycles):
            print(f"  Cycle {cycle + 1}")
            
            # Add bogus functions
            source = self.add_bogus_functions(source, 2)
            
            # Add fake calls
            source = self.add_fake_calls(source)
            
            self.stats['cycles'] += 1
        
        self.stats['obfuscated_size'] = len(source)
        return source
    
    def compile_code(self, source_file, output_binary, platform='linux'):
        """Compile the obfuscated code"""
        clang = self.find_clang()
        
        # Use absolute paths to avoid path issues
        abs_source = os.path.abspath(source_file)
        abs_output = os.path.abspath(output_binary)
        
        print(f"Checking source file: {abs_source}")
        print(f"File exists: {os.path.exists(abs_source)}")
        
        if not os.path.exists(abs_source):
            print(f"ERROR: Source file {abs_source} does not exist!")
            return None
        
        cmd = [clang, abs_source, '-o', abs_output]
        
        # Add platform-specific flags
        if platform == 'windows':
            cmd = [clang, '-target', 'x86_64-w64-mingw32', abs_source, '-o', abs_output + '.exe']
            abs_output += '.exe'
        
        print(f"Compiling: {' '.join(cmd)}")
        
        try:
            result = subprocess.run(cmd, capture_output=True, text=True)
            if result.returncode == 0:
                print("Compilation successful!")
                print(f"Binary created at: {abs_output}")
                return abs_output
            else:
                print(f"Compilation failed:")
                print(f"STDOUT: {result.stdout}")
                print(f"STDERR: {result.stderr}")
                return None
        except Exception as e:
            print(f"Compilation error: {e}")
            return None
    
    def generate_report(self, input_file, output_file, options):
        """Generate obfuscation report"""
        report = {
            'timestamp': datetime.now().isoformat(),
            'input_parameters': {
                'input_file': input_file,
                'output_file': output_file,
                'cycles': options.cycles,
                'platform': options.platform
            },
            'output_attributes': {
                'original_size_bytes': self.stats['original_size'],
                'obfuscated_size_bytes': self.stats['obfuscated_size'],
                'size_increase_factor': round(self.stats['obfuscated_size'] / self.stats['original_size'], 2) if self.stats['original_size'] > 0 else 0
            },
            'obfuscation_statistics': {
                'bogus_functions_added': self.stats['bogus_functions'],
                'string_encryptions': self.stats['string_encryptions'],
                'fake_calls_inserted': self.stats['fake_calls'],
                'obfuscation_cycles_completed': self.stats['cycles']
            },
            'methods_used': [
                'Bogus Function Insertion',
                'Fake Call Injection',
                'String Encryption' if self.stats['string_encryptions'] > 0 else None
            ]
        }
        
        # Remove None values from methods used
        report['methods_used'] = [m for m in report['methods_used'] if m is not None]
        
        return report

def main():
    parser = argparse.ArgumentParser(description='Simple LLVM Obfuscator')
    parser.add_argument('input_file', help='Input C source file')
    parser.add_argument('-o', '--output', help='Output obfuscated file')
    parser.add_argument('-b', '--binary', help='Output binary file')
    parser.add_argument('-c', '--cycles', type=int, default=1, help='Obfuscation cycles')
    parser.add_argument('-p', '--platform', choices=['linux', 'windows'], default='linux')
    parser.add_argument('--no-strings', action='store_true', help='Disable string obfuscation')
    
    args = parser.parse_args()
    
    if not os.path.exists(args.input_file):
        print(f"Error: Input file {args.input_file} not found!")
        sys.exit(1)
    
    # Set default output names
    base_name = os.path.splitext(args.input_file)[0]
    if not args.output:
        args.output = f"{base_name}_obfuscated.c"
    if not args.binary:
        args.binary = f"{base_name}_obfuscated"
    
    try:
        obfuscator = SimpleObfuscator()
        
        # Step 1: Obfuscate source
        print("=== STEP 1: Source Code Obfuscation ===")
        enable_strings = not args.no_strings
        obfuscated_source = obfuscator.obfuscate_source(args.input_file, args.cycles, enable_strings)
        
        # Write obfuscated source
        with open(args.output, 'w') as f:
            f.write(obfuscated_source)
        print(f"Obfuscated source saved to: {args.output}")
        
        # Step 2: Compile to binary
        print("\n=== STEP 2: Compilation ===")
        binary_path = obfuscator.compile_code(args.output, args.binary, args.platform)
        
        # Step 3: Generate report
        print("\n=== STEP 3: Report Generation ===")
        report = obfuscator.generate_report(args.input_file, args.output, args)
        
        # Save report
        report_file = f"{base_name}_report.json"
        with open(report_file, 'w') as f:
            json.dump(report, f, indent=2)
        
        # Print summary
        print("\n" + "="*50)
        print("OBFUSCATION SUMMARY")
        print("="*50)
        print(f"Input file: {args.input_file}")
        print(f"Obfuscated source: {args.output}")
        print(f"Binary: {binary_path if binary_path else 'Failed'}")
        print(f"Report: {report_file}")
        print(f"Original size: {obfuscator.stats['original_size']} bytes")
        print(f"Obfuscated size: {obfuscator.stats['obfuscated_size']} bytes")
        print(f"Bogus functions: {obfuscator.stats['bogus_functions']}")
        print(f"Fake calls: {obfuscator.stats['fake_calls']}")
        print(f"String encryptions: {obfuscator.stats['string_encryptions']}")
        print(f"Cycles completed: {obfuscator.stats['cycles']}")
        print("="*50)
        
        if binary_path and os.path.exists(binary_path):
            print(f"\nTest the binary: {binary_path}")
            print("Run it with: ./" + os.path.basename(binary_path))
            
            # Test string obfuscation effectiveness
            if obfuscator.stats['string_encryptions'] > 0:
                print(f"\nTo test string obfuscation:")
                print(f"  Original: strings {args.input_file}")
                print(f"  Obfuscated: strings {binary_path}")
                print(f"  Compare the outputs - strings should be hidden!")
        
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

if __name__ == '__main__':
    main()