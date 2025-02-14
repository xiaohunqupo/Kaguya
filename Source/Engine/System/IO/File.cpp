#include "File.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

bool File::Exists(const std::filesystem::path& Path)
{
	const DWORD FileAttributes = GetFileAttributes(Path.c_str());
	return FileAttributes != INVALID_FILE_ATTRIBUTES && !(FileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}

FileStream File::Create(const std::filesystem::path& Path)
{
	return FileStream(Path, FileMode::Create, FileAccess::Write);
}

FileStream File::Read(const std::filesystem::path& Path)
{
	return FileStream(Path, FileMode::Open, FileAccess::Read);
}
