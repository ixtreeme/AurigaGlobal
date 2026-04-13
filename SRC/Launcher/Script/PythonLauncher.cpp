#include "StdAfx.h"
#include <Python2.7/frameobject.h>
#include "../Pack/EterPackManager.h"

#include "PythonLauncher.h"

#include "../SecureLayer/obfuscate.h"

CPythonLauncherIxtreeme::CPythonLauncherIxtreeme(): m_poModule(nullptr), m_poDic(nullptr)
{
	Py_Initialize();
}

CPythonLauncherIxtreeme::~CPythonLauncherIxtreeme()
{
	Clear();
}

void CPythonLauncherIxtreeme::Clear()
{
	Py_Finalize();
}


std::string g_stTraceBuffer[512];
int	g_nCurTraceN = 0;

void Traceback()
{
	std::string str;

	for (int i = 0; i < g_nCurTraceN; ++i)
	{
		str.append(g_stTraceBuffer[i]);
		str.append("\n");
	}

	PyObject * exc;
	PyObject * v;
	PyObject * tb;
	const char * errStr;

	PyErr_Fetch(&exc, &v, &tb);

	if (PyString_Check(v))
	{
		errStr = PyString_AS_STRING(v);
		str.append("Error: ");
		str.append(errStr);

		Tracef("%s\n", errStr);
	}
	Py_DECREF(exc);
	Py_DECREF(v);
	Py_DECREF(tb);
	LogBoxf("Traceback:\n\n%s\n", str.c_str());
}

int TraceFunc(PyObject * obj, PyFrameObject * f, int what, PyObject *arg)
{
	const char * funcname;
	char szTraceBuffer[128];

	switch (what)
	{
		case PyTrace_CALL:
			if (g_nCurTraceN >= 512)
				return 0;

			if (Py_OptimizeFlag)
				f->f_lineno = PyCode_Addr2Line(f->f_code, f->f_lasti);

			funcname = PyString_AsString(f->f_code->co_name);

			_snprintf(szTraceBuffer, sizeof(szTraceBuffer), "Call: File \"%s\", line %d, in %s",
					  PyString_AsString(f->f_code->co_filename),
					  f->f_lineno,
					  funcname);

			g_stTraceBuffer[g_nCurTraceN++]=szTraceBuffer;
			break;

		case PyTrace_RETURN:
			if (g_nCurTraceN > 0)
				--g_nCurTraceN;
			break;

		case PyTrace_EXCEPTION:
			if (g_nCurTraceN >= 512)
				return 0;

			PyObject * exc_type = nullptr, * exc_value = nullptr, * exc_traceback = nullptr;

			PyTuple_GetObject(arg, 0, &exc_type);
			PyTuple_GetObject(arg, 1, &exc_value);
			PyTuple_GetObject(arg, 2, &exc_traceback);

			Py_ssize_t len;
			const char * exc_str;
			PyObject_AsCharBuffer(exc_type, &exc_str, &len);

			_snprintf(szTraceBuffer, sizeof(szTraceBuffer), "Exception: File \"%s\", line %d, in %s",
					  PyString_AS_STRING(f->f_code->co_filename),
					  f->f_lineno,
					  PyString_AS_STRING(f->f_code->co_name));

			g_stTraceBuffer[g_nCurTraceN++]=szTraceBuffer;

			break;
	}
	return 0;
}

void CPythonLauncherIxtreeme::SetTraceFunc(int (*pFunc)(PyObject * obj, PyFrameObject * f, int what, PyObject *arg))
{
	PyEval_SetTrace(pFunc, nullptr);
}

bool CPythonLauncherIxtreeme::Create()
{

	//Py_SetProgramName((char*)c_szProgramName);
#ifdef _DEBUG
	PyEval_SetTrace(TraceFunc, NULL);
#endif
	m_poModule = PyImport_AddModule( AY_OBFUSCATE("__main__"));

	if (!m_poModule)
		return false;

	m_poDic = PyModule_GetDict(m_poModule);
	

    PyObject * builtins = PyImport_ImportModule(AY_OBFUSCATE("__builtin__"));
	PyModule_AddIntConstant(builtins, "TRUE", 1);
	PyModule_AddIntConstant(builtins, "FALSE", 0);
    PyDict_SetItemString(m_poDic, AY_OBFUSCATE("__builtins__"), builtins);
	Py_DECREF(builtins);

	if (!RunLine("import __main__"))
		return false;

	if (!RunLine("import sys"))
		return false;

	return true;
}

