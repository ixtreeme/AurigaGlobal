#ifndef __INC_REF_H__
#define __INC_REF_H__

#include "ReferenceObject.h"
#include <cassert>

template<typename T>
class CRef
{
public:
    struct FClear
    {
        void operator()(CRef<T>& rRef)
        {
            rRef.Clear();
        }
    };

    CRef() : m_pObject(nullptr) {}

    explicit CRef(CReferenceObject* pObject)
    {
        Initialize(pObject);
    }

    CRef(const CRef& c_rRef)
    {
        Initialize(c_rRef.m_pObject);
    }

    ~CRef()
    {
        Clear();
    }

    CRef& operator=(CReferenceObject* pObject)
    {
        SetPointer(pObject);
        return *this;
    }

    CRef& operator=(const CRef& c_rRef)
    {
        SetPointer(c_rRef.m_pObject);
        return *this;
    }

    void Clear()
    {
        if (m_pObject)
        {
            m_pObject->Release();
            m_pObject = nullptr;
        }
    }

    bool IsNull() const
    {
        return m_pObject == nullptr;
    }

	void SetPointer(CReferenceObject* pObject)
	{
		CReferenceObject* pOldObject = m_pObject;

		m_pObject = pObject;

		if (m_pObject)
			m_pObject->AddReference();

		if (pOldObject)
			pOldObject->Release();
	}

    T* GetPointer() const
    {
        return dynamic_cast<T*>(m_pObject);
    }

    T* operator->() const
    {
       // assert(m_pObject && "Dereferencing a null pointer!");
        return dynamic_cast<T*>(m_pObject);
    }

private:
    void Initialize(CReferenceObject* pObject)
    {
      //  assert(!m_pObject && "Object already initialized!");

        m_pObject = pObject;
        if (m_pObject)
            m_pObject->AddReference();
    }

private:
    CReferenceObject* m_pObject;
};

#endif
