#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include "hwid.h"
#include "license.h"
#include "resource.h"
#include "rsa_cng.h"

#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

static HWND g_hwid = nullptr;
static HWND g_auth = nullptr;
static void* g_pubKey = nullptr;
static void* g_pubAlg = nullptr;

/** RSA 2048 public CNG blob 通常约 270+ 字节 */
static constexpr DWORD kEmbedPublicMinBytes = 200;

static bool LoadRcData(unsigned id, std::vector<uint8_t>& out) {
  HMODULE mod = GetModuleHandleW(nullptr);
  HRSRC hr = FindResourceW(mod, MAKEINTRESOURCEW(id), RT_RCDATA);
  if (!hr) return false;
  DWORD sz = SizeofResource(mod, hr);
  if (sz == 0) return false;
  HGLOBAL hg = LoadResource(mod, hr);
  if (!hg) return false;
  const auto* p = static_cast<const uint8_t*>(LockResource(hg));
  if (!p) return false;
  out.assign(p, p + sz);
  return true;
}

static bool LooksLikePe(const std::vector<uint8_t>& b) {
  return b.size() >= 2 && b[0] == 'M' && b[1] == 'Z';
}

static std::wstring ExeDir() {
  wchar_t p[MAX_PATH]{};
  GetModuleFileNameW(nullptr, p, MAX_PATH);
  std::wstring s = p;
  auto pos = s.find_last_of(L"\\/");
  if (pos != std::wstring::npos) s.resize(pos);
  return s;
}

static std::string WideToUtf8(std::wstring_view w) {
  if (w.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr,
                              nullptr);
  if (n <= 0) return {};
  std::string s(static_cast<size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
  return s;
}

static std::wstring Utf8ToWide(const std::string& u) {
  if (u.empty()) return L"";
  int n = MultiByteToWideChar(CP_UTF8, 0, u.c_str(), static_cast<int>(u.size()), nullptr, 0);
  if (n <= 0) return L"";
  std::wstring w(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, u.c_str(), static_cast<int>(u.size()), w.data(), n);
  return w;
}

static void LoadPublicKeyOnce() {
  if (g_pubKey) return;
  std::wstring err;
  std::vector<uint8_t> embed;
  if (LoadRcData(IDR_EMBED_PUBLIC, embed) && embed.size() >= kEmbedPublicMinBytes) {
    if (RsaLoadPublicKeyFromMemory(embed.data(), embed.size(), &g_pubKey, &g_pubAlg, err))
      return;
  }
  std::wstring path = ExeDir() + L"\\public.blb";
  if (RsaLoadPublicKeyFromFile(path, &g_pubKey, &g_pubAlg, err))
    return;
  MessageBoxW(nullptr,
              (L"无法加载公钥：资源内嵌无效且未找到同目录 public.blb。\n" + err).c_str(),
              L"授权", MB_ICONERROR);
}

static void OnActivate(HWND wnd) {
  LoadPublicKeyOnce();
  if (!g_pubKey) return;

  wchar_t buf[8192]{};
  GetWindowTextW(g_auth, buf, static_cast<int>(sizeof(buf) / sizeof(buf[0])));
  std::string token = WideToUtf8(buf);
  while (!token.empty() && (token.back() == '\n' || token.back() == '\r' || token.back() == ' '))
    token.pop_back();

  std::string hw = HwidFingerprintUtf8();
  std::string err;
  if (!LicenseVerifyToken(token, g_pubKey, hw, err)) {
    std::wstring werr = Utf8ToWide(err);
    MessageBoxW(wnd, werr.c_str(), L"授权失败", MB_ICONWARNING);
    return;
  }

  std::wstring dir = ExeDir();
  std::wstring pay;
  bool payloadFromTemp = false;
  std::vector<uint8_t> emb;
  if (LoadRcData(IDR_EMBED_PAYLOAD, emb) && LooksLikePe(emb)) {
    wchar_t tmp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmp);
    pay = std::wstring(tmp) + L"bsphp_payload_" + std::to_wstring(GetCurrentProcessId()) + L"_" +
          std::to_wstring(GetTickCount64()) + L".exe";
    HANDLE h = CreateFileW(pay.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                           nullptr);
    if (h == INVALID_HANDLE_VALUE) {
      MessageBoxW(wnd, L"无法写入临时目录释放 payload。", L"启动失败", MB_ICONERROR);
      return;
    }
    DWORD wn = 0;
    BOOL ok = WriteFile(h, emb.data(), static_cast<DWORD>(emb.size()), &wn, nullptr);
    CloseHandle(h);
    if (!ok || wn != emb.size()) {
      DeleteFileW(pay.c_str());
      MessageBoxW(wnd, L"释放内嵌 payload 失败。", L"启动失败", MB_ICONERROR);
      return;
    }
    payloadFromTemp = true;
  } else {
    pay = dir + L"\\payload.exe";
  }

  HINSTANCE hi = ShellExecuteW(wnd, L"open", pay.c_str(), nullptr, dir.c_str(), SW_SHOW);
  if (reinterpret_cast<INT_PTR>(hi) <= 32) {
    if (payloadFromTemp) DeleteFileW(pay.c_str());
    MessageBoxW(wnd, L"无法启动程序：内嵌无效且同目录无有效 payload.exe。", L"启动失败", MB_ICONERROR);
    return;
  }
  DestroyWindow(wnd);
}

static LRESULT CALLBACK WndProc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_CREATE: {
      CreateWindowW(L"STATIC", L"机器码（发给作者注册）：", WS_CHILD | WS_VISIBLE, 20, 16, 360, 20, wnd,
                    nullptr, nullptr, nullptr);
      g_hwid = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL,
                               20, 40, 520, 28, wnd, nullptr, nullptr, nullptr);
      CreateWindowW(L"STATIC", L"授权码（粘贴注册机生成的 Base64）：", WS_CHILD | WS_VISIBLE, 20, 80, 400, 20,
                    wnd, nullptr, nullptr, nullptr);
      g_auth = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                               WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
                               20, 104, 520, 120, wnd, nullptr, nullptr, nullptr);
      CreateWindowW(L"BUTTON", L"验证并启动", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 20, 240, 160, 36,
                    wnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(1)), nullptr, nullptr);
      std::wstring h = Utf8ToWide(HwidFingerprintUtf8());
      SetWindowTextW(g_hwid, h.c_str());
      return 0;
    }
    case WM_COMMAND:
      if (LOWORD(wp) == 1 && HIWORD(wp) == BN_CLICKED) OnActivate(wnd);
      return 0;
    case WM_DESTROY:
      RsaFreeKey(g_pubKey, g_pubAlg);
      g_pubKey = nullptr;
      g_pubAlg = nullptr;
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
  wc.lpszClassName = L"BsphpVmpLauncher";
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  RegisterClassW(&wc);

  HWND w = CreateWindowExW(0, L"BsphpVmpLauncher", L"软件授权",
                           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT,
                           CW_USEDEFAULT, 580, 340, nullptr, nullptr, hi, nullptr);
  ShowWindow(w, show);

  MSG m{};
  while (GetMessageW(&m, nullptr, 0, 0) > 0) {
    TranslateMessage(&m);
    DispatchMessageW(&m);
  }
  return 0;
}
