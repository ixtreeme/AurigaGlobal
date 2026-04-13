/*----- atoi function -----*/
inline bool str_to_number (bool& out, const char *in)
{
	if (nullptr==in || 0==in[0])	return false;

	out = (strtol(in, nullptr, 10) != 0);
	return true;
}

inline bool str_to_number (char& out, const char *in)
{
	if (nullptr==in || 0==in[0])	return false;

	out = (char) strtol(in, nullptr, 10);
	return true;
}

inline bool str_to_number (unsigned char& out, const char *in)
{
	if (nullptr==in || 0==in[0])	return false;

	out = (unsigned char) strtoul(in, nullptr, 10);
	return true;
}

inline bool str_to_number (short& out, const char *in)
{
	if (nullptr==in || 0==in[0])	return false;

	out = (short) strtol(in, nullptr, 10);
	return true;
}

inline bool str_to_number (unsigned short& out, const char *in)
{
	if (nullptr==in || 0==in[0])	return false;

	out = (unsigned short) strtoul(in, nullptr, 10);
	return true;
}

inline bool str_to_number (int& out, const char *in)
{
	if (nullptr==in || 0==in[0])	return false;

	out = (int) strtol(in, nullptr, 10);
	return true;
}

inline bool str_to_number (uint32_t& out, const char *in)
{
	if (nullptr==in || 0==in[0])	return false;

	out = (uint32_t) strtoul(in, nullptr, 10);
	return true;
}

inline bool str_to_number (long& out, const char *in)
{
	if (nullptr==in || 0==in[0])	return false;

	out = (long) strtol(in, nullptr, 10);
	return true;
}

inline bool str_to_number (unsigned long& out, const char *in)
{
	if (nullptr==in || 0==in[0])	return false;

	out = (unsigned long) strtoul(in, nullptr, 10);
	return true;
}

inline bool str_to_number (long long& out, const char *in)
{
	if (nullptr==in || 0==in[0])	return false;

	out = (long long) strtoull(in, NULL, 10);
	return true;
}

inline bool str_to_number (float& out, const char *in)
{
	if (nullptr==in || 0==in[0])	return false;

	out = (float) strtof(in, nullptr);
	return true;
}

inline bool str_to_number (double& out, const char *in)
{
	if (nullptr==in || 0==in[0])	return false;

	out = (double) strtod(in, nullptr);
	return true;
}

//inline bool str_to_number(uint64_t& out, const char* in)
//{
//	if (nullptr == in || 0 == in[0])	return false;
//
//	out = (uint64_t)strtoull(in, nullptr, 10);
//	return true;
//}

#ifdef __FreeBSD__
inline bool str_to_number (long double& out, const char *in)
{
	if (0==in || 0==in[0])	return false;

	out = (long double) strtold(in, NULL);
	return true;
}
#endif


/*----- atoi function -----*/
