/*
 * minifiledialog.h - Cross-platform native file dialogs for C++17
 * https://github.com/adaoraul/minifiledialog
 * MIT License
 */

#ifndef MINIFILEDIALOG_H
#define MINIFILEDIALOG_H

#include <string>
#include <vector>
#include <utility>

namespace minifiledialog {

struct Filter {
    std::string description;
    std::string patterns;
};

// Open a file dialog and return the selected file path (empty if canceled)
std::string open_file(
    const std::string& title = "Open File",
    const std::string& default_path = ".",
    const std::vector<Filter>& filters = {{"All Files", "*"}}
);

// Save a file dialog and return the selected file path (empty if canceled)
std::string save_file(
    const std::string& title = "Save File",
    const std::string& default_path = ".",
    const std::string& default_filename = "untitled",
    const std::vector<Filter>& filters = {{"All Files", "*"}}
);

// Select a folder dialog and return the selected folder path (empty if canceled)
std::string select_folder(
    const std::string& title = "Select Folder",
    const std::string& default_path = "."
);

} // namespace minifiledialog

#endif // MINIFILEDIALOG_H

#ifdef MINIFILEDIALOG_IMPLEMENTATION

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shobjidl.h>
#include <shlwapi.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shlwapi.lib")

namespace minifiledialog {

static std::wstring to_wstring(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size_needed);
    return wstr;
}

static std::string from_wstring(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], size_needed, NULL, NULL);
    return str;
}

std::string open_file(const std::string& title, const std::string& default_path, const std::vector<Filter>& filters) {
    std::string result;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr)) {
        IFileOpenDialog* pFileOpen = NULL;
        hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileOpen));
        if (SUCCEEDED(hr)) {
            if (!title.empty()) {
                pFileOpen->SetTitle(to_wstring(title).c_str());
            }

            if (!default_path.empty()) {
                IShellItem* pFolder = NULL;
                if (SUCCEEDED(SHCreateItemFromParsingName(to_wstring(default_path).c_str(), NULL, IID_PPV_ARGS(&pFolder)))) {
                    pFileOpen->SetFolder(pFolder);
                    pFolder->Release();
                }
            }

	        // Fix wild pointer: pre-allocate space to avoid vector reallocation
	        std::vector<std::wstring> descs;
	        std::vector<std::wstring> patterns;
	        std::vector<COMDLG_FILTERSPEC> spec;
	        descs.reserve(filters.size());
	        patterns.reserve(filters.size());
	        spec.reserve(filters.size());

	        for (const auto& f : filters) {
	            descs.push_back(to_wstring(f.description));
	            patterns.push_back(to_wstring(f.patterns));
	            spec.push_back({descs.back().c_str(), patterns.back().c_str()});
	        }
	        if (!spec.empty()) {
	            pFileOpen->SetFileTypes((UINT)spec.size(), spec.data());
	        }

            hr = pFileOpen->Show(NULL);
            if (SUCCEEDED(hr)) {
                IShellItem* pItem = NULL;
                hr = pFileOpen->GetResult(&pItem);
                if (SUCCEEDED(hr)) {
                    PWSTR pszFilePath = NULL;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                    if (SUCCEEDED(hr)) {
                        result = from_wstring(pszFilePath);
                        CoTaskMemFree(pszFilePath);
                    }
                    pItem->Release();
                }
            }
            pFileOpen->Release();
        }
        CoUninitialize();
    }
    return result;
}

