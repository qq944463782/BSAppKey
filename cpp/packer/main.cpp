#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include <fstream>
#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

// 与 launcher/resource.h 一致（供 UpdateResource）
#define IDR_EMBED_PUBLIC 101
#define IDR_EMBED_PAYLOAD 102

static HWND g_st = nullptr;

static std::wstring ExeDir() {
  wchar_t p[MAX_PATH]{};
  GetModuleFileNameW(nullptr, p, MAX_PATH);
  std::wstring s = p;
  auto pos = s.find_last_of(L"\\/");
  if (pos != std::wstring::npos) s.resize(pos);
  return s;
}

static bool ReadWholeFile(const std::wstring& path, std::vector<uint8_t>& out, std::wstring& err) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    err = L"无法打开文件";
    return false;
  }
  f.seekg(0, std::ios::end);
  auto sz = f.tellg();
  if (sz <= 0) {
    err = L"文件为空";
    return false;
  }
  f.seekg(0, std::ios::beg);
  out.resize(static_cast<size_t>(sz));
  if (!f.read(reinterpret_cast<char*>(out.data()), sz)) {
    err = L"读取失败";
    return false;
  }
  return true;
}

static std::wstring FindShellTemplate() {
  std::wstring d = ExeDir();
  const wchar_t* names[] = {L"shell_template.exe", L"bl_launcher.exe"};
  for (auto* n : names) {
    std::wstring p = d + L"\\" + n;
    DWORD a = GetFileAttributesW(p.c_str());
    if (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY)) return p;
  }
  return {};
}

#pragma pack(push, 2)
struct GrpIconEntry {
  BYTE bWidth;
  BYTE bHeight;
  BYTE bColorCount;
  BYTE bReserved;
  WORD wPlanes;
  WORD wBitCount;
  DWORD dwBytesInRes;
  WORD nId;
};
#pragma pack(pop)

static bool LoadResourceBytes(HMODULE mod, LPCWSTR type, LPCWSTR name, WORD lang,
                              const void** outPtr, DWORD* outSz) {
  HRSRC hr = FindResourceExW(mod, type, name, lang);
  if (!hr) return false;
  DWORD sz = SizeofResource(mod, hr);
  HGLOBAL hg = LoadResource(mod, hr);
  if (!hg || sz == 0) return false;
  void* p = LockResource(hg);
  if (!p) return false;
  *outPtr = p;
  *outSz = sz;
  return true;
}

static bool FindIconData(HMODULE mod, WORD iconId, WORD groupLang, const void** outPtr, DWORD* outSz,
                         WORD* outLang) {
  const WORD tryLangs[] = {groupLang, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), 0, 1033};
  for (WORD lang : tryLangs) {
    if (LoadResourceBytes(mod, RT_ICON, MAKEINTRESOURCEW(iconId), lang, outPtr, outSz)) {
      if (outLang) *outLang = lang;
      return true;
    }
  }
  return false;
}

struct IconEnumCtx {
  HMODULE src;
  HANDLE hUpd;
  bool done;
};

static BOOL CALLBACK EnumGroupIconLang(HMODULE mod, LPCWSTR, LPCWSTR name, WORD lang, LONG_PTR lp) {
  auto* ctx = reinterpret_cast<IconEnumCtx*>(lp);
  const void* grp = nullptr;
  DWORD grpsz = 0;
  if (!LoadResourceBytes(mod, RT_GROUP_ICON, name, lang, &grp, &grpsz) || grpsz < 6) return TRUE;

  if (!UpdateResourceW(ctx->hUpd, RT_GROUP_ICON, name, lang, const_cast<void*>(grp), grpsz)) return TRUE;

  const auto* bytes = static_cast<const BYTE*>(grp);
  WORD idCount = *reinterpret_cast<const WORD*>(bytes + 4);
  size_t off = 6;
  for (WORD i = 0; i < idCount && off + sizeof(GrpIconEntry) <= grpsz; ++i) {
    const auto* ent = reinterpret_cast<const GrpIconEntry*>(bytes + off);
    WORD iconId = ent->nId;
    off += sizeof(GrpIconEntry);

    const void* idata = nullptr;
    DWORD isz = 0;
    WORD iconLang = lang;
    if (FindIconData(mod, iconId, lang, &idata, &isz, &iconLang))
      UpdateResourceW(ctx->hUpd, RT_ICON, MAKEINTRESOURCEW(iconId), iconLang, const_cast<void*>(idata), isz);
  }
  ctx->done = true;
  return FALSE;
}

static BOOL CALLBACK EnumGroupIconName(HMODULE mod, LPCWSTR, LPWSTR name, LONG_PTR lp) {
  auto* ctx = reinterpret_cast<IconEnumCtx*>(lp);
  EnumResourceLanguagesW(mod, RT_GROUP_ICON, name, EnumGroupIconLang, lp);
  return ctx->done ? FALSE : TRUE;
}

