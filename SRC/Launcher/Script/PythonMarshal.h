#ifndef __INC_ETERMARSHAL_H__
#define __INC_ETERMARSHAL_H__

extern PyObject * _PyMarshal_ReadObjectFromFile(FILE* fp);
extern PyObject * _PyMarshal_ReadLastObjectFromFile(FILE* fp);
extern int32_t _PyMarshal_ReadLongFromFile(FILE *fp);

#endif