std::string save_file(const std::string& title, const std::string& default_path, const std::string& default_filename, const std::vector<Filter>& filters) {
    std::string result;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr)) {
        IFileSaveDialog* pFileSave = NULL;
        hr = CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileSave));
        if (SUCCEEDED(hr)) {
            if (!title.empty()) {
                pFileSave->SetTitle(to_wstring(title).c_str());
            }

            if (!default_filename.empty()) {
                pFileSave->SetFileName(to_wstring(default_filename).c_str());
            }

            if (!default_path.empty()) {
                IShellItem* pFolder = NULL;
                if (SUCCEEDED(SHCreateItemFromParsingName(to_wstring(default_path).c_str(), NULL, IID_PPV_ARGS(&pFolder)))) {
                    pFileSave->SetFolder(pFolder);
                    pFolder->Release();
                }
            }

	        // Fix wild pointer: pre-allocate space
	        std::vector<std::wstring> descs;
	        std::vector<std::wstring> patterns;
	        std::vector<COMDLG_FILTERSPEC> spec;
	        descs.reserve(filters.size());
	        patterns.reserve(filters.size());
	        spec.reserve(filters.size());
            for (const auto& f : filters) {
                descs.push_back(to_wstring(f.description));
                patterns.push_back(to_wstring(f.patterns));
                spec.push_back({descs.back().c_str(), patterns.back().c_str()});
            }
            pFileSave->SetFileTypes((UINT)spec.size(), spec.data());

            hr = pFileSave->Show(NULL);
            if (SUCCEEDED(hr)) {
                IShellItem* pItem = NULL;
                hr = pFileSave->GetResult(&pItem);
                if (SUCCEEDED(hr)) {
                    PWSTR pszFilePath = NULL;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                    if (SUCCEEDED(hr)) {
                        result = from_wstring(pszFilePath);
                        CoTaskMemFree(pszFilePath);
                    }
                    pItem->Release();
                }
            }
            pFileSave->Release();
        }
        CoUninitialize();
    }
    return result;
}

std::string select_folder(const std::string& title, const std::string& default_path) {
    std::string result;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr)) {
        IFileOpenDialog* pFolderDialog = NULL;
        hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFolderDialog));
        if (SUCCEEDED(hr)) {
            DWORD dwOptions;
            hr = pFolderDialog->GetOptions(&dwOptions);
            if (SUCCEEDED(hr)) {
                pFolderDialog->SetOptions(dwOptions | FOS_PICKFOLDERS);
            }

            if (!title.empty()) {
                pFolderDialog->SetTitle(to_wstring(title).c_str());
            }

            if (!default_path.empty()) {
                IShellItem* pFolder = NULL;
                if (SUCCEEDED(SHCreateItemFromParsingName(to_wstring(default_path).c_str(), NULL, IID_PPV_ARGS(&pFolder)))) {
                    pFolderDialog->SetFolder(pFolder);
                    pFolder->Release();
                }
            }

            hr = pFolderDialog->Show(NULL);
            if (SUCCEEDED(hr)) {
                IShellItem* pItem = NULL;
                hr = pFolderDialog->GetResult(&pItem);
                if (SUCCEEDED(hr)) {
                    PWSTR pszFolderPath = NULL;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFolderPath);
                    if (SUCCEEDED(hr)) {
                        result = from_wstring(pszFolderPath);
                        CoTaskMemFree(pszFolderPath);
                    }
                    pItem->Release();
                }
            }
            pFolderDialog->Release();
        }
        CoUninitialize();
    }
    return result;
}

} // namespace minifiledialog

#elif defined(__APPLE__)
#include <Cocoa/Cocoa.h>

namespace minifiledialog {

std::string open_file(const std::string& title, const std::string& default_path, const std::vector<Filter>& filters) {
    std::string result;
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        [panel setTitle:[NSString stringWithUTF8String:title.c_str()]];
        [panel setCanChooseFiles:YES];
        [panel setCanChooseDirectories:NO];
        [panel setAllowsMultipleSelection:NO];

        if (!default_path.empty()) {
            [panel setDirectoryURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:default_path.c_str()]]];
        }

        NSMutableArray* allowedTypes = [NSMutableArray array];
        for (const auto& f : filters) {
            std::string patterns = f.patterns;
            size_t pos = 0;
            while (pos < patterns.size()) {
                size_t next = patterns.find(';', pos);
                std::string pat = patterns.substr(pos, next - pos);
                pos = next == std::string::npos ? next : next + 1;
                if (pat == "*" || pat == "*.*") continue;
                if (pat.starts_with("*.")) {
                    [allowedTypes addObject:[NSString stringWithUTF8String:pat.substr(2).c_str()]];
                }
            }
        }
        if (allowedTypes.count > 0) {
            [panel setAllowedFileTypes:allowedTypes];
        }

        if ([panel runModal] == NSModalResponseOK) {
            result = std::string([[panel URL] fileSystemRepresentation]);
        }
    }
    return result;
}

