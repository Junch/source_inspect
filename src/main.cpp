#include <pybind11/pybind11.h>
#include <windows.h>
#include <string>
#include <dbghelp.h>
#include <sstream>
#pragma comment(lib,"dbghelp.lib")

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

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
    m_hProcess = OpenProcess(PROCESS_ALL_ACCESS, 0, pid);
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    m_bInitialzed = SymInitialize(m_hProcess, NULL, TRUE);
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

    return ret;
}

SymLineInfo Inspect::GetLineFromSymName(PCWSTR pSymName)
{
    SymLineInfo ret;

    TCHAR szSymbolName[MAX_SYM_NAME];
    ULONG64 buffer[(sizeof(SYMBOL_INFOW) +
        MAX_SYM_NAME * sizeof(TCHAR) +
        sizeof(ULONG64) - 1) /
        sizeof(ULONG64)];
    PSYMBOL_INFOW pSymbol = (PSYMBOL_INFOW)buffer;

    wcscpy_s(szSymbolName, MAX_SYM_NAME, pSymName);
    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbol->MaxNameLen = MAX_SYM_NAME;

    if (!SymFromNameW(m_hProcess, pSymName, pSymbol))
    {
        return ret;
    }

    ret = GetLineFromAddr64(pSymbol->Address);
    return ret;
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