bool CPythonLauncherIxtreeme::RunCompiledFile(const char* c_szFileName)
{

	FILE * fp = fopen(c_szFileName, "rb");

	if (!fp)
		return false;

	PyCodeObject *co;
	PyObject *v;
	int32_t magic;
	//int32_t PyImport_GetMagicNumber();

	magic = _PyMarshal_ReadLongFromFile(fp);

	if (magic != PyImport_GetMagicNumber())
	{
		PyErr_SetString(PyExc_RuntimeError, "Bad magic number in .pyc file");
		fclose(fp);
		return false;
	}

	_PyMarshal_ReadLongFromFile(fp);
	v = _PyMarshal_ReadLastObjectFromFile(fp);

	fclose(fp);

	if (!v || !PyCode_Check(v))
	{
		Py_XDECREF(v);
		PyErr_SetString(PyExc_RuntimeError, "Bad code object in .pyc file");
		return false;
	}

	co = (PyCodeObject *) v;
	v = PyEval_EvalCode(co, m_poDic, m_poDic);
/*	if (v && flags)
		flags->cf_flags |= (co->co_flags & PyCF_MASK);*/
	Py_DECREF(co);
	if (!v)
	{
		Traceback();
		return false;
	}

	Py_DECREF(v);
	if (Py_FlushLine())
		PyErr_Clear();


	return true;
}


bool CPythonLauncherIxtreeme::RunMemoryTextFile(const char* c_szFileName, UINT uFileSize, const void* c_pvFileData)
{
	auto c_pcFileData = static_cast<const CHAR*>(c_pvFileData);

	std::string stConvFileData;
	stConvFileData.reserve(uFileSize);

	{
		for (UINT i = 0; i < uFileSize; ++i)
		{
			if (c_pcFileData[i] != 13)
				stConvFileData += c_pcFileData[i];
		}
	}

	const CHAR* c_pcConvFileData = stConvFileData.c_str();
	PyObject* pCompiledCode = Py_CompileString(c_pcConvFileData, c_szFileName, Py_file_input);//fix
	if (!pCompiledCode)
		return false;
	PyObject* pResult = PyEval_EvalCode((PyCodeObject*)pCompiledCode, m_poDic, m_poDic);
	Py_DECREF(pCompiledCode);//ref c
	if (!pResult)
		return false;

	Py_DECREF(pResult);
	if (Py_FlushLine())
		PyErr_Clear();
	return true;

}

bool CPythonLauncherIxtreeme::RunFile(const char* c_szFileName)
{
	std::unique_ptr<char[]>acBufData;

	DWORD dwBufSize = 0;

	{
		CMappedFile file;
		const VOID* pvData = nullptr;
		CEterPackManager::Instance().Get(file, c_szFileName, &pvData);

		dwBufSize = file.Size();
		if (dwBufSize == 0)
			return false;

		acBufData = std::make_unique<char[]>(dwBufSize);
		memcpy(acBufData.get(), pvData, dwBufSize);
	}

	bool ret = false;

	ret = RunMemoryTextFile(c_szFileName, dwBufSize, acBufData.get());

	return ret;
}

bool CPythonLauncherIxtreeme::RunLine(const char* c_szSrc)
{
	PyObject * v = PyRun_String(c_szSrc, Py_file_input, m_poDic, m_poDic);

	if (!v)
	{
		Traceback();
		return false;
	}

	Py_DECREF(v);
	return true;
}

const char* CPythonLauncherIxtreeme::GetError()
{
	PyObject* exc;
	PyObject* v;
	PyObject* tb;

	PyErr_Fetch(&exc, &v, &tb);

	if (PyString_Check(v))
		return PyString_AS_STRING(v);

	return "";
}
