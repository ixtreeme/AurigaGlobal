#pragma once

#ifdef ENABLE_SWITCHBOT


class CPythonSwitchbot : public CSingleton <CPythonSwitchbot>
{
public:
	#pragma pack(1)
	struct TSwitchbotAttributeAlternativeTable
	{
		TPlayerItemAttribute attributes[MAX_NORM_ATTR_NUM];

		bool IsConfigured() const
		{
			for (const auto& it : attributes)
			{
				if (it.bType && it.sValue)
				{
					return true;
				}
			}

			return false;
		}
	};

	struct TSwitchbotTable
	{
		uint32_t player_id;
		bool active[SWITCHBOT_SLOT_COUNT];
		bool finished[SWITCHBOT_SLOT_COUNT];
		uint32_t items[SWITCHBOT_SLOT_COUNT];
		TSwitchbotAttributeAlternativeTable alternatives[SWITCHBOT_SLOT_COUNT][SWITCHBOT_ALTERNATIVE_COUNT];

		TSwitchbotTable() : player_id(0)
		{
			memset(&items, 0, sizeof(items));
			memset(&alternatives, 0, sizeof(alternatives));
			memset(&active, false, sizeof(active));
			memset(&finished, false, sizeof(finished));
		}
	};

	struct TSwitchbottAttributeTable
	{
		uint8_t attribute_set;
		int apply_num;
		int32_t max_value;
	};

#pragma pack()

	CPythonSwitchbot();
	virtual ~CPythonSwitchbot();

	void Initialize();
	void Update(const TSwitchbotTable& table);

	void SetAttribute(uint8_t slot, uint8_t alternative, uint8_t attrIdx, uint8_t attrType, int attrValue);
	TPlayerItemAttribute GetAttribute(uint8_t slot, uint8_t alternative, uint8_t attrIdx);
	void GetAlternatives(uint8_t slot, std::vector<TSwitchbotAttributeAlternativeTable>& attributes);
	int GetAttibuteSet(uint8_t slot);
	bool IsActive(uint8_t slot);
	bool IsFinished(uint8_t slot);

	void ClearSlot(uint8_t slot);
	void ClearAttributeMap();

	void AddAttributeToMap(const TSwitchbottAttributeTable& table);
	void GetAttributesBySet(int iAttributeSet, std::vector<TPlayerItemAttribute>& vec_attributes);
	long GetAttributeMaxValue(int iAttributeSet, uint8_t attrType);

protected:
	TSwitchbotTable m_SwitchbotTable;
	std::map<uint8_t, std::map<uint8_t, int32_t>> m_map_AttributesBySet;
};
#endif