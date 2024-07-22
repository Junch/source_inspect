#include <pybind11/pybind11.h>
#include <windows.h>
#include <string>
#include <Psapi.h>
#include <sstream>
#include <pathcch.h>
#define DBGHELP_TRANSLATE_TCHAR
#include <dbghelp.h>
#pragma comment(lib,"dbghelp.lib")
#pragma comment(lib,"pathcch.lib")

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

std::wstring GetMainModulePath(HANDLE hProcess) {
    WCHAR fullPath[MAX_PATH];
    DWORD len = MAX_PATH - 1;
    if (FAILED(::QueryFullProcessImageName(hProcess, 0, fullPath, &len))) {
        return L""; // Unable to retrieve process image name
    }

    if (FAILED(::PathCchRemoveFileSpec(fullPath, MAX_PATH))) 
    {
        return L"";
    }

    return std::wstring(fullPath);
}

std::wstring GetLastErrorAsString()
{
    //Get the error message ID, if any.
    DWORD errorMessageID = ::GetLastError();
    if(errorMessageID == 0) {
        return std::wstring(); //No error message has been recorded
    }
    
    LPWSTR messageBuffer = nullptr;

    //Ask Win32 to give us the string version of that message ID.
    //The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
    size_t size = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&messageBuffer, 0, NULL);
    
    //Copy the error message into a std::string.
    std::wstring message(messageBuffer, size);
    
    //Free the Win32's string's buffer.
    LocalFree(messageBuffer);
            
    return message;
}

namespace py = pybind11;

struct SymLineInfo
{
    SymLineInfo() : LineNumber(0) {}
    std::wstring FileName;
    DWORD        LineNumber;
};

class Inspect
{
public:
    Inspect(DWORD pid);
    ~Inspect();

    SymLineInfo GetLineFromAddr64(DWORD64 dwAddress);
    SymLineInfo GetLineFromSymName(PCWSTR pFunctionName);
private:
    HANDLE m_hProcess;
    BOOL m_bInitialzed;
};

Inspect::Inspect(DWORD pid)
{
    m_hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES );
    m_bInitialzed = SymInitialize(m_hProcess, NULL, TRUE);

    WCHAR path[MAX_PATH];
    if (SymGetSearchPath(m_hProcess, path, MAX_PATH)) 
    {
        std::wstring szSearchPath = std::wstring(path) + L";" + GetMainModulePath(m_hProcess);
        SymSetSearchPath(m_hProcess, szSearchPath.c_str());
    }
}

Inspect::~Inspect()
{
    if (m_bInitialzed)
    {
        SymCleanup(m_hProcess);
    }

    CloseHandle(m_hProcess);
}

SymLineInfo Inspect::GetLineFromAddr64(DWORD64 dwAddress)
{
    SymLineInfo ret;

    DWORD  dwDisplacement;
    IMAGEHLP_LINEW64 line;
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    if (SymGetLineFromAddrW64(m_hProcess, dwAddress, &dwDisplacement, &line))
    {
        ret.FileName = line.FileName;
        ret.LineNumber = line.LineNumber;
    }
    else 
    {
        auto szErr = std::wstring(L"SymGetLineFromAddrW64 ") + GetLastErrorAsString();
        ::OutputDebugString(szErr.c_str());
    }

    return ret;
}

SymLineInfo Inspect::GetLineFromSymName(PCWSTR pSymName)
{
    SymLineInfo lineInfo;

    TCHAR szSymbolName[MAX_SYM_NAME];
    ULONG64 buffer[(sizeof(SYMBOL_INFOW) +
        MAX_SYM_NAME * sizeof(TCHAR) +
        sizeof(ULONG64) - 1) /
        sizeof(ULONG64)];
    PSYMBOL_INFOW pSymbol = (PSYMBOL_INFOW)buffer;

    wcscpy_s(szSymbolName, MAX_SYM_NAME, pSymName);
    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFOW);
    pSymbol->MaxNameLen = MAX_SYM_NAME;

    if (SymFromName(m_hProcess, pSymName, pSymbol))
    {
        lineInfo = GetLineFromAddr64(pSymbol->Address);
    }
    else
    {
        auto szErr = std::wstring(L"SymFromName ") + GetLastErrorAsString();
        ::OutputDebugString(szErr.c_str());
    }

    return lineInfo;
}

PYBIND11_MODULE(sourceline, m) {
    m.doc() = R"pbdoc(
        Pybind11 sourceline plugin
        -----------------------

        .. currentmodule:: sourceline

        .. autosummary::
           :toctree: _generate

           add
           subtract
    )pbdoc";

    py::class_<SymLineInfo>(m, "SymLineInfo")
        .def(py::init<>())
        .def_readwrite("FileName", &SymLineInfo::FileName)
        .def_readwrite("LineNumber", &SymLineInfo::LineNumber)
        .def("__repr__", [](const SymLineInfo &a) {
            std::wostringstream oss;
            oss << "<SymLineInfo " <<  a.FileName << L":" << a.LineNumber << L">";
            return oss.str();
         });

    py::class_<Inspect>(m, "Inspect")
        .def(py::init<DWORD>())
        .def("GetLineFromAddr64", &Inspect::GetLineFromAddr64, "Get the file line from the address", py::arg("address"))
        .def("getLineFromSymName", &Inspect::GetLineFromSymName, "Get the file line from the symbol name", py::arg("symName"));

    // m.def("add", &add, R"pbdoc(
    //     Add two numbers

    //     Some other explanation about the add function.
    // )pbdoc");

    // m.def("subtract", [](int i, int j) { return i - j; }, R"pbdoc(
    //     Subtract two numbers

    //     Some other explanation about the subtract function.
    // )pbdoc");

#ifdef VERSION_INFO
    m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "dev";
#endif
}
