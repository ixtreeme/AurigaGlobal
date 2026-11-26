#pragma once


class CMobVnumHelper
{
public:
	static	bool	IsPhoenix(uint32_t vnum)				{ return 34001 == vnum; }
	static	bool	IsIcePhoenix(uint32_t vnum)				{ return 34003 == vnum; }
	static	bool	IsPetUsingPetSystem(uint32_t vnum)	{ return (IsPhoenix(vnum) || IsReindeerYoung(vnum)) || IsIcePhoenix(vnum); }
	static	bool	IsReindeerYoung(uint32_t vnum)	{ return 34002 == vnum; }
	static	bool	IsRamadanBlackHorse(uint32_t vnum)		{ return 20119 == vnum || 20219 == vnum || 22022 == vnum; }
};

class CVnumHelper
{
};

