#include <fstream>
#include <windows.h>
#include <istream>
#include "ModelManager.h"
#include <tchar.h>


typedef std::list<std::filesystem::path>	TPathInfoList;

static std::string s_IgnoreKeywords[] = { "ComboInputData", "AttackingData", "MotionEventData", "LoopData", };

bool FileIntoString(const std::filesystem::path& path, std::string* outString = nullptr)
{
	assert(NULL != outString);

	std::ifstream fs;
	std::string line;

	fs.open(path, std::ios::in);

	if (!fs.is_open())
	{
		char errorMsg[255] = { 0, };
		strerror_s(errorMsg, sizeof(errorMsg), errno);

		printf("[FAIL] %s\n\t%s \n", path.string().c_str(), errorMsg);

		return true;
	}

	outString->clear();

	fs.clear();

	while (std::getline(fs, line))
		outString->append(line);

	return true;
}

static bool IsNeedIgnoreMSA(const std::filesystem::path& msaPath)
{
	if (false == std::filesystem::is_regular_file(msaPath))
		return false;

	std::string fileContent;
	if (false == FileIntoString(msaPath, &fileContent))
		return false;

	for (const auto& s_IgnoreKeyword : s_IgnoreKeywords)
		if (fileContent.find(s_IgnoreKeyword) != std::string::npos)
			return true;

	return false;
}

bool IsNeedCalcAccumulation(const std::filesystem::path& path)
{
	static std::string s_NeedCalcAccumulations[] = { "walk", "run" };

	auto filename = path.string();
	std::ranges::transform(filename, filename.begin(), [](const unsigned char c) {return std::tolower(c); });

	for (const auto& s_NeedCalcAccumulation : s_NeedCalcAccumulations)
	{
		if (filename.find(s_NeedCalcAccumulation) != std::string::npos)
			return true;
	}

	return false;
}


enum EResult
{
	EResult_OK,
	EResult_Ignore,
	EResult_Fail
};

EResult MakeMSA(const std::filesystem::path& filePath, std::string* outMsg = nullptr)
{
	bool bIsAccumulationMotion = false;
	granny_real32 duration = 0.0f;
	granny_matrix_4x4 modelMatrix = {};

	assert(0 != outMsg);

	*outMsg = "OK";

	if (false == CModel::IsGrannyFile(filePath))
		return EResult_Fail;

	std::filesystem::path basePath = filePath.parent_path();

	std::filesystem::path msaPath = basePath / (filePath.stem().string() + ".msa");

	if (IsNeedIgnoreMSA(msaPath))
	{
		*outMsg = "FAIL - Complicated MSA";
		return EResult_Ignore;
	}


	granny_file* grannyFile = GrannyReadEntireFile(filePath.string().c_str());
	granny_file_info* fileInfo = GrannyGetFileInfo(grannyFile);

	if (1 != fileInfo->AnimationCount)
	{
		*outMsg = "IGNORE - NO Animation";

		if (1 < fileInfo->AnimationCount)
			*outMsg = "IGNORE - Too many animations";

		if (0 < fileInfo->ModelCount)
		{
			CModelManager::Instance().RegisterModel(filePath);
			*outMsg = "IGNORE - MODEL FILE(NO Animation)";
		}

		GrannyFreeFile(grannyFile);
		return EResult_Ignore;
	}

	CModel* modelWrapper = CModelManager::Instance().GetModel(filePath);
	if (nullptr == modelWrapper)
	{
		modelWrapper = CModelManager::Instance().GetModel(filePath.parent_path());

		if (nullptr == modelWrapper)
		{
			modelWrapper = CModelManager::Instance().AutoRegisterAndGetModel(filePath, 2);
		}

		if (nullptr == modelWrapper)
		{
			*outMsg = "FAIL - Can't find model file";
			GrannyFreeFile(grannyFile);
			return EResult_Fail;
		}
	}

	for (int i = 0; i < fileInfo->AnimationCount; ++i)
	{
		granny_animation* animation = fileInfo->Animations[i];
		granny_model* model = modelWrapper->GetModel();
		const int boneCount = model->Skeleton->BoneCount;
		duration = animation->Duration;

		if (IsNeedCalcAccumulation(filePath))
		{
			int trackIndex = -1, bip01Index = -1;

			if (!GrannyFindTrackGroupForModel(animation, model->Name, &trackIndex))
				trackIndex = 0;

			if (1 > animation->TrackGroupCount)
			{
				*outMsg = "FAIL - Invalid Track Group Count";
				GrannyFreeFile(grannyFile);
				return EResult_Fail;
			}

			if (!GrannyFindBoneByName(model->Skeleton, "Bip01", &bip01Index))
				bip01Index = 0;

			granny_model_instance* modelInstance = GrannyInstantiateModel(model);

			granny_local_pose* localPose = GrannyNewLocalPose(boneCount);
			granny_world_pose* worldPose = GrannyNewWorldPose(boneCount);

			granny_controlled_animation_builder* builder = GrannyBeginControlledAnimation(0.0f, animation);
			GrannySetTrackGroupTarget(builder, trackIndex, modelInstance);
			GrannySetTrackGroupAccumulation(builder, trackIndex, GrannyConstantExtractionAccumulation);
			granny_control* control = GrannyEndControlledAnimation(builder);

			GrannySetControlClock(control, 0.0f);
			GrannySampleModelAnimationsAccelerated(modelInstance, boneCount, nullptr, localPose, worldPose);

			modelMatrix[0][0] = modelMatrix[1][1] = modelMatrix[2][2] = modelMatrix[3][3] = 1.0f;

			GrannyUpdateModelMatrix(modelInstance, 0.0f, (granny_real32*)modelMatrix, (granny_real32*)modelMatrix, false);


			modelMatrix[0][0] = modelMatrix[1][1] = modelMatrix[2][2] = modelMatrix[3][3] = 1.0f;

			const granny_real32 endTime = animation->Duration - 0.000001f;
			GrannySetControlClock(control, endTime);
			GrannySampleModelAnimationsAccelerated(modelInstance, boneCount, nullptr, localPose, worldPose);
			GrannyUpdateModelMatrix(modelInstance, endTime, (granny_real32*)modelMatrix, (granny_real32*)modelMatrix, false);

			if (40.0f < fabs(modelMatrix[3][1]))
				bIsAccumulationMotion = true;

			GrannyFreeControlOnceUnused(control);
			GrannyFreeCompletedModelControls(modelInstance);
			GrannyFreeLocalPose(localPose);
			GrannyFreeWorldPose(worldPose);

			GrannyFreeModelInstance(modelInstance);
		}
	}

	GrannyFreeFile(grannyFile);


#if 1
	FILE* fp = nullptr;
	fopen_s(&fp, msaPath.string().c_str(), "wt");

	if (nullptr == fp)
	{
		*outMsg = "FAIL - Can't write MSA file";
		return EResult_Fail;
	}

	fprintf(fp, "ScriptType               MotionData\n");
	fprintf(fp, "\n");

	fprintf(fp, "MotionFileName           \"%s\"\n", filePath.string().c_str());
	fprintf(fp, "MotionDuration           %f\n", duration);


	if (bIsAccumulationMotion)
		fprintf(fp, "Accumulation             %.2f\t%.2f\t%.2f\n", 0.0f, modelMatrix[3][1], 0.0f);

	fprintf(fp, "\n");

	fclose(fp);
#endif

	return EResult_OK;
}