/** 把源 exe 的主图标组写入正在更新的目标 exe（失败则忽略，不阻断加壳） */
static void CopyIconsFromPayloadExe(const std::wstring& payloadPath, HANDLE hUpd) {
  HMODULE mod = LoadLibraryExW(payloadPath.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE);
  if (!mod) return;
  IconEnumCtx ctx{mod, hUpd, false};
  EnumResourceNamesW(mod, RT_GROUP_ICON, EnumGroupIconName, reinterpret_cast<LONG_PTR>(&ctx));
  FreeLibrary(mod);
}

static WORD DetectRcdataLang(const std::wstring& pePath, int resId) {
  HMODULE mod = LoadLibraryExW(pePath.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE);
  if (!mod) return MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
  WORD lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
  EnumResourceLanguagesW(
      mod, RT_RCDATA, MAKEINTRESOURCEW(resId),
      [](HMODULE, LPCWSTR, LPCWSTR, WORD wLang, LONG_PTR lp) -> BOOL {
        *reinterpret_cast<WORD*>(lp) = wLang;
        return FALSE;
      },
      reinterpret_cast<LONG_PTR>(&lang));
  FreeLibrary(mod);
  return lang;
}

static bool BuildProtected(const std::wstring& tmpl, const std::wstring& out,
                           const std::vector<uint8_t>& pub, const std::vector<uint8_t>& pay,
                           const std::wstring& payloadPath, std::wstring& err) {
  if (pub.size() < 200) {
    err = L"public.blb 过小，请使用注册机生成的公钥文件";
    return false;
  }
  if (pay.size() < 2 || pay[0] != 'M' || pay[1] != 'Z') {
    err = L"所选文件不是有效 Win32 exe（缺少 MZ）";
    return false;
  }
  if (!CopyFileW(tmpl.c_str(), out.c_str(), FALSE)) {
    err = L"复制模板失败（检查输出路径是否可写、文件是否被占用）";
    return false;
  }
  WORD lang = DetectRcdataLang(tmpl, IDR_EMBED_PUBLIC);
  HANDLE hUpd = BeginUpdateResourceW(out.c_str(), FALSE);
  if (!hUpd) {
    err = L"BeginUpdateResource 失败";
    DeleteFileW(out.c_str());
    return false;
  }
  BOOL okPub = UpdateResourceW(hUpd, RT_RCDATA, MAKEINTRESOURCEW(IDR_EMBED_PUBLIC), lang,
                               const_cast<uint8_t*>(pub.data()), static_cast<DWORD>(pub.size()));
  BOOL okPay = UpdateResourceW(hUpd, RT_RCDATA, MAKEINTRESOURCEW(IDR_EMBED_PAYLOAD), lang,
                               const_cast<uint8_t*>(pay.data()), static_cast<DWORD>(pay.size()));
  if (!okPub || !okPay) {
    EndUpdateResource(hUpd, TRUE);
    DeleteFileW(out.c_str());
    err = L"UpdateResource 失败（模板是否含 ID 101/102 的 RCDATA？请用本仓库编译的 bl_launcher）";
    return false;
  }
  CopyIconsFromPayloadExe(payloadPath, hUpd);
  if (!EndUpdateResource(hUpd, FALSE)) {
    DeleteFileW(out.c_str());
    err = L"EndUpdateResource 失败";
    return false;
  }
  return true;
}

static void SetStatus(const wchar_t* t) { SetWindowTextW(g_st, t); }

static void BrowseOpen(HWND wnd, int editId, const wchar_t* filter) {
  OPENFILENAMEW ofn{};
  wchar_t file[MAX_PATH] = {};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = wnd;
  ofn.lpstrFile = file;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFilter = filter;
  ofn.nFilterIndex = 1;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
  if (GetOpenFileNameW(&ofn)) SetDlgItemTextW(wnd, editId, file);
}

static void BrowseSave(HWND wnd, int editId) {
  OPENFILENAMEW ofn{};
  wchar_t file[MAX_PATH] = L"protected_app.exe";
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = wnd;
  ofn.lpstrFile = file;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFilter = L"Executable\0*.exe\0All\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
  if (GetSaveFileNameW(&ofn)) SetDlgItemTextW(wnd, editId, file);
}

