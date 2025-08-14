#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <memoryapi.h>
#include <algorithm>

#pragma comment(lib, "psapi.lib")

struct HeapInfo {
    HANDLE heapHandle;
    DWORD processId;
    SIZE_T heapSize;
    SIZE_T committedSize;
    SIZE_T uncommittedSize;
    SIZE_T allocatedSize;
    SIZE_T freeSize;
    DWORD blockCount;
    std::vector<MEMORY_BASIC_INFORMATION> regions;
    std::vector<std::string> extractedTexts;
};

struct ProcessInfo {
    DWORD processId;
    std::string processName;
    std::vector<HeapInfo> heaps;
    SIZE_T totalHeapSize;
    SIZE_T totalCommittedSize;
    SIZE_T totalAllocatedSize;
    SIZE_T totalFreeSize;
    DWORD totalBlockCount;
};

class WindowsHeapExtractor {
private:
    std::vector<ProcessInfo> processes;

    DWORD FindProcessIdByName(const std::string& processName) {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) {
            return 0;
        }

        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(hSnapshot, &pe32)) {
            do {
                // pe32.szExeFile is already a narrow string, no conversion needed
                std::string currentProcessName(pe32.szExeFile);
                
                if (_stricmp(currentProcessName.c_str(), processName.c_str()) == 0) {
                    CloseHandle(hSnapshot);
                    std::cout << "Process found: " << pe32.th32ProcessID << std::endl;
                    return pe32.th32ProcessID;
                }
            } while (Process32Next(hSnapshot, &pe32));
        }

        CloseHandle(hSnapshot);
        return 0;
    }

    bool GetHeapInformation(HANDLE hProcess, HANDLE hHeap, HeapInfo& heapInfo) {
        // This function is kept for compatibility but simplified
        heapInfo.heapHandle = hHeap;
        heapInfo.blockCount = 0;
        heapInfo.allocatedSize = 0;
        heapInfo.freeSize = 0;
        heapInfo.heapSize = 0;
        heapInfo.committedSize = 0;
        heapInfo.uncommittedSize = 0;
        return true;
    }

    bool IsPrintableText(const char* data, size_t size) {
        if (size < 4) return false;
        
        // Check if it looks like UTF-16 text
        bool hasNulls = false;
        bool hasPrintable = false;
        
        for (size_t i = 0; i < size && i < 100; i++) { // Check first 100 bytes
            if (data[i] == 0) {
                hasNulls = true;
            } else if (data[i] >= 32 && data[i] <= 126) {
                hasPrintable = true;
            }
        }
        
        return hasPrintable && hasNulls;
    }

    std::string ExtractUTF16Text(const char* data, size_t size) {
        std::string result;
        
        // Convert UTF-16 to UTF-8
        int wideSize = MultiByteToWideChar(CP_UTF8, 0, data, size, NULL, 0);
        if (wideSize > 0) {
            std::wstring wideStr(wideSize, 0);
            MultiByteToWideChar(CP_UTF8, 0, data, size, &wideStr[0], wideSize);
            
            int utf8Size = WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, NULL, 0, NULL, NULL);
            if (utf8Size > 0) {
                result.resize(utf8Size - 1);
                WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, &result[0], utf8Size, NULL, NULL);
            }
        }
        
        return result;
    }

    bool ExtractTextFromMemory(HANDLE hProcess, LPVOID address, SIZE_T size, std::vector<std::string>& texts) {
        if (size < 16) return false; // Too small to contain meaningful text
        
        // Limit the size to avoid long processing times
        if (size > 64 * 1024) { // Max 64KB
            size = 64 * 1024;
        }
        
        std::vector<char> buffer(size);
        SIZE_T bytesRead;
        
        if (!ReadProcessMemory(hProcess, address, buffer.data(), size, &bytesRead) || bytesRead == 0) {
            return false;
        }
        
        // Look for UTF-16 text patterns with smart skipping
        int textFound = 0;
        size_t i = 0;
        
        while (i < bytesRead - 32) {
            // Look for UTF-16 text patterns
            if (i + 4 < bytesRead) {
                // Check for alternating printable characters and nulls (UTF-16)
                bool isUTF16 = false;
                int printableCount = 0;
                int nullCount = 0;
                
                for (size_t j = 0; j < 20 && (i + j + 1) < bytesRead; j += 2) {
                    if (buffer[i + j] >= 32 && buffer[i + j] <= 126) {
                        printableCount++;
                    }
                    if (buffer[i + j + 1] == 0) {
                        nullCount++;
                    }
                }
                
                if (printableCount > 5 && nullCount > 2) {
                    isUTF16 = true;
                }
                
                if (isUTF16) {
                    // Try to extract UTF-16 text
                    std::string text;
                    size_t textEnd = i;
                    
                    for (size_t j = 0; j < 200 && (i + j + 1) < bytesRead; j += 2) {
                        if (buffer[i + j] >= 32 && buffer[i + j] <= 126 && buffer[i + j + 1] == 0) {
                            text += buffer[i + j];
                            textEnd = i + j + 2;
                        } else if (buffer[i + j] == 0 && buffer[i + j + 1] == 0) {
                            break; // End of string
                        }
                    }
                    
                    if (text.length() > 3) {
                        // Clean up the text
                        text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
                        text.erase(std::remove(text.begin(), text.end(), '\n'), text.end());
                        text.erase(std::remove(text.begin(), text.end(), '\t'), text.end());
                        
                        if (text.length() > 3) {
                            // Check if this text is already in our list
                            bool found = false;
                            for (const auto& existing : texts) {
                                if (existing == text) {
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                texts.push_back(text);
                                textFound++;
                            }
                        }
                    }
                    
                    // Skip to the end of this text to avoid overlapping
                    if (textEnd > i) {
                        i = textEnd;
                    } else {
                        i += 2; // Move forward by 2 bytes (UTF-16)
                    }
                } else {
                    i += 2; // Move forward by 2 bytes (UTF-16)
                }
            } else {
                i++; // Move forward by 1 byte
            }
        }
        
        if (textFound > 0) {
            std::cout << "      Found " << textFound << " text strings" << std::endl;
        }
        
        return true;
    }

    bool GetMemoryRegions(HANDLE hProcess, HeapInfo& heapInfo) {
        MEMORY_BASIC_INFORMATION mbi;
        LPVOID address = 0;
        int regionCount = 0;
        int userDataRegions = 0;
        
        std::cout << "  Scanning memory regions for user data..." << std::endl;
        
        while (VirtualQueryEx(hProcess, address, &mbi, sizeof(mbi))) {
            regionCount++;
            
            if (regionCount % 100 == 0) {
                std::cout << "    Scanned " << regionCount << " regions..." << std::endl;
            }
            
            // Focus on memory regions that typically contain user data
            bool isUserDataRegion = false;
            
            if (mbi.State == MEM_COMMIT) {
                // MEM_PRIVATE - Process heap, stack, and dynamically allocated memory
                if (mbi.Type == MEM_PRIVATE) {
                    // Check if it's readable and writable (typical for user data)
                    if ((mbi.Protect & PAGE_READWRITE) || 
                        (mbi.Protect & PAGE_READONLY) ||
                        (mbi.Protect & PAGE_EXECUTE_READ) ||
                        (mbi.Protect & PAGE_EXECUTE_READWRITE)) {
                        isUserDataRegion = true;
                    }
                }
                // MEM_MAPPED - Memory-mapped files (could contain user data)
                else if (mbi.Type == MEM_MAPPED) {
                    // Focus on smaller mapped regions that might contain user data
                    if (mbi.RegionSize < 10 * 1024 * 1024) { // Less than 10MB
                        isUserDataRegion = true;
                    }
                }
            }
            
            if (isUserDataRegion) {
                heapInfo.regions.push_back(mbi);
                userDataRegions++;
                
                // Extract text from user data regions
                if (mbi.RegionSize < 1024 * 1024) { // Less than 1MB for performance
                    std::cout << "    Extracting text from user data region " << userDataRegions 
                              << " (size: " << FormatSize(mbi.RegionSize) 
                              << ", type: " << (mbi.Type == MEM_PRIVATE ? "Private" : "Mapped")
                              << ", protection: 0x" << std::hex << mbi.Protect << std::dec << ")" << std::endl;
                    ExtractTextFromMemory(hProcess, mbi.BaseAddress, mbi.RegionSize, heapInfo.extractedTexts);
                }
            }
            
            address = (LPVOID)((DWORD_PTR)mbi.BaseAddress + mbi.RegionSize);
            
            if (address < mbi.BaseAddress) {
                break; // Overflow occurred
            }
        }
        
        std::cout << "  Total regions scanned: " << regionCount << std::endl;
        std::cout << "  User data regions found: " << userDataRegions << std::endl;
        return true;
    }



    std::string FormatSize(SIZE_T size) {
        const char* units[] = {"B", "KB", "MB", "GB"};
        int unitIndex = 0;
        double sizeInUnits = static_cast<double>(size);
        
        while (sizeInUnits >= 1024.0 && unitIndex < 3) {
            sizeInUnits /= 1024.0;
            unitIndex++;
        }
        
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << sizeInUnits << " " << units[unitIndex];
        return oss.str();
    }

public:
    bool ExtractHeapData(const std::string& processName) {
        DWORD processId = FindProcessIdByName(processName);
        if (processId == 0) {
            std::cout << "Process '" << processName << "' not found!" << std::endl;
            return false;
        }

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
        if (hProcess == NULL) {
            std::cout << "Failed to open process. Error: " << GetLastError() << std::endl;
            return false;
        }

        ProcessInfo processInfo;
        processInfo.processId = processId;
        processInfo.processName = processName;
        processInfo.totalHeapSize = 0;
        processInfo.totalCommittedSize = 0;
        processInfo.totalAllocatedSize = 0;
        processInfo.totalFreeSize = 0;
        processInfo.totalBlockCount = 0;

        std::cout << "Getting process memory information..." << std::endl;
        
        // Get process memory information
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
            HeapInfo heapInfo;
            heapInfo.heapHandle = NULL;
            heapInfo.heapSize = pmc.WorkingSetSize;
            heapInfo.committedSize = pmc.PagefileUsage;
            heapInfo.uncommittedSize = pmc.WorkingSetSize - pmc.PagefileUsage;
            heapInfo.allocatedSize = pmc.PrivateUsage;
            heapInfo.freeSize = pmc.PagefileUsage - pmc.PrivateUsage;
            heapInfo.blockCount = 0;
            
            std::cout << "Extracting memory regions and text..." << std::endl;
            GetMemoryRegions(hProcess, heapInfo);
            std::cout << "Memory extraction completed." << std::endl;
            processInfo.heaps.push_back(heapInfo);
            
            processInfo.totalHeapSize = heapInfo.heapSize;
            processInfo.totalCommittedSize = heapInfo.committedSize;
            processInfo.totalAllocatedSize = heapInfo.allocatedSize;
            processInfo.totalFreeSize = heapInfo.freeSize;
            processInfo.totalBlockCount = heapInfo.blockCount;
        }

        processes.push_back(processInfo);
        CloseHandle(hProcess);
        return true;
    }

    void PrintHeapReport() {
        for (const auto& process : processes) {
            std::cout << "\n" << std::string(80, '=') << std::endl;
            std::cout << "HEAP EXTRACTION REPORT" << std::endl;
            std::cout << std::string(80, '=') << std::endl;
            std::cout << "Process Name: " << process.processName << std::endl;
            std::cout << "Process ID: " << process.processId << std::endl;
            std::cout << "Number of Heaps: " << process.heaps.size() << std::endl;
            std::cout << std::endl;

            // Summary
            std::cout << "SUMMARY:" << std::endl;
            std::cout << "  Total Heap Size: " << FormatSize(process.totalHeapSize) << std::endl;
            std::cout << "  Total Committed: " << FormatSize(process.totalCommittedSize) << std::endl;
            std::cout << "  Total Allocated: " << FormatSize(process.totalAllocatedSize) << std::endl;
            std::cout << "  Total Free: " << FormatSize(process.totalFreeSize) << std::endl;
            std::cout << "  Total Blocks: " << process.totalBlockCount << std::endl;
            std::cout << std::endl;

            // Detailed heap information
            for (size_t i = 0; i < process.heaps.size(); i++) {
                const auto& heap = process.heaps[i];
                std::cout << "HEAP " << i + 1 << " (Handle: 0x" << std::hex << heap.heapHandle << std::dec << "):" << std::endl;
                std::cout << "  Size: " << FormatSize(heap.heapSize) << std::endl;
                std::cout << "  Committed: " << FormatSize(heap.committedSize) << std::endl;
                std::cout << "  Uncommitted: " << FormatSize(heap.uncommittedSize) << std::endl;
                std::cout << "  Allocated: " << FormatSize(heap.allocatedSize) << std::endl;
                std::cout << "  Free: " << FormatSize(heap.freeSize) << std::endl;
                std::cout << "  Blocks: " << heap.blockCount << std::endl;
                std::cout << "  Memory Regions: " << heap.regions.size() << std::endl;
                std::cout << "  Extracted Texts: " << heap.extractedTexts.size() << std::endl;
                std::cout << std::endl;

                // Memory regions
                if (!heap.regions.empty()) {
                    std::cout << "  MEMORY REGIONS:" << std::endl;
                    for (size_t j = 0; j < heap.regions.size(); j++) {
                        const auto& region = heap.regions[j];
                        std::cout << "    Region " << j + 1 << ": " << std::endl;
                        std::cout << "      Base Address: 0x" << std::hex << region.BaseAddress << std::dec << std::endl;
                        std::cout << "      Size: " << FormatSize(region.RegionSize) << std::endl;
                        std::cout << "      State: " << (region.State == MEM_COMMIT ? "Committed" : "Reserved") << std::endl;
                        std::cout << "      Type: ";
                        switch (region.Type) {
                            case MEM_PRIVATE: std::cout << "Private"; break;
                            case MEM_MAPPED: std::cout << "Mapped"; break;
                            case MEM_IMAGE: std::cout << "Image"; break;
                            default: std::cout << "Unknown"; break;
                        }
                        std::cout << std::endl;
                        std::cout << "      Protection: 0x" << std::hex << region.Protect << std::dec << std::endl;
                    }
                    std::cout << std::endl;
                }
                
                // Display extracted texts for this heap
                if (!heap.extractedTexts.empty()) {
                    std::cout << "  EXTRACTED TEXTS:" << std::endl;
                    for (size_t j = 0; j < heap.extractedTexts.size(); j++) {
                        std::cout << "    Text " << j + 1 << ": " << heap.extractedTexts[j] << std::endl;
                    }
                    std::cout << std::endl;
                }
            }
        }
    }

    void SaveReportToFile(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cout << "Failed to create report file: " << filename << std::endl;
            return;
        }

        for (const auto& process : processes) {
            file << "HEAP EXTRACTION REPORT" << std::endl;
            file << "Process Name: " << process.processName << std::endl;
            file << "Process ID: " << process.processId << std::endl;
            file << "Number of Heaps: " << process.heaps.size() << std::endl;
            file << std::endl;

            file << "SUMMARY:" << std::endl;
            file << "Total Heap Size: " << FormatSize(process.totalHeapSize) << std::endl;
            file << "Total Committed: " << FormatSize(process.totalCommittedSize) << std::endl;
            file << "Total Allocated: " << FormatSize(process.totalAllocatedSize) << std::endl;
            file << "Total Free: " << FormatSize(process.totalFreeSize) << std::endl;
            file << "Total Blocks: " << process.totalBlockCount << std::endl;
            file << std::endl;

            for (size_t i = 0; i < process.heaps.size(); i++) {
                const auto& heap = process.heaps[i];
                 file << "HEAP " << i + 1 << " (Handle: 0x" << std::hex << heap.heapHandle << std::dec << "):" << std::endl;
                 file << "Size: " << FormatSize(heap.heapSize) << std::endl;
                 file << "Committed: " << FormatSize(heap.committedSize) << std::endl;
                 file << "Allocated: " << FormatSize(heap.allocatedSize) << std::endl;
                 file << "Free: " << FormatSize(heap.freeSize) << std::endl;
                 file << "Blocks: " << heap.blockCount << std::endl;
                 file << "Extracted Texts: " << heap.extractedTexts.size() << std::endl;
                 
                 if (!heap.extractedTexts.empty()) {
                     file << "TEXTS:" << std::endl;
                     for (size_t j = 0; j < heap.extractedTexts.size(); j++) {
                         file << "  Text " << j + 1 << ": " << heap.extractedTexts[j] << std::endl;
                     }
                 }
                 file << std::endl;
             }
        }

        file.close();
        std::cout << "Report saved to: " << filename << std::endl;
    }
};

int main() {
    std::cout << "Windows Heap Extractor" << std::endl;
    std::cout << "======================" << std::endl;

    WindowsHeapExtractor extractor;
    std::string processName;

    std::cout << "Enter process name (e.g., notepad.exe): ";
    std::getline(std::cin, processName);

    if (processName.empty()) {
        std::cout << "Process name cannot be empty!" << std::endl;
        return 1;
    }

    std::cout << "\nSearching for process: " << processName << std::endl;
    std::cout << "Extracting heap data..." << std::endl;

    if (extractor.ExtractHeapData(processName)) {
        extractor.PrintHeapReport();
        
        std::string filename = processName + "_heap_report.txt";
        extractor.SaveReportToFile(filename);
    } else {
        std::cout << "Failed to extract heap data!" << std::endl;
        return 1;
    }

    std::cout << "\nPress Enter to exit...";
    std::cin.get();
    return 0;
}