int _tmain(int argc, _TCHAR* argv[])
{
	TPathInfoList pathInfoList;

	std::locale::global(std::locale("kor"));
	std::string msg;

	FILE* fpLog = nullptr;

	std::string logPath = std::string(argv[0]) + ".log";
	freopen_s(&fpLog, logPath.c_str(), "a+t", stdout);


	for (int i = 1; i < argc; ++i)
	{
		std::filesystem::path inPath = argv[i];

		if (false == std::filesystem::exists(inPath))
			continue;

		const bool bIsDirectory = is_directory(inPath);

		if (CModel::IsGrannyFile(inPath))
		{
			if (CModel::IsGrannyModelFile(inPath))
			{
				CModelManager::Instance().RegisterModel(inPath);
			}
			else
				pathInfoList.push_back(inPath);
		}

		if (bIsDirectory)
		{
			for (std::filesystem::recursive_directory_iterator end, dir_iter(inPath); dir_iter != end; ++dir_iter)
			{
				const std::filesystem::path& curPath = *dir_iter;

				if (CModel::IsGrannyFile(curPath))
					pathInfoList.push_back(curPath);
			}
		}
	}

	for (TPathInfoList::iterator iter = pathInfoList.begin(); iter != pathInfoList.end(); ++iter)
	{
		const TPathInfoList::value_type& path = *iter;

		EResult resultCode = MakeMSA(path, &msg);
		char sep = std::filesystem::path::preferred_separator;

		std::filesystem::path parentPath = path.parent_path();
		std::string shortPath = parentPath.parent_path().filename().string() + sep + parentPath.filename().string() + sep + path.filename().string();

		tm t;
		time_t timer;
		timer = time(nullptr);
		localtime_s(&t, &timer);

		printf("%04d/%02d/%02d %02d:%02d:%02d [%s] %s\n",
			t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec,msg.c_str(), shortPath.c_str());
	}

	CModelManager::Instance().Destroy();

	return 0;
}