std::string save_file(const std::string& title, const std::string& default_path, const std::string& default_filename, const std::vector<Filter>& filters) {
    std::string result;
    @autoreleasepool {
        NSSavePanel* panel = [NSSavePanel savePanel];
        [panel setTitle:[NSString stringWithUTF8String:title.c_str()]];
        [panel setNameFieldStringValue:[NSString stringWithUTF8String:default_filename.c_str()]];

        if (!default_path.empty()) {
            [panel setDirectoryURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:default_path.c_str()]]];
        }

        NSMutableArray* allowedTypes = [NSMutableArray array];
        for (const auto& f : filters) {
            std::string patterns = f.patterns;
            size_t pos = 0;
            while (pos < patterns.size()) {
                size_t next = patterns.find(';', pos);
                std::string pat = patterns.substr(pos, next - pos);
                pos = next == std::string::npos ? next : next + 1;
                if (pat == "*" || pat == "*.*") continue;
                if (pat.starts_with("*.")) {
                    [allowedTypes addObject:[NSString stringWithUTF8String:pat.substr(2).c_str()]];
                }
            }
        }
        if (allowedTypes.count > 0) {
            [panel setAllowedFileTypes:allowedTypes];
        }

        if ([panel runModal] == NSModalResponseOK) {
            result = std::string([[panel URL] fileSystemRepresentation]);
        }
    }
    return result;
}

std::string select_folder(const std::string& title, const std::string& default_path) {
    std::string result;
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        [panel setTitle:[NSString stringWithUTF8String:title.c_str()]];
        [panel setCanChooseFiles:NO];
        [panel setCanChooseDirectories:YES];
        [panel setAllowsMultipleSelection:NO];

        if (!default_path.empty()) {
            [panel setDirectoryURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:default_path.c_str()]]];
        }

        if ([panel runModal] == NSModalResponseOK) {
            result = std::string([[panel URL] fileSystemRepresentation]);
        }
    }
    return result;
}

} // namespace minifiledialog

#elif defined(__linux__)
#include <gtk/gtk.h>

namespace minifiledialog {

std::string open_file(const std::string& title, const std::string& default_path, const std::vector<Filter>& filters) {
    std::string result;
    if (!gtk_init_check(NULL, NULL)) return result;

    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        title.c_str(),
        NULL,
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL
    );

    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), default_path.c_str());

    for (const auto& f : filters) {
        GtkFileFilter* filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, f.description.c_str());
        std::string patterns = f.patterns;
        size_t pos = 0;
        while (pos < patterns.size()) {
            size_t next = patterns.find(';', pos);
            std::string pat = patterns.substr(pos, next - pos);
            pos = next == std::string::npos ? next : next + 1;
            gtk_file_filter_add_pattern(filter, pat.c_str());
        }
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    }

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        result = filename;
        g_free(filename);
    }

    gtk_widget_destroy(dialog);
    while (gtk_events_pending()) gtk_main_iteration();

    return result;
}

std::string save_file(const std::string& title, const std::string& default_path, const std::string& default_filename, const std::vector<Filter>& filters) {
    std::string result;
    if (!gtk_init_check(NULL, NULL)) return result;

    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        title.c_str(),
        NULL,
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save", GTK_RESPONSE_ACCEPT,
        NULL
    );

    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), default_path.c_str());
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), default_filename.c_str());
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

    for (const auto& f : filters) {
        GtkFileFilter* filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, f.description.c_str());
        std::string patterns = f.patterns;
        size_t pos = 0;
        while (pos < patterns.size()) {
            size_t next = patterns.find(';', pos);
            std::string pat = patterns.substr(pos, next - pos);
            pos = next == std::string::npos ? next : next + 1;
            gtk_file_filter_add_pattern(filter, pat.c_str());
        }
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    }

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        result = filename;
        g_free(filename);
    }

    gtk_widget_destroy(dialog);
    while (gtk_events_pending()) gtk_main_iteration();

    return result;
}

std::string select_folder(const std::string& title, const std::string& default_path) {
    std::string result;
    if (!gtk_init_check(NULL, NULL)) return result;

    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        title.c_str(),
        NULL,
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Select", GTK_RESPONSE_ACCEPT,
        NULL
    );

    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), default_path.c_str());

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        result = filename;
        g_free(filename);
    }

    gtk_widget_destroy(dialog);
    while (gtk_events_pending()) gtk_main_iteration();

    return result;
}

} // namespace minifiledialog

#endif

#endif // MINIFILEDIALOG_IMPLEMENTATION