#ifndef _cube_h_
#define _cube_h_


#define CUBE_MAX_NUM	24	// OLD:INVENTORY_MAX_NUM
#define CUBE_MAX_DISTANCE	1000


struct CUBE_RENEWAL_VALUE
{
	uint32_t	vnum;
	int		count;

	bool operator == (const CUBE_RENEWAL_VALUE& b)
	{
		return (this->count == b.count) && (this->vnum == b.vnum);
	}
};

struct CUBE_RENEWAL_DATA
{
	std::vector<uint16_t>		npc_vnum;
	std::vector<CUBE_RENEWAL_VALUE>	item;
	std::vector<CUBE_RENEWAL_VALUE>	reward;
	std::string								category;
	int						percent;

	int64_t				gold;

#ifdef ENABLE_GAYA_SYSTEM
	unsigned int 			gaya;
#endif

#ifdef ENABLE_CUBE_RENEWAL_COPY_WORLDARD
	uint32_t 					allowCopy;
#endif
	CUBE_RENEWAL_DATA();

}; 


void Cube_init ();
bool Cube_load (const char *file);
bool Cube_InformationInitialize();
void Cube_open (entt::entity chEntity);
void Cube_close(entt::entity chEntity);
void Cube_Make(entt::entity chEntity, int index, int count_item, int index_item_improve);
void SendDateCubeRenewalPackets(entt::entity chEntity, uint8_t subheader, uint32_t npcVNUM = 0);

#endif