static void OnBuild(HWND wnd) {
  wchar_t wIn[MAX_PATH]{}, wPub[MAX_PATH]{}, wOut[MAX_PATH]{};
  GetDlgItemTextW(wnd, 3001, wIn, MAX_PATH);
  GetDlgItemTextW(wnd, 3002, wPub, MAX_PATH);
  GetDlgItemTextW(wnd, 3003, wOut, MAX_PATH);
  if (!wIn[0] || !wPub[0] || !wOut[0]) {
    MessageBoxW(wnd, L"请填写：原始 exe、public.blb、输出 exe 路径。", L"加壳工具", MB_ICONWARNING);
    return;
  }
  std::wstring tmpl = FindShellTemplate();
  if (tmpl.empty()) {
    MessageBoxW(
        wnd,
        L"未找到壳模板：请将「shell_template.exe」或「bl_launcher.exe」放在本工具同一目录。\n"
        L"（通常先编译 BlLauncher，再把 bl_launcher.exe 复制为 shell_template.exe）",
        L"加壳工具", MB_ICONERROR);
    return;
  }
  std::vector<uint8_t> pub, pay;
  std::wstring err;
  SetStatus(L"正在读取文件…");
  if (!ReadWholeFile(wPub, pub, err)) {
    SetStatus(L"就绪");
    MessageBoxW(wnd, (L"读取 public.blb 失败：" + err).c_str(), L"加壳工具", MB_ICONERROR);
    return;
  }
  if (!ReadWholeFile(wIn, pay, err)) {
    SetStatus(L"就绪");
    MessageBoxW(wnd, (L"读取原始 exe 失败：" + err).c_str(), L"加壳工具", MB_ICONERROR);
    return;
  }
  SetStatus(L"正在写入资源并生成…");
  if (!BuildProtected(tmpl, wOut, pub, pay, wIn, err)) {
    SetStatus(L"就绪");
    MessageBoxW(wnd, (L"生成失败：" + err).c_str(), L"加壳工具", MB_ICONERROR);
    return;
  }
  SetStatus(L"完成");
  MessageBoxW(wnd, L"已生成带授权的 exe，可发给客户。请勿泄露 private.blb。", L"加壳工具",
              MB_ICONINFORMATION);
}

static LRESULT CALLBACK WndProc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_CREATE: {
      int y = 12;
      CreateWindowW(L"STATIC", L"① 你的原始程序（已编译 exe）：", WS_CHILD | WS_VISIBLE, 12, y, 400, 18, wnd,
                    nullptr, nullptr, nullptr);
      y += 22;
      CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 12, y, 420,
                      24, wnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(3001)), nullptr, nullptr);
      CreateWindowW(L"BUTTON", L"浏览…", WS_CHILD | WS_VISIBLE, 440, y - 2, 72, 28, wnd,
                    reinterpret_cast<HMENU>(static_cast<UINT_PTR>(4001)), nullptr, nullptr);
      y += 36;
      CreateWindowW(L"STATIC", L"② 公钥 public.blb（注册机同目录生成）：", WS_CHILD | WS_VISIBLE, 12, y, 440,
                    18, wnd, nullptr, nullptr, nullptr);
      y += 22;
      CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 12, y, 420,
                      24, wnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(3002)), nullptr, nullptr);
      CreateWindowW(L"BUTTON", L"浏览…", WS_CHILD | WS_VISIBLE, 440, y - 2, 72, 28, wnd,
                    reinterpret_cast<HMENU>(static_cast<UINT_PTR>(4002)), nullptr, nullptr);
      y += 36;
      CreateWindowW(L"STATIC", L"③ 输出给客户用的加壳 exe（保存路径）：", WS_CHILD | WS_VISIBLE, 12, y, 440, 18,
                    wnd, nullptr, nullptr, nullptr);
      y += 22;
      CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 12, y, 420,
                      24, wnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(3003)), nullptr, nullptr);
      CreateWindowW(L"BUTTON", L"浏览…", WS_CHILD | WS_VISIBLE, 440, y - 2, 72, 28, wnd,
                    reinterpret_cast<HMENU>(static_cast<UINT_PTR>(4003)), nullptr, nullptr);
      y += 40;
      CreateWindowW(L"BUTTON", L"一键生成加壳 exe", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 12, y, 200, 36,
                    wnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(4004)), nullptr, nullptr);
      y += 48;
      g_st = CreateWindowW(L"STATIC", L"就绪。模板需与本程序同目录：shell_template.exe 或 bl_launcher.exe",
                           WS_CHILD | WS_VISIBLE, 12, y, 520, 36, wnd,
                           reinterpret_cast<HMENU>(static_cast<UINT_PTR>(3008)), nullptr, nullptr);
      return 0;
    }
    case WM_COMMAND: {
      if (HIWORD(wp) == BN_CLICKED) {
        switch (LOWORD(wp)) {
          case 4001:
            BrowseOpen(wnd, 3001, L"Programs\0*.exe\0All\0*.*\0");
            break;
          case 4002:
            BrowseOpen(wnd, 3002, L"Key blob\0*.blb\0All\0*.*\0");
            break;
          case 4003:
            BrowseSave(wnd, 3003);
            break;
          case 4004:
            OnBuild(wnd);
            break;
        }
      }
      return 0;
    }
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProcW(wnd, msg, wp, lp);
  }
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int show) {
  INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES};
  InitCommonControlsEx(&icc);

  WNDCLASSW wc{};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hi;
  wc.lpszClassName = L"BsphpVmpPacker";
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  RegisterClassW(&wc);

  HWND w = CreateWindowExW(0, L"BsphpVmpPacker",
                           L"bsphp.com 验证系统安全防护学习 · 授权加壳工具（开发商使用）",
                           WS_OVERLAPPED | WS_CAPTION |
                                                                  WS_SYSMENU | WS_MINIMIZEBOX,
                           CW_USEDEFAULT, CW_USEDEFAULT, 560, 320, nullptr, nullptr, hi, nullptr);
  ShowWindow(w, show);

  MSG m{};
  while (GetMessageW(&m, nullptr, 0, 0) > 0) {
    TranslateMessage(&m);
    DispatchMessageW(&m);
  }
  return 0;
}
