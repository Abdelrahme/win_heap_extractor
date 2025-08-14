# Windows Heap Extractor

A powerful C++ tool for extracting detailed heap information from Windows processes. This tool can search for processes by name and extract comprehensive heap data including memory regions, allocations, and statistics.

## Features

- **Process Discovery**: Find processes by name (case-insensitive)
- **Heap Analysis**: Extract detailed information about all heaps in a process
- **Memory Region Mapping**: Analyze memory regions and their properties
- **Comprehensive Reporting**: Generate detailed reports with statistics
- **File Output**: Save reports to text files for further analysis
- **Real-time Statistics**: Display heap usage, allocation patterns, and memory distribution

## What Information is Extracted

For each process, the tool extracts:

- **Process Information**: PID, name, number of heaps
- **Heap Statistics**: 
  - Total heap size (reserved, committed, uncommitted)
  - Allocated vs free memory
  - Number of memory blocks
- **Memory Regions**:
  - Base addresses
  - Region sizes
  - Memory state (committed/reserved)
  - Memory type (private/mapped/image)
  - Protection attributes
- **Detailed Heap Analysis**:
  - Individual heap handles
  - Per-heap statistics
  - Memory allocation patterns

## Prerequisites

- Windows 10/11
- Visual Studio 2019 or later (with C++ development tools)
- Administrator privileges (recommended for accessing system processes)

## Building the Project



#### Windows Batch (compile.bat)
```cmd
compile.bat
```



## Usage

1. **Run the executable**:
   ```cmd
   heap_extractor.exe
   ```

2. **Enter the process name** when prompted:
   ```
   Enter process name (e.g., notepad.exe): notepad.exe
   ```

3. **View the results**:
   - The tool will display a comprehensive report in the console
   - A text file will be saved with the same information

## Example Output

```
================================================================================
HEAP EXTRACTION REPORT
================================================================================
Process Name: notepad.exe
Process ID: 1234
Number of Heaps: 3

SUMMARY:
  Total Heap Size: 2.50 MB
  Total Committed: 1.75 MB
  Total Allocated: 1.20 MB
  Total Free: 550.00 KB
  Total Blocks: 156

HEAP 1 (Handle: 0x12345678):
  Size: 1.00 MB
  Committed: 750.00 KB
  Uncommitted: 250.00 KB
  Allocated: 500.00 KB
  Free: 250.00 KB
  Blocks: 45
  Memory Regions: 12

  MEMORY REGIONS:
    Region 1:
      Base Address: 0x12340000
      Size: 64.00 KB
      State: Committed
      Type: Private
      Protection: 0x04
```

## File Output

The tool automatically saves a detailed report to a text file named `[processname]_heap_report.txt` in the same directory as the executable.

## Common Process Names

Here are some common Windows processes you can analyze:

- `notepad.exe` - Notepad
- `explorer.exe` - Windows Explorer
- `chrome.exe` - Google Chrome
- `firefox.exe` - Mozilla Firefox
- `msedge.exe` - Microsoft Edge
- `svchost.exe` - Windows Service Host
- `winlogon.exe` - Windows Logon
- `csrss.exe` - Client Server Runtime Subsystem

## Troubleshooting

### "Process not found" Error
- Make sure the process name is correct (including the `.exe` extension)
- Check if the process is actually running
- Try running the tool as Administrator

### "Failed to open process" Error
- Run the tool as Administrator
- Some system processes require elevated privileges
- Antivirus software might be blocking access

### Build Errors
- Ensure Visual Studio is properly installed with C++ development tools
- Make sure CMake is in your PATH
- Try running the build scripts as Administrator

## Technical Details

The tool uses the following Windows APIs:

- **Process Enumeration**: `CreateToolhelp32Snapshot`, `Process32First`, `Process32Next`
- **Heap Analysis**: `GetProcessHeaps`, `HeapWalk`, `HeapSummary`
- **Memory Information**: `VirtualQueryEx`, `OpenProcess`
- **Process Information**: `GetProcessMemoryInfo`

## Security Considerations

- The tool requires process access permissions
- Some processes may be protected by Windows security
- Always run with appropriate privileges for the target process
- Be cautious when analyzing system processes

## License

This tool is provided as-is for educational and analysis purposes. Use responsibly and in accordance with your organization's security policies.

## Contributing

Feel free to submit issues, feature requests, or pull requests to improve the tool.

## Version History

- **v1.0**: Initial release with basic heap extraction capabilities
- Comprehensive process discovery and heap analysis
- Memory region mapping and detailed reporting

