// © 2025 Adam Badke. All rights reserved.
#include "Dialog_Win32.h"

#include "../Util/TextUtils.h"


namespace win32
{
	bool Dialog::OpenFileDialogBox(
		std::string const& filterName,
		std::vector<std::string> const& allowedExtensions,
		std::string& filepathOut)
	{
		// Build our list of filter names and extensions:
		std::wstring const wideFilterName = util::ToWideString(filterName);

		// Combine extensions into a single semicolon-separated string
		std::wstringstream extensionsStream;
		for (size_t i = 0; i < allowedExtensions.size(); ++i)
		{
			extensionsStream << util::ToWideString(allowedExtensions[i]);
			
			if (i < allowedExtensions.size() - 1) // Add semicolon between extensions
			{
				extensionsStream << L";";
			}
		}
		std::wstring const& extensionsStr = extensionsStream.str();

		// Create vector of COMDLG_FILTERSPEC entries:
		std::vector<COMDLG_FILTERSPEC> fileFilters;
		fileFilters.push_back( {wideFilterName.c_str(), extensionsStr.c_str() } );

		// Add the "All Files" filter:
		fileFilters.push_back({ L"All Files (*.*)", L"*.*" });


		// Initialize the COM library for use by the calling thread:
		HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		if (FAILED(hr))
		{
			return false;
		}

		// Create a file open dialog:
		IFileOpenDialog* fileOpenDialog = nullptr;
		hr = CoCreateInstance(
			CLSID_FileOpenDialog,
			nullptr,
			CLSCTX_ALL,
			IID_IFileOpenDialog,
			reinterpret_cast<void**>(&fileOpenDialog));

		if (FAILED(hr))
		{
			CoUninitialize();
			return false;
		}

		// Set our filters:
		fileOpenDialog->SetFileTypes(static_cast<UINT>(fileFilters.size()), fileFilters.data());

		// Show the file open dialog:
		hr = fileOpenDialog->Show(nullptr); // hwndOwner = nullptr
		if (FAILED(hr))
		{
			fileOpenDialog->Release();
			CoUninitialize();
			return false;
		}

		// Get the selection result:
		IShellItem* shellItem = nullptr;
		hr = fileOpenDialog->GetResult(&shellItem);
		if (FAILED(hr))
		{
			fileOpenDialog->Release();
			CoUninitialize();
			return false;
		}

		// Get the display name of the selected shell item:
		PWSTR selectedFilePath = nullptr;
		hr = shellItem->GetDisplayName(SIGDN_FILESYSPATH, &selectedFilePath);
		if (FAILED(hr))
		{
			shellItem->Release();
			fileOpenDialog->Release();
			CoUninitialize();
			return false;
		}

		// Convert our filepath from wchars:
		filepathOut = util::FromWideString(std::wstring(selectedFilePath));

		// Cleanup:
		CoTaskMemFree(selectedFilePath);
		shellItem->Release();
		fileOpenDialog->Release();
		CoUninitialize();
		
		return true;
	}